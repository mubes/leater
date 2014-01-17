/****************************************************************************
 *    Sigma Delta ADC using LPC800 Analog Comparator
 *
 ****************************************************************************
 * Code to implement a sigma/delta ADC on LPC800. (By now loosely) based on
 * code from NXP that had the original copyright statement on it;
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors'
 * relevant copyright in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers. This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 *
 * This code is only included if leater is built for Thermistor sensors.
 *
 ****************************************************************************/

#include <LPC8xx.h>
#include "config.h"
#include "adc.h"
#include "flags.h"

#ifdef SENSOR_THERMISTOR
/* --------------------------------------------------------------------------
   SD-ADC Module internal configuration
   --------------------------------------------------------------------------*/

/* The result of the ADC reading */
static uint32_t last_adc_val;
static int32_t _storedTemperature=TEMP_INVALID;
#define MAX_RESISTANCE   30000                        // Max resistance we should see if there's a sensor

// Best fit values
#define ADC_SCALE_FACTOR 10000
#define ADC_M   4
#define ADM_C   32884 //32053

/* --------------------------------------------------------------------------
   Analog comparator driver
   --------------------------------------------------------------------------*/

#ifndef ADC_ANALOG_COMP_PIN
#error "Need ADC_ANALOG_COMP_PIN defining"
#endif

#ifndef ADC_FEEDBACK_PIN
#error "Need ADC_FEEDBACK_PIN defining"
#endif

// If this is defined it will be used
//#define SD_ADC_DEBUG_PIN      15

#define ACMP_IN_VLADDER_OUTPUT    0x00
#define ACMP_IN_ACMP_I1           0x01
#define ACMP_IN_ACMP_I2           0x02
#define ACMP_IN_INTERNAL_BANDGAP  0x06

// Configuration parameters for ADC
#define CONFIG_SD_ADC_PRESCALER       255  // Prescaler for count; 1-255. Tradeoff of accuracy vs speed
#define CONFIG_WINDOW_SIZE            8192 // Was originally  1024...number of bits of resolution
#define CONFIG_VLADDER_PRESCALER      15

// ============================================================================================
void SCT_IRQHandler(void)

{
    LPC_SCT->CTRL_H=4;
    LPC_SCT->CTRL_L=4;
    last_adc_val=LPC_SCT->COUNT_L;
    flag_post(FLAG_ADC_READ);
    LPC_SCT->EVFLAG = 1;
}
// ============================================================================================
static void acmp_init(void)

/** \brief Init analog comparator */

{
    /* Power up comparator */
    LPC_SYSCON ->PDRUNCFG &= ~(1 << 15);

    /* Clear comparator reset */
    LPC_SYSCON ->PRESETCTRL |=  (1 << 12);

    /* Enable register access to the comparator */
    LPC_SYSCON ->SYSAHBCLKCTRL |= (1 << 19);
}
// ============================================================================================
static void acmp_deinit(void)

/** \brief Disable analog comparator */

{
    /* Disable register access to the comparator */
    LPC_SYSCON ->SYSAHBCLKCTRL &=~(1 << 19);

    /* Reset the comparator */
    LPC_SYSCON ->PRESETCTRL &= ~(1 << 12); /*< Reset the comparator */

    /* Power down comparator */
    LPC_SYSCON ->PDRUNCFG |=  (1 << 15);
}
// ============================================================================================
static void acmp_configure(uint32_t vp_channel, uint32_t vn_channel, uint32_t is_output_synced)

/** \brief Configure the analog comparator
 *
 * \param vp_channel Positive input
 * \param vn_channel Negative input
 * \param is_output_synced 0 if output should not be sync with the core clock.
 *                           Otherwise it should be synced
 *
 */

{
    LPC_CMP ->CTRL = 0;
    LPC_CMP ->CTRL |= ((vp_channel & 7) << 8) | ((vn_channel & 7) << 11);

    if (is_output_synced)
        {
            LPC_CMP ->CTRL |= (1 << 6);
        }
    else
        {
            /* Disable register access to the comparator to save power*/
            LPC_SYSCON ->SYSAHBCLKCTRL &=~(1 << 19);
        }
}
// ============================================================================================
static void vladder_enable(uint32_t vladder_div)

