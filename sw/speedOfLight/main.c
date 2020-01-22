
// you just lost the game
// - josh & the bartenders

// code would be prettier if i had more time, i promise
// - josh     B)
#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include <stdlib.h>			// need RNG
#include <stdio.h>

#include "usbQc.h"
#include "buttons.h"


//********************** def hw specific params

#define Player1ButtonX 3
#define Player1ButtonY 0

#define Player2ButtonX 2
#define Player2ButtonY 0

#define DebugButtonX 0
#define DebugButtonY 1


#define MultiplierDecaySeconds 1
#define MultiplierDecayTicks 0	// TODO check that ticks change it

#define AttractPatternCount 1

//********************* eeprom locations for game vars

#define EEP_ADDR_HighScore1P_H 0x00
#define EEP_ADDR_HighScore1P_L 0x01

#define EEP_ADDR_HighScore2P_H 0x02
#define EEP_ADDR_HighScore2P_L 0x03

#define EEP_ADDR_RoundTime 0x10
#define EEP_ADDR_BonusTime 0x20

#define EEP_ADDR_RandomSeed_H 0x40
#define EEP_ADDR_RandomSeed_L 0x41

#define EEP_ADDR_MultMax 0x50
#define EEP_ADDR_BonusPtCount 0x60
#define EEP_ADDR_SoundEnabled 0x70

#define EEP_ADDR_MultTimeKill_H 0x80
#define EEP_ADDR_MultTimeKill_L 0x81


//********************** def prototypes

uint8_t GameMode;			// controls next game mode (0 for 1p, 1 for 2p)
uint8_t RoundTime;
uint8_t BonusTime;
volatile uint8_t TimeRemaining;
uint16_t HighScore1P, HighScore2P;

uint8_t gameledsX[6];
uint8_t gameledsY[6];

uint16_t P1Score, P2Score;
uint8_t P1Multiplier, P2Multiplier;
uint8_t P1MultTimeS, P2MultTimeS;		// for multiplier decay keep track of seconds
uint16_t P1MultTimeT, P2MultTimeT;		// as well as "sub-seconds" (timer ticks)
uint8_t MultiplierMax;
uint8_t BonusPointCount;
uint8_t SoundEnabled;
uint16_t MultTimeKill;


volatile uint8_t soundPlaying;
volatile uint16_t beep_index = 0;
volatile uint16_t note_index = 0;
const uint16_t notes[] = {4545, 4050, 3822, 3405, 3034, 2863, 2551, 2273, 2024, 1911, 1703, 1517,
			  1431, 1351, 1276, 1203, 1136, 1073, 1012, 956, 901, 851, 804, 758, 716, 676, 638, 602};


void Game();
void Display321();
void Bonus();
void Attractive();
void debugMode();
uint8_t AttractCheckGameStart(uint16_t count);

void ShowWinner();
void playChirp(uint8_t);
void checkMultiplers();
void IncrementScore(uint8_t, uint16_t);

void EEPROM_write(uint16_t uiAddress, uint8_t ucData);
uint8_t EEPROM_read(uint16_t uiAddress);

static int uart_putchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
	uart_putchar('\r', stream);
	while(!(UCSR0A &( 1 << UDRE0)));
	UDR0 = c;
	return 0;
}

void init_uart(){
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (3<<UCSZ00);
	UBRR0L = 51;
}

