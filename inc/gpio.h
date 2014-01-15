/*
 * General purpose I/O - used for LED and heater control.
 */


#ifndef GPIO_H_
#define GPIO_H_

#ifndef GPIO_GREEN_LED
#error "Need GPIO_GREEN_LED defining"
#endif

#ifndef GPIO_HEATER
#error "Need GPIO_HEATER defining"
#endif

// ============================================================================================
void gpioSet_led(uint32_t ledToLight, uint32_t state);
void gpioHeat(uint32_t isOn);

void gpioInit(void);
// ============================================================================================

#endif /* GPIO_H_ */