/** \brief Enable Voltage ladder output
 *
 *  \param vladder_div Voltage ladder division
 */

{
    uint32_t delay;
    LPC_CMP ->LAD = ((vladder_div & 0x1F) << 1) | 1;

    /* Delay is needed to stabilize the VLadder output */
    for (delay = 0; delay < 0xFF; delay++);
}
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
void sdadcStartup(void)


/** \brief Initialize SCT to be used in sigma delta ADC
 *
 * The SCT is needed to sample, decimate and average the analog comparator
 * output.
 *
 * ACMP_O ----> CTIN_0 -----> LOW_COUNTER
 *                             |       |
 *           Event #1: Triggered       Event #2: Triggered when ACMP_O falling
 *           when ACMP_O rising                      |
 *                    |                              |
 *              SET CTOUT_0                   CLEAR CTOUT_0
 *
 * Note - CTOUT is only used if debug is set!
 *
 * HIGH_COUNTER------> Event #0: Triggered when window size limit is reached
 *                                      |
 *                    CLEAR HIGH_COUNTER and CLEAR LOW_COUNTER
 *
 * The SCT is operating without any state variable
 */

{
    /* Enable register access to the SCT */
    LPC_SYSCON ->SYSAHBCLKCTRL |= (1 << 8);

    /* Clear SCT reset */
    LPC_SYSCON ->PRESETCTRL |=  (1 << 8);

    /* Set SCT:
     * - As two 16-bit counters
     * - Use bus clock as the SCT and prescaler clock
     * - Allows reload the match registers
     * - Sync CTIN_0 with bus clock before triggering any event
     */
    LPC_SCT->CONFIG = 0x200;

    /* Set averaging window size. Reset the HIGH counter when maximum
     * window size reached, and generate interrupt to save the LOW counter value
     *
     * This maximum counting is used as max window size
     */
    LPC_SCT->REGMODE_H = 0x0000;          /*< Use HIGH counter register 0 as Match */
    LPC_SCT->MATCH[0].H     = CONFIG_WINDOW_SIZE - 1;
    LPC_SCT->MATCHREL_H[0]  = CONFIG_WINDOW_SIZE - 1;
    LPC_SCT->EVENT[0].CTRL  = 0x00001010; /*< Set event when MR0 = COUNT_H = max window size */
    LPC_SCT->EVENT[0].STATE = 0x00000003; /*< Trigger event at any state */

    /* Low timer; Set capture event to capture the analog comparator output. The capture
     * event will be triggered if the analog output is toggled   */

    LPC_SCT->EVENT[1].CTRL  =
        0x00002400; /*< Set event when CTIN_0 is rising                                       */
    LPC_SCT->EVENT[1].STATE = 0x00000003; /*< Trigger event at any state */
    LPC_SCT->START_L = 0x0002;            /*< Start counting on LOW counter when
                                              Event #1 triggered */
#if defined (SD_ADC_DEBUG_PIN)
    LPC_SCT->OUT[0].SET = 2;              /*< Set CTOUT_0 if Event #1 triggered */
#endif

    LPC_SCT->EVENT[2].CTRL  = 0x00002800; /*< Set event when CTIN_0 is falling */
    LPC_SCT->EVENT[2].STATE = 0x00000003; /*< Trigger event at any state */
    LPC_SCT->STOP_L = 0x0004;            /*< Stop counting on LOW counter when
                                              Event #2 triggered */
#if defined (SD_ADC_DEBUG_PIN)
    LPC_SCT->OUT[0].CLR = 4;              /*< Clear CTOUT_0 if Event #2
                                              triggered */
#endif

    /* Trigger interrupt when Event #0 triggered */
    LPC_SCT->EVEN =    0x00000001;
}
// ============================================================================================
void sdadcGet_sample(void)

