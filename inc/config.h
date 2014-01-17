/*
 * Leater Configuration
 * ====================
 *
 * All primary configuration is in this file.  Parameters can be modified here but be aware that non-volatile
 * configuration elements will only be updated by either changing them via the command line and committing them,
 * or by updating the LEATER_VERSION_NUMBER which will cause them to be overwritten with a new version.
 *
 */

#ifndef _CONFIG_
#define _CONFIG_

#include "LPC8xx.h"
#include "dutils.h"

// ----- Version numbering
// -----------------------
#define LEATER_VERSION_NUMBER   0x14011501
#define LEATER_VERSION          "1.00, 15th Jan 2014"

// ----- Type of temperature sensor
// --------------------------------
//#define SENSOR_THERMISTOR   1
#define SENSOR_THERMOCOUPLE 2

// ----- UART Baudrate
// -------------------
#define UART_BAUDRATE           115200

// ----- Non-volatile configuration storage structure
// --------------------------------------------------
typedef uint32_t BOOL;
#define mS              1000                          // Number of mS in a Sec
#define DEGREE          10                            // Number of fractions in a degree
#define TEMP_INVALID    (1000*DEGREE)                 // Flag for invalid temp - 1000 degC should be enough!

// Profile Configuration data
// --------------------------
#define SETPOINT_IDLE    0xFFFFFFFF                   // If we don't want any heating effect
#define SETPOINT_STATIC  0xFFFFFFFE                   // If we just want to use a static point

#define MAX_PROFILE_STEPS 5                           // Number of steps in a single profile
#define MAX_PROFILES      3                           // ...and the number of profiles in total
#include "profile.h"

// Beware, profile names need to be of correct length as specified in profile.h
#define LEATER_PROFILES                                                     \
{                                                                           \
        .name="Oven Reflow - Leaded\0\0",                                   \
        .step[0]={.temp=PROFILE_REACHTEMP|(150*DEGREE),.time=70*mS},        \
        .step[1]={.temp=PROFILE_REACHTIME|(175*DEGREE),.time=130*mS},       \
        .step[2]={.temp=PROFILE_REACHTEMP|(220*DEGREE),.time=40*mS},        \
        .step[3]={.temp=PROFILE_REACHTIME|(220*DEGREE),.time=5*mS},         \
        .step[4]={.temp=PROFILE_REACHTIME|(50*DEGREE),.time=100*mS}         \
    },                                                                      \
    {   .name="Oven Reflow - Unleaded",                                     \
        .step[0]={.temp=PROFILE_REACHTEMP|(150*DEGREE),.time=70*mS},        \
        .step[1]={.temp=PROFILE_REACHTIME|(175*DEGREE),.time=130*mS},       \
        .step[2]={.temp=PROFILE_REACHTEMP|(245*DEGREE),.time=40*mS},        \
        .step[3]={.temp=PROFILE_REACHTIME|(245*DEGREE),.time=5*mS},         \
        .step[4]={.temp=PROFILE_REACHTIME|(50*DEGREE),.time=100*mS}         \
    },                                                                      \
    {   .name="Component Bake\0\0\0\0\0\0\0",                               \
        .step[0]={.temp=PROFILE_REACHTEMP|(125*DEGREE),.time=2000*mS},      \
        .step[1]={.temp=PROFILE_REACHTIME|(125*DEGREE),.time=24*60*60*mS},  \
        .step[2]={.temp=PROFILE_END,.time=0*mS},                            \
        .step[3]={.temp=PROFILE_END,.time=0*mS},                            \
        .step[4]={.temp=PROFILE_END,.time=0*mS}                             \
    }

// System Configuration data
// -------------------------
// The default configuration that will be used if all else fails
#define LEATER_CONFIG               \
    .version=LEATER_VERSION_NUMBER, \
    .logOutputCSV=TRUE,             \
    .setPoint=13*DEGREE,            \
    .recordInterval=20,             \
    .k=3,                           \
    .Cp=10,                         \
    .Ci=400,                        \
    .Cd=30000,                      \
    .profile={LEATER_PROFILES},     \
    .defaultProfile=SETPOINT_STATIC

