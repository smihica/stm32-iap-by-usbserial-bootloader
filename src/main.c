#include "init.h"
#include <string.h>
#include "main.h"
#include "stm32f3xx_it.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "crc.h"
#include "TI_aes_128.h"

typedef void (*pFunction)(void);

uint8_t gSystemInitialized = 0;

uint8_t  USB_rx_buffer[0xFF];
volatile uint8_t USB_rx_buffer_lead_ptr = 0;
volatile uint8_t USB_rx_buffer_lead_ptr_last = 0;

uint8_t  USB_tx_buffer[0xFF];
volatile uint8_t USB_tx_buffer_lead_ptr = 0;

/* Global macros */
#define ACK                 0x79
#define NACK                0x1F
#define CMD_ERASE           0x43
#define CMD_GETID           0x02
#define CMD_WRITE           0x2b
#define CMD_RESET           0xCC

#define APP_START_ADDRESS   0x08008000 /* In STM32F303RE this corresponds with the start address of Page 6 */

#define SRAM_SIZE           64*1024 // STM32F303RE has 64KB of RAM
#define SRAM_END            (SRAM_BASE + SRAM_SIZE)
#define FLASH_TOTAL_PAGES   255

#define ENABLE_BOOTLOADER_PROTECTION 0
#define PAGES_TO_PROTECT (OB_WRP_PAGES0TO1 | OB_WRP_PAGES2TO3 | OB_WRP_PAGES4TO5)
/* Private variables ---------------------------------------------------------*/

/* The AES_KEY cannot be defined const, because the aes_enc_dec() function
 temporarily modifies its content */
uint8_t AES_KEY[] = { 0x4D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x69, 0x6E, 0x67,
                      0x20, 0x20, 0x53, 0x54, 0x4D, 0x33, 0x32 };

extern CRC_HandleTypeDef hcrc;

