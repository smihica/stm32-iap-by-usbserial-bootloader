#ifndef __MAIN_H
#define __MAIN_H

typedef enum run_mode {
    APP_MODE     = 0x00,
    FLASHER_MODE = 0x01,
} run_mode_t;

typedef enum flasher_state {
    NOT_IN       = 0x00,
    WAITING      = 0x01,
    ERASING      = 0x02,
    WRITING      = 0x03,
} flasher_state_t;

typedef enum firm_partition {
    FIRM0 = 0x00,
    FIRM1 = 0x01,
} firm_partition_t;

void SystemClock_Config(void);

void main_tick_1ms();
void main_tick_5ms();
void main_tick_10ms();
void main_tick_50ms();
void main_tick_100ms();
void main_tick_500ms();
void main_tick_1s();

#endif // __MAIN_H
