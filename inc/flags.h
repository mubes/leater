/*
 * The flag handler.  Flags are used for signalling between subsystems and for communication from
 * the interrupt layer to the base layer. Safety is assured by means of turning off interrupts
 * during critical sections.
 */

#ifndef FLAGS_H_
#define FLAGS_H_

#include <LPC8xx.h>

// ...add more flags here as required, and update the FLAGNAMES and ALLFLAGS macros to suit
#define FLAG_ADC_READ     (1<<0)                    // ADC reading
#define FLAG_TICK         (1<<1)                    // Clock tick
#define FLAG_GOTTEMP      (1<<2)                    // A temperature reading is available
#define FLAG_UARTRX       (1<<3)                    // A character has been received in the interrupt level
#define FLAG_TEMPCALLBACK (1<<4)                    // A temperature callback has matured

#define FLAGNAMES "ADC_READ","TICK","GOTTEMP","UARTRX","TEMPCALLBACK"

#define ALLFLAGS (FLAG_ADC_READ|FLAG_TICK|FLAG_SEREV|FLAG_GOTTEMP|FLAG_UARTRX|FLAG_TEMPCALLBACK)

// ============================================================================================
void flag_post(uint32_t flag_to_set);               // Post a new flag
uint32_t flag_test(uint32_t
                   flag_to_check);         // Test for a specific flag, and reset it in the process
uint32_t flag_clear(void);                          // See if there are any flags set at all
uint32_t flag_get(
    void);                            // Get the current flagset, and reset them all in the process
const char *flag_getname(uint32_t
                         flat_to_check);   // Return the name of the requested flag (for pretty printing)

void flagInit(void);                                // The initialisation function
// ============================================================================================
#endif /* FLAGS_H_ */
