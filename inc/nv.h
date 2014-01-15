/*
 * Manage non-volatile storage.  This is the 'spare' flash from the top of program to the end
 * of the flash space.  The current configuration is stored right at the very top and the rest
 * of the spare memory is given over to log storage.
 *
* Non-volatile storage. Contains two things; The config (at the end of the available space) and the rest of the
 * spare flash is given over to generic storage, which can only be read from/written to in a stream manner. Note that
 * 0xFFFFFFFF is not allowed to the written, as that's used to flag empty memory!  So we have to bit-stuff.
 * We don't have any real backup memory so this is the only easy way to find out where is empty.
 */

#ifndef NV_H_
#define NV_H_

#include <LPC8xx.h>
#include "config.h"

// Special tags in log memory to indicate specific conditions
#define NV_EMPTY         0xFFFFFFFF

// ----- Iterators for transiting through a block of memory
typedef enum {NV_OK, NV_ERROR, NV_ENDSTATE} nvState;

// ----- nv Iterators
typedef struct
{
	uint32_t *rp;
	nvState state;
} nvIterator;

// ============================================================================================
// Information about the part
// --------------------------
uint32_t nvRead_partID(void);							// Read the ROM version identifier
uint32_t nvRead_partVersion(void);						// Read the part identifier

// Read/write to/from NV
// ---------------------
BOOL nvWrite_entry(uint32_t val_to_write);				// Write value to store
uint32_t nvGetSpace(void);								// Return the amount of free space left
uint32_t nvTotalSpace(void);
BOOL nvFlush(void);										// Flush the whole of the log memory
// Return total space in NV memory
uint32_t nvTotalSpace(void);							// Iterator over NV storage for read access

// ----------------------------------------
nvState nvIteratorState(nvIterator *n);					// Return current state of iterator
uint32_t nvIteratorNext(nvIterator *n);					// Get next entry from nv store
void nvInitIterator(nvIterator *n);						// Initialise read iterator

// Configuration storage read/write
// --------------------------------
BOOL nvWriteConfig(const ConfigStoreType *c);			// Scrub and re-write the config to nv memory
void nvReadConfig(ConfigStoreType *c);					// Get configuration from nv ram into config buffer

// ...and the initialisation function
// ----------------------------------
BOOL nvInit(void);										// Initialisation function for non-volatile memory
// ============================================================================================
#endif /* NV_H_ */
