/*
 * ubsQc.c
 *
 * Created: 11/20/2019 8:50:06 AM
 *  Author: tchase
 */ 

#include "usbQc.h"
#include <util/delay.h>

void usbQcInit(){
	_handshake(); 
	set5V(); 
}

void _handshake(){
	_dm600mV();
	_delay_ms(1500);
	_dm0V();
	_delay_ms(2);
	
}

void set5V(){
	_dp600mV();
	_dm0V();
	
}

void set9V(){
	_dp3V3();
	_dm600mV();
	
}

void set12V(){
	_dp600mV();
	_dm600mV();
}