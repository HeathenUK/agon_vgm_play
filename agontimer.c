/*
 * Title:			AGON timer interface
 * Author:			Jeroen Venema
 * Created:			06/11/2022
 * Last Updated:	22/01/2023
 * 
 * Modinfo:
 * 06/11/2022:		Initial version
 * 22/01/2023:		Interrupt-based freerunning functions added for timer0
 */

#include <defines.h>
#include <ez80.h>
#include <stdio.h>
//#include "mos-setvector.h"
#include "mos-interface.h"
#include "agontimer.h"

#define TMR0_COUNTER_1ms	(unsigned short)(((18432000 / 1000) * 1) / 16)

void *_timer0_prevhandler;						// used to (re)store the previous handler for the interrupt

// start timer0 on a millisecond interval
// this function registers an interrupt handler and requires timer0_end to de-register the handler after use


/* void timer0_begin(int interval)
{
	unsigned char tmp;
	unsigned short rr;
	
//	mos_setintvector(PRT0_IVECT, timer0_handler);
	_timer0_prevhandler = mos_setintvector(PRT0_IVECT, timer0_handler);
	//mos_setintvector(PRT4_IVECT, timer_int_function);

	timer0 = 0;
	TMR0_CTL = 0x00;
	rr = (unsigned short)(((18432000 / 1000) * interval) / 16);
	TMR0_RR_H = (unsigned char)(rr >> 8);
	TMR0_RR_L = (unsigned char)(rr);
	tmp = TMR0_CTL;
    TMR0_CTL = 0x57;
} */

/* void timer0_begin (int interval, int clkdiv) {
    unsigned short rr;
    unsigned char clkbits = 0;
    unsigned char ctl;

	_timer0_prevhandler = mos_setintvector(PRT0_IVECT, timer0_handler);
	
    switch (clkdiv) {
        case 4:   clkbits = 0x00; break;
        case 16:  clkbits = 0x04; break;
        case 64:  clkbits = 0x08; break;
        case 256: clkbits = 0x0C; break;
    }
    ctl = 0x53 | clkbits; // Continuous mode, reload and restart enabled, and enable the timer

    //rr = (unsigned short)((18432000 / 100) / clkdiv) * interval;
	//rr = (unsigned short)(((18432000 / 1000) * interval) / clkdiv);
	rr = (unsigned short)((18432000 / clkdiv) / 1000000) * interval;

    TMR0_CTL = 0x00; // Disable the timer and clear all settings    
    TMR0_RR_L = (unsigned char)(rr);
    TMR0_RR_H = (unsigned char)(rr >> 8);
    TMR0_CTL = ctl;
	timer0 = 0;
} */

void timer0_begin (int reload_value, int clk_divider) {
    
    //timer0_period (in SECONDS) = (reload_value * clk_divider) / system_clock_frequency (in Hz)
    
	unsigned char clkbits = 0;
    unsigned char ctl;
	//printf("Timer with RR: %u and CLKDIV: %u\r\n",reload_value, clk_divider);

    _timer0_prevhandler = mos_setintvector(PRT0_IVECT, timer0_handler);

    switch (clk_divider) {
        case 4:   clkbits = 0x00; break;
        case 16:  clkbits = 0x04; break;
        case 64:  clkbits = 0x08; break;
        case 256: clkbits = 0x0C; break;
    }
    ctl = 0x53 | clkbits; // Continuous mode, reload and restart enabled, and enable the timer    

    TMR0_CTL = 0x00; // Disable the timer and clear all settings
    TMR0_RR_L = (unsigned char)(reload_value);
    TMR0_RR_H = (unsigned char)(reload_value >> 8);
	timer0 = 0;
    TMR0_CTL = ctl;
}

void timer0_end(void)
{
	TMR0_CTL = 0x00;
	TMR0_RR_L = 0x00;
	TMR0_RR_H = 0x00;
	mos_setintvector(PRT0_IVECT, _timer0_prevhandler);
	timer0 = 0;
}

// delay for number of ms using TMR0
// this routine doesn't use the interrupt handler for TMR0
void delayms(int ms)
{
	unsigned short rr;
	UINT16 timer0;

	rr = TMR0_COUNTER_1ms - 19;	// 1,7% correction for cycles during while(ms) loop
	
	TMR0_CTL = 0x00;	// disable timer0
	
	while(ms)
	{	
		TMR0_RR_H = (unsigned char)(rr >> 8);
		TMR0_RR_L = (unsigned char)(rr);
				
		TMR0_CTL = 0x87; // enable, single pass, stop at 0, start countdown immediately
		do
		{
			timer0 = TMR0_DR_L;
			timer0 = timer0 | (TMR0_DR_H << 8);		
		}
		while(timer0);
		ms--;
	}
}