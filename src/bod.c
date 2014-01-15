/*
 * Brownout management and tracking.
 */

#include "config.h"
#include "bod.h"
#include "timers.h"
#include "log.h"

static BOOL bodActive;				// Flag indicating that brownout has been triggered
static timerType t;					// Timer for this state machine
#define BOD_RECOVERY_TIME 200       // Time in mS to recover from BOD event
static uint32_t bodCountStore;		// Number of brownouts that have occurred

// ============================================================================================
void BOD_IRQHandler(void)

// IRQ from brownout system

{
	if (bodActive) timerDel(&t); // This shouldn't really happen
	timerAdd(&t, TIMER_ORIGIN_BOD, 0, BOD_RECOVERY_TIME);
	bodActive=TRUE;
	bodCountStore++;

	// Disable BOD to stop it keeping on firing
	NVIC_DisableIRQ(BOD_IRQn);
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
BOOL bodIsActive(void)

// Return if a brownout has been triggered

{
	return bodActive;
}
// ============================================================================================
void bodTimeout(void)

// The brownout has timed out - release the IRQ to trigger again if needed

{
	bodActive=FALSE;
	logWrite(LOG_BOD_TRIGGERED,TRUE);
	NVIC_EnableIRQ(BOD_IRQn);
}
// ============================================================================================
uint32_t bodCount(void)

// Return number of brownouts that have occured

{
	return bodCountStore;
}
// ============================================================================================
void bodInit(void)

// Initialisation function

{
	bodActive=FALSE;
	timerInit(&t);
	LPC_SYSCON->BODCTRL=(0x01<<2); // Set an interrupt at 2.3V
	NVIC_EnableIRQ(BOD_IRQn);
}
// ============================================================================================
