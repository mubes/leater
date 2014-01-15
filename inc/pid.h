/*
 * PID control algorithm
 */

#ifndef PID_H_
#define PID_H_

#include "config.h"

typedef struct
{
	int32_t Cp;
	int32_t Ci;
	int32_t Cd;
	int32_t bias;

	int32_t	interval;

	int32_t setPoint;

	int32_t prevError;
	int32_t prevPv;
	int32_t integral;
	int32_t integralLimit;
	int32_t outputMax;
	int32_t outputMin;
	int32_t output;

	BOOL	monitorOn;
	BOOL 	automatic;
} pidInstanceType;

// ============================================================================================
void pidSetParams(pidInstanceType *p, int32_t Cp, int32_t Ci, int32_t Cd, int32_t interval, int32_t bias, int32_t integralLimit, int32_t outputMax, int32_t outputMin);
int32_t pidCalc(pidInstanceType *p, int32_t pvSet);		// Perform PID iteration
void pidReset(pidInstanceType *p);						// Reset process control history
int32_t pidGetSetpoint(pidInstanceType *p);				// Return current setpoint
void pidMonitor(pidInstanceType *p, BOOL isOnSet);		// Flag if we're printing PID activities
void pidSetSetpoint(pidInstanceType *p, int32_t setpointSet);  	// Update setpoint
void pidSetManualOutput(pidInstanceType *p, int32_t outputSet);	// Set manual output level (for open loop working)
void pidSetOutputAuto(pidInstanceType *p,BOOL automaticSet);	// Set manual or automatic output (open or closed loop)
BOOL pidIsClosedloop(pidInstanceType *p);				// Return flag indicating if in closed loop mode
int32_t pidGetOutput(pidInstanceType *p);				// Return current output level
// ============================================================================================

#endif /* PID_H_ */
