#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "usbd_def.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

void USB_Init(void);
void USB_Send(uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // __USB_DEVICE_H
