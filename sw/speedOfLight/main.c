
// you just lost the game
// - josh & the bartenders


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
#define EEP_ADDR_BonusPtCount 60


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

volatile uint16_t beep_index = 0;
volatile uint16_t note_index = 0;
const uint16_t notes[12] = {4545, 4050, 3822, 3405, 3034, 2863, 2551, 2273, 2024, 1911, 1703, 1517};


void Game();
void Display321();
void Bonus();
void Attractive();
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
	
	printf("Welcome to bartending robot OS ver 4.7B\n");
	
	sei();
	
	while(1){							//REMOVE THIS, FOR TESTING
		for(int x = 0; x < 6; x++){
			for (int y = 0; y < 6; y++)
			{
				if (isButtonDown(x, y))
				{
					printf("%d, %d\n\n", x, y);
					setButtonLed(x, y, 1);
					
					/*(while(isButtonDown(x,y)){	//wait for button unpress
						;
					}*/
				}else{
					setButtonLed(x, y, 0);
				}
			}
		}
		
		
	}
	
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
	
	printf("AUDIO TIMERS & PORT SET");

	//load values from eeprom
	HighScore1P = (EEPROM_read(EEP_ADDR_HighScore1P_H) << 8) | (EEPROM_read(EEP_ADDR_HighScore1P_L));
	HighScore2P = (EEPROM_read(EEP_ADDR_HighScore2P_H) << 8) | (EEPROM_read(EEP_ADDR_HighScore2P_L));
	RoundTime = EEPROM_read(EEP_ADDR_RoundTime);
	BonusTime = EEPROM_read(EEP_ADDR_BonusTime);
	MultiplierMax = EEPROM_read(EEP_ADDR_MultMax);
	BonusPointCount = EEPROM_read(EEP_ADDR_BonusPtCount);
	
	// load random value from eeprom as seed and write new random value
	srand((EEPROM_read(EEP_ADDR_RandomSeed_H) << 8) | EEPROM_read(EEP_ADDR_RandomSeed_L));
	EEPROM_write(EEP_ADDR_RandomSeed_H, rand()%256 );
	EEPROM_write(EEP_ADDR_RandomSeed_L, rand()%256 );
	
	printf("EEPROM VALUES SET");
	
	// set up timer4 for game timer
	TCCR4A = (1 << WGM41);	// CTC mode
	TCCR4B = 0;				// disable timer until game start (set to 64 prescale later in code)
	TIMSK4 = (1 << OCIE4A);
	OCR4A = 15625;			// set up for 1s

	sei();

	while (1)
	{
		printf("STARTING ATTRACT MODE");
		Attractive();		// loop thru patterns until game start is pressed (TODO IMPLEMENT DEBUG PATTERN)
		Game();
		_delay_ms(750);
		Bonus();
		
		if(GameMode == 0){
			if(P1Score > HighScore1P){
				HighScore1P = P1Score;
				ShowWinner();			//if new high score, flash screen
			}
			}else{
			if((P1Score > HighScore2P) || (P2Score > HighScore2P)){
				HighScore2P = (P1Score > P2Score ? P1Score : P2Score);
			}
			ShowWinner();				//if 2 player mode show winner anyway
		}
		
		_delay_ms(750);
		

	}
}

ISR(TIMER0_COMPA_vect)		// audio interrupt
{
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
		break;
	}
	note_index += 1;
}

ISR(TIMER4_COMPA_vect){		// use timer 4 for decreasing game time every 1s
	if(TimeRemaining > 0){
		TimeRemaining--;
	}
}

