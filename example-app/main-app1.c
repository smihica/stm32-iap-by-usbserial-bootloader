/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"
#include "main.h"
#include "stm32f3xx_it.h"
#include "gpio.h"
#include "nvs.h"

/* Private function prototypes -----------------------------------------------*/

uint8_t gSystemInitialized = 0;
volatile uint8_t reset_sw_on = 0;

int main(void) {
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    nvs_init();
    gSystemInitialized = 1;
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, LED_G_Pin);
        HAL_Delay(100);
        if (reset_sw_on) {
            uint8_t next_run_mode = 0x01; // FLASHER_MODE
            if (nvs_put("RUN_MODE", &next_run_mode, 1, 1) == NVS_OK &&
                nvs_commit()                              == NVS_OK)
            {
                HAL_Delay(10);
                NVIC_SystemReset();
                HAL_Delay(1000);
            }
        }
    }
}

void main_tick_1ms() {}
void main_tick_5ms() {}
void main_tick_10ms() {}
void main_tick_50ms() {
    uint32_t on = HAL_GPIO_ReadPin(GPIOC, SW_Pin) == 1 ? 0 : 1;
    if (on && !reset_sw_on) reset_sw_on = 1;
}
void main_tick_100ms() {
    HAL_GPIO_TogglePin(GPIOB, LED_R_Pin);
}
void main_tick_500ms() {}
void main_tick_1s()    {}

void SystemClock_Config(void)
{

    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        PANIC("osc config.");
    }

    /**Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                 |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        PANIC("rcc clock config.");
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_TIM34;
    PeriphClkInit.USBClockSelection    = RCC_USBCLKSOURCE_PLL_DIV1_5;
    PeriphClkInit.Tim34ClockSelection  = RCC_TIM34CLK_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        PANIC("periph clk config");
    }

    /**Configure the Systick interrupt time
     */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    /**Configure the Systick
     */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0xF, 0xF); // Lowest priority
}

#ifdef USE_FULL_ASSERT

/**
 * @brief Reports the name of the source file and the source line number
 * where the assert_param error has occurred.
 * @param file: pointer to the source file name
 * @param line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
 * @}
 */

/**
 * @}
 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
