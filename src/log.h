#ifndef LOG_H_
#define LOG_H_

#include "config.h"
#include "nv.h"

#ifndef LOG_ELEMENT_BITS
#error "LOG_ELEMENT_BITS not defined - needed to inform how many distinct log variables there are"
#endif

#ifndef LOG_BITS
#error "No Logging entities defined, create LOG_BITS #define please"
#endif

// ----- Iterators for transiting through a log
typedef enum {LOG_OK, LOG_ERROR, LOG_ENDSTATE} logStateType;

typedef struct
{
    uint32_t currentLog;    // What log number are we on?
    uint32_t readVal;       // Current read value from memory
    uint32_t bitsAvail;     // Number of bits available in this read word
    uint32_t bitOneCount;   // Number of 1's in a row (for bit stuffing removal)
    uint32_t variable;      // The variable last read
    uint32_t value;         // The value for the variable last read
    nvIterator nv;          // The underlying iterator over the NV memory
    logStateType state;     // Current state
} logIterator;

// ============================================================================================
// Iterator over a log - define one of these to access
// ---------------------------------------------------
void logInitIterator(logIterator
                     *n);                       // Iterators cycle over the log data.  They are set to initial values here
logStateType logIteratorState(logIterator
                              *n);              // Return current state of the log iterator
uint32_t logIteratorNext(logIterator *n);                   // Get the next element from the log
BOOL logGotoLog(logIterator *n, uint32_t logNum);           // Goto a specific log number
uint32_t logCurrentLog(logIterator *n);                     // Return number of current log
BOOL logGotoEnd(logIterator
                *n);                            // Cycle though the log entries until we get to empty space

// Accessors for current value from log
uint32_t logVariable(logIterator
                     *n);                       // Get the next logged element from the iterator
const char *logVariableName(logIterator
                            *n);                // Get the prettyprintable name of the next logged element from the iterator
uint32_t logValue(logIterator *n);                          // Get the value from the iterator
uint32_t logScale(logIterator
                  *n);                          // Get the integer representing the scale for the reading
const char *logUnits(logIterator
                     *n);                       // Get the string representing the units for the variable

// Log manipulation
// ----------------
BOOL logNewLog(
    void);                                       // Create a new log at the end of the storage
BOOL logFlushLogs(
    void);                                    // Flush all logs from the store and create a new one
uint32_t logNumLogs(void);                                  // Return number of logs in the store

// Data entry
// ----------
BOOL logWrite(uint32_t variable,
              uint32_t value);           // Write a logged variable to the store...commit to underlying storage as necessary.

// ...and the init function - must be called _before_ any other routines
// ---------------------------------------------------------------------
BOOL logInit(void);                                         // Initialise logging module
// ============================================================================================
#endif /* LOG_H_ */
