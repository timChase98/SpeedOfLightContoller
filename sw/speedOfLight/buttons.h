/*
 * buttons.h
 *
 * Created: 11/27/2019 9:33:37 AM
 *  Author: tchase
 */ 


#ifndef BUTTONS_H_
#define BUTTONS_H_

#define  LED_L 0 // led latch on B.0
#define  LED_B 3 // led blank on E.3


void buttonsInit();
void clearLeds(uint8_t);

void spiSetup();
void tmrSetup();
uint8_t isButtonDown(uint8_t x, uint8_t y);
void setButtonLed(uint8_t x, uint8_t y, uint8_t value);
int getButtonLed(uint8_t x, uint8_t y);
void setScore(uint8_t display, uint16_t value);


const extern uint8_t sevenSegmentDecode[17]; 


#endif /* BUTTONS_H_ */