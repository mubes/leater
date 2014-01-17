/*
 * Execute a stored temperature profile. The profiles themselves are defined externally and are kept
 * in the sysConfig structure.
 */

#include "timers.h"
#include "command.h"
#include "sensor.h"
#include "printf.h"
#include "heater.h"
#include "log.h"

typedef struct

{
    enum
    {
        ProfileStateIdle, ProfileStateRunning, ProfileStateError
    } state;
    uint32_t stepNumber;            // What step are we currently on?
    uint32_t totalRuntime;          // How long has the sequence been running?

    uint32_t stepType;              // What kind of step is this one?
    int32_t remainingStepTime;      // Time remaining in this step
    uint32_t commandedTemp;         // The temperature we're asking for
    int32_t stepTemp;               // Change per unit time that's requested
    uint32_t intendedFinal;         // Intended final temp for this step
    BOOL dirPositive;               // Are we waiting for an increase or decrease?

    uint32_t profile;               // What profile are we running?
    timerType t;                    // Timer for this state machine
    BOOL traceOn;                   // Is tracing turned on
} _ProfileStateType;

static _ProfileStateType _condition;

// Interval between temperature profile changes in mS
#define INC_INTERVAL 4000

const char * const _stepnames[]= {PROFILENAMES};

// ============================================================================================
void _nextStep(void)

// Execute the step currently being pointed to

{
    char constructString[300];      // Construction string for output

    // Step tag
    sprintf(constructString,"[%s %d/%d] ", sysConfig.profile[_condition.profile].name,
            _condition.profile + 1, _condition.stepNumber+1);

    if (_condition.stepNumber >= MAX_PROFILE_STEPS)
        {
            // Hey, we ended!
            sprintf(constructString,"%sEnds (%d seconds total runtime)",constructString,
                    _condition.totalRuntime / mS);
            profileStop();
        }
    else
        {
            // Another step, so label the step type
            sprintf(constructString,"%s%s ",constructString,
                    _stepnames[(sysConfig.profile[_condition.profile].step[_condition.stepNumber].command &
                                PROFILE_COMMAND_MASK)>>28]);

            switch (sysConfig.profile[_condition.profile].step[_condition.stepNumber].command &
                    PROFILE_COMMAND_MASK)
                {
                        // -----------------
                    case PROFILE_FOREVER:
                        // We are just doing to stay in this state until actively stopped
                        stateChangeSetpoint(
                            sysConfig.profile[_condition.profile].step[_condition.stepNumber].temp&PROFILE_TEMP_MASK);
                        sprintf(constructString,"%sFOREVER stay at %d°C",constructString,
                                (sysConfig.profile[_condition.profile].step[_condition.stepNumber].temp)&PROFILE_TEMP_MASK /
                                DEGREE);
                        break;

                        // -----------------
                    case PROFILE_JUMP:
                        // We are going to move to another profile
                        sprintf(constructString,"%sJUMP request to profile %d",constructString,
                                sysConfig.profile[_condition.profile].step[_condition.stepNumber].otherProfile);
                        if (sysConfig.profile[_condition.profile].step[_condition.stepNumber].otherProfile < MAX_PROFILES)
                            {
                                _condition.totalRuntime += sysConfig.profile[_condition.profile].step[_condition.stepNumber].time;
                                // Pretty up the printing if after the first step
                                if (_condition.traceOn)
                                    commandReportLine("%s\n",constructString);
                                profileRun(sysConfig.profile[_condition.profile].step[_condition.stepNumber].otherProfile);
                                return;
                            }
                        else
                            _condition.state = ProfileStateError;
                        break;

                        // -----------------
                    case PROFILE_END:
                        sprintf(constructString,"%sSTOP request",constructString);
                        profileStop();
                        break;

                        // -----------------
                    default:
                        // This is just the next step in the process....
                        // Fold in any 'overtime'...this is negative, so subtracting it actually adds it to the total
                        _condition.totalRuntime -= _condition.remainingStepTime;

                        sprintf(constructString,"%s %d°C in %d seconds (%d seconds elapsed)",constructString,
                                ((sysConfig.profile[_condition.profile].step[_condition.stepNumber].temp)&PROFILE_TEMP_MASK) /
                                DEGREE,
                                (sysConfig.profile[_condition.profile].step[_condition.stepNumber].time) / mS,
                                _condition.totalRuntime / mS);

                        // Now prepare the next step....
                        _condition.remainingStepTime =
                            sysConfig.profile[_condition.profile].step[_condition.stepNumber].time;
                        _condition.stepType = sysConfig.profile[_condition.profile].step[_condition.stepNumber].command &
                                              PROFILE_COMMAND_MASK;

                        // Store the intended final temperature for this step
                        _condition.intendedFinal =
                            sysConfig.profile[_condition.profile].step[_condition.stepNumber].temp&PROFILE_TEMP_MASK;

                        // And increment either TEMP or TIME depending on step type
                        if ((sysConfig.profile[_condition.profile].step[_condition.stepNumber].command &
                                PROFILE_COMMAND_MASK) == PROFILE_REACHTEMP)
                            {
                                // This is just a reach-temp-as-quickly-as-possible step
                                _condition.commandedTemp=_condition.intendedFinal;
                                _condition.stepTemp=0;  // We don't need any increments
                                // Instantly aim for final target temp
                                stateChangeSetpoint(_condition.commandedTemp);
                            }
                        else
                            {
                                // This is a reach time step ... smooth out the temperature change over the time
                                // Division of negatives does strange things ... so avoid it
                                _condition.stepTemp = _dabs((((int32_t)_condition.commandedTemp-(int32_t)_condition.intendedFinal)
                                                             *INC_INTERVAL))/_condition.remainingStepTime;
                            }

                        if (_condition.commandedTemp > _condition.intendedFinal)
                            {
                                _condition.dirPositive = FALSE;
                                _condition.stepTemp=-_condition.stepTemp;
                            }
                        else
                            _condition.dirPositive = TRUE;

                        // Schedule a timer tick to occur almost immediately ... this will trigger an iteration
                        timerAdd(&_condition.t, TIMER_ORIGIN_PROFILE, 0, 1);
                        break;
                }
        }

    // Assume we will run for the whole period - if it's different it'll be fixed up at the end of the period
    _condition.totalRuntime += sysConfig.profile[_condition.profile].step[_condition.stepNumber].time;


    // All done - tidy and print
    // Pretty up the printing if after the first step
    if (_condition.traceOn)
        {
            if (_condition.stepNumber)
                commandReportLine("%s\n",constructString);
            else
                commandprintf("%s\n",constructString);
        }
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void profileTimeout(void)

// A profile tick - adjust the setpoint to the correct value

{
    if (((_condition.remainingStepTime < INC_INTERVAL) && (_condition.stepType != PROFILE_REACHTEMP))
            || ((sensorReturnReading() >= _condition.intendedFinal) && (_condition.dirPositive)
                && (_condition.stepType == PROFILE_REACHTEMP))
            || ((sensorReturnReading() <= _condition.intendedFinal) && (!_condition.dirPositive)
                && (_condition.stepType == PROFILE_REACHTEMP)))
        {
            //We've finished with this step - move to the next one
            _condition.stepNumber++;
            _nextStep();
            return;
        }

    // Only increment the time if we've got some left
    if (_condition.remainingStepTime > 0) _condition.commandedTemp += _condition.stepTemp;
    else _condition.commandedTemp=_condition.intendedFinal;

    // ... but update the remaining step time regardless .... this is 'overtime' when negative
    _condition.remainingStepTime -= INC_INTERVAL;

    stateChangeSetpoint(_condition.commandedTemp);
    timerAdd(&_condition.t, TIMER_ORIGIN_PROFILE, 0, INC_INTERVAL);
}
// ============================================================================================
BOOL profileRun(uint32_t profileNum)

// Start running a heating profile

{
    if (profileNum >= MAX_PROFILES) return FALSE;
    if (_condition.state != ProfileStateIdle)
        {
            commandprintf("Already running profile\n");
            return FALSE;
        }

    // Starting _condition is whatever temperature we have....
    _condition.commandedTemp = sensorReturnReading();

    if (_condition.commandedTemp == TEMP_INVALID)
        {
            commandprintf("Sensor not working - cannot run a profile\n");
            return FALSE;
        }

    // All tests passed, start the process
    _condition.stepNumber = 0;
    _condition.remainingStepTime = 0;
    _condition.totalRuntime = 0;
    _condition.profile = profileNum;
    _condition.state = ProfileStateRunning;

    logNewLog();
    _nextStep();
    return TRUE;
}
// ============================================================================================
BOOL profileStop(void)

// Stop an active profile

{
    if (sysConfig.defaultProfile == SETPOINT_STATIC) stateChangeSetpoint(sysConfig.setPoint);
    else
        stateChangeSetpoint(SETPOINT_IDLE);

    if (timerRunning(&_condition.t)) timerDel(&_condition.t);
    _condition.state = ProfileStateIdle;
    logNewLog();
    return TRUE;
}
// ============================================================================================
void profileInit(void)

// Initialise profile management

{
    _condition.state = ProfileStateIdle;
    _condition.profile = 0;
    _condition.traceOn = FALSE;

    // Autostart profile if necessary
    if ((sysConfig.defaultProfile>0)
            && (sysConfig.defaultProfile <= MAX_PROFILES)) profileRun(sysConfig.defaultProfile-1);
    else
        profileStop();
}
// ============================================================================================
char *profileState(void)

// Return text string representing the current state of the machine

{
    static char profileText[80];

    // If we're not running closed loop then indicate that
    if (!stateAutomatic())
        {
            sprintf(profileText,"Open:%d>",stateOutputLevel());
            return profileText;
        }

    switch (_condition.state)
        {
                // -------------------
            case ProfileStateIdle:
                if (stateGetSetpoint() == SETPOINT_IDLE) return "IDLE>";

                sprintf(profileText, "Static(");
                break;
                // ----------------------
            case ProfileStateRunning:
                sprintf(profileText, "Run(%d/%d ", _condition.profile+1,_condition.stepNumber+1);
                break;
                // --------------------
            case ProfileStateError:
                sprintf(profileText, "Error(%d/%d ", _condition.profile+1, _condition.stepNumber+1);
                break;

                // -----
            default:
                sprintf(profileText,"!!!! (");
                break;
        }

    if (sensorReturnReading()!=TEMP_INVALID)
        sprintf(profileText,"%sS:%d.%d°C A:%d.%d°C H:%d%%)>", profileText, stateGetSetpoint() / DEGREE,
                ((stateGetSetpoint()*10) / DEGREE)%10,sensorReturnReading() / DEGREE,
                ((sensorReturnReading()*10) / DEGREE)%10,heaterGetPercentage());
    else
        sprintf(profileText,"%sSensor Fault)>",profileText);

    return profileText;
}
// ===========================================================================================
BOOL profileIsIdle(void)

// Boolean indicating if we're currently idle or not

{
    return (_condition.state == ProfileStateIdle);
}
// ===========================================================================================
void profileTraceOn(BOOL isOnSet)

// Set profile tracing status

{
    _condition.traceOn = isOnSet;
}
// ===========================================================================================
const char *profileGetStepname(uint32_t stepType)

// Return name for this step

{
    return _stepnames[stepType];
}
// ===========================================================================================
