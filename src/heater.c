/*
 * Control the heating element
 */

#include "config.h"
#include "heater.h"
#include "gpio.h"
#include "timers.h"

static uint32_t _onPropNumerator;
static uint32_t _onPropDenominator;

static BOOL _isNumerator=FALSE;
static timerType heaterTimer;
// ============================================================================================
void heaterTimeout(void)

// The heater has ticked, so modify the state

{
	if (_isNumerator)
	{
		// Heat on part has come to an end
		if (_onPropDenominator>_onPropNumerator)
		{
			gpioHeat(OFF);
			timerAdd(&heaterTimer, TIMER_ORIGIN_HEATER, 0, _onPropDenominator-_onPropNumerator);
		}
	}
	else
	{
		// Heat off part has come to an end
		gpioHeat(ON);
		timerAdd(&heaterTimer, TIMER_ORIGIN_HEATER, 0, _onPropNumerator );
	}
	_isNumerator=!_isNumerator;
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void heaterSetLevel(uint32_t numeratorSet, uint32_t denominatorSet)

// Modify the power level output by the heater

{
	_onPropNumerator=numeratorSet;
	_onPropDenominator=denominatorSet;

	if (timerRunning(&heaterTimer))
	{
		if ((_onPropNumerator==0) || (_onPropDenominator==0))
		{
			// We don't want any more heat, so stop everything
			timerDel(&heaterTimer);
			gpioHeat(OFF);
		}
		// Otherwise, the new times will be picked up on the next iteration
		return;
	}

	// ...alternatively, we need to _start a timer if there is heating to be done
	if ((_onPropNumerator!=0) && (_onPropDenominator!=0))
	{
		// Can use the timer to tick the heater
		_isNumerator=FALSE;
		heaterTimeout();
	}
	else
		// otherwise, for safetys sake, make sure we're off
		gpioHeat(OFF);
}
// ============================================================================================
void heaterInit(void)

// Perform the initialisation

{
	timerInit(&heaterTimer);
}
// ============================================================================================
uint32_t heaterGetPercentage(void)

// Return the percentage of time the heater is on

{
	if ((_onPropNumerator==0) || (_onPropDenominator==0)) return 0;

	return ((_onPropNumerator*100)/_onPropDenominator);
}
// ============================================================================================