int main(void)
{
	usbQcInit();
	QCset12V();
	
	buttonsInit();
	init_uart();
	stdout = &mystdout;
	printf("\n\nWelcome to bartending robot OS ver 4.7B\n");
	
	sei();
	/*
	uint8_t i = 0;
	while(1){
	setScore(LEFT, i++);
	setScore(TIMER, i);
	//setScore(RIGHT, i);
	setScoreSegment(8, 0x0F);
	setScoreSegment(7, 0x10);
	setScoreSegment(6, 0x11);
	_delay_ms(100);
	}*/
	
	// initialize timer0 for audio circuit
	DDRB |= (1 << DDB1) | (1 << DDB5); // output speaker pin B1
	ICR1 = 0;	// TOP of counter, start OFF
	OCR1A = 0; //0x1FFF;	// OFF at
	TCCR1A |= (1 << COM1A1) | (1 << WGM11); //|(0 << COM1B1);
	TCCR1B |= (1 << WGM12 ) | (1 << WGM13) | (1 << CS41);
	PORTB &= ~(1 << 1);
	
	TCCR0A = (1 << WGM01);	// enable CTC mode timer 0
	TCCR0B = 0;		//disable clock for timer0, as it will be enabled when "beeped"
	OCR0A = 125;
	TIMSK0 = ( 1 << OCIE0A );
	
	printf("AUDIO TIMERS & PORT SET\n");

	//load values from eeprom
	
	

	HighScore1P = (EEPROM_read(EEP_ADDR_HighScore1P_H) << 8) | (EEPROM_read(EEP_ADDR_HighScore1P_L));
	HighScore2P = (EEPROM_read(EEP_ADDR_HighScore2P_H) << 8) | (EEPROM_read(EEP_ADDR_HighScore2P_L));
	RoundTime = EEPROM_read(EEP_ADDR_RoundTime);
	BonusTime = EEPROM_read(EEP_ADDR_BonusTime);
	MultiplierMax = EEPROM_read(EEP_ADDR_MultMax);
	BonusPointCount = EEPROM_read(EEP_ADDR_BonusPtCount);
	SoundEnabled = EEPROM_read(EEP_ADDR_SoundEnabled);
	MultTimeKill = (EEPROM_read(EEP_ADDR_MultTimeKill_H) << 8) | (EEPROM_read(EEP_ADDR_MultTimeKill_L));
	
	// load random value from eeprom as seed and write new random value
	srand((EEPROM_read(EEP_ADDR_RandomSeed_H) << 8) | EEPROM_read(EEP_ADDR_RandomSeed_L));
	EEPROM_write(EEP_ADDR_RandomSeed_H, rand()%256 );
	EEPROM_write(EEP_ADDR_RandomSeed_L, rand()%256 );
	
	printf("EEPROM VALUES SET:\n");
	printf("HS1P: %d \t HS2P: %d \t RT: %d \t BT: %d \t MMAX: %d \t BPC: %d \t SND: %d \t MulTck: %d \n",HighScore1P, HighScore2P, RoundTime, BonusTime, MultiplierMax, BonusPointCount, SoundEnabled, MultTimeKill);
	printf("RANDOMSEED: 0x%X%X\n\n", EEPROM_read(EEP_ADDR_RandomSeed_H), EEPROM_read(EEP_ADDR_RandomSeed_L));
	
	
	
	
	// set up timer4 for game timer
	TCCR4B = (1 << WGM42)|(0b101 << CS40);	// enable timer 0 (game timer)
	TIMSK4 = (1 << OCIE4A);
	OCR4A = 15625;			// set up for 1s

	sei();

	/*	while(1){
	for(int x = 0; x < 6; x++){
	for(int y = 0; y < 6; y++){
	setButtonLed(x,y,isButtonDown(x,y));
	if(isButtonDown(x,y)){
	printf("%d, %d\n\n",x,y);
	}
	
	}
	}
	}*/
	
	for(uint8_t dispindex = 0; dispindex < 9; dispindex++){
		setScoreSegment(dispindex, 18);
	}
	if(isButtonDown(DebugButtonX, DebugButtonY)){
		debugMode();
	}

	while (1)
	{
		printf("\n\n\nSTARTING ATTRACT MODE\n");
		Attractive();		// loop thru patterns until game start is pressed (TODO IMPLEMENT DEBUG PATTERN)
		printf("STARTING GAME\n");
		Game();
		_delay_ms(750);
		Bonus();
		
		if(GameMode == 0){
			if(P1Score > HighScore1P){
				HighScore1P = P1Score;
				printf("new high score %d", HighScore1P);
				EEPROM_write(EEP_ADDR_HighScore1P_H, HighScore1P >> 8);
				EEPROM_write(EEP_ADDR_HighScore1P_L, HighScore1P & 0xFF);
				
				ShowWinner();			//if new high score, flash screen
			}
			}else{
			if((P1Score > HighScore2P) || (P2Score > HighScore2P)){
				HighScore2P = (P1Score > P2Score ? P1Score : P2Score);
				EEPROM_write(EEP_ADDR_HighScore2P_H, HighScore2P >> 8);
				EEPROM_write(EEP_ADDR_HighScore2P_L, HighScore2P & 0xFF);
			}
			ShowWinner();				//if 2 player mode show winner anyway
		}
		
		_delay_ms(750);
		

	}
}

ISR(TIMER0_COMPA_vect)		// audio interrupt
{
	DDRB |= (1<<1);
	soundPlaying = 1;
	switch(note_index)
	{	//every 8ms
		
		case 0+1:	//0ms, 8ms on
		ICR1 = notes[beep_index];	//1st note
		OCR1A = notes[beep_index]/2;
		break;
		case 2+1:	// 16ms, 24, 32 on
		ICR1 = notes[beep_index+1];	//2nd note
		OCR1A = notes[beep_index]/2;
		break;
		case 5+1:	// 40ms, 48, 56, 64, 72, 80, 88, 96, on
		ICR1 = notes[beep_index+2];	// 3rd note
		OCR1A = notes[beep_index]/2;
		break;
		case 13+1:
		//ICR1 = 0;					//off
		OCR1A = 0;
		break;
		
		case 63+1:
		beep_index = 0;
		note_index = -1;
		TCCR0B &= ~(0b101 << CS00);
		TCNT0 = 0;
		DDRB &= ~(1<<1);
		soundPlaying = 0;
		break;
	}
	note_index += 1;
}

