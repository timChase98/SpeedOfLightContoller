/*
* buttons.c
*
* Created: 11/27/2019 9:33:50 AM
*  Author: tchase
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "buttons.h"

// 9 bytes of score display digits + 6 bytes of LED ROWs
volatile uint8_t ledMemory[9 + 6] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F}; // memory for led states

// 2 bytes for score display and 1 byte for each corner matrix
volatile uint8_t ledData[2 + 4] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // data buffer to shift out to the led registers

// 1 byte per row,
volatile uint8_t buttonMemory[6];

volatile uint8_t scoreDigitCounter = 0; // counter for scoreboard multiplexing
volatile uint8_t muxCounter = 0; // multiplex row counter
volatile uint8_t spiByteCounter = 0; // counter for SPI transfers

const uint8_t sevenSegmentDecode[17] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x00};


void buttonsInit(){
	// set DDR for button matrix
	DDRC = 0x3F; // set to outputs
	PORTD = 0xFF; // set pullup resistors
	
	// set DDR for latch and blank pins
	DDRB |= 1 << LED_L | 1 << LED_B;
	
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
	OCR3A = 3700;
	OCR3B = 1850;
	TIMSK3 |= (1 << OCIE3B) | 1 << (OCIE3A);
	TCNT3 = 0;
}

uint8_t isButtonDown(uint8_t x, uint8_t y){
	return buttonMemory[y] & (1 << x); // may need to change to 1 << (5 - x) test this later
}

void setButtonLed(uint8_t x, uint8_t y, uint8_t value){
	if(value){
		ledMemory[9 + y] |= 1 << x;
		return;
	}
	ledMemory[9 + y] &= ~(1 << x);
	
}

void setScore(uint8_t display, uint16_t value){
	
}


ISR(TIMER3_COMPA_vect){
	// calculate data for led rows
	// rows span across two adjacent shift registers
	// row 0, 1, 2 are on LED 2 and 3
	// row 3, 4, 5 are on LED 4 and 5
	// the row is in the lower nibble and column data is in the upper nibble
	// for each row the two registers need to be set and the other two need to be cleared
	
	if(muxCounter < 3){
		ledData[0] = (1 << muxCounter) | ((ledMemory[9 + muxCounter] >> 4) << 4);
		ledData[1] = (1 << muxCounter) | ((ledMemory[9 + muxCounter] & 0x0F) << 4);
		ledData[2] = 0;
		ledData[3] = 0;
	}
	else{
		ledData[2] = (1 << (muxCounter - 3)) | ((ledMemory[9 + muxCounter] >> 4) << 4);
		ledData[3] = (1 << (muxCounter - 3)) | ((ledMemory[9 + muxCounter] & 0x0F) << 4);
		ledData[0] = 0;
		ledData[1] = 0;
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
	PORTB &= ~(1 << LED_B); // blank leds
	PORTB |= (1 << LED_L); // set led latch high
	PORTB |= (1 << LED_B); // unblank leds
	PORTB &= ~(1 << LED_L); // set led latch low
	spiByteCounter = 0;
	SPDR0 = ledData[spiByteCounter++];
}

ISR(TIMER3_COMPB_vect){
	// Read in Buttons
	PORTC = ~(1 << muxCounter);// set 1 bit to a 0
	buttonMemory[muxCounter] = ~PIND;
	
}

ISR(SPI0_STC_vect){
	if (spiByteCounter >= 6){
		PORTB |= 1 << LED_L; // set led latch high
		return;
	}
	SPDR0 = ledData[spiByteCounter++];
}






















//