/* Private function prototypes -----------------------------------------------*/
void _start(void);
void CHECK_AND_SET_FLASH_PROTECTION(void);
void cmdErase(uint8_t *pucData);
void cmdGetID(uint8_t *pucData);
void cmdWrite(uint8_t *pucData);
void cmdReset(uint8_t *pucData);
int main(void);
void MX_CRC_Init(void);

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

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    // power on wait (soft starting voltage)
    HAL_Delay(100);

    // peripheral
    GPIO_Init();

    if (HAL_GPIO_ReadPin(GPIOC, SW_Pin) == GPIO_PIN_RESET) {

        CRC_Init();
        USB_Init();

#if ENABLE_BOOTLOADER_PROTECTION
        /* Ensures that the first sector of flash is write-protected preventing that the
           bootloader is overwritten */
        CHECK_AND_SET_FLASH_PROTECTION();
#endif

        HAL_Delay(200);

        gSystemInitialized = 1;


        while (1) {
            if (USB_rx_buffer_lead_ptr > USB_rx_buffer_lead_ptr_last) {

                while (1) {
                    switch (USB_rx_buffer[0]) {
                    case CMD_GETID:
                        if (USB_rx_buffer_lead_ptr >= 5) {
                            cmdGetID(USB_rx_buffer);
                            shift_buffer(USB_rx_buffer, 0xFF, 5);
                            USB_rx_buffer_lead_ptr -= 5;
                            if (USB_rx_buffer_lead_ptr > 0) continue;
                        }
                        break;
                    case CMD_ERASE:
                        if (USB_rx_buffer_lead_ptr >= 6) {
                            cmdErase(USB_rx_buffer);
                            shift_buffer(USB_rx_buffer, 0xFF, 6);
                            USB_rx_buffer_lead_ptr -= 6;
                            if (USB_rx_buffer_lead_ptr > 0) continue;
                        }
                        break;
                    case CMD_WRITE:
                        if (USB_rx_buffer_lead_ptr >= 9) {
                            cmdWrite(USB_rx_buffer);
                            if (USB_rx_buffer_lead_ptr > 0) continue;
                        }
                        break;
                    case CMD_RESET:
                        if (USB_rx_buffer_lead_ptr >= 5) {
                            cmdReset(USB_rx_buffer);
                            if (USB_rx_buffer_lead_ptr > 0) continue;
                        }
                    default:
                        USB_rx_buffer_lead_ptr = 0;
                        break;
                    }
                    break;
                }
                USB_rx_buffer_lead_ptr_last = USB_rx_buffer_lead_ptr;
                USBD_CDC_ReceivePacket(&hUsbDeviceFS);
            }
            HAL_Delay(10);
        }
    } else {
        /* USER_BUTTON is not pressed. We first check if the first 4 bytes starting from
           APP_START_ADDRESS contain the MSP (end of SRAM). If not, the LD2 LED blinks quickly. */
        if (*((uint32_t*) APP_START_ADDRESS) != SRAM_END) {
            while (1) {
                HAL_Delay(30);
                HAL_GPIO_TogglePin(GPIOB, LED_R_Pin);
            }
        } else {

            uint32_t  JumpAddress = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
            pFunction Jump = (pFunction)JumpAddress;

            GPIO_DeInit();
            HAL_RCC_DeInit();
            HAL_DeInit();

            SysTick->CTRL = 0;
            SysTick->LOAD = 0;
            SysTick->VAL  = 0;

            SCB->VTOR = APP_START_ADDRESS;

            __set_MSP(*(__IO uint32_t*)APP_START_ADDRESS);
            Jump();

            #if 0
            /* A valid program seems to exist in the second sector: we so prepare the MCU
               to start the main firmware */
            GPIO_DeInit(); //Puts GPIOs in default state
            SysTick->CTRL = 0x0; //Disables SysTick timer and its related interrupt
            HAL_DeInit();

            // HAL_RCC_DeInit(); //Disable all interrupts related to clock
            RCC->CIR = 0x00000000;
            __set_MSP(*((volatile uint32_t*) APP_START_ADDRESS)); //Set the MSP

            __DMB(); //ARM says to use a DMB instruction before relocating VTOR */
            SCB->VTOR = APP_START_ADDRESS; //We relocate vector table to the sector 1
            __DSB(); //ARM says to use a DSB instruction just after relocating VTOR */

            /* We are now ready to jump to the main firmware */
            uint32_t JumpAddress = *((volatile uint32_t*) (APP_START_ADDRESS + 4));
            void (*reset_handler)(void) = (void*)JumpAddress;
            reset_handler(); //We start the execution from he Reset_Handler of the main firmware

            for (;;) ; //Never coming here

            #endif
        }
    }
}

