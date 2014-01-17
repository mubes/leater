#include <LPC8xx.h>

#include "config.h"
#include "adc.h"
#include "uart.h"
#include "gpio.h"
#include "flags.h"
#include "timers.h"
#include "nv.h"
#include "command.h"
#include "statemachine.h"
#include "printf.h"
#include "log.h"
#include "bod.h"
#include "profile.h"
#include "sensor.h"
#include "ledflash.h"
#ifdef DEBUG
#include <cr_mtb_buffer.h>
__CR_MTB_BUFFER(1024);
#endif

// System Config is used all over the place, so we make a concession and define it globally
ConfigStoreType sysConfig;

const ConfigStoreType defaultSysConfig =
{ LEATER_CONFIG }; // This is the default config, in case we need it

// ============================================================================================
BOOL _getConfig(void)

// Read config and check version numbers, updating if necessary

{
    nvReadConfig(&sysConfig);

    if (sysConfig.version != LEATER_VERSION_NUMBER)
        {
            nvWriteConfig(&defaultSysConfig);
            nvReadConfig(&sysConfig);
            return TRUE;
        }
    return FALSE;
}
// ============================================================================================
#ifdef DEBUG

#define CHECK_LEN 64
extern uint32_t _pvHeapStart;
uint32_t * const freemem = &_pvHeapStart;

void _prepareHeapCheck(void)

{
    // Write some data which we hope won't change
    uint32_t i=0;
    while (i<CHECK_LEN)
        {
            freemem[i]=i;
            i++;
        }
}
// ============================================================================================
void _heapCheck(void)

{
    // Write some data which we hope won't change
    uint32_t i=0;
    while (i<CHECK_LEN)
        {
            ASSERT(freemem[i]==i);
            i++;
        }
}
#endif
// ============================================================================================
// ============================================================================================
// ============================================================================================
int main(void)

{

    uint32_t flagSet;
    BOOL newConfig;


    // Wake up the system
    SystemCoreClockUpdate();
#ifdef DEBUG
    _prepareHeapCheck();
#endif
    timersInit();
    uartInit();
    init_printf(0, uartPrintfPutchar);
    ledInit();
    bodInit();
    logInit();
    newConfig=_getConfig();
    flagInit();
    gpioInit();
    gpioHeat(OFF);
    commandInit();
    stateInit();
    sensorInit();
    profileInit();
    if (newConfig)
        printf("Config Defaults Version %08X loaded\n",LEATER_VERSION_NUMBER);


    // ...and enter the main loop
    while (1)
        {
            flagSet = flag_get();
            if (!flagSet) __WFI();
            else
                {
#ifdef SENSOR_THERMISTOR
                    if (flagSet & FLAG_ADC_READ)
                        {
                            flagSet &= ~FLAG_ADC_READ;
                            sdadcHandleADCRead();
                        }
#endif
                    if (flagSet & FLAG_UARTRX)
                        {
                            flagSet &=~FLAG_UARTRX;
                            uartEvent();
                        }

                    if (flagSet & FLAG_TICK)
                        {
                            flagSet &= ~FLAG_TICK;
                            timerDispatch();
                        }

                    if (flagSet & FLAG_GOTTEMP)
                        {
                            flagSet &= ~FLAG_GOTTEMP;
                            sensorReadingReady();
                        }

                    if (flagSet & FLAG_TEMPCALLBACK)
                        {
                            flagSet &= ~FLAG_TEMPCALLBACK;
                            stateReadingArrived();
                        }

                    // There shouldn't be anything left to handle
                    if (flagSet)
                        {
                            ASSERT(FALSE);;
                        }
                }
#ifdef DEBUG
            _heapCheck();
#endif
        }
    return 0;
}
// ============================================================================================
