/*
 * Execute a stored temperature profile. The profiles themselves are defined externally and are kept
 * in the sysConfig structure.
 */

#ifndef PROFILE_H_
#define PROFILE_H_

#include "config.h"

#ifndef MAX_PROFILE_STEPS
#error MAX_PROFILE_STEPS needs to be defined for profile.h
#endif

#ifndef MAX_PROFILES
#error MAX_PROFILES needs to be defined for profile.h
#endif

#define PROFILE_REACHTIME    0x00000000 	// Run profile element for specified time
#define PROFILE_FOREVER 	 0x10000000		// Keep this step active forever
#define PROFILE_JUMP		 0x20000000		// Jump to new profile
#define PROFILE_END 		 0x30000000		// End of profile
#define PROFILE_REACHTEMP	 0x40000000	    // Attain a specific temperature with maximum gradient
#define PROFILE_COMMAND_MASK 0xF0000000		// Mask for command frame
#define PROFILE_TEMP_MASK 	 0x0FFFFFFF		// Mask for time

// Ensure these names match the list above!
#define PROFILENAMES "REACHTIME","FOREVER","JUMP","END","REACHTEMP"

#define NO_PROFILE_ACTIVE    0xFFFFFFFF		// Flag indicating no profile active
#define PROFILE_NAME_LENGTH  23  			// Maximum name length for a profile name

// This is the structure for a profile - these end up stored in the config
typedef struct

{
	char name[PROFILE_NAME_LENGTH];
	struct {
		union
		{
			uint32_t command;
			uint32_t temp;
		};
		union
		{
			uint32_t time;
			uint32_t otherProfile;
		};
	} step[MAX_PROFILE_STEPS];
} ProfileType;

// ============================================================================================
void profileTimeout(void);					// Timer tick
BOOL profileRun(uint32_t profileNum);		// Run profile command
BOOL profileStop(void);						// Stop profile command
BOOL profileIsIdle(void);					// Indicator that no profile is running
char *profileState(void);					// Return tag string for profile state
void profileTraceOn(BOOL isOnSet);			// Turn profile tracing on/off
const char *profileGetStepname(uint32_t stepType); // Return name for this step
void profileInit(void);						// Initialise the profile subsystem
// ============================================================================================
#endif /* PROFILE_H_ */
