/*
 * Control the heating element
 */

#ifndef HEATER_H_
#define HEATER_H_

#include "config.h"

// ============================================================================================
void heaterTimeout(
    void);                                       // The heater has ticked, so modify the state
void heaterSetLevel(uint32_t numerator,
                    uint32_t denominator);  // Modify the power level output by the heater

uint32_t heaterGetPercentage(
    void);                             // Return the percentage of time the heater is on
void heaterInit(void);                                          // Perform the initalisation
// ============================================================================================

#endif /* HEATER_H_ */
