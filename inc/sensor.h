/*
 * Abstracted sensor class which maps to whatever underlying sensors are in use.
 */

#include "config.h"

// ============================================================================================
int32_t sensorReturnReading(void);      // Get most recent reading from sensor
void sensorRequestCallback(
    void);       // Flag to let state machine know when a new reading is available
void sensorReadingReady(void);          // A new temperature reading is available
void sensorTimeout(void);               // A timer event has occured

void sensorInit(void);                  // Initialise sensor module
// ============================================================================================
