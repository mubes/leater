/*
 * SPI reader module. Targeting MAX6675. Returns readings from the sensor.
 */

#include "config.h"
#include "flags.h"
#include "uart.h"
#include "printf.h"

#ifdef SENSOR_THERMOCOUPLE

#ifndef SPIPORT
#error "SPIPORT must be defined"
#endif
#ifndef SPI_CLK
#error "SPI_CLK must be defined"
#endif
#ifndef SPI_MISO
#error "SPI_MISO must be defined"
#endif
#ifndef SPI_SEL
#error "SPI_SEL must be defined"
#endif

#define SPI_FRAMELEN  15

static volatile uint32_t rxedData=0;
static volatile BOOL hiLo=FALSE;

// ============================================================================================
void SPI0_IRQHandler(void)

// Interrupt handler for SPI

{
    SPIPORT->RXDAT; // Reading data automatically clears the data pending IRQ
    hiLo=!hiLo;
    if (hiLo)
        {
            // Put the first sixteen bits into the high byte
            rxedData=(SPIPORT->RXDAT&0xFFFF)<<16;

            // ...and get the second sixteen bits, with control bits to indicate we're finished
            SPIPORT->TXDATCTL=(SPI_FRAMELEN<<24)|(1<<20)|(1<<21);
        }
    else
        {
            rxedData|=(SPIPORT->RXDAT&0xFFFF);
            flag_post(FLAG_GOTTEMP);
        }
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
void spiInit(void)

// Initialise the SPI subsystem

{
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<11);   // Enable SPI0
    LPC_SYSCON->PRESETCTRL &= ~(1<<0);
    LPC_SYSCON->PRESETCTRL |= (1<<0);               // Reset SPI0

    LPC_SWM->PINASSIGN3=(LPC_SWM->PINASSIGN3&0x00FFFFFF)|(SPI_CLK<<24);
    LPC_SWM->PINASSIGN4=(LPC_SWM->PINASSIGN4&0xFF0000FF)|(SPI_SEL<<16)|(SPI_MISO<<8);

    SPIPORT->DIV=1000;            // Don't need anything too fast - there's not much else going on!
    SPIPORT->DLY=0;
    SPIPORT->INTENSET=(1<<0);     // Interrupt on RX data available
    NVIC_DisableIRQ(SPI0_IRQn);
    NVIC_ClearPendingIRQ(SPI0_IRQn);
    NVIC_EnableIRQ(SPI0_IRQn);
    SPIPORT->CFG=(1<<0)|(1<<2);   // Enabled, master mode
}
// ============================================================================================
void spiRequest_sample(void)

// Request a new sample from the sensor

{
    // This will trigger a reception - we don't care about the sent data
    SPIPORT->TXDATCTL=(SPI_FRAMELEN<<24);
    // no control bits on this send -> |(1<<20)|(1<<21);
}
// ============================================================================================
uint32_t spiGet_Result(void)

// Return most recently read result from the sensor

{
#ifdef DAVES_SENSOR
    return rxedData&4?TEMP_INVALID:(((rxedData&0xFFFF)>>3)*(DEGREE/4));
#else
    return rxedData&4?TEMP_INVALID:(((rxedData&0xFFFF)>>3)*(DEGREE/4));
#endif
}
// ============================================================================================
#endif

