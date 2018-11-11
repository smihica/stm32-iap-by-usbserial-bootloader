#include "init.h"
#include "partition.h"
#include <string.h>
#include "main.h"
#include "stm32f3xx_it.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "crc.h"
#include "nvs.h"
#include "packet.h"
#include "dispatcher.h"

typedef void (*pFunction)();

volatile uint8_t gSystemInitialized = 0;

uint8_t  USB_rx_buffer[0x200];
volatile uint32_t USB_rx_buffer_lead_ptr = 0;

#define ENABLE_BOOTLOADER_PROTECTION 0
#define PAGES_TO_PROTECT (OB_WRP_PAGES0TO1 | OB_WRP_PAGES2TO3 | OB_WRP_PAGES4TO5)

void _start(void);
void CHECK_AND_SET_FLASH_PROTECTION(void);
int main(void);

/* Minimal vector table */
uint32_t *vector_table[] __attribute__((section(".isr_vector"))) = {
    (uint32_t *) SRAM_END, // initial stack pointer
    (uint32_t *) _start,   // _start is the Reset_Handler
    (uint32_t *) NMI_Handler,
    (uint32_t *) HardFault_Handler,
    (uint32_t *) MemManage_Handler,
    (uint32_t *) BusFault_Handler,
    (uint32_t *) UsageFault_Handler,
    0,
    0,
    0,
    0,
    (uint32_t *) SVC_Handler,
    (uint32_t *) DebugMon_Handler,
    0,
    (uint32_t *) PendSV_Handler,
    (uint32_t *) SysTick_Handler,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	(uint32_t *) USB_LP_CAN_RX0_IRQHandler
};

uint32_t const * const __nvs_flash[NVS_SIZE / 4] __attribute__((section(".nvs_flash"))) = { 0 };

// Begin address for the initialization values of the .data section.
// defined in linker script
extern uint32_t _sidata;
// Begin address for the .data section; defined in linker script
extern uint32_t _sdata;
// End address for the .data section; defined in linker script
extern uint32_t _edata;
// Begin address for the .bss section; defined in linker script
extern uint32_t _sbss;
// End address for the .bss section; defined in linker script
extern uint32_t _ebss;

inline void
__attribute__((always_inline))
__initialize_data(uint32_t* from, uint32_t* region_begin, uint32_t* region_end) {
  // Iterate and copy word by word.
  // It is assumed that the pointers are word aligned.
  uint32_t *p = region_begin;
  while (p < region_end)
    *p++ = *from++;
}

inline void
__attribute__((always_inline))
__initialize_bss(uint32_t* region_begin, uint32_t* region_end) {
  // Iterate and copy word by word.
  // It is assumed that the pointers are word aligned.
  uint32_t *p = region_begin;
  while (p < region_end)
    *p++ = 0;
}

void __attribute__ ((noreturn,weak))
_start(void) {
    __initialize_data(&_sidata, &_sdata, &_edata);
    __initialize_bss(&_sbss, &_ebss);
    main();
    for (;;);
}

void shift_buffer(uint8_t* buffer, uint8_t buffer_len, uint8_t shift_count)
{
    for (uint8_t i = 0; i < buffer_len - shift_count; i++) {
        buffer[i] = buffer[shift_count+i];
    }
}

static flasher_state_t flasher_state = NOT_IN;

static void __attribute__ ((noreturn)) flasher_main()
{
    USB_Init();
    packet_parser_init();

#if ENABLE_BOOTLOADER_PROTECTION
    /* Ensures that the first sector of flash is write-protected preventing that the
       bootloader is overwritten */
    CHECK_AND_SET_FLASH_PROTECTION();
#endif

    flasher_state = WAITING;

    packet_t p = { 0 };

    for(;;) {
        if (USB_rx_buffer_lead_ptr > 0) {
            packet_parser_bulk_push(USB_rx_buffer, USB_rx_buffer_lead_ptr);
            while (packet_parse(&p)) {
                dispatch_packet(&p);
            }
            USB_rx_buffer_lead_ptr = 0;
            USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        }
    }
}

