#include "init.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "gpio.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

extern uint8_t USB_rx_buffer[0x7F];
extern volatile uint8_t USB_rx_buffer_lead_ptr;

// this is not used.
USBD_CDC_LineCodingTypeDef LineCoding = {
    115200 * 8, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* nb. of bits 8*/
};

static int8_t CDC_Init_FS     (void);
static int8_t CDC_DeInit_FS   (void);
static int8_t CDC_Control_FS  (uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS  (uint8_t* pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS
};

static int8_t CDC_Init_FS(void)
{
    USB_rx_buffer_lead_ptr = 0;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, USB_rx_buffer);
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    USB_rx_buffer_lead_ptr = 0;
    return (USBD_OK);
}

static int8_t CDC_Control_FS  (uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    switch (cmd) {
    case CDC_SEND_ENCAPSULATED_COMMAND:
        break;
    case CDC_GET_ENCAPSULATED_RESPONSE:
        break;
    case CDC_SET_COMM_FEATURE:
        break;
    case CDC_GET_COMM_FEATURE:
        break;
    case CDC_CLEAR_COMM_FEATURE:
        break;
        /*******************************************************************************/
        /* Line Coding Structure                                                       */
        /*-----------------------------------------------------------------------------*/
        /* Offset | Field       | Size | Value  | Description                          */
        /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
        /* 4      | bCharFormat |   1  | Number | Stop bits                            */
        /*                                        0 - 1 Stop bit                       */
        /*                                        1 - 1.5 Stop bits                    */
        /*                                        2 - 2 Stop bits                      */
        /* 5      | bParityType |  1   | Number | Parity                               */
        /*                                        0 - None                             */
        /*                                        1 - Odd                              */
        /*                                        2 - Even                             */
        /*                                        3 - Mark                             */
        /*                                        4 - Space                            */
        /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
        /*******************************************************************************/
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format     = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype   = pbuf[6];
        break;
    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)(LineCoding.bitrate);
        pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
        pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
        pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
        pbuf[4] = LineCoding.format;
        pbuf[5] = LineCoding.paritytype;
        pbuf[6] = LineCoding.datatype;
        break;
    case CDC_SET_CONTROL_LINE_STATE:
        break;
    case CDC_SEND_BREAK:
        break;
    default:
        break;
    }
    return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    (void)Buf;
    __disable_irq();
	if (USB_rx_buffer_lead_ptr == 0) {
		USB_rx_buffer_lead_ptr = (*Len);
		if (USB_rx_buffer_lead_ptr == 0) {
			USBD_CDC_ReceivePacket(&hUsbDeviceFS);
		}
	} else {
		PANIC("Receive traffic accident");
	}
    __enable_irq();
    return (USBD_OK);
}
