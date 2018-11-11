#ifndef __PARTITION_H
#define __PARTITION_H

#ifdef STM32F303xE

// flash total 512k

#define BOOTLOADER_SIZE           (30  * 0x400)       // 30k
#define NVS_SIZE                  (2   * 0x400)       // 2k
#define FIRM0_SIZE                ((240 * 0x400) - 4) // 240k
#define FIRM1_SIZE                ((240 * 0x400) - 4) // 240k

#define BASE_ADDRESS              (0x08000000)  // start address of flash
#define BOOTLOADER_START_ADDRESS  (BASE_ADDRESS)
#define NVS_START_ADDRESS         (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE)
#define NVS_END_ADDRESS           ((NVS_START_ADDRESS + NVS_SIZE) - 1)
#define FIRM0_START_ADDRESS       (NVS_START_ADDRESS + NVS_SIZE)
#define FIRM0_END_ADDRESS         ((FIRM0_START_ADDRESS + FIRM0_SIZE) - 1)
#define FIRM0_CRC_ADDRESS         (FIRM0_START_ADDRESS + FIRM0_SIZE)
#define FIRM1_START_ADDRESS       ((FIRM0_START_ADDRESS + FIRM0_SIZE) + 4)
#define FIRM1_END_ADDRESS         ((FIRM1_START_ADDRESS + FIRM1_SIZE) - 1)
#define FIRM1_CRC_ADDRESS         (FIRM1_START_ADDRESS + FIRM1_SIZE)

#endif

#endif
