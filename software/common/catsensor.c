/******************************************************************************/
/* File    :	catsensor.c						      */
/* Function:	Cat sensor functional implementation			      */
/* Author  :	Robert Delien						      */
/*		Copyright (C) 2010, Clockwork Engineering		      */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
#include <htc.h>

#include "hardware.h"			/* Flexible hardware configuration */

#include "catsensor.h"
#include "timer.h"

extern void catsensor_event (unsigned char detected);


/******************************************************************************/
/* Macros								      */
/******************************************************************************/

#define	PING_TIME	(SECOND / 10)			/* Ping every 100 ms */
#define	DEBOUNCE_TIME	(3 * PING_TIME)			/* Debounce for 3 pings */


/******************************************************************************/
/* Global Data								      */
/******************************************************************************/

static bit		pinging		= 0;		/* Indicates an on-going ping */
static bit		echoed		= 0;		/* Stores an echo received */
static bit		detected_cur	= 0;		/* Current detection state */
static bit		detected_old	= 0;		/* Previous detection state (to detect differences) */
static bit		detected_dbc	= 0;		/* Debounced detection state */
static struct timer	debouncer	= NEVER;	/* Timer to debounce detection state */
static struct timer	pingtime	= EXPIRED;	/* Timer to schedule pings */


/******************************************************************************/
/* Local Prototypes							      */
/******************************************************************************/


/******************************************************************************/
/* Global Implementations						      */
/******************************************************************************/

void catsensor_init (void)
/******************************************************************************/
/* Function:	Module initialisation routine				      */
/*		- Initializes the module				      */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
{
	/*
	 * Initialize timer 2
	 */
	/* Select frequency */
	PR2 = 0x54 ;
	/* Postscaler to 1:16
	 * Prescaler to 1:4
	 * Timer 2 On */
	T2CON = 0x7D ;
	/* Select duty cycle  */
	CCPR1L = 0x2A ;
	/* Set PWM mode
	 * Select duty cycle  */
	CCP1CON = 0x1F ;
	/* Clear timer 2 interrupt status */
	TMR2IF = 0;
	/* Enable timer 2 interrupt */
	TMR2IE = 1;
}
/* End: catsensor_init */


void catsensor_work (void)
/******************************************************************************/
/* Function:	Module worker routine					      */
/*		- Debounces the decoupled signal and notifies on changes      */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
{
	if (!pinging &&
	    timeoutexpired(&pingtime)) {
	    	/* Set timer for next ping */
		settimeout(&pingtime, PING_TIME);
		/* Reset the echo */
		echoed = 0;
		/* Set the pinging flag to enable detection */
		pinging = 1;

		/* Select frequency */
		PR2 = 0x54 ;
		/* Postscaler to 1:16
		 * Prescaler to 1:4
		 * Timer 2 On */
		T2CON = 0x7D ;
		/* Select duty cycle  */
		CCPR1L = 0x2A ;
		/* Set PWM mode
		 * Select duty cycle  */
		CCP1CON = 0x1F ;
		/* Clear timer 2 interrupt status */
		TMR2IF = 0;
		/* Enable timer 2 interrupt */
		TMR2IE = 1;
	}
	/* Debounce the decouples 'detect' signal */
	if (detected_cur != detected_old) {
		settimeout(&debouncer, DEBOUNCE_TIME);
		detected_old = detected_cur;
	}

	/* Notify is changed */
	if (timeoutexpired(&debouncer)) {
		timeoutnever(&debouncer);
		if (detected_cur != detected_dbc) {
			detected_dbc = detected_cur;
			catsensor_event(detected_dbc);
		}
	}
} /* catsensor_work */


void catsensor_isr_timer (void)
/******************************************************************************/
/* Function:	Timer interrupt service routine				      */
/*		- Interrupt will toggle pinging				.     */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
{
	if (pinging) {
		/* End ping in progress */
		pinging = 0;
/* Disable timer 2 interrupt */
PR2 = 0 ;
/* Postscaler to 1:16
 * Prescaler to 1:4
 * Timer 2 On */
T2CON = 0 ;
/* Select duty cycle  */
CCPR1L = 0 ;
TMR2IE = 0;
TMR2IF = 0;
TMR2ON = 0;
CCP1CON = 0;

// TBD: Fix naughty direct-latch update so eventlog_track() gets called
CATSENSOR_LED(LAT) &= ~CATSENSOR_LED_MASK;
		/* The echo is the detection */
		detected_cur = echoed;
	}
}
/* End: catsensor_isr_timer */


void catsensor_isr_input (void)
/******************************************************************************/
/* Function:	Input interrupt service routine				      */
/*		- Interrupt will handle echo				.     */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
{
	if ( (pinging) &&
	     !(CATSENSOR(PORT) & CATSENSOR_MASK) ) {
		echoed = 1;
	}
}
/* End: catsensor_isr_input */


/******************************************************************************/
/* Local Implementations						      */
/******************************************************************************/
