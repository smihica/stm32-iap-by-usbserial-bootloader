#include "init.h"
#include "gpio.h"

char log_buffer[0x100];

_Noreturn void _PANIC() {
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, LED_R_Pin);
        for (volatile uint32_t c = 0; c < 0x07FFFF; c++);
    }
}
