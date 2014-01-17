/*
 * Optimised logging....only uses minimum number of bits to store any logging data, and deals with the
 * fact that logs may be stopped at any time (e.g. when power fails or during brownout).
 *
 * Uses an iterator which returns the next uint32 from the data store (or puts it there).
 *
 */

#include "log.h"
#include "nv.h"

static uint32_t numLogs;      // Number of distinct logs
static uint32_t bitStream;    // The bits being written to file
static uint32_t bitStreamLen; // How many of them we've got
static uint32_t bitOneCount;  // How many 1's have we got - do we need to bit-stuff?

// Defines used for bit stuffing and variable masking
#define MAX_ONES 30
#define MAX_BITS 32
#define LOG_ELEMENT_BITS_MASK  ((1<<LOG_ELEMENT_BITS)-1)

// Special value to indicate a new session.
#define LOG_SESSION_START 0xFFFFFFFE

// ... the names of the variables, populated from the LOG_BITS define
static const struct
{
    uint32_t length;
    uint32_t scale;
    char *units;
    char *name;
} vardesc[]= { LOG_BITS };

// ============================================================================================
BOOL _pump(BOOL x)

// Internal bit pump for entries, includes bit-stuffer to avoid false end-of-data markers

{
    if (x)
        {
            if (bitOneCount++==MAX_ONES)
                {
                    // Too many 1's in a row - force a zero in there
                    bitOneCount=0;
                    _pump(0);
                }
        }
    else
        bitOneCount=0;

    bitStream=(bitStream>>1)|(x?0x80000000:0);
    bitStreamLen++;

    if (bitStreamLen==MAX_BITS)
        {
            bitStreamLen=0;
            return nvWrite_entry(bitStream);
        }
    return TRUE;
}
// ============================================================================================
void _getNext(logIterator *n)

// Get next uint32 from the underlying non-volatile storage

{
    n->bitsAvail=MAX_BITS;
    n->readVal=nvIteratorNext(&n->nv);

    switch (n->readVal)
        {
            case NV_EMPTY:
                n->state=LOG_ENDSTATE;
                return;

            case LOG_SESSION_START:
                n->currentLog++;
                n->bitOneCount=0;
                _getNext(n);
                break;

            default:
                break;
        }
}// ============================================================================================
BOOL _unpump(logIterator *n)

// Pull bit from the linear store, removing bit-stuffing as nessessary.

