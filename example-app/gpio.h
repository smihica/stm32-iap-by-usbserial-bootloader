#ifndef __GPIO_H
#define __GPIO_H

#include "init.h"

#define USB_RST_Pin       GPIO_PIN_8  // PA8
#define USB_CONNECTED_Pin GPIO_PIN_12 // PB12

#define LED_R_Pin         GPIO_PIN_2  // PB2
#define LED_G_Pin         GPIO_PIN_1  // PB1
#define LED_B_Pin         GPIO_PIN_0  // PB0

#define SW_Pin            GPIO_PIN_13 // PC13

void GPIO_Init();
void GPIO_DeInit();
uint8_t GPIO_usb_is_connected();

#endif
