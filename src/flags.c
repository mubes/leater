/*
 * The flag handler.  Flags are used for signalling between subsystems and for communication from
 * the interrupt layer to the base layer. Safety is assured by means of turning off interrupts
 * during critical sections.
 */

#include "flags.h"
#include "dutils.h"

static volatile uint32_t flags;					// The current state of the flags
static const char *flagname[]={FLAGNAMES};		// ...and their names, for pretty-printing
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void flag_post(uint32_t flag_to_set)

// Post a new flag

{
	denter_critical();
	flags|=flag_to_set;
	dleave_critical();
}
// ============================================================================================
uint32_t flag_get(void)

// Get the current flagset, and reset them all in the process

{
	uint32_t flagRetSet;
	denter_critical();
	flagRetSet=flags;
	flags=0;
	dleave_critical();

	return flagRetSet;
}
// ============================================================================================
uint32_t flag_test(uint32_t flag_to_check)

// Test for a specific flag, and reset it in the process

{
	uint32_t isSet;
	denter_critical();
	isSet=(flags&flag_to_check)!=0;
	if (isSet) flags&=~flag_to_check;
	dleave_critical();

	return isSet;
}
// ============================================================================================
uint32_t flag_clear(void)

// See if there are any flags set at all

{
	return (flags==0);
}
// ============================================================================================
const char *flag_getname(uint32_t flag_to_check)

// Return the name of the requested flag (for pretty printing)

{
	uint32_t c=0;

	if (!flag_to_check) return "No Event";

	while ((c<32) && (!(flag_to_check&(1<<c)))) c++;

	if (c<32)
		return flagname[c];
	return "??????";
}
// ============================================================================================
void flagInit(void)

// Initialise the flags system

{
	flags=0;
}
// ============================================================================================
