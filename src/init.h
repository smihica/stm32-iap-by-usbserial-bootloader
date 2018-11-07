#ifndef _INIT_H
#define _INIT_H

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_tim.h"

extern uint8_t gSystemInitialized;
extern char log_buffer[];

#include "usb_device.h"

#define _LOG_OUTPUT_TO()                                                \
    {                                                                   \
        if (gSystemInitialized) {                                       \
            USB_Send((uint8_t*)log_buffer, len);                        \
        }                                                               \
    }

#define LOG(...)                                                        \
    {                                                                   \
        int len = snprintf(log_buffer, 255, "%s (%d) - ", __FILE__, __LINE__); \
        len += snprintf(log_buffer+len, 255-len, __VA_ARGS__);          \
        len += snprintf(log_buffer+len, 255-len, "\n");                 \
        _LOG_OUTPUT_TO();                                               \
    }

#define PANIC(...)                                                      \
    {                                                                   \
        LOG(__VA_ARGS__);                                               \
        _PANIC();                                                       \
    }

    void _PANIC(void);

#endif