static __attribute__ ((noreturn))
void jump_to_firmware(firm_partition_t firm)
{
    __IO uint32_t* start_address = (
        firm == FIRM0 ?
        (__IO uint32_t*)(FIRM0_START_ADDRESS) :
        (__IO uint32_t*)(FIRM1_START_ADDRESS)
    );
    uint32_t  jump_address = *(start_address+1);
    pFunction jump = (pFunction)(jump_address);

    CRC_DeInit();
    GPIO_DeInit();
    HAL_RCC_DeInit();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    __DMB(); // ARM says to use a DMB instruction before relocating VTOR
    SCB->VTOR = (uint32_t)(start_address); // relocate vector table.
    __DSB(); // ARM says to use a DSB instruction just after relocating VTOR

    __set_MSP(*start_address);
    jump();

    for (;;);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    HAL_Delay(50);
    GPIO_Init();
    CRC_Init();
    nvs_init();
    gSystemInitialized = 1;

    uint8_t run_mode = APP_MODE;
    uint8_t run_firm = FIRM0;
    uint16_t size = 0;

    if (nvs_get("RUN_MODE", &run_mode, &size, 1) == KEY_NOT_FOUND ||
        nvs_get("RUN_FIRM", &run_firm, &size, 1) == KEY_NOT_FOUND)
    {
        run_mode = FLASHER_MODE;
        run_firm = FIRM1;
        nvs_clear();
        nvs_put("RUN_MODE", &run_mode, 1, 1);
        nvs_put("RUN_FIRM", &run_firm, 1, 1);
        if (nvs_commit() != NVS_OK) {
            PANIC("flash commit failed");
        }
    }

    if (run_mode == FLASHER_MODE) {
        flasher_main();
    } else {
        uint8_t tried_other_side = 0;
        uint32_t crc = 0, calc_crc = 0;
    verify_firm_and_run:
        switch (run_firm) {
        case FIRM0: {
            crc      = *(((uint32_t*)FIRM0_CRC_ADDRESS));
            calc_crc = calc_crc32((uint32_t*)(FIRM0_START_ADDRESS), FIRM0_SIZE);
            if (crc == calc_crc) {
                // verified
                jump_to_firmware(FIRM0);
            } else if (tried_other_side) {
                run_mode = FLASHER_MODE;
                run_firm = FIRM1;
                nvs_put("RUN_MODE", &run_mode, 1, 1);
                nvs_put("RUN_FIRM", &run_firm, 1, 1);
                nvs_commit();
                NVIC_SystemReset();
            } else {
                // data broken?
                run_firm = FIRM1;
                tried_other_side = 1;
                goto verify_firm_and_run;
            }
            break;
        }
        case FIRM1:
        default: {
            crc      = *(((uint32_t*)FIRM1_CRC_ADDRESS));
            calc_crc = calc_crc32((uint32_t*)(FIRM1_START_ADDRESS), FIRM1_SIZE);
            if (crc == calc_crc) {
                // verified
                jump_to_firmware(FIRM1);
            } else if (tried_other_side) {
                run_mode = FLASHER_MODE;
                run_firm = FIRM1;
                nvs_put("RUN_MODE", &run_mode, 1, 1);
                nvs_put("RUN_FIRM", &run_firm, 1, 1);
                nvs_commit();
                NVIC_SystemReset();
            } else {
                // data broken?
                run_firm = FIRM0;
                tried_other_side = 1;
                goto verify_firm_and_run;
            }
            break;
        }
        }
    }
}

void CHECK_AND_SET_FLASH_PROTECTION(void) {
    FLASH_OBProgramInitTypeDef obConfig;

    /* Retrieves current OB */
    HAL_FLASHEx_OBGetConfig(&obConfig);

    /* If the first sector is not protected */
    if ((obConfig.WRPPage & PAGES_TO_PROTECT) == PAGES_TO_PROTECT) {
        HAL_FLASH_Unlock(); //Unlocks flash
        HAL_FLASH_OB_Unlock(); //Unlocks OB
        obConfig.OptionType = OPTIONBYTE_WRP;
        obConfig.WRPState = OB_WRPSTATE_ENABLE; //Enables changing of WRP settings
        obConfig.WRPPage = PAGES_TO_PROTECT; //Enables WP on first pages
        HAL_FLASHEx_OBProgram(&obConfig); //Programs the OB
        HAL_FLASH_OB_Launch(); //Ensures that the new configuration is saved in flash
        HAL_FLASH_OB_Lock(); //Locks OB
        HAL_FLASH_Lock(); //Locks flash
    }
}

uint32_t toggle_time_r = 0;
uint32_t toggle_time_g = 0;
uint32_t toggle_time_b = 0;

void main_tick_1ms() {}
void main_tick_5ms() {
    USB_SendTick();
}
void main_tick_10ms()  {}
void main_tick_50ms()  {
    if (toggle_time_r > 0) {
        HAL_GPIO_TogglePin(GPIOB, LED_R_Pin);
        toggle_time_r--;
    }
    if (toggle_time_g > 0) {
        HAL_GPIO_TogglePin(GPIOB, LED_G_Pin);
        toggle_time_g--;
    }
    if (toggle_time_b > 0) {
        HAL_GPIO_TogglePin(GPIOB, LED_B_Pin);
        toggle_time_b--;
    }
}
void main_tick_100ms() {}
void main_tick_500ms() {}
void main_tick_1s() {}

void SystemClock_Config(void)
{

    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    // RCC_OscInitStruct.HSIState = RCC_HSI_ON;
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
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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

    HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_LSE, RCC_MCODIV_1);

    /**Configure the Systick interrupt time
     */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    /**Configure the Systick
     */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    __HAL_RCC_PWR_CLK_ENABLE();

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0xF, 0xF); // Lowest priority
}