ISR(TIMER4_COMPA_vect){		// use timer 4 for decreasing game time every 1s
	if(TimeRemaining > 0){
		TimeRemaining--;
	}
}

void debugMode(){
	uint8_t page = 0;
	uint8_t maxpages = 7;
	
	DDRB |= (1<<1);
	ICR1 = notes[0];		// play debug tone
	OCR1A = notes[0] / 2;
	_delay_ms(100);
	ICR1 = notes[1];
	OCR1A = notes[1] / 2;
	_delay_ms(100);
	ICR1 = notes[0];
	OCR1A = notes[0] / 2;
	_delay_ms(100);
	ICR1 = notes[1];
	OCR1A = notes[1] / 2;
	_delay_ms(100);
	DDRB &= ~(1<<1);
	
	while(isButtonDown(0,1)){
		// CLOSE and OPEN buttons are the same, wait for initial release
	}
	_delay_ms(50);
	
	while(1){
		
		switch(page){
			case 0:	//high score 1p
			setScoreSegment(RIGHT100, 19);	//HI-SC1
			setScoreSegment(RIGHT10, 1);
			setScoreSegment(RIGHT1, 18);
			setScoreSegment(TIMER100, 5);
			setScoreSegment(TIMER10, 0xC);
			setScoreSegment(TIMER1, 1);
			setScore(LEFT, HighScore1P);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				HighScore1P = 0;
				setScore(LEFT, HighScore1P);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				HighScore1P = (HighScore1P + 1) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, HighScore1P);
					if(TCNT4 > 6500){
						HighScore1P = (HighScore1P + 1) % 1000;
						TCNT4 = 6250;
					}
				}	
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				HighScore1P = (HighScore1P + 999) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, HighScore1P);
					if(TCNT4 > 6500){
						HighScore1P = (HighScore1P + 999) % 1000;
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END HIGH SCORE 1p
			
			
			
			
			case 1:	//high score 2p
			setScoreSegment(RIGHT100, 19);	//HI-SC2
			setScoreSegment(RIGHT10, 1);
			setScoreSegment(RIGHT1, 18);
			setScoreSegment(TIMER100, 5);
			setScoreSegment(TIMER10, 0xC);
			setScoreSegment(TIMER1, 2);
			setScore(LEFT, HighScore2P);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				HighScore2P = 0;
				setScore(LEFT, HighScore1P);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				HighScore2P = (HighScore2P + 1) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, HighScore2P);
					if(TCNT4 > 6500){
						HighScore2P = (HighScore2P + 1) % 1000;
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				HighScore2P = (HighScore2P + 999) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, HighScore2P);
					if(TCNT4 > 6500){
						HighScore2P = (HighScore2P + 999) % 1000;
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END HIGH SCORE 2p
			
			
			
			case 2:	//round time
			setScoreSegment(RIGHT100, 20);	//rnd tn
			setScoreSegment(RIGHT10, 21);
			setScoreSegment(RIGHT1, 0xd);
			setScoreSegment(TIMER100, 16);
			setScoreSegment(TIMER10, 22);
			setScoreSegment(TIMER1, 21);
			setScore(LEFT, RoundTime);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				RoundTime = 40;
				setScore(LEFT, RoundTime);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				RoundTime = (RoundTime + 1);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, RoundTime);
					if(TCNT4 > 6500){
						RoundTime = (RoundTime + 1);
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				RoundTime = (RoundTime + 255);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, RoundTime);
					if(TCNT4 > 6500){
						RoundTime = (RoundTime + 255);
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END ROUND TIME
			
			
			
			
			case 3:	//bonus time
			setScoreSegment(RIGHT100, 0xb);	//rnd tn
			setScoreSegment(RIGHT10, 21);
			setScoreSegment(RIGHT1, 5);
			setScoreSegment(TIMER100, 16);
			setScoreSegment(TIMER10, 22);
			setScoreSegment(TIMER1, 21);
			setScore(LEFT, BonusTime);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				BonusTime = 3;
				setScore(LEFT, BonusTime);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				BonusTime = (BonusTime + 1);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, BonusTime);
					if(TCNT4 > 6500){
						BonusTime = (BonusTime + 1);
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				BonusTime = (BonusTime + 255);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, BonusTime);
					if(TCNT4 > 6500){
						BonusTime = (BonusTime + 255);
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END ROUND TIME
			
			
			
			
			case 4:	//multiplier ticks
			setScoreSegment(RIGHT100, 0xE);	//3ul tck
			setScoreSegment(RIGHT10, 23);
			setScoreSegment(RIGHT1, 1);
			setScoreSegment(TIMER100, 22);
			setScoreSegment(TIMER10, 0xc);
			setScoreSegment(TIMER1, 0xc);
			setScore(LEFT, MultTimeKill);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				MultTimeKill = 999;
				setScore(LEFT, MultTimeKill);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				MultTimeKill = (MultTimeKill + 1) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, MultTimeKill);
					if(TCNT4 > 6500){
						MultTimeKill = (MultTimeKill + 1) % 1000;
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				MultTimeKill = (MultTimeKill + 999) % 1000;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, MultTimeKill);
					if(TCNT4 > 6500){
						MultTimeKill = (MultTimeKill + 999) % 1000;
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END MULT TICK
			
			
			
			
			
			case 5:	//bonus round points
			setScoreSegment(RIGHT100, 0xb);	//3ul tck
			setScoreSegment(RIGHT10, 21);
			setScoreSegment(RIGHT1, 5);
			setScoreSegment(TIMER100, 24);
			setScoreSegment(TIMER10, 22);
			setScoreSegment(TIMER1, 5);
			setScore(LEFT, BonusPointCount);
			
			setButtonLed(2,2,1);		// RESET score led
			
			if(isButtonDown(2,2)){
				BonusPointCount = 2;
				setScore(LEFT, BonusPointCount);
				DDRB |= (1<<1);	//play reset
				ICR1 = notes[3];
				OCR1A = notes[3] / 2;
				_delay_ms(35);
				DDRB &= ~(1<<1);
				while(isButtonDown(2,2))
				{
					//wait for unpush
				}
			}
			
			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				BonusPointCount = (BonusPointCount + 1);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, BonusPointCount);
					if(TCNT4 > 6500){
						BonusPointCount = (BonusPointCount + 1);
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				BonusPointCount = (BonusPointCount + 255);
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, BonusPointCount);
					if(TCNT4 > 6500){
						BonusPointCount = (BonusPointCount + 255);
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END BONUS PT COUNT
			
			
			
			
			
			
			case 6:	//sound on
			setScoreSegment(RIGHT100, 5);
			setScoreSegment(RIGHT10, 21);
			setScoreSegment(RIGHT1, 0xd);
			setScoreSegment(TIMER100, 16);
			setScoreSegment(TIMER10, 16);
			setScoreSegment(TIMER1, 16);
			
			setScore(LEFT, SoundEnabled);

			if(isButtonDown(1,3)){	//UP
				TCNT4 = 0;
				SoundEnabled = (SoundEnabled + 1) % 2;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[2];
				OCR1A = notes[2] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,3)){
					setScore(LEFT, SoundEnabled);
					if(TCNT4 > 6500){
						SoundEnabled = (SoundEnabled + 1) % 2;
						TCNT4 = 6250;
					}
				}
			}
			if(isButtonDown(1,4)){	//DOWN
				TCNT4 = 0;
				SoundEnabled = (SoundEnabled + 1) % 2;
				
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[1];
				OCR1A = notes[1] / 2;
				_delay_ms(15);
				DDRB &= ~(1<<1);
				
				while(isButtonDown(1,4)){
					setScore(LEFT, SoundEnabled);
					if(TCNT4 > 6500){
						SoundEnabled = (SoundEnabled + 1) % 2;
						TCNT4 = 6250;
					}
				}
			}
			
			break;		//*************END SOUND ON
			
			
			
			
			
		}





		setButtonLed(0,4,1);	//NEXT button
		setButtonLed(2,4,1);	//PREV button
		setButtonLed(1,3,1);	//UP button
		setButtonLed(1,4,1);	//DOWN button
		setButtonLed(0,1,1);	//CLOSE button
		
		if(isButtonDown(0,4)){	// goto next page
			page = (page+1) % maxpages;
			
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[0];
			OCR1A = notes[0] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			
			while(isButtonDown(0,4)){
				;				//wait until button release
			}
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[1];
			OCR1A = notes[1] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			
			clearLeds(0);
			
		}
		if(isButtonDown(2,4)){	// goto next page
			page = (page + maxpages - 1) % maxpages;
			
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[0];
			OCR1A = notes[0] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			
			while(isButtonDown(2,4)){
				;				//wait until button release
			}
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[1];
			OCR1A = notes[1] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			
			clearLeds(0);
		}
		if(isButtonDown(0,1)){	//CLOSE debug
			//first SAVE all values
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[3];
			OCR1A = notes[3] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			_delay_ms(35);
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[3];
			OCR1A = notes[3] / 2;
			_delay_ms(35);
			DDRB &= ~(1<<1);
			
			EEPROM_write(EEP_ADDR_HighScore1P_H, HighScore1P >> 8);
			EEPROM_write(EEP_ADDR_HighScore1P_L, HighScore1P & 0xFF);
			EEPROM_write(EEP_ADDR_HighScore2P_H, HighScore2P >> 8);
			EEPROM_write(EEP_ADDR_HighScore2P_L, HighScore2P & 0xFF);
			EEPROM_write(EEP_ADDR_RoundTime, RoundTime);
			EEPROM_write(EEP_ADDR_BonusTime, BonusTime);
			EEPROM_write(EEP_ADDR_MultTimeKill_H, MultTimeKill >> 8);
			EEPROM_write(EEP_ADDR_MultTimeKill_L, MultTimeKill & 0xFF);
			EEPROM_write(EEP_ADDR_SoundEnabled, SoundEnabled);
			EEPROM_write(EEP_ADDR_BonusPtCount, BonusPointCount);
			
			return;
		}
	}
}

