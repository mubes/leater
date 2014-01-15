/*
 * Brownout management and tracking.
 */

#ifndef BOD_H_
#define BOD_H_

#include "config.h"
#include "bod.h"

// ============================================================================================
BOOL bodIsActive(void);				// Return if a brownout has been triggered
void bodTimeout(void);				// The brownout has timed out - release the IRQ to trigger again if needed
uint32_t bodCount(void);			// Return number of brownouts that have occurred

void bodInit(void);					// Initialisation function
// ============================================================================================
#endif /* BOD_H_ */