void Game(){

	// game mode is set when leaving attract mode
	Display321();
	
	P1Score = 0;			// reset scores and multipliers
	P2Score = 0;
	P1Multiplier = 1;
	P2Multiplier = 1;

	// pick 6 random leds to enable regardless of mode
	// (3 left side 3 right side)
	for(int i = 0; i < 3; i++){
		gameledsX[i] = rand() % 3;			// turn 3 on right side
		gameledsY[i] = rand() % 5;
		
		gameledsX[i+3] = 3 + rand() % 3;	// turn 3 on left side
		gameledsY[i+3] = rand() % 5;
	}


	TimeRemaining = RoundTime;
	TCNT4 = 0;					// reset timer
	TCCR4B = (0b101 << CS40);	// enable timer 0 (game timer)
	
	setScore(0, HighScore1P);	// will be replaced if 2p mode
	
	while(TimeRemaining > 0){

		
		setScore(1, TimeRemaining);
		if(GameMode == 1){
			setScore(0, P1Score);	// if 2p mode, show left p1 and right p2 scores
			setScore(2, P2Score);
			}else{
			setScore(2, P1Score);	// otherwise show p1 score and high score
		}

		for(uint8_t i = 0; i < 6; i++){
			if (isButtonDown(gameledsX[i], gameledsY[i]) ){

				uint8_t oldX = gameledsX[i];
				uint8_t oldY = gameledsY[i];

				do{		// move led to random DIFFERENT spot
					if(i >= 3){
						gameledsX[i+3] = 3 + rand() % 3;
						}else{
						gameledsX[i] = rand() % 3;
					}
					gameledsY[i] = rand() % 5;

				}while((gameledsX[i] == oldX) && (gameledsY[i] == oldY));


				if(GameMode == 0){
					IncrementScore(0, P1Multiplier);	// add score and inc multiplier (singleplayer)
					playChirp(P1Multiplier);
					P1Multiplier = (P1Multiplier == MultiplierMax) ? MultiplierMax : (P1Multiplier + 1);
					}else{
					if(i >= 3){
						IncrementScore(1, P2Multiplier);	// add depending on side of button pressed
						playChirp(P2Multiplier);
						P2Multiplier = (P2Multiplier == MultiplierMax) ? MultiplierMax : (P2Multiplier + 1);
						}else{
						IncrementScore(0, P1Multiplier);
						playChirp(P1Multiplier);
						P1Multiplier = (P1Multiplier == MultiplierMax) ? MultiplierMax : (P1Multiplier + 1);
					}
				}

			}
		}
		
		
		if (P1Multiplier > 1){
			if ( (P1MultTimeS + MultiplierDecaySeconds) <= TimeRemaining){
				if(P1MultTimeT < MultiplierDecayTicks){	// TODO INCORPORATE TICK OFFSET
					P1Multiplier--;
				}
			}
		}
		if (P2Multiplier > 1){
			if ( (P2MultTimeS + MultiplierDecaySeconds) <= TimeRemaining){
				if(P2MultTimeT < MultiplierDecayTicks){	// TODO INCORPORATE TICK OFFSET
					P2Multiplier--;
				}
			}
		}
		
		
		//_delay_ms(50);	// TODO maybe change this later

	}
	
	// game is over, stop timer
	TCCR4B = (0b000 << CS40);
	
}

void Bonus(){

	// blink leds to indicate bonus round start

	for(uint8_t i = 0; i < 30; i++){
		setButtonLed(i % 6, i / 5, 1); // TODO VERIFY THIS WORKS, turn all leds on
	}
	_delay_ms(500);

	for(uint8_t i = 0; i < 30; i++){
		setButtonLed(i % 6, i / 5, 0); // TODO VERIFY THIS WORKS, turn all leds on
	}
	_delay_ms(500);

	for(uint8_t i = 0; i < 30; i++){
		setButtonLed(i % 6, i / 5, 1); // TODO VERIFY THIS WORKS, turn all leds on
	}
	_delay_ms(250);

	uint32_t HasPressed = 0;	// store 30 stored buttons
	
	TimeRemaining = BonusTime;
	TCNT4 = 0;
	TCCR4B = (0b101 << CS40);	// enable timer 0 (game timer)
	
	setScore(0, GameMode == 0 ? HighScore1P : HighScore2P);
	
	while(TimeRemaining > 0){
		
		setScore(1, TimeRemaining);
		if(GameMode == 1){
			setScore(0, P1Score);	// if 2p mode, show left p1 and right p2 scores
			setScore(2, P2Score);
			}else{
			setScore(2, P1Score);	// otherwise show p1 score and high score
		}
		
		for(uint8_t x = 0; x < 6; x++){
			for(uint8_t y = 0; y < 5; y++){
				if( isButtonDown(x, y) && (( 1 << (x*5 + y) ) == 0)){		// check later

					HasPressed |= (1 << (x*5 + y) );
					setButtonLed(x,y,0);

					if(GameMode == 0){
						IncrementScore(0,BonusPointCount);		// TODO later include mil
						}else{
						if(x >= 3){
							IncrementScore(1,BonusPointCount);
							}else{
							IncrementScore(0,BonusPointCount);
						}
					}

				}
			}
		}
	}
	TCCR4B = (0b000 << CS40);
	
	setScore(1, 0); // TODO MAKE DASHES

}

void Attractive(){
	setScore(0, HighScore1P);
	setScore(1, 0);			// TODO MAKE DASHES AND FLASH "1P" on left and "2P" on right
	setScore(0, HighScore2P);
	while(1){
		uint8_t blinkyMode = rand() % AttractPatternCount;	// todo make random, ADD DISPLAY BLANKING
		switch(blinkyMode){	// multiple blinky modes (todo randomly pick one)
			case 0:
			for(uint8_t mode = 1; mode > 0; mode--){	// turn on then turn	off (add more mod 2 to repeat muliple times)
				for(uint8_t x = 0; x < 6; x++){			// iterate each LED
					for (uint8_t y = 0; y < 5; y++){
						setButtonLed(x, y, mode);	//setbuttonled from buttons.h
						if( AttractCheckGameStart(100) ){
							goto EndAttract;
						}

					}
				}
				break;
			}
			
		}
		EndAttract:
		return;		// start the game
	}

}