void Game(){

	// game mode is set when leaving attract mode
	Display321();
	printf("GAME START\n");
	
	P1Score = 0;			// reset scores and multipliers
	P2Score = 0;
	P1Multiplier = 1;
	P2Multiplier = 1;

	// pick 6 random leds to enable regardless of mode
	// (3 left side 3 right side)
	for(int i = 0; i < 3; i++){
		gameledsX[i] = rand() % 3;			// turn 3 on right side
		gameledsY[i] = 1+ (rand() % 5);
		
		gameledsX[i+3] = 3 + rand() % 3;	// turn 3 on left side
		gameledsY[i+3] = 1+(rand() % 5);
	}
	
	uint16_t ticksrem;			// used in resetting multiplier
	
	TimeRemaining = RoundTime;
	printf("round time %d\n", TimeRemaining);
	TCNT4 = 0;					// reset timer
	
	setScore(0, HighScore1P);	// will be replaced if 2p mode
	uint8_t lastTime = TimeRemaining;
	while(TimeRemaining > 0){
		
		
		if((SoundEnabled) & (lastTime != TimeRemaining) & (soundPlaying == 0)){
			lastTime = TimeRemaining;
			DDRB |= (1<<1);	//make ticking noise
			ICR1 = notes[4 + (TimeRemaining%2)];
			OCR1A = notes[4 + (TimeRemaining%2)] / 2;
			_delay_us(3500);
			DDRB &= ~(1<<1);
		}
		//printf("%ds REMAINING\n", TimeRemaining);
		
		setScore(1, TimeRemaining);
		if(GameMode == 1){
			setScore(0, P1Score);	// if 2p mode, show left p1 and right p2 scores
			setScore(2, P2Score);
			}else{
			setScore(2, P1Score);	// otherwise show p1 score and high score
		}

		for(uint8_t i = 0; i < 6; i++){
			if (isButtonDown(gameledsX[i], gameledsY[i]) ){

				uint8_t newX, newY;
				while(1){				// repeatedly search for new open location
					if(i >= 3){
						//gameLedsX[i]
						newX = 3 + (rand() % 3);
						}else{
						newX = rand() % 3;
					}
					newY = 1 + (rand() % 5);
					//gameLedsY
					
					uint8_t clearSpot = 1;
					for(uint8_t idx = 0; idx < 6; idx++){
						if((gameledsX[idx] == newX) && (gameledsY[idx] == newY)){
							clearSpot = 0;	// "new" location is in use
						}
					}
					if(clearSpot){
						gameledsX[i] = newX;
						gameledsY[i] = newY;
						break;
					}
					
				}

				if(GameMode == 0){
					IncrementScore(0, P1Multiplier);	// add score and inc multiplier (singleplayer)
					playChirp(P1Multiplier);
					
					P1Multiplier = (P1Multiplier == MultiplierMax) ? MultiplierMax : (P1Multiplier + 1);
					ticksrem = 15*MultTimeKill;
					P1MultTimeT = OCR4A - TCNT4;	// subseconds counting down
					if(ticksrem > P1MultTimeT){		//
						P1MultTimeS = TimeRemaining - MultiplierDecaySeconds;
						P1MultTimeT = (OCR4A - (ticksrem - P2MultTimeT));
						}else{
						P1MultTimeS = TimeRemaining;
						P1MultTimeT = P1MultTimeT - ticksrem;	// this second, earlier
					}
					
					}else{
					if(i >= 3){
						IncrementScore(1, P2Multiplier);	// add depending on side of button pressed
						playChirp(P2Multiplier);
						
						P2Multiplier = (P2Multiplier == MultiplierMax) ? MultiplierMax : (P2Multiplier + 1);
						
						ticksrem = 15*MultTimeKill;
						P2MultTimeT = OCR4A - TCNT4;	// subseconds counting down
						if(ticksrem > P2MultTimeT){		// 
							P2MultTimeS = TimeRemaining - MultiplierDecaySeconds;
							P2MultTimeT = (OCR4A - (ticksrem - P2MultTimeT));
						}else{
							P2MultTimeS = TimeRemaining;
							P2MultTimeT = P2MultTimeT - ticksrem;	// this second, earlier
						}
						
						}else{
						IncrementScore(0, P1Multiplier);
						playChirp(P1Multiplier);
						
						P1Multiplier = (P1Multiplier == MultiplierMax) ? MultiplierMax : (P1Multiplier + 1);
						
						
						
					}
				}
			}
		}
		
		
		
		
		if (P1Multiplier > 1){
			uint16_t tcountdown = OCR4A - TCNT4;
			if((TimeRemaining == P1MultTimeS) && (tcountdown <= P1MultTimeT)){
				P1Multiplier = 1;
			}
		}
		
		if (P2Multiplier > 1){
			uint16_t tcountdown = OCR4A - TCNT4;
			if((TimeRemaining == P2MultTimeS) && (tcountdown <= P2MultTimeT)){
				P2Multiplier = 1;
			}
		}
		
		
		for(uint8_t ledIndexX = 0; ledIndexX < 6; ledIndexX++){
			for(uint8_t ledIndexY = 0; ledIndexY < 6; ledIndexY++){
				uint8_t found = 0;
				for(uint8_t ledIndex = 0; ledIndex < 6; ledIndex++){
					if((ledIndexX == gameledsX[ledIndex]) && (ledIndexY == gameledsY[ledIndex]))	//led is on in game
					{
						found = 1;
						break;
					}
				}
				setButtonLed(ledIndexX, ledIndexY, found);
			}
		}
		for(uint8_t ledIndex = 0; ledIndex < 6; ledIndex++){	//turn on game leds and turn others off
			setButtonLed(gameledsX[ledIndex], gameledsY[ledIndex], 1);
		}
		
		//_delay_ms(50);	// TODO maybe change this later

	}
	setScoreSegment(TIMER1, 18); // 18 is the dash
	setScoreSegment(TIMER10, 18);
	setScoreSegment(TIMER100, 18);
	printf("GAMa ovar\n");
	// game is over, stop timer
	
}