uint8_t queue_USB(uint8_t const* buffer, uint8_t size)
{
    __disable_irq();
    uint8_t i;
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

/* Performs a flash erase of a given number of sectors/pages.
 *
 * An erase command has the following structure:
 *
 * ----------------------------------------
 * | CMD_ID | # of sectors      |  CRC32  |
 * | 1 byte |     1 byte        | 4 bytes |
 * |--------|-------------------|---------|
 * |  0x43  | N or 0xFF for all |   CRC   |
 * ----------------------------------------
 */
void cmdErase(uint8_t *pucData) {
    FLASH_EraseInitTypeDef eraseInfo;
    uint32_t ulBadBlocks = 0, ulCrc = 0;
    uint32_t pulCmd[] = { pucData[0], pucData[1] };

    memcpy(&ulCrc, pucData + 2, sizeof(uint32_t));

    /* Checks if provided CRC is correct */
    if (ulCrc == HAL_CRC_Calculate(&hcrc, pulCmd, 2) &&
        (pucData[1] > 0 && (pucData[1] < FLASH_TOTAL_PAGES - 16 || pucData[1] == 0xFF))) {
        /* If data[1] contains 0xFF, it deletes all sectors; otherwise
         * the number of sectors specified. */
        eraseInfo.PageAddress = APP_START_ADDRESS;
        eraseInfo.NbPages = pucData[1] == 0xFF ? FLASH_TOTAL_PAGES - 16 : pucData[1];
        eraseInfo.TypeErase = FLASH_TYPEERASE_PAGES;

        HAL_FLASH_Unlock(); //Unlocks the flash memory
        HAL_FLASHEx_Erase(&eraseInfo, &ulBadBlocks); //Deletes given sectors */
        HAL_FLASH_Lock(); //Locks again the flash memory

        /* Sends an ACK */
        pucData[0] = ACK;
        // HAL_UART_Transmit(&huart2, (uint8_t *) pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);
    } else {
        /* The CRC is wrong: sends a NACK */
        pucData[0] = NACK;
        // HAL_UART_Transmit(&huart2, pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);
    }
}

/* Retrieve the STM32 MCU ID
 *
 * A GET_ID command has the following structure:
 *
 * --------------------
 * | CMD_ID |  CRC32  |
 * | 1 byte | 4 bytes |
 * |--------|---------|
 * |  0x02  |   CRC   |
 * --------------------
 */
void cmdGetID(uint8_t *pucData) {
    uint16_t usDevID;
    uint32_t ulCrc = 0;
    uint32_t ulCmd = pucData[0];

    memcpy(&ulCrc, pucData + 1, sizeof(uint32_t));

    /* Checks if provided CRC is correct */
    if (ulCrc == HAL_CRC_Calculate(&hcrc, &ulCmd, 1)) {
        usDevID = (uint16_t) (DBGMCU->IDCODE & 0xFFF); //Retrieves MCU ID from DEBUG interface

        /* Sends an ACK */
        pucData[0] = ACK;

        // HAL_UART_Transmit(&huart2, pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);

        /* Sends the MCU ID */
        // HAL_UART_Transmit(&huart2, (uint8_t *) &usDevID, 2, HAL_MAX_DELAY);
        queue_USB((uint8_t const*) &usDevID, 2);
    } else {
        /* The CRC is wrong: sends a NACK */
        pucData[0] = NACK;
        // HAL_UART_Transmit(&huart2, pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);
    }
}

/* Retrieve the STM32 MCU ID
 *
 * A RESET command has the following structure:
 *
 * --------------------
 * | CMD_ID |  CRC32  |
 * | 1 byte | 4 bytes |
 * |--------|---------|
 * |  0xCC  |   CRC   |
 * --------------------
 */
void cmdReset(uint8_t *pucData) {
    uint32_t ulCrc = 0;
    uint32_t ulCmd = pucData[0];

    memcpy(&ulCrc, pucData + 1, sizeof(uint32_t));

    /* Checks if provided CRC is correct */
    if (ulCrc == HAL_CRC_Calculate(&hcrc, &ulCmd, 1)) {
        /* Sends an ACK */
        pucData[0] = ACK;

        // HAL_UART_Transmit(&huart2, pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);

        HAL_Delay(1000);

        NVIC_SystemReset();

    } else {
        /* The CRC is wrong: sends a NACK */
        pucData[0] = NACK;
        // HAL_UART_Transmit(&huart2, pucData, 1, HAL_MAX_DELAY);
        queue_USB(pucData, 1);
    }
}

/* Performs a write of 16 bytes starting from the specified address.
 *
 * A write command has the following structure:
 *
 * ----------------------------------------
 * | CMD_ID | starting address  |  CRC32  |
 * | 1 byte |     4 byte        | 4 bytes |
 * |--------|-------------------|---------|
 * |  0x2b  |    0x08004000     |   CRC   |
 * ----------------------------------------
 *
 * The second message has the following structure
 *
 * ------------------------------
 * |    data bytes    |  CRC32  |
 * |      16 bytes    | 4 bytes |
 * |------------------|---------|
 * | BBBBBBBBBBBBBBBB |   CRC   |
 * ------------------------------
 */
void cmdWrite(uint8_t *pucData) {
    uint32_t ulSaddr = 0, ulCrc = 0;

    memcpy(&ulSaddr, pucData + 1, sizeof(uint32_t));
    memcpy(&ulCrc, pucData + 5, sizeof(uint32_t));

    uint32_t pulData[5];
    for(int i = 0; i < 5; i++) pulData[i] = pucData[i];

    // consume header here
    shift_buffer(USB_rx_buffer, 0xFF, 9);
    USB_rx_buffer_lead_ptr -= 9;

    /* Checks if provided CRC is correct */
    if (ulCrc == HAL_CRC_Calculate(&hcrc, pulData, 5) && ulSaddr >= APP_START_ADDRESS) {

        /* Sends an ACK */
        uint8_t ack = ACK;
        queue_USB((uint8_t const *) &ack, 1);

        // waiting for receive
        USB_rx_buffer_lead_ptr_last = 0;
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);

        /* Now retrieves given amount of bytes plus the CRC32 */
        while (1) {
            if (USB_rx_buffer_lead_ptr > 0) {
                if (USB_rx_buffer_lead_ptr == 20) {
                    break;
                } else {
                    PANIC("traffic accident");
                }
                USB_rx_buffer_lead_ptr = 0;
                USBD_CDC_ReceivePacket(&hUsbDeviceFS);
            }
            HAL_Delay(5);
        }

        memcpy(&ulCrc, pucData + 16, sizeof(uint32_t));

        /* Checks if provided CRC is correct */
        if (ulCrc == HAL_CRC_Calculate(&hcrc, (uint32_t*) pucData, 4)) {
            HAL_FLASH_Unlock(); //Unlocks the flash memory

            /* Decode the sent bytes using AES-128 ECB */
            aes_enc_dec((uint8_t*) pucData, AES_KEY, 1);
            /*
            for (uint8_t i = 0; i < 16; i++) {
                // Store each byte in flash memory starting from the specified address
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, ulSaddr, pucData[i]);
                ulSaddr += 1;
            }
            */
            for (uint8_t i = 0; i < 4; i++) {
                /* Store each byte in flash memory starting from the specified address */
                /*
                uint8_t tmp;
                tmp = pucData[i*4];
                pucData[i*4]   = pucData[i*4+3];
                pucData[i*4+3] = tmp;
                tmp = pucData[i*4+1];
                pucData[i*4+1] = pucData[i*4+2];
                pucData[i*4+2] = tmp;
                */
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ulSaddr, ((uint32_t*)pucData)[i]);
                ulSaddr += 4;
            }
            HAL_FLASH_Lock(); //Locks again the flash memory

            /* Sends an ACK */
            ack = ACK;
            // HAL_UART_Transmit(&huart2, (uint8_t *) pucData, 1, HAL_MAX_DELAY);
            queue_USB((uint8_t const *) &ack, 1);

            shift_buffer(USB_rx_buffer, 0xFF, 20);
            USB_rx_buffer_lead_ptr -= 20;

        } else {
            shift_buffer(USB_rx_buffer, 0xFF, 20);
            USB_rx_buffer_lead_ptr -= 20;

            goto sendnack;
        }
    } else {
        goto sendnack;
    }

    return;

sendnack:
    {
        uint8_t nack = NACK;
        queue_USB((uint8_t const *) &nack, 1);
    }

    return;
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


void main_tick_1ms() {}
void main_tick_5ms() {
    if (USB_tx_buffer_lead_ptr > 0) {
        USB_Send(USB_tx_buffer, USB_tx_buffer_lead_ptr);
        USB_tx_buffer_lead_ptr = 0;
    }
}
void main_tick_10ms()  {}
void main_tick_50ms()  {}
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
