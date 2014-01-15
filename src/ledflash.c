#include "timers.h"
#include "ledflash.h"
#include "gpio.h"

static timerType ledFlash;						// Timer for LED flash
LedFlashType 	 flashKind=LEDFLASH_OFF;		// Type of flashing that's going on
uint32_t		 flashIndex;					// Where we are in the sequence

static uint32_t flashTable[LEDFLASH_MAX][3]={
		{200,200,0,},
		{100,1000,0 }
};

// ============================================================================================
void ledTimeout(void)

// ...the LED has finished its flash cycle


{
	// Invert current state of the LED
	gpioSet_led(GPIO_GREEN_LED, (flashIndex%2==1));
	if (!flashTable[flashKind][++flashIndex]) flashIndex=0;
	timerAddInc(&ledFlash, TIMER_ORIGIN_LEDFLASH, 0, flashTable[flashKind][flashIndex]);
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void ledSetState(LedFlashType flashSet)

{
	if (flashSet>=LEDFLASH_OFF)
	{
		timerDel(&ledFlash);
		gpioSet_led(GPIO_GREEN_LED, OFF);
		return;
	}

	if (flashSet!=flashKind)
	{
		if (timerRunning(&ledFlash))
			timerDel(&ledFlash);
		flashKind=flashSet;
		flashIndex=0;
		ledTimeout();
	}
}
// ============================================================================================
void ledInit(void)

{
	timerInit(&ledFlash);
}
// ============================================================================================

