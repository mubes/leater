/*
 * The main statemachine for controlling the current temperature of the system.  Temperature can be changed by
 * requests to this machine, but it is only responsible for achieving a static temperature, profiles are created
 * and executed elsewhere and delivered to this machine as updates to the static point.
 */

#include <LPC8xx.h>
#include "config.h"
#include "statemachine.h"
#include "nv.h"
#include "flags.h"
#include "adc.h"
#include "timers.h"
#include "command.h"
#include "log.h"
#include "statemachine.h"
#include "sensor.h"
#include "heater.h"
#include "printf.h"
#include "pid.h"
#include "gpio.h"
#include "ledflash.h"

// Cycle time when heater is on
#define CYCLE_LEN		1000

#define LED_FLASH_TIME	        		  50    // Time in MS for LED to flash while collecting sample
#define LED_ERROR_FLASH_TIME			 150	// Time in MS for LED to flash under error condition

static pidInstanceType pidInstance;				// The PID control instance
static BOOL isErrored;							// Is the heater currently in a error state?
static timerType tstate;						// Timer for this state machine

// ============================================================================================
void contribute_log_entry(uint32_t temperature_set, uint32_t onproportion_set, uint32_t setpoint_set, uint32_t time_tick)

// Add part of an entry for logging - we don't log as often as we refresh the sample, so
// the purpose of this routine is to average the readings and write them at the logging interval.

{
	static uint32_t acc_temp = 0, acc_onprop = 0, acc_time = 0, acc_setpoint = 0;
	static uint32_t old_onprop = 0, old_setpoint = 0;
	uint32_t time_part;

	// Only work in integer seconds, to keep sizes under control
	time_tick/=mS;
	acc_time += time_tick;			// Time is in integer seconds

	if (acc_time >= sysConfig.recordInterval)
	{
		// It's time to write some data
		time_part = (sysConfig.recordInterval) - (acc_time - time_tick);
	}
	else
		time_part = time_tick;

	// Accumulate variables ....
	acc_temp += temperature_set * time_part;
	acc_onprop += onproportion_set * time_part;
	acc_setpoint += setpoint_set * time_part;

	if (acc_time >= sysConfig.recordInterval)
	{
		acc_onprop/=acc_time;
		acc_temp/=acc_time;
		acc_setpoint/=acc_time;

		// Write the average for this period of time
		if (old_onprop != acc_onprop)
		{
			logWrite(LOG_ON_PERCENTAGE, acc_onprop);
			old_onprop = acc_onprop;
		}

		if (old_setpoint != acc_setpoint)
		{
			logWrite(LOG_SETPOINT_SET, acc_setpoint);
			old_setpoint = acc_setpoint;
		}

		// We make sure temperature is written last as it simplifies reconstruction
		logWrite(LOG_TEMPERATURE, acc_temp);

		// Zero the variables
		acc_temp = 0;
		acc_onprop = 0;
		acc_time = 0;
		acc_setpoint = 0;

		// .. and store whatever is left over for the next minute
		if (time_tick-time_part)
			contribute_log_entry(temperature_set, onproportion_set, setpoint_set, time_tick - time_part);
	}
}
// ============================================================================================
void _getReading(void)

{
	uint32_t temperature, output;

	// See if we have got a valid temperature
	temperature = sensorReturnReading();

	// Only turn the LED off if we've got legal data, otherwise flash it
	if (temperature == TEMP_INVALID)
	{
		ledSetState(LEDFLASH_ERROR);
		heaterSetLevel(0, CYCLE_LEN);
		isErrored=TRUE;
		sensorRequestCallback(); 	// Get another reading as soon as we can
		return;
	}

	ledSetState(LEDFLASH_NORMAL);
	// ...otherwise we've got a valid temp, so do the business with it
	if (pidGetSetpoint(&pidInstance) == SETPOINT_IDLE)
		output = 0;
	else
		output=pidCalc(&pidInstance,temperature);

	heaterSetLevel(output, CYCLE_LEN);
	contribute_log_entry(temperature, heaterGetPercentage(), pidGetSetpoint(&pidInstance), CYCLE_LEN);

	// Now wait for a while before doing it all again
	timerAdd(&tstate, TIMER_ORIGIN_STATEMACHINE, 0, CYCLE_LEN);
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void statePIDMonitor(BOOL isOn)

// Update status of the PID monitor

{
	pidMonitor(&pidInstance,isOn);
}
// ============================================================================================
void statePIDSet(void)

// Update PID from settings in sysConfig

{
	pidSetParams(&pidInstance, sysConfig.Cp, sysConfig.Ci, sysConfig.Cd, CYCLE_LEN, 0, CYCLE_LEN/25, CYCLE_LEN, 0);
}
// ============================================================================================
void stateInit(void)

// Initialise the state subsystem

{
	statePIDSet();
	pidSetSetpoint(&pidInstance,SETPOINT_IDLE);		// Unless we're told differently, then don't try and perform any heating

	isErrored=TRUE;

	// wait for a temperature reading to arrive...
	sensorRequestCallback();

	timerInit(&tstate);
	stateLogsFlushed();
}
// ============================================================================================
void stateLogsFlushed(void)

// Callback that the logs have been flushed - put the initial information into them

{
//.. log the record interval
	logWrite(LOG_RECORD_INTERVAL, sysConfig.recordInterval);
	logWrite(LOG_SETPOINT_SET, pidGetSetpoint(&pidInstance));
}
// ============================================================================================
void stateTimeout(uint32_t timerNumber)

// Callback that there has been a timeout to the state machine

{
	_getReading();
}
// ============================================================================================
void stateReadingArrived(void)

// We got a new temperature reading - process it

{
	_getReading();
}
// ============================================================================================
void stateChangeSetpoint(uint32_t newSetpoint)

// Request to change the setpoint

{
	if (pidGetSetpoint(&pidInstance)==newSetpoint) return;

	pidSetSetpoint(&pidInstance,newSetpoint);
}
// ============================================================================================
uint32_t stateGetSetpoint(void)

// Request to find current set point

{
	return pidGetSetpoint(&pidInstance);
}
// ============================================================================================
BOOL stateAutomatic(void)

// Return if in automatic or open loop mode

{
	return pidIsClosedloop(&pidInstance);
}
// ============================================================================================
int32_t stateOutputLevel(void)

// Get manual mode output level

{
	return pidGetOutput(&pidInstance);
}
// ============================================================================================
void stateLoopOpen(BOOL isOpen)

// Set loop to open or closed mode

{
	pidSetOutputAuto(&pidInstance,!isOpen);
}
// ============================================================================================
void stateSetOutputLevel(int32_t levelSet)

// Set manual output level in open loop mode

{
	pidSetManualOutput(&pidInstance,levelSet);
}
// ============================================================================================
