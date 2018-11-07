#include "init.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

USBD_HandleTypeDef hUsbDeviceFS;

void USB_Init(void)
{
    USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    USBD_Start(&hUsbDeviceFS);
}

#define USB_BUSY_BREAK_THRESHOLD 0x07FFFF

void USB_Send(uint8_t* buffer, uint32_t size)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    if (hcdc == NULL || size == 0) return;
    volatile uint32_t busy_times = 0;
    while (1) {
        hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc->TxState != 0) { // USBD_BUSY
            if (++busy_times > USB_BUSY_BREAK_THRESHOLD) {
                _PANIC();
            }
            else continue;
        } else {
            break;
        }
    }
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, buffer, size);
    USBD_CDC_TransmitPacket(&hUsbDeviceFS);
    while (1) {
        hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc->TxState == 0) break;
    }
}


