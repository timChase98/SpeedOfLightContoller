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
	#define QC_DDR DDRB 
	#define QC_PORT PORTB
	#define QC_DP 0
	#define QC_DM 1


// macro definitions of pin states used for handshake and setting voltage
// these should only be used inside of the setVoltage functions 
	#define _dm0V() QC_PORT &= ~(1 << QC_DM); QC_DDR |= 1 << QC_DM

	#define _dp600mV() QC_DDR &= ~(1 << QC_DP)
	#define _dm600mV() QC_DDR &= ~(1 << QC_DM)

	#define _dp3V3() QC_PORT |= 1 << QC_DP; QC_DDR |= 1 << QC_DP
	#define _dm3V3() QC_PORT |= 1 << QC_DM; QC_DDR |= 1 << QC_DM
	

// prototypes 

	void usbQcInit();
	void _handshake();
	void QCset12V();
	void QCset5V();
	void QCset9V();



#endif /* USBQC_H_ */