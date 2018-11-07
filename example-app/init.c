#include "init.h"
#include "gpio.h"

#ifndef PC

char log_buffer[256];

_Noreturn void _PANIC() {
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, LED_R_Pin);
        for (volatile uint32_t c = 0; c < 0x07FFFF; c++);
    }
}

#endif
