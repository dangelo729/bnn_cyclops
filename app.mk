TARGET := $(notdir $(APP_ELF))
LD_SCRIPT := app/app.ld
SOURCES = \
	app/main.cpp \
	$(wildcard drivers/*.cpp) \
	libDaisy/core/startup_stm32h750xx.c \
	libDaisy/src/sys/system_stm32h7xx.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_adc.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_crc.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_dac.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_dma.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_gpio.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_mdma.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_pwr.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_rcc.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_tim.c \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usart.c

TGT_AR := $(ARM_AR)
TGT_CC := $(ARM_CC)
TGT_CXX := $(ARM_CXX)

T_ELF := $(TARGET_DIR)/$(TARGET)
T_MAP := $(TARGET_DIR)/$(TARGET:.elf=.map)
T_HEX := $(TARGET_DIR)/$(TARGET:.elf=.hex)
T_LSS := $(TARGET_DIR)/$(TARGET:.elf=.lss)

$(T_ELF): $(LD_SCRIPT)

ARCHFLAGS := \
    -mcpu=cortex-m7 \
    -mthumb \
    -mthumb-interwork \
    -mfpu=auto \
    -mfloat-abi=hard \
    -mfp16-format=alternative

WARNFLAGS := \
    -Wall \
    -Wextra \
    -Wno-undef \
    -Wdouble-promotion

OPTFLAGS := \
    -ffunction-sections \
    -fdata-sections \
    -fshort-enums \
    -finline-functions \
    -finline-functions-called-once \
    -ffast-math \
    -fsingle-precision-constant

TGT_CFLAGS := -ggdb3 -O3 $(ARCHFLAGS) $(WARNFLAGS) $(OPTFLAGS) -std=gnu11 \
	-Wno-missing-attributes -Wno-unused-parameter -Wno-attributes
TGT_CXXFLAGS := -ggdb3 -O3 $(ARCHFLAGS) $(WARNFLAGS) $(OPTFLAGS) -std=gnu++2a \
	-Wno-register -fno-exceptions -fno-rtti

TGT_LDLIBS  := -lm -lc -lnosys
TGT_LDFLAGS := $(ARCHFLAGS) -Wl,--gc-sections \
    -specs=nosys.specs -specs=nano.specs \
	-L$(TARGET_DIR) \
	-LlibDaisy/build \
	-T$(LD_SCRIPT) \
	-Wl,-Map=$(T_MAP) \
	-Wl,--print-memory-usage

TGT_INCDIRS := \
	libDaisy \
	libDaisy/src/ \
	libDaisy/src/sys \
	libDaisy/Drivers/CMSIS/Include/ \
	libDaisy/Drivers/CMSIS/DSP/Include \
	libDaisy/Drivers/CMSIS/Device/ST/STM32H7xx/Include \
	libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/

TGT_DEFS :=  \
	USE_HAL_DRIVER \
	STM32H750xx \
	HSE_VALUE=16000000 \
	CORE_CM7  \
	STM32H750IB \
	ARM_MATH_CM7 \
	USE_FULL_LL_DRIVER

define TGT_POSTMAKE
$(ARM_OBJDUMP) -CdhtS $(T_ELF) > $(T_LSS)
endef

define TGT_POSTCLEAN
$(RM) $(T_MAP) $(T_HEX) $(T_LSS)
endef