{
    BOOL retVal;

    retVal=n->readVal&1;

    // Make sure we don't need to get another uint32
    if ((!--n->bitsAvail) && (n->state==LOG_OK))
        _getNext(n);
    else
        n->readVal>>=1;

    if (n->state!=LOG_OK)
        return FALSE;

    // Check for bit-stuffing
    if (retVal)
        {
            if (++n->bitOneCount==MAX_ONES)
                _unpump(n);  // Pull the next value
        }
    else
        n->bitOneCount=0;

    return retVal;
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
// ============================================================================================
void logInitIterator(logIterator *n)

// Iterators cycle over the log data.  They are set to initial values here

{
    nvInitIterator(&n->nv);     // Initialise the underlying store iterator
    n->currentLog=0;
    n->state=LOG_OK;
    n->bitOneCount=0;
    _getNext(n);                // Get the first data
}
// ============================================================================================
BOOL logIteratorNext(logIterator *n)

// Get the next element from the log

{
    uint32_t activeLog = n->currentLog;  // Keep this log in case it changes
    n->variable=0;
    n->value=0;

    uint32_t bitsToRead=LOG_ELEMENT_BITS;

    // Start by reading the name of the variable
    while ((bitsToRead--) && (n->state==LOG_OK)
            && (activeLog==n->currentLog)) n->variable=(n->variable<<1)|_unpump(n);

    if (n->state!=LOG_OK) return FALSE;

    // This is a whole new session in the log - recursively grab the next one
    if (activeLog!=n->currentLog)
        return logIteratorNext(n);

    // Now the value
    bitsToRead=vardesc[n->variable].length;
    while ((bitsToRead--) && (n->state==LOG_OK)
            && (activeLog==n->currentLog)) n->value=(n->value<<1)|_unpump(n);

    if (activeLog!=n->currentLog)
        return logIteratorNext(n);

    return (n->state==LOG_OK);
}
// ============================================================================================
uint32_t logVariable(logIterator *n)

// Get the next logged element from the iterator

{
    return (n->variable);
}
// ============================================================================================
const char *logVariableName(logIterator *n)

// Get the prettyprintable name of the next logged element from the iterator

{
    return vardesc[n->variable].name;
}
// ============================================================================================
uint32_t logValue(logIterator *n)

// Get the value from the iterator

{
    return n->value*vardesc[n->variable].scale;
}
// ============================================================================================
const char *logUnits(logIterator *n)

// Get the string representing the units for the variable

{
    return vardesc[n->variable].units;
}
// ============================================================================================
uint32_t logScale(logIterator *n)

// Get the integer representing the scale for the reading (i.e. number of bits after decimal point)

{
    return vardesc[n->variable].scale;
}
// ============================================================================================
logStateType logIteratorState(logIterator *n)

// Return current state of the log iterator

{
    return n->state;
}
// ============================================================================================
BOOL logGotoEnd(logIterator *n)

// Cycle though the log entries until we get to empty space

{
    // We can fast forward over the other entries without unpumping them as
    // we know that a new session is always on a four byte boundary
    while (n->state==LOG_OK)
        _getNext(n);

    return (n->state!=LOG_OK);
}
// ============================================================================================
BOOL logGotoLog(logIterator *n, uint32_t logNum)

// Goto a specific log number - assumption is made that we are starting at the beginning
// or, at least, a log lower than the one we are trying to reach.

{
    // We can fast forward over the other entries without unpumping them as
    // we know that a new session is always on a four byte boundry
    while ((n->state==LOG_OK) && (n->currentLog<logNum))
        _getNext(n);

    return (n->state==LOG_OK);
}
// ============================================================================================
uint32_t logCurrentLog(logIterator *n)

// Return number of current log

{
    return n->currentLog;
}
// ============================================================================================
BOOL logNewLog(void)

// Create a new log at the end of the storage

{
    bitStreamLen=0;
    bitOneCount=0;
    numLogs++;
    return nvWrite_entry(LOG_SESSION_START);
}
// ============================================================================================
BOOL logWrite(uint32_t variable, uint32_t value)

// Write a logged variable to the store...commit to underlying storage as necessary.

{
    uint32_t bitsToWrite=LOG_ELEMENT_BITS;
    BOOL goodWrite=TRUE;
    uint32_t varBitsToWrite=vardesc[variable].length;

    // Max out the ranges
    value/=vardesc[variable].scale;
    if (value>(1<<varBitsToWrite)-1) value=(1<<varBitsToWrite)-1;

    ASSERT(variable<LOG_ELEMENT_BITS_MASK);

    // Start by writing the name of the variable
    while (bitsToWrite--)
        goodWrite&=_pump(variable&(1<<bitsToWrite));

    // Now the value
    while (varBitsToWrite--)
        goodWrite&=_pump(value&(1<<varBitsToWrite));

    return goodWrite;
}
// ============================================================================================
uint32_t logNumLogs(void)

// Return number of logs in the store

{
    return numLogs;
}
// ============================================================================================
BOOL logFlushLogs(void)

// Flush all logs from the store and create a new one

{
    nvFlush();
    numLogs=0;
    return logNewLog();
}
// ============================================================================================
BOOL logInit(void)

// Initialise logging module

{
    logIterator n;
    nvInit();
    numLogs=0;

    // Spin through memory counting number of valid logs that are there
    logInitIterator(&n);
    logGotoEnd(&n);
    numLogs=logCurrentLog(&n);

    return logNewLog();
}
// ============================================================================================
