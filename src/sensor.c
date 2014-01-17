/*
 * Abstracted sensor class which maps to whatever underlying sensors are in use.
 */

#include "sensor.h"
#ifdef SENSOR_THERMISTOR
#include "adc.h"
#endif

#ifdef SENSOR_THERMOCOUPLE
#include "spi.h"
#endif

#include "flags.h"
#include "timers.h"

static timerType intervalTimer;             // Timer between temperature readings
static BOOL callback=FALSE;                 // Do we want to tell someone we've got a reading?
//@@@static
int32_t currentTemp;                        // Most recently read temperature

// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routine
// ============================================================================================
// ============================================================================================
// ============================================================================================
// ============================================================================================
int32_t sensorReturnReading(void)

// Return the most recently obtained reading

{
    return currentTemp;
}
// ============================================================================================
uint32_t sensorInterval(void)

// Return minimum interval between sensor readings in mS

{
#ifdef SENSOR_THERMISTOR
    return THERMISTOR_MINIMUM_INTERVAL;
#endif

#ifdef SENSOR_THERMOCOUPLE
    return THERMOCOUPLE_MINIMUM_INTERVAL;
#endif

}
// ============================================================================================
void sensorInit(void)

// Initialise the sensor system

{
#ifdef SENSOR_THERMISTOR
    sdadcInit();
    sdadcStartup();
#endif

#ifdef SENSOR_THERMOCOUPLE
    spiInit();
#endif
    // Perform a fake timeout to get the first sample
    currentTemp=TEMP_INVALID;
    sensorTimeout();
}
// ============================================================================================
void sensorReadingReady(void)

{
    static int32_t filter;
    uint32_t newTemp;

#ifdef SENSOR_THERMISTOR
    newTemp = sdadcGetResult();
#endif

#ifdef SENSOR_THERMOCOUPLE
    newTemp = spiGet_Result();
#endif

    if (newTemp==TEMP_INVALID)
        // If the value is invalid, then reflect it immediately
        currentTemp=newTemp;
    else
        {
            // ...we have a genuine reading
            if (currentTemp==TEMP_INVALID)
                // This is the first valid reading, so just grab it
                filter=newTemp<<sysConfig.k;
            else
                // This is a subsequent reading - account for it
                filter=filter-(filter>>sysConfig.k)+newTemp;

            // Whatever happens, we can now extract the temperature
            currentTemp=filter>>sysConfig.k;
        }

    if (callback)
        {
            callback=FALSE;
            flag_post(FLAG_TEMPCALLBACK);
        }
}
// ============================================================================================
void sensorTimeout(void)

// Time to grab another sample
{
#ifdef SENSOR_THERMISTOR
    sdadcGet_sample();
#endif

#ifdef SENSOR_THERMOCOUPLE
    spiRequest_sample();
#endif
    timerAdd(&intervalTimer, TIMER_ORIGIN_SENSOR, 0, sensorInterval());
}
// ============================================================================================
void sensorRequestCallback(void)

{
    callback=TRUE;
}
// ============================================================================================