void Bonus(){

	// blink leds to indicate bonus round start

	for(uint8_t blinkIndex = 0; blinkIndex < 6; blinkIndex++){
		clearLeds(blinkIndex % 2);
		if(SoundEnabled){
			DDRB |= (1<<1);	//play page tone
			ICR1 = notes[blinkIndex % 2];
			OCR1A = notes[blinkIndex % 2] / 2;
			_delay_ms(125);
			DDRB &= ~(1<<1);
		}
		
	}
	_delay_ms(250);
	
	Display321();

	TimeRemaining = BonusTime;
	
	printf("STARTING BONUS\n");
	
	setScore(0, GameMode == 0 ? HighScore1P : HighScore2P);
	uint8_t pressed[] = {0,0,0,0,0,0};
	TCNT4 = 0;
	uint8_t lastTime = TimeRemaining;
	while(TimeRemaining > 0){
		
		if(SoundEnabled & (lastTime != TimeRemaining) & (soundPlaying == 0)){
			lastTime = TimeRemaining;
			DDRB |= (1<<1);	//make ticking noise
			ICR1 = notes[4 + (TimeRemaining%2)];
			OCR1A = notes[4 + (TimeRemaining%2)] / 2;
			_delay_us(3500);
			DDRB &= ~(1<<1);
		}
		
		setScore(1, TimeRemaining);
		if(GameMode){
			setScore(0, P1Score);	// if 2p mode, show left p1 and right p2 scores
			setScore(2, P2Score);
			}else{
			setScore(2, P1Score);	// otherwise show p1 score and high score
		}

		for(uint8_t x = 0; x < 6; x++){
			for(uint8_t y = 1; y < 6; y++){
				uint8_t isavail = (pressed[x] & (1 << y)) == 0 ? 1 : 0;
				setButtonLed(x,y,isavail);
				
				if(isButtonDown(x,y) && isavail){
					playChirp(1);
					pressed[x] |= (1 << y);
					if((x >= 3) && (GameMode)){	//player 2 bonus
						P2Score += BonusPointCount;
						}else{
						P1Score += BonusPointCount;
					}
				}
			}
		}
		
	}
	
	setScoreSegment(TIMER1, 18); // 18 is the dash
	setScoreSegment(TIMER10, 18);
	setScoreSegment(TIMER100, 18);

}

