SHELL := cmd.exe
.SHELLFLAGS := /C
.RECIPEPREFIX := >
.DEFAULT_GOAL := all

TARGET := uart_bridge
BUILD := ../build/bridge

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

PROGRAMMER ?= STM32_Programmer_CLI.exe
STLINK_SN ?= 066EFF555380535067043227

CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS := $(CPU_FLAGS) -std=c11 -Og -g3
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections -MMD -MP

ASFLAGS := $(CPU_FLAGS) -g3 -x assembler-with-cpp

LINKER_SCRIPT := STM32F439ZITX_FLASH.ld

LDFLAGS := $(CPU_FLAGS) -T$(LINKER_SCRIPT) -nostartfiles
LDFLAGS += --specs=nano.specs --specs=nosys.specs
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map

LDLIBS := -Wl,--start-group -lc -lgcc -Wl,--end-group

C_SOURCES := $(wildcard Src/*.c)
C_OBJECTS := $(patsubst Src/%.c,$(BUILD)/%.o,$(C_SOURCES))
STARTUP_OBJECT := $(BUILD)/startup.o

OBJECTS := $(C_OBJECTS) $(STARTUP_OBJECT)
DEPS := $(C_OBJECTS:.o=.d)

ELF := $(BUILD)/$(TARGET).elf
BIN := $(BUILD)/$(TARGET).bin
HEX := $(BUILD)/$(TARGET).hex

.PHONY: all flash clean

all: $(ELF) $(BIN) $(HEX)

$(BUILD):
>if not exist "..\build\bridge" mkdir "..\build\bridge"

$(BUILD)/%.o: Src/%.c Makefile | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(STARTUP_OBJECT): Startup/startup_stm32f439zitx.s Makefile | $(BUILD)
>$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJECTS) $(LINKER_SCRIPT) Makefile
>$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
>$(SIZE) $@

$(BIN): $(ELF)
>$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
>$(OBJCOPY) -O ihex $< $@

flash: $(ELF)
>"$(PROGRAMMER)" -c port=SWD mode=UR sn=$(STLINK_SN) -w "$(ELF)" -v -rst

clean:
>if exist "..\build\bridge" rmdir /S /Q "..\build\bridge"

-include $(DEPS)