{
    /* Start the HIGH counter:
     * - Counting up
     * - Single direction
     * - Prescaler = bus_clk / CONFIG_SD_ADC_PRESCALER
     * - Reset the HIGH counter
     */
    LPC_SCT->CTRL_H = ((CONFIG_SD_ADC_PRESCALER - 1) << 5) | 0x08;

    /* Stop the LOW counter:
     * - Counting up
     * - Single direction
     * - Prescaler = bus_clk / CONFIG_SD_ADC_PRESCALER
     * - Reset the LOW counter
     */
    LPC_SCT->CTRL_L = ((CONFIG_SD_ADC_PRESCALER - 1) << 5) | 0x0A;
}
// ============================================================================================
static void sct_deinit(void)

{
    /* Stop SCT */
    LPC_SCT->CTRL_H = 0x04;

    /* Reset SCT */
    LPC_SYSCON ->PRESETCTRL &= ~(1 << 8);

    /* Disable register access to the SCT */
    LPC_SYSCON ->SYSAHBCLKCTRL &=~(1 << 8);
}
// ============================================================================================
void sdadcInit(void)

{
    // Init the SD ADC pins

    // Setup the analogue comparison input pin (ACMP_I1)
    LPC_IOCON->ANALOG_COMP_PIO   = 0x80;
    LPC_SWM->PINENABLE0 &= ~(1 << ADC_ANALOG_COMP_PIN);

    // Setup the analogue comparison output pin (ACMP_O) which is used for feedback
    LPC_IOCON->FEEDBACK_PIO  = 0x80; // Disable pull up / pull down resistor for feedback pin
    LPC_SWM->PINASSIGN8 &=~(0xFF << 8);
    LPC_SWM->PINASSIGN8 |= (ADC_FEEDBACK_PIN << 8);  /* Enable ACMP_O */

    // Setup CTIN_0 (which will also be the feedback pin)
    LPC_SWM->PINASSIGN5 &=~(0xFF << 24);
    LPC_SWM->PINASSIGN5 |= (ADC_FEEDBACK_PIN << 24);  /* Enable CTIN_0 */

#ifdef SD_ADC_DEBUG_PIN
    // Setup CTOUT_0 if debug is enabled
    LPC_SWM->PINASSIGN6 &=~(0xFF << 24);
    LPC_SWM->PINASSIGN6 |= (SD_ADC_DEBUG_PIN << 24);
#endif

    sdadcResume();
    NVIC_EnableIRQ(SCT_IRQn);
}
// ============================================================================================
void sdadcResume(void)

/** \brief Resume sigma delta ADC operation
 *
 */
{
    acmp_init();
    vladder_enable(CONFIG_VLADDER_PRESCALER);

    /* Analog comparator setup:
     *     V+ => VDD / 2
     *     V- => ACMP_I1 (PIO0_0)
     *     Output => comparator_out_pin
     */
    acmp_configure(ACMP_IN_VLADDER_OUTPUT, ACMP_IN_ACMP_I1, 1);
}
// ============================================================================================
void sdadcSuspend(void)

/** \brief Suspend the sigma delta ADC operation
 *
 */

{
    LPC_CMP ->LAD = 0; // Disable Vladder
    sct_deinit();
    acmp_deinit();
}
// ============================================================================================
uint32_t sdadc_lpf(uint32_t adc_val)

{
    static uint32_t yn_1=0;
    return (yn_1 = ((980 * adc_val) +(44 * yn_1)) >> 10);
}
// ============================================================================================
inline uint32_t sdadcGet_val(void)

{
    return last_adc_val;
}
// ============================================================================================
inline uint32_t sdadcGet_Voltage(void)

{
    return ADM_C-ADC_M*last_adc_val;
}
// ============================================================================================
void sdadcHandleADCRead(void)

{
    int32_t resistance;
    uint32_t voltage;

    // We have got a valid ADC reading
    voltage = sdadcGet_Voltage();

    // These curves are specific to the components that are on the board
    resistance = (((ADM_C - voltage) * 2100) / voltage);
    _storedTemperature = 469350 - 54 * resistance;

    if (resistance > MAX_RESISTANCE)
        _storedTemperature=TEMP_INVALID;

    // Whatever happens, we got a temperature, so let everyone know
    flag_post(FLAG_GOTTEMP);
}
// ============================================================================================
uint32_t sdadcGetResult(void)

{
    return _storedTemperature;
}
// ============================================================================================
#endif
