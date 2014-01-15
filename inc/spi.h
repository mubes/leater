/*
 * SPI reader module. Targeting MAX6675. Returns readings from the sensor.
 */


#ifndef SPI_H_
#define SPI_H_

#include "config.h"

// ============================================================================================
#define THERMOCOUPLE_MINIMUM_INTERVAL 	300	// Minimum interval between reading requests

void spiRequest_sample(void);				// Request a new sample from the sensor
uint32_t spiGet_Result(void);				// Return most recently read result from the sensor

void spiInit(void);							// Initialise the SPI subsystem
// ============================================================================================
#endif /* SPI_H_ */
