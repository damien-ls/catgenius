/******************************************************************************/
/* File    :	geniediag.c						      */
/* Function:	CatGenius Diagnostics Application			      */
/* Author  :	Robert Delien						      */
/*		Copyright (C) 2010, Clockwork Engineering		      */
/* History :	16 Feb 2010 by R. Delien:				      */
/*		- Initial revision.					      */
/******************************************************************************/
#include <htc.h>
#include <stdio.h>

#include "../common/hardware.h"		/* Flexible hardware configuration */

#include "../common/timer.h"
#include "../common/serial.h"
#include "../common/i2c.h"
#include "../common/catsensor.h"
#include "../common/water.h"

#ifdef HAS_COMMANDLINE
#include "../common/cmdline.h"
#include "../common/cmdline_tag.h"
#include "../common/cmdline_box.h"
#endif /* HAS_COMMANDLINE */

#ifdef HAS_BLUETOOTH
#include "../common/bluetooth.h"
#endif /* HAS_BLUETOOTH */

#include "userinterface.h"

/******************************************************************************/
/* Macros								      */
/******************************************************************************/

#if (defined _16F877A)
#  ifdef __DEBUG
	__CONFIG(FOSC_XT & WDTE_OFF & PWRTE_ON & BOREN_OFF & LVP_OFF & CPD_OFF & WRT_OFF & DEBUG_ON & CP_OFF);
#  else
	__CONFIG(FOSC_XT & WDTE_ON  & PWRTE_ON & BOREN_ON  & LVP_OFF & CPD_ON  & WRT_OFF & DEBUG_OFF & CP_ON);
#  endif
#elif (defined _16F1939)
#  ifdef __DEBUG
	__CONFIG(FOSC_XT & WDTE_OFF & PWRTE_OFF & MCLRE_ON & CP_OFF & CPD_OFF & BOREN_OFF & CLKOUTEN_OFF & IESO_OFF & FCMEN_OFF);
	__CONFIG(WRT_OFF & /* VCAPEN_OFF &*/ PLLEN_OFF & STVREN_ON & BORV_HI & LVP_OFF);
#  else
	__CONFIG(FOSC_XT & WDTE_ON  & PWRTE_ON & MCLRE_ON & CP_ON  & CPD_ON  & BOREN_ON  & CLKOUTEN_OFF & IESO_OFF & FCMEN_OFF);
	__CONFIG(WRT_OFF & /* VCAPEN_OFF &*/ PLLEN_OFF & STVREN_ON & BORV_HI & LVP_OFF);
#  endif
#endif


/******************************************************************************/
/* Global Data								      */
/******************************************************************************/

#ifdef __RESETBITS_ADDR
extern bit		__powerdown;
extern bit		__timeout;
#endif /* __RESETBITS_ADDR */

#ifdef HAS_COMMANDLINE
static int start (int argc, char* argv[]);
static int setup (int argc, char* argv[]);
static int lock (int argc, char* argv[]);

/* command line commands */
const struct command	commands[] = {
	{"echo", echo},
	{"help", help},
	{"bowl", bowl},
	{"arm", arm},
	{"dosage", dosage},
	{"tap", tap},
	{"drain", drain},
	{"dryer", dryer},
	{"cat", cat},
	{"water", water},
	{"heat", heat},
	{"start", start},
	{"setup", setup},
	{"lock", lock},
	{"tag", tag},
	{"", NULL}
};
#endif /* HAS_COMMANDLINE */

static unsigned char	PORTB_old;


/******************************************************************************/
/* Local Prototypes							      */
/******************************************************************************/

static void interrupt_init (void);

/******************************************************************************/
/* Global Implementations						      */
/******************************************************************************/

void main (void)
{
	unsigned char	flags;

	/* Init the hardware */
	flags = catgenie_init();

#ifdef HAS_BLUETOOTH
	serial_init(BT_BITRATE);
	bluetooth_init();
	serial_term();
#endif /* HAS_BLUETOOTH */

	/* Initialize the serial port */
	serial_init(BITRATE);

	printf("\n*** GenieDiag ***\n");
	if (!nPOR) {
		DBG("Power-on reset\n");
		flags |= POWER_FAILURE;
	} else if (!nBOR) {
		DBG("Brown-out reset\n");
		flags |= POWER_FAILURE;
	}
#ifdef __RESETBITS_ADDR
	else if (!__timeout)
		DBG("Watchdog reset\n");
	else if (!__powerdown)
		DBG("Pin reset (sleep)\n");
	else
		DBG("Pin reset\n");
#else
	else
		DBG("Unknown reset\n");
#endif /* __RESETBITS_ADDR */
	nPOR = 1;
	nBOR = 1;

	if (flags & START_BUTTON)
		DBG("Start button held\n");
	if (flags & SETUP_BUTTON)
		DBG("Setup button held\n");

	/* Initialize software timers */
	timer_init();

	/* Initialize the I2C bus */
	i2c_init();

	/* Initialize the cat sensor */
	catsensor_init();

	/* Initialize the water sensor and valve */
	water_init();

	/* Initialize the user interface */
	userinterface_init();

#ifdef HAS_COMMANDLINE
	/* Initialize the command line interface */
	cmdline_init();
#endif /* HAS_COMMANDLINE */

	/* Initialize interrupts */
	interrupt_init();

	/* Execute the run loop */
	for(;;){
		catsensor_work();
		water_work();
		catgenie_work();
		userinterface_work();
#ifdef HAS_COMMANDLINE
		cmdline_work();
#endif /* HAS_COMMANDLINE */
#ifndef __DEBUG
		CLRWDT();
#endif
	}
}


/******************************************************************************/
/* Local Implementations						      */
/******************************************************************************/

static void interrupt_init (void)
{
	PORTB_old = PORTB;

	/* Enable peripheral interrupts */
	PEIE = 1;

	/* Enable interrupts */
	GIE = 1;
}

static void interrupt isr (void)
{
	unsigned char temp;

	/* Timer 1 interrupt */
	if (TMR1IF) {
		/* Reset interrupt */
		TMR1IF = 0;
		/* Handle interrupt */
		timer_isr();
	}
	/* Timer 2 interrupt */
	if (TMR2IF) {
		/* Reset interrupt */
		TMR2IF = 0;
		/* Handle interrupt */
		catsensor_isr_timer();
	}
	/* Port B interrupt */
#if (defined _16F877A)
	if (RBIF) {
#elif (defined _16F1939)
	if (IOCIF) {
#endif
		/* Detected changes */
		temp = PORTB ^ PORTB_old;
		/* Reset interrupt */
#if (defined _16F877A)
		RBIF = 0;
#elif (defined _16F1939)
		IOCBF = 0;
		IOCIF = 0;
#endif
		/* Handle interrupt */
		if (temp & CATSENSOR_MASK)
			catsensor_isr_input();
		/* Update the old status */
		PORTB_old = PORTB ;
	}
	/* (E)USART interrupts */
	if (RCIF)
		serial_rx_isr();
	if (TXIF)
		serial_tx_isr();
}


#ifdef HAS_COMMANDLINE
static int start (int argc, char* argv[])
{
	if (argc > 1)
		return ERR_SYNTAX;

	start_short();
	return ERR_OK;
}


static int setup (int argc, char* argv[])
{
	if (argc > 1)
		return ERR_SYNTAX;

	setup_short();
	return ERR_OK;
}


static int lock (int argc, char* argv[])
{
	if (argc > 1)
		return ERR_SYNTAX;

	both_long();
	return ERR_OK;
}
#endif /* HAS_COMMANDLINE */