void Attractive(){
	setScore(2, HighScore1P);
	
	setScoreSegment(TIMER1, 18); // 18 is the dash
	setScoreSegment(TIMER10, 18);
	setScoreSegment(TIMER100, 18);
	
	setScore(0, HighScore2P);
	while(1){
		uint8_t blinkyMode = rand() % 4;	// todo make random, ADD DISPLAY BLANKING
		switch(blinkyMode){	// multiple blinky modes (todo randomly pick one)
			case 0:
			for(uint8_t count = 0; count < 2; count++){
				for(uint8_t y = 1; y < 6; y++){
					for(uint8_t x = 0; x < 6; x++){
						setButtonLed( (y%2)==0 ? x : (5-x) ,y,1);
						if( AttractCheckGameStart(600) ){
							
							goto EndAttract;
						}
					}
					setButtonLed(Player1ButtonX, Player1ButtonY, y%2);
					setButtonLed(Player2ButtonX, Player2ButtonY, (y+1) % 2);
				}
				for(uint8_t y = 1; y < 6; y++){
					for(uint8_t x = 0; x < 6; x++){
						setButtonLed( (y%2)==0 ? x : (5-x) ,y,0);
						if( AttractCheckGameStart(600) ){
							goto EndAttract;
						}
					}
					setButtonLed(Player1ButtonX, Player1ButtonY, (y+1)%2);
					setButtonLed(Player2ButtonX, Player2ButtonY,(y) % 2);
				}
			}
			break;
			
			case 1:
			for(uint8_t count = 0; count < 216; count++){
				
				uint8_t x = rand()%6;
				uint8_t y = 1+(rand()%5);
				setButtonLed(x,y,!getButtonLed(x,y));
				if( AttractCheckGameStart(260) ){
					goto EndAttract;
				}
				
				if(count % 12 == 0){
					setButtonLed(Player1ButtonX, Player1ButtonY, (count/12)%2);
					setButtonLed(Player2ButtonX, Player2ButtonY,(1+(count/12))%2);
				}
			}
			break;
			
			
			case 2:
			for(uint8_t count = 0; count < 24; count++){
				for(uint8_t y = 1; y < 6; y++){
					for(uint8_t x = 0; x < 6; x++){
						setButtonLed( (y%2)==0 ? x : (5-x) ,y,1);
						
					}
					setButtonLed(Player1ButtonX, Player1ButtonY, y%2);
					setButtonLed(Player2ButtonX, Player2ButtonY, (y+1) % 2);
				}
				if( AttractCheckGameStart(1000) ){
					
					goto EndAttract;
				}
			}
			break;
			
			case 3:
			for(uint8_t count = 0; count < 2; count++){
				for(uint8_t x = 0; x < 6; x++){
					for(uint8_t y = 1; y < 6; y++){
						setButtonLed( (y%2)==0 ? x : (5-x) ,y,1);
						if( AttractCheckGameStart(600) ){
							
							goto EndAttract;
						}
					}
					setButtonLed(Player1ButtonX, Player1ButtonY, x%2);
					setButtonLed(Player2ButtonX, Player2ButtonY, (x+1) % 2);
				}
				for(uint8_t x = 0; x < 6; x++){
					for(uint8_t y = 1; y < 6; y++){
						setButtonLed( (y%2)==0 ? x : (5-x) ,y,0);
						if( AttractCheckGameStart(600) ){
							goto EndAttract;
						}
					}
					setButtonLed(Player1ButtonX, Player1ButtonY, (x+1)%2);
					setButtonLed(Player2ButtonX, Player2ButtonY,(x) % 2);
				}
			}
			break;
			
		}
	}
	EndAttract:
	for(uint8_t scoredisp = 0; scoredisp < 9; scoredisp++){
		setScoreSegment(scoredisp, 16);	//turn off the displays
	}
	return;		// start the game

}