void ShowWinner(){
	
	for(uint8_t i = 0; i < 30; i++){
		setButtonLed(i % 6, i / 5, 0); // TODO VERIFY THIS WORKS, turn all leds on
	}								 //blank screen
	
	if(GameMode == 0){
		for(uint8_t count = 0; count < 8; count++){			// flash whole screen
			setScore(0, HighScore1P);
			setScore(2, HighScore1P);
			
			for(uint8_t i = 0; i < 30; i++){
				setButtonLed(i % 6, i / 5, i%2); // TODO VERIFY THIS WORKS, turn all leds on
			}									// TODO make checkerboard
			
			_delay_ms(250);
			setScore(0, 0);	// TODO TURN SEGMENTS OFF
			setScore(2, 0);
			
			for(uint8_t i = 0; i < 30; i++){
				setButtonLed(i % 6, i / 5, 1 - (i%2)); // TODO VERIFY THIS WORKS, turn all leds on
			}
			
			_delay_ms(250);
		}
		}else{										//2p mode, show winner every time
		uint8_t winnerhalf = P1Score > P2Score ? 0 : 3; //index half based on winner
		for(uint8_t i = 0; i < 4; i++){
			
			if((P1Score == HighScore2P) || (P2Score == HighScore2P)){
				setScore(0, HighScore2P);
				setScore(1, 0);	//TODO EMPTY? show 2P flashing opposite
				setScore(2, HighScore2P);
			}
			
			for(uint8_t x = winnerhalf; x < (winnerhalf+3); x++){
				for(uint8_t y = 0; y < 5; y++){
					setButtonLed(x,y,1);
				}
			}
			_delay_ms(250);
			
			if((P1Score == HighScore2P) || (P2Score == HighScore2P)){
				setScore(0, 0); // TODO EMPTY
				setScore(1, 0);	//TODO SHOW 1P if 1p has hs else 2p
				setScore(2, 0); // todo empty
			}
			
			for(uint8_t x = winnerhalf; x < (winnerhalf+3); x++){
				for(uint8_t y = 0; y < 5; y++){
					setButtonLed(x,y,0);
				}
			}
			_delay_ms(250);
		}
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

/*const uint8_t onledsX[42] = { 0,1,2,3,3,0,1,2,3,3,0,1,2,3, // matrix of x positions for on leds
							  1,2,3,4,4,1,2,3,4,1,1,2,3,4,	 // uses top left as 0,0, so inverted
							  4,3,4,4,4,3,4,5,5,5,5,5,5,5};*/
const uint8_t onledsX[42] = { 5,4,3,2,2,5,4,3,2,2,5,4,3,2,
							  4,3,2,1,1,4,3,2,1,4,4,3,2,1,
							  1,2,1,1,1,2,1,0,0,0,0,0,0,0};
const uint8_t onledsY[42] = { 0,0,0,0,1,2,2,2,2,3,4,4,4,4,
							  0,0,0,0,1,2,2,2,2,3,4,4,4,4,
							  0,1,1,2,3,4,4,4,4,4,4,4,4,4};

void Display321(){	//TODO ADD TONES FOR EACH DIGIT
	
	for(uint8_t number = 3; number >= 1; number--){
		// turn off all leds
		for(uint8_t i = 0; i < 3; i++){
			setScore(i, number);
		}
		for(uint8_t i = 0; i < 30; i++){
			setButtonLed(i % 6, i / 5, 0); // TODO VERIFY THIS WORKS
		}
		for(uint8_t i = 0; i < 14; i++){	//
			setButtonLed(onledsX[i + (14*number)], onledsY[i + (14*number)], 1);
			//TODO FIX THIS
		}
		_delay_ms(1000);
	}
}

void playChirp(uint8_t tone){
	beep_index = tone;
	note_index = 0;
	TCCR0B = (0b101 << CS00);	//turn on clock for speaker
}

uint8_t AttractCheckGameStart(uint16_t count){
	// pauses some time while checking player button inputs, skip and start game if pressed
	for(uint16_t delayCount = 0; delayCount < count; delayCount++){
		//delay some ms between each button light and check for game start
		if(isButtonDown(Player1ButtonX, Player1ButtonY) || isButtonDown(Player2ButtonX, Player2ButtonY)){
			GameMode = isButtonDown(Player2ButtonX, Player2ButtonY);
			return 1;	// instantly return and quit waiting
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