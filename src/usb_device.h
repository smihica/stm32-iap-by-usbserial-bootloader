#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "usbd_def.h"
#include "packet.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

void USB_Init(void);
uint32_t USB_SendQueuePacket(packet_t const* packet);
uint32_t USB_SendQueue(uint8_t const* buffer, uint32_t size);
void USB_SendTick();
void USB_Send(uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // __USB_DEVICE_H
