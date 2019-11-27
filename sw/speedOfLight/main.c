/*
* 388SpeedOfLight.c
*
* Created: 10/22/2019 1:43:19 PM
* Author : tchase
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbQc.h"
#include "buttons.h"

int main(void)
{
	// set up the power supply to give 12V 
	usbQcInit();
	QCset12V();

	// setup for the buttons and LED's 
	buttonsInit();
	
	sei();
	
	uint8_t attractAnimation = 1;
	while (1)
	{
		switch(attractAnimation){
			case 0:
			break;
			case 1:
			// swipe horizontally
			for(uint8_t x = 0; x < 6; x++){
				for(uint8_t y = 0; y < 5; y++){
					//setButtonLed(x, y, 1);
				}
				_delay_ms(100);
			}
			
			for(uint8_t x = 0; x < 6; x++){
				for(uint8_t y = 0; y < 5; y++){
					//setButtonLed(x, y, 0);
				}
				_delay_ms(100);
			}
			break;
		}
	}
}
