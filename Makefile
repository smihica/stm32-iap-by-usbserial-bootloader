TARGET = bootloader-firmware

DEBUG = 1
OPT = -Os

# Build path
BUILD_DIR = build

# C sources
C_SOURCES =  \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_gpio.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash_ex.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc_ex.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pcd.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pcd_ex.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_crc.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_crc_ex.c \
drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_cortex.c \
middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c \
$(wildcard src/*.c)

# binaries
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS = $(PREFIX)gcc
CP = $(PREFIX)objcopy
AR = $(PREFIX)ar
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# CFLAGS
# cpu
CPU = -mcpu=cortex-m4

# fpu
FPU = -mfpu=fpv4-sp-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# AS defines
AS_DEFS =

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32F303xE

# AS includes
AS_INCLUDES =

# C includes
C_INCLUDES =  \
-Isrc \
-Idrivers/STM32F3xx_HAL_Driver/Inc \
-Idrivers/STM32F3xx_HAL_Driver/Inc/Legacy \
-Idrivers/CMSIS/Device/ST/STM32F3xx/Include \
-Idrivers/CMSIS/Include \
-Imiddlewares/ST/STM32_USB_Device_Library/Core/Inc \
-Imiddlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"

# link script
LDSCRIPT = ld/ldscript-bootloader.ld

# libraries
LIBS = -lc -lnosys
LIBDIR =
LDFLAGS = -nostartfiles -nostdlib $(MCU) --specs=nano.specs --specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS)

## ------------------------------------

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

flash: all
	openocd -f configs/stm32f3xx_stlink21.cfg -c "program ./build/$(TARGET).elf verify reset exit"

# binary
# list of C objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -fR .dep $(BUILD_DIR)

# dependencies
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
