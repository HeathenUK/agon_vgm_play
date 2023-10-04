;
; Title:	AGON Interrupt handler for timer0 in userspace
; Author:	Jeroen Venema
; Created:	22/01/2023
; Last Updated:	22/01/2023

			INCLUDE "ez80f92.inc"

			.ASSUME	ADL = 1
			SEGMENT CODE
			
			XDEF	_timer0_handler
			XDEF	_timer0

; AGON Timer 0 Interrupt Handler
;
_timer0_handler:	
			DI
			PUSH	AF
			IN0		A,(TMR0_CTL)		; Clear the timer interrupt
			PUSH	HL
			PUSH	DE
			
			LD		DE, 3
			LD		HL, (_timer0)		; Increment the delay timer
			
			ADD		HL, DE
			;ADD		BC,6
			
			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC; x5
			
			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC ;x5

			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC
			; INC		BC ;x5	

			LD		(_timer0), HL
			POP		DE
			POP		HL
			POP		AF
			EI
			RETI.L
	
			SEGMENT DATA
			
_timer0			DS	3