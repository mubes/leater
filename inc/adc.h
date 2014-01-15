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

#ifndef _ADC_H_
#define _ADC_H_

#include "config.h"

// ============================================================================================
#define THERMISTOR_MINIMUM_INTERVAL 1000 // Minimum interval between readings
#ifdef SENSOR_THERMISTOR
void sdadcGet_sample(void);
void sdadcSuspend(void);
void sdadcResume(void);
uint32_t sdadc_lpf(uint32_t adc_val);
uint32_t sdadcGet_val(void);
uint32_t sdadcGet_Voltage(void);
void sdadcHandleADCRead(void);
uint32_t sdadcGetResult(void);


void sdadcInit(void);
void sdadcStartup(void);

// ============================================================================================

#endif

#endif
