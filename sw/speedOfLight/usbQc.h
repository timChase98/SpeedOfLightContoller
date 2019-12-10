/*
 * usbQc.h
 *
 * Created: 11/20/2019 8:49:48 AM
 *  Author: tchase
 */ 

#include <avr/io.h>

#ifndef USBQC_H_
#define USBQC_H_

// port definitons for pins being used for USB QC control 
// (assumes both pins are on the same port)
	#define QC_DDR DDRE 
	#define QC_PORT PORTE
	#define QC_DP 1
	#define QC_DM 0


// macro definitions of pin states used for handshake and setting voltage
// these should only be used inside of the setVoltage functions 
	

// prototypes 

	void usbQcInit();
	void _handshake();
	void QCset12V();
	void QCset5V();
	void QCset9V();

void _dm0V();
void _dp600mV();
void _dm600mV();
void _dp3V3();
void _dm3V3();


#endif /* USBQC_H_ */