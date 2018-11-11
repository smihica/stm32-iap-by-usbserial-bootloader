#include "init.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

#define USB_BUSY_BREAK_THRESHOLD 0x07FFFF

USBD_HandleTypeDef hUsbDeviceFS;
static uint8_t USB_tx_buffer[0x200] = { 0 };
static volatile uint8_t USB_tx_buffer_lead_ptr = 0;

void USB_Init(void)
{
    USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    USBD_Start(&hUsbDeviceFS);
}

uint32_t USB_SendQueuePacket(packet_t const* p)
{
    __disable_irq();
    uint32_t w = packet_serialize(p, USB_tx_buffer+USB_tx_buffer_lead_ptr, 0x200-USB_tx_buffer_lead_ptr);
    USB_tx_buffer_lead_ptr += w;
    __enable_irq();
    return w;
}

uint32_t USB_SendQueue(uint8_t const* buffer, uint32_t size)
{
    __disable_irq();
    uint32_t i;
    for (i = 0; i < size; i++) {
        USB_tx_buffer[USB_tx_buffer_lead_ptr++] = buffer[i];
        if (USB_tx_buffer_lead_ptr >= 0xFF) {
            __enable_irq();
            return i+1;
        }
    }
    __enable_irq();
    return i;
}

void USB_SendTick()
{
    if (USB_tx_buffer_lead_ptr > 0) {
        USB_Send(USB_tx_buffer, USB_tx_buffer_lead_ptr);
        USB_tx_buffer_lead_ptr = 0;
    }
}

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