void ShowWinner(){
	clearLeds(0);
	if(GameMode && (P1Score == P2Score)){
		GameMode = 0;		// if there's a tie, just show 1p winning screen	
	}
	if(GameMode == 0){
		
		for(uint8_t count = 0; count < 24; count++){			// flash whole screen
			
			setScore(RIGHT, HighScore1P);
			setScore(LEFT, HighScore1P);
			
			for(uint8_t ledx = 0; ledx < 6; ledx++){
				for(uint8_t ledy = 0; ledy < 6; ledy++){
					setButtonLed(ledx, ledy, (ledx+ledy+count)%2 );
				}
			}
			if(SoundEnabled){
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[5+(count%4)];
				OCR1A = notes[5+(count%4)] / 2;
			}
			_delay_ms(100);
		}
		DDRB &= ~(1<<1);
		}else{										//2p mode, show winner every time
		uint8_t winnerhalf = P1Score > P2Score ? 0 : 3; //index half based on winner
		
		clearLeds(0);
		for(uint8_t count = 0; count < 24; count++){			// flash whole screen
			
			if(P1Score > P2Score){
				setScoreSegment(RIGHT100, 16);
				setScoreSegment(RIGHT10, 16);
				setScoreSegment(RIGHT1, 16);
				setScore(LEFT, P1Score);
			}else{
				setScore(RIGHT, P2Score);
				setScoreSegment(LEFT100, 16);
				setScoreSegment(LEFT10, 16);
				setScoreSegment(LEFT1, 16);
			}
			
			
			for(uint8_t ledx = 0; ledx < 3; ledx++){
				for(uint8_t ledy = 0; ledy < 6; ledy++){
					setButtonLed(winnerhalf+ledx, ledy, (ledx+ledy+count)%2 );
				}
			}
			if(SoundEnabled){
				DDRB |= (1<<1);	//play tone
				ICR1 = notes[5+(count%4)];
				OCR1A = notes[5+(count%4)] / 2;
			}
			_delay_ms(100);
		}
		DDRB &= ~(1<<1);
	}
}

