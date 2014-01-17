/*
 * Utility routines used in various places
 */

#include "dutils.h"
#include <LPC8xx.h>
#include "config.h"

uint32_t _critDepth;            // Depth of critical sections currently embedded in
// ============================================================================================
void denter_critical(void)

// Enter a critical section, maintaining a depth count

{
    __disable_irq();
    _critDepth++;
}
// ============================================================================================
void dleave_critical(void)

// Leave a critical section, enabling interrupts if appropriate

{
    ASSERT(_critDepth);
    if (!--_critDepth)
        __enable_irq();
}
// ============================================================================================
