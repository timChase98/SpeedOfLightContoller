/*
* buttons.c
*
* Created: 11/27/2019 9:33:50 AM
*  Author: tchase
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "buttons.h"
#include <stdio.h>

// 9 bytes of score display digits + 6 bytes of LED ROWs
volatile uint8_t ledMemory[9 + 6] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // memory for led states

// 2 bytes for score display and 1 byte for each corner matrix
volatile uint8_t ledData[2 + 4] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // data buffer to shift out to the led registers

// 1 byte per row,
volatile uint8_t buttonMemory[6];

volatile uint8_t scoreDigitCounter = 0; // counter for scoreboard multiplexing
volatile uint8_t muxCounter = 0; // multiplex row counter
volatile uint8_t spiByteCounter = 0; // counter for SPI transfers

const uint8_t sevenSegmentDecode[17] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x00};


void buttonsInit(){
	// set DDR for button matrix
	DDRD = 0xFF;
	PORTC = 0xFF;
	
	// set DDR for latch and blank pins
	DDRB |= 1 << LED_L;
	DDRE|= 1 << LED_B;
	DDRE |= 1 << 2; // enable LEDs
	PORTE |= 1 << LED_B;
	
	spiSetup();
	tmrSetup();
	
}

void spiSetup(){
	// MOSI and SCK to outputs
	DDRB |= (1<<3)|(1<<5);
	// STC int enabled master mode, clk/16
	SPCR0 = (1 << SPIE) | (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

void tmrSetup(){
	// timer 3 is used to update the LED display as well as read the button matrix.
	// COMPA will trigger the next multiplexing cycle of the led matrices
	// COMPB will read the next line of buttons
	
	// CTC mode clk/1
	TCCR3B = (1 << WGM12) | (1 << CS11);
	//OCR3A = 3700;
	OCR3A = 3000;
	OCR3B = 1850;
	TIMSK3 |= (1 << OCIE3B) | 1 << (OCIE3A);
	TCNT3 = 0;
}

uint8_t isButtonDown(uint8_t x, uint8_t y){
	return buttonMemory[x] & (1 << y); // may need to change to 1 << (5 - x) test this later
}

void setButtonLed(uint8_t x, uint8_t y, uint8_t value){
	if(value){
		ledMemory[9 + y] |= 1 << x;
		return;
	}
	ledMemory[9 + y] &= ~(1 << x);
	
}

int getButtonLed(uint8_t x, uint8_t y){
		return (ledMemory[9 + y] >> x) & 0x01;
	
}

void setScore(uint8_t display, uint16_t value){
	
}

void clearLeds(uint8_t mode){
	for(uint8_t ledx = 0; ledx < 6; ledx++){
		for(uint8_t ledy = 0; ledy < 6; ledy++){
			setButtonLed(ledx, ledy, mode);
		}	
	}
}


ISR(TIMER3_COMPA_vect){
	// calculate data for led rows
	// rows span across two adjacent shift registers
	// col 0, 1, 2 are on LED 3 and 0
	// col 3, 4, 5 are on LED 2 and 1
	// the row is in the lower nibble and column data is in the upper nibble
	// for each row the two registers need to be set and the other two need to be cleared
	
	// ledData for the matrices are stored as a byte array of the column data
	
	if (muxCounter < 3)
	{
		ledData[3] = ((1 << muxCounter)) | ((ledMemory[9 + muxCounter] & 0b00000111)<<4);
		ledData[0] = 0;
		ledData[1] = 0;
		ledData[2] = ((1 << muxCounter)) | ((ledMemory[9 + muxCounter] & 0b00111000)<<1);
	}
	else{
		ledData[3] = 0;
		ledData[0] = ((1 << (muxCounter-3))) | ((ledMemory[9 + muxCounter] & 0b00000111)<<4);
		ledData[1] = ((1 << (muxCounter-3))) | ((ledMemory[9 + muxCounter] & 0b00111000)<<1);
		ledData[2] = 0;
	}
	
	muxCounter = (muxCounter + 1) % 6;
	
	// calculate data for score display
	
	ledData[5] = sevenSegmentDecode[ledMemory[scoreDigitCounter]];
	if (scoreDigitCounter == 0){
		ledData[5] |= 1 << 7; // set MSB for D0
		ledData[4] = 0;
	}
	else{
		ledData[4] = 1 << (scoreDigitCounter - 1);
	}

	scoreDigitCounter = (scoreDigitCounter + 1) % 9;
	

	// clear contents of shift register and latch
	PORTB &= ~(1 << LED_L); // set led latch low
	PORTE &= ~(1 << LED_B); // blank leds
	PORTB |= (1 << LED_L); // set led latch high
	PORTE |= (1 << LED_B); // unblank leds
	PORTB &= ~(1 << LED_L); // set led latch low
	spiByteCounter = 0;
	SPDR0 = ledData[spiByteCounter++];
}

/*
ISR(TIMER3_COMPB_vect){
	// Read in Buttons
	
	PORTD = ~(1 << muxCounter) << 2;// set 1 bit to a 0
	PORTD = 0xFF; 
	buttonMemory[muxCounter] = ~PINC;
	
}*/

ISR(TIMER3_COMPB_vect){
	/*DDRB |= 0b00111111;	// ENABLE OUTPUTS FOR LOW
	PORTC = 0xFF; //PULLUPS
	for(uint8_t colB = 0; colB < 6; colB++){
		PORTD = ~(1 << colB) << 2;
		//buttonMemory[(colB)%6] = ~PINC;
		buttonMemory[colB] = 0xFF;
		for(uint8_t rowB = 0; rowB < 6; rowB++){
			PORTC = (1 << rowB);
			buttonMemory[colB] &= (1 << rowB);
		}
	}*/
	/*
	PORTC = 0xFF;
	DDRD = 0xFF;
	DDRD &= ~(1<<(muxCounter+2));
	buttonMemory[muxCounter] = ~PINC; 
	DDRD = 0xFF; 
	*/
	
	DDRD = 0xFF;
	DDRC = 0x00;
	
	PORTD = ~(1<<(muxCounter+2));
	buttonMemory[muxCounter] = ~PINC;
	PORTD = 0xFF;
}

ISR(SPI0_STC_vect){
	if (spiByteCounter >= 6){
		PORTB |= 1 << LED_L; // set led latch high
		return;
	}
	SPDR0 = ledData[spiByteCounter++];
}






















//