/*
 * The main statemachine for controlling the current temperature of the system.  Temperature can be changed by
 * requests to this machine, but it is only responsible for achieving a static temperature, profiles are created
 * and executed elsewhere and delivered to this machine as updates to the static point.
 */

#ifndef STATEMACHINE_H_
#define STATEMACHINE_H_

// ============================================================================================
// Trigger events from outside of this machine
void stateTimeout(uint32_t timerNumber);                // Callback to State machine from timer
void stateLedTimeout(void);                             // Callback to LED from timer

void stateLogsFlushed(
    void);                            // Logs have been flushed - write any essential data
uint32_t stateGetSetpoint(void);                        // Request to find current set point
void stateChangeSetpoint(uint32_t newSetpoint);         // Request to change the setpoint
void stateStateMonitor(BOOL isOn);                      // Change state monitoring status
void statePIDMonitor(BOOL isOn);                        // Change PID monitoring status
void stateReadingArrived(void);                         // ...a reading has arrived

void statePIDSet(void);                                 // Update PID from settings in sysConfig
BOOL stateAutomatic(void);                              // Return if in automatic or open loop mode
int32_t stateOutputLevel(void);                         // Get manual mode output level
void stateSetOutputLevel(int32_t levelSet);             // Set manual output level in open loop mode
void stateLoopOpen(BOOL isOpen);                        // Set loop to open or closed mode
void stateInit(void);                                   // ... and the initialisation function
// ============================================================================================
#endif /* STATEMACHINE_H_ */