/*
 * Note that k is the scaling constant for the low pass filter. Checkout
 * http://www.edn.com/design/systems-design/4320010/A-simple-software-lowpass-filter-suits-embedded-system-applications
 *
 * k        Normalised B/W  Rise Time (Samples)
 * 1:       0.1197              3
 * 2:       0.0466              8
 * 3:       0.0217              16
 * 4:       0.0104              34
 * 5:       0.0051              69
 * 6:       0.0026              140
 * 7:       0.0012              280
 * 8:       0.0007              561
 */

// Log Configuration data
// ----------------------
// How many elment bits to use (keep low to improve log density)
#define LOG_ELEMENT_BITS       4

// Add the things to be logged, and their lengths in bits, here
#define LOG_TEMPERATURE            0
#define LOG_ON_PERCENTAGE          1
#define LOG_RECORD_INTERVAL        2
#define LOG_SETPOINT_SET           3
#define LOG_BOD_TRIGGERED          4

#define LOG_BITS         {12, DEGREE/10, "°C", "Temperature"}, \
    {7, 1     , "%%", "On Percentage"},    \
    {7, 1     , "s", "Record Interval"},   \
    {8, DEGREE, "°C", "Setpoint"},            \
    {1, 1     , "BOOL", "BOD Triggered"}

// ------ Pin Settings
// -------------------

/*
 * **X** means pin X is pre-allocated.
 *
 *               1--PIO0_17     PIO0_14--20 SPI_SEL
 * **GREEN_LED** 2--PIO0_13     PIO0_0---19 **UART_RX** (ACMP_IN)
 *    **BUTTON** 3--PIO0_12     PIO0_6---18 SPI_CLK
 *     **RESET** 4--PIO0_5      PIO0_7---17             (ADC_FEEDBACK)
 *   **UART_TX** 5--PIO0_4      Vss------16 **Gnd**
 *       **TCK** 6--PIO0_3      Vdd------15 **3v3 Out**
 *       **TMS** 7--PIO0_2      PIO0_8---14 **XTALIN**
 *               8--PIO0_11     PIO0_9---13 **XTALOUT**
 *               9--PIO0_10     PIO0_1---12 HEATER
 *              10--PIO0_16     PIO0_15--11 SPI_MISO
 */

// ADC
#define ADC_ANALOG_COMP_PIN 0
#define ANALOG_COMP_PIO     PIO0_0  // Analogue comparator input
#define ADC_FEEDBACK_PIN    7
#define FEEDBACK_PIO        PIO0_7  // Feedback pin


// GPIO
#define GPIO_GREEN_LED      13 // Was 12 in original leater
#define GPIO_HEATER         1  // Was 13 in original leater

// SPI
#define SPIPORT             LPC_SPI0
#define SPI_CLK             6
#define SPI_MISO            15
#define SPI_SEL             14

// UART
#define UART_TX_PIN         4
#define UART_RX_PIN         0

// ============================================================================================
// ============================================================================================
// ============================================================================================
//Internal Setup
// ============================================================================================
// ============================================================================================
// ============================================================================================
#define DEBUG 1
#define FALSE 0
#define TRUE  (!FALSE)
#define OFF   FALSE
#define ON    TRUE

typedef enum {  CONFIG_ITEM_version, CONFIG_ITEM_logOutputCSV, CONFIG_ITEM_setPoint,
                CONFIG_ITEM_recordInterval, CONFIG_ITEM_PID, CONFIG_ITEM_defaultProfile, CONFIG_ITEM_K,
                CONFIG_ITEM_defaults, CONFIG_ITEM_reload, CONFIG_ITEM_help, CONFIG_ITEM_end
             } config_item_enum;

#define LEATER_CONFIG_STRINGLIST "Version","logOutputCSV","setPoint","recordInterval", "PID", "defaultProfile", "K", "defaults", "reload", "help", 0

// The internal structure of the config - if this is changed, then you _must_ update the version number
typedef struct
{
    uint32_t    version;            // Version of the datafile
    BOOL        logOutputCSV;       // Are we outputting CSV or formatted?
    uint32_t    setPoint;           // What is the current setpoint?
    uint32_t    recordInterval;             // Interval between recordings into the log
    uint32_t    k;                          // Filter k value (bits)
    int32_t     Cp;                         // PID P value
    int32_t     Ci;                         // PID I value
    int32_t     Cd;                         // PID D value
    ProfileType profile[MAX_PROFILES];      // Profiles
    uint32_t    defaultProfile;             // ....and the default profile to use
} ConfigStoreType;

// The configuration of the overall system
extern ConfigStoreType sysConfig;
#endif
