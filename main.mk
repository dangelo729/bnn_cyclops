-include env.inc.mk

GCC_PATH    ?= $(dir $(shell which arm-none-eabi-gcc))
ARM_CC      := $(GCC_PATH)/arm-none-eabi-gcc
ARM_CXX     := $(GCC_PATH)/arm-none-eabi-g++
ARM_OBJCOPY := $(GCC_PATH)/arm-none-eabi-objcopy
ARM_AR      := $(GCC_PATH)/arm-none-eabi-ar
ARM_OBJDUMP := $(GCC_PATH)/arm-none-eabi-objdump
ARM_SIZE    := $(GCC_PATH)/arm-none-eabi-size
ARM_NM      := $(GCC_PATH)/arm-none-eabi-nm
ARM_GDB     := $(GCC_PATH)/arm-none-eabi-gdb

VARIANT_DELAY ?= 0
VARIANT_LINE_IN ?= 0
VARIANT_REVERSE ?= 0

VARIANT_NAME := recorder
ifneq ($(VARIANT_DELAY),0)
VARIANT_NAME := $(VARIANT_NAME).delay
endif
ifneq ($(VARIANT_REVERSE),0)
VARIANT_NAME := $(VARIANT_NAME).reverse
endif
ifneq ($(VARIANT_LINE_IN),0)
VARIANT_NAME := $(VARIANT_NAME).with-line-in
else
VARIANT_NAME := $(VARIANT_NAME).no-line-in
endif

.PHONY: variant_name
variant_name:
	@echo $(VARIANT_NAME)

BUILD_DIR := build/$(VARIANT_NAME)
DEFS := VARIANT_DELAY=$(VARIANT_DELAY) \
	VARIANT_LINE_IN=$(VARIANT_LINE_IN) \
	VARIANT_REVERSE=$(VARIANT_REVERSE)
TARGET_DIR := $(BUILD_DIR)/artifact
SUBMAKEFILES := app.mk
INCDIRS := .

APP_ELF := $(TARGET_DIR)/app.elf
APP_HEX := $(TARGET_DIR)/app.hex
.DEFAULT_GOAL := $(APP_ELF)

%.hex: %.elf
	$(ARM_OBJCOPY) -O ihex $^ $@

%.bin: %.hex
	$(ARM_OBJCOPY) --gap-fill 0xFF -O binary -I ihex $^ $@

%.lss: %.elf
	$(ARM_OBJDUMP) -CdhtS $^ > $@

.PHONY: app
app: $(APP_HEX)

.PHONY: sym
sym: $(APP_ELF)
	$(ARM_NM) -CnS $< | less

.PHONY: top
top: $(APP_ELF)
	$(ARM_NM) -CrS --size-sort $< | less

PGM_DEVICE ?= -f interface/stlink.cfg

OPENOCD_CMD := openocd -c "debug_level 1" \
	$(PGM_DEVICE) \
	-f target/stm32h7x.cfg

.PHONY: load
load: $(APP_ELF)
	$(OPENOCD_CMD) -c "program $< verify reset exit"

.PHONY: reset
reset:
	$(OPENOCD_CMD) -c init -c reset -c exit

.PHONY: openocd
openocd:
	$(OPENOCD_CMD) -c init -c reset -c halt

.PHONY: debug
debug: $(APP_ELF)
	@if ! nc -z localhost 3333; then \
		echo "\n\t[Error] OpenOCD is not running! \
			Start it with: 'make openocd'\n"; exit 1; \
	else \
		$(ARM_GDB) -tui -ex "target extended localhost:3333" \
			-ex "monitor arm semihosting enable" \
			-ex "monitor reset halt" \
			-ex "load" \
			-ex "monitor reset init" \
			$(GDBFLAGS) $<; \
	fi
