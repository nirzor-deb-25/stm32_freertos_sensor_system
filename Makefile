# Windows command interpreter.
SHELL := cmd.exe
.SHELLFLAGS := /C

.RECIPEPREFIX := >
.DEFAULT_GOAL := all

TARGET := stm32_freertos_sensor
BUILD := build
LINKER_SCRIPT := STM32F407VGTX_FLASH.ld

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
PROGRAMMER ?= STM32_Programmer_CLI.exe

# Your Discovery board's ST-LINK.
# Override on another board: make flash STLINK_SN=its_serial_number
STLINK_SN ?= 0671FF495170535067222342

# Cortex-M4F port requires floating-point instruction support.
# softfp retains the base calling convention.
CPU_FLAGS := -mcpu=cortex-m4 -mthumb
CPU_FLAGS += -mfpu=fpv4-sp-d16 -mfloat-abi=softfp

FREERTOS_DIR := ThirdParty/FreeRTOS-Kernel
FREERTOS_PORT := $(FREERTOS_DIR)/portable/GCC/ARM_CM4F

CPPFLAGS := -IInc
CPPFLAGS += -I$(FREERTOS_DIR)/include
CPPFLAGS += -I$(FREERTOS_PORT)

CFLAGS := $(CPU_FLAGS) -std=c11 -Og -g3
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -MMD -MP

ASFLAGS := $(CPU_FLAGS) -g3 -x assembler-with-cpp

LDFLAGS := $(CPU_FLAGS) -T$(LINKER_SCRIPT) -nostartfiles
LDFLAGS += --specs=nano.specs --specs=nosys.specs
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map

LDLIBS := -Wl,--start-group -lc -lgcc -Wl,--end-group

APP_SOURCES := $(wildcard Src/*.c)
APP_OBJECTS := $(patsubst Src/%.c,$(BUILD)/%.o,$(APP_SOURCES))

KERNEL_OBJECTS := $(BUILD)/freertos/tasks.o
KERNEL_OBJECTS += $(BUILD)/freertos/queue.o
KERNEL_OBJECTS += $(BUILD)/freertos/list.o
KERNEL_OBJECTS += $(BUILD)/freertos/port.o
KERNEL_OBJECTS += $(BUILD)/freertos/heap_4.o

STARTUP_OBJECT := $(BUILD)/startup_stm32f407vgtx.o

OBJECTS := $(APP_OBJECTS) $(KERNEL_OBJECTS) $(STARTUP_OBJECT)

DEPS := $(APP_OBJECTS:.o=.d)
DEPS += $(KERNEL_OBJECTS:.o=.d)

ELF := $(BUILD)/$(TARGET).elf
BIN := $(BUILD)/$(TARGET).bin
HEX := $(BUILD)/$(TARGET).hex

.PHONY: all kernel clean size flash

all: $(ELF) $(BIN) $(HEX)

kernel: $(KERNEL_OBJECTS)

$(BUILD):
>if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BUILD)/freertos: | $(BUILD)
>if not exist "$(BUILD)\freertos" mkdir "$(BUILD)\freertos"

# Application sources.
$(BUILD)/%.o: Src/%.c Makefile | $(BUILD)
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# FreeRTOS core sources.
$(BUILD)/freertos/%.o: $(FREERTOS_DIR)/%.c Makefile | $(BUILD)/freertos
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Selected processor port.
$(BUILD)/freertos/port.o: $(FREERTOS_PORT)/port.c Makefile | $(BUILD)/freertos
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Exactly one FreeRTOS heap implementation.
$(BUILD)/freertos/heap_4.o: $(FREERTOS_DIR)/portable/MemMang/heap_4.c Makefile | $(BUILD)/freertos
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STARTUP_OBJECT): Startup/startup_stm32f407vgtx.s Makefile | $(BUILD)
>$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJECTS) $(LINKER_SCRIPT) Makefile
>$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
>$(SIZE) $@

$(BIN): $(ELF)
>$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
>$(OBJCOPY) -O ihex $< $@

size: $(ELF)
>$(SIZE) $(ELF)

flash: $(ELF)
>"$(PROGRAMMER)" -c port=SWD mode=UR sn=$(STLINK_SN) -w "$(ELF)" -v -rst

clean:
>if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"

-include $(DEPS)