void IncrementScore(uint8_t Player, uint16_t value){
	if(Player == 0){
		P1Score += value;
		P1Score = P1Score > 999 ? 999 : P1Score;
		}else if(Player == 1){
		P2Score += value;
		P2Score = P2Score > 999 ? 999 : P2Score;
	}
}


const uint8_t onledsX[33] = {3, 2, 1, 1, 3, 2, 1, 1, 3, 2, 1,
	4, 3, 2, 2, 4, 3, 2, 4, 4, 3, 2,
2, 3, 2, 2, 2, 3, 2, 1, 0, 0, 0};
const uint8_t onledsY[33] = {1, 1, 1, 2, 3, 3, 3, 4, 5, 5, 5,
	1, 1, 1, 2, 3, 3, 3, 4, 5, 5, 5,
1, 2, 2, 3, 4, 5, 5, 5, 0, 0, 0};

void Display321(){
	
	clearLeds(0);
	
	if(GameMode){	//light up 2p button
		for(uint8_t i = 0; i < 8; i++){
			setButtonLed(Player2ButtonX, Player2ButtonY, i%2);
			if(SoundEnabled){
				DDRB |= (1<<1);	//play page tone
				ICR1 = notes[i];
				OCR1A = notes[i] / 2;
			}
			_delay_ms(62);
			DDRB &= ~(1<<1);
		}
		}else{
		for(uint8_t i = 0; i < 8; i++){
			setButtonLed(Player1ButtonX, Player1ButtonY, i%2);
			if(SoundEnabled){
				DDRB |= (1<<1);	//play page tone
				ICR1 = notes[i];
				OCR1A = notes[i] / 2;
			}
			_delay_ms(62);
			DDRB &= ~(1<<1);
		}
	}

	_delay_ms(650);
	for(uint8_t count = 3; count >= 1; count--){
		for(uint8_t disp = 0; disp < 3; disp++){
			setScore(disp, count);
		}
		printf("%d\n", count);
		clearLeds(0);
		
		for(uint8_t index = 0; index < 11; index++){
			setButtonLed(onledsX[index + ((3-count)*11)], onledsY[index + ((3-count)*11)], 1);
		}
		playChirp(3-count);
		_delay_ms(1000);
	}
}

void playChirp(uint8_t tone){
	if(!SoundEnabled){
		return;
	}
	beep_index = tone;
	note_index = 0;
	TCCR0B = (0b101 << CS00);	//turn on clock for speaker
}

uint8_t attractButtonMemory[] = {0,0,0,0,0,0};
	
uint8_t AttractCheckGameStart(uint16_t count){
	// pauses some time while checking player button inputs, skip and start game if pressed
	for(uint16_t delayCount = 0; delayCount < count; delayCount++){
		//delay some ms between each button light and check for game start
		if(isButtonDown(Player1ButtonX, Player1ButtonY) || isButtonDown(Player2ButtonX, Player2ButtonY)){
			GameMode = isButtonDown(Player2ButtonX, Player2ButtonY);
			printf("STARTING %dP mode\n", GameMode+1);
			return 1;	// instantly return and quit waiting
		}
		for(uint8_t x = 0; x < 6; x++){
			for(uint8_t y = 1; y < 6; y++){
				if(isButtonDown(x,y)){
					if((attractButtonMemory[x] & (1 << y)) == 0){	//button not pressed
						attractButtonMemory[x] |= (1 << y);
						if(SoundEnabled){
							DDRB |= (1<<1);	//make ticking noise
							ICR1 = notes[rand() % 8];
							OCR1A = notes[4 + (TimeRemaining%2)] / 2;
						}
						_delay_ms(20);
						DDRB &= ~(1<<1);
					}
				}else{
					attractButtonMemory[x] &= ~(1 << y);
				}
			}
		}
	}
	return 0;	// return that it finished without button presses
}


void EEPROM_write(uint16_t uiAddress, uint8_t ucData)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE))
	;
	/* Set up address and Data Registers */
	EEAR = uiAddress;
	EEDR = ucData;
	/* Write logical one to EEMPE */
	EECR |= (1<<EEMPE);
	/* Start eeprom write by setting EEPE */
	EECR |= (1<<EEPE);
}

uint8_t EEPROM_read(uint16_t uiAddress)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE))
	;
	/* Set up address register */
	EEAR = uiAddress;
	/* Start eeprom read by writing EERE */
	EECR |= (1<<EERE);
	/* Return data from Data Register */
	return EEDR;
}

