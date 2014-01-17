/*
 * PID control algorithm
 */

#include "pid.h"
#include "command.h"
#include "timers.h"

// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void pidSetParams(pidInstanceType *p, int32_t Cp, int32_t Ci, int32_t Cd, int32_t interval,
                  int32_t bias, int32_t integralLimit, int32_t outputMax, int32_t outputMin)

{
    // In case the parameters have been updated then we need to re-compute the integral term
    // this allows smooth transition from the old to new ranges
    if (p->integral)
        {
            // @@@ TODO
            p->integral=0;
            if (_dabs(p->integral) > p->integralLimit) p->integral = _dsign(p->integral)*p->integralLimit;
        }

    p->Cp=Cp;
    p->Ci=Ci;
    p->Cd=Cd;
    p->interval=interval;
    p->bias=bias;
    p->integralLimit=integralLimit;
    p->outputMax=outputMax;
    p->outputMin=outputMin;
    p->monitorOn=FALSE;
    p->automatic=TRUE;
}
// ============================================================================================
void pidMonitor(pidInstanceType *p, BOOL isOnSet)

// Flag if we're printing PID activities

{
    p->monitorOn=isOnSet;
}
// ============================================================================================
void pidReset(pidInstanceType *p)

// Reset process control history

{
    p->integral=p->prevError=0;
}
// ============================================================================================
int32_t pidGetSetpoint(pidInstanceType *p)

// Return current setpoint

{
    return p->setPoint;
}
// ============================================================================================
void pidSetSetpoint(pidInstanceType *p, int32_t setpointSet)

// Update setpoint

{
    p->setPoint=setpointSet;
}
// ============================================================================================
int32_t pidCalc(pidInstanceType *p, int32_t pvSet)

// Perform PID iteration

{
    int32_t error,derivative;

    if (!p->automatic)
        {
            if (p->monitorOn)
                commandReportLine("%d,%d,%d\n", timerSecs(),pvSet, p->output);
            return p->output;
        }

    error = ((int32_t) p->setPoint-pvSet);
    p->integral+=error;

    // Floor the integral so it doesn't go out of range
    if (_dabs(p->integral) > p->integralLimit) p->integral = _dsign(p->integral)*p->integralLimit;

    derivative = (p->Cd*(p->prevPv-pvSet))/p->interval;
    p->prevPv=pvSet;

    p->output = p->bias + p->Cp*error + ((p->integral*p->interval)/p->Ci) + derivative;

    // Floor the output so it doesn't go out of range
    if (p->output > p->outputMax) p->output = p->outputMax;
    if (p->output < p->outputMin) p->output = p->outputMin;

    if (p->monitorOn)
        commandReportLine("%d,%d,%d,%d,%d,%d,%d\n", timerSecs(),p->setPoint,pvSet,p->Cp*error,
                          ((p->integral*p->interval)/p->Ci),derivative, p->output);

    p->prevError = error;

    return p->output;
}
// ============================================================================================
void pidSetManualOutput(pidInstanceType *p, int32_t outputSet)

// Set manual output level (for open loop working)

{
    p->output=outputSet;
    if (p->output > p->outputMax) p->output = p->outputMax;
    if (p->output < p->outputMin) p->output = p->outputMin;
}
// ============================================================================================
void pidSetOutputAuto(pidInstanceType *p, BOOL automaticSet)

// Set manual or automatic output (open or closed loop)

{
    p->automatic=automaticSet;
}
// ============================================================================================
BOOL pidIsClosedloop(pidInstanceType *p)

// Return flag indicating if in closed loop mode

{
    return p->automatic;
}
// ============================================================================================
int32_t pidGetOutput(pidInstanceType *p)

// Return current output level

{
    return p->output;
}
// ============================================================================================
