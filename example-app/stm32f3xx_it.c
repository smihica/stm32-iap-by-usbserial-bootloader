#include "stm32f3xx_hal.h"
#include "stm32f3xx.h"
#include "stm32f3xx_it.h"
#include "main.h"

/**
* @brief This function handles Non maskable interrupt.
*/
void NMI_Handler(void)
{
}

/**
* @brief This function handles Hard fault interrupt.
*/
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
* @brief This function handles Memory management fault.
*/
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
* @brief This function handles Pre-fetch fault, memory access fault.
*/
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
* @brief This function handles Undefined instruction or illegal state.
*/
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
* @brief This function handles System service call via SWI instruction.
*/
void SVC_Handler(void)
{
}

/**
* @brief This function handles Debug monitor.
*/
void DebugMon_Handler(void)
{
}

/**
* @brief This function handles Pendable request for system service.
*/
void PendSV_Handler(void)
{
}

volatile uint32_t msCount = 0;
extern uint8_t gSystemInitialized;

/**
* @brief This function handles System tick timer.
*/
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();

  if (gSystemInitialized == 0) return;

  main_tick_1ms();
  if (msCount % 5 == 0) {
      main_tick_5ms();
      if (msCount % 10 == 0) {
          main_tick_10ms();
          if (msCount % 50 == 0) {
              main_tick_50ms();
              if (msCount % 100 == 0) {
                  main_tick_100ms();
                  if (msCount % 500 == 0) {
                      main_tick_500ms();
                      if (msCount % 1000 == 0) {
                          main_tick_1s();
                          msCount = 0;
                      }
                  }
              }
          }
      }
  }

  msCount++;

}
