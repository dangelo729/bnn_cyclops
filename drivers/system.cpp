#include "drivers/system.h"

#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <atomic>
#include <initializer_list>

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_rcc.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_pwr.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_tim.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_iwdg.h"
#include "drivers/profiling.h"
#include "drivers/irq.h"
#include "drivers/serial.h"

#include "common/config.h"

namespace recorder::system
{

static Serial serial_;
static std::atomic_uint32_t ticks_;
static uint32_t wakeup_flags_;

static void InitFPU(void)
{
    // Enable FPU coprocessors
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));

    // Enable lazy stacking
    FPU->FPCCR |= FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;

    // Enable flush to zero
    FPU->FPDSCR |= FPU_FPDSCR_FZ_Msk;

    // Enable alternate half-precision mode
    FPU->FPDSCR |= FPU_FPDSCR_AHP_Msk;

    // Round toward negative infinity
    FPU->FPDSCR &= ~FPU_FPDSCR_RMode_Msk;
    FPU->FPDSCR |= (2 << FPU_FPDSCR_RMode_Pos);
}

static void ConfigureClocks(void)
{
    uint32_t power_scaling = PWR_REGULATOR_VOLTAGE_SCALE3;
    uint32_t flash_latency = FLASH_LATENCY_1;

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(power_scaling);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    RCC_OscInitTypeDef osc_init = {};
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState = RCC_HSI_DIV1;
    if (HAL_GetREVID() <= REV_ID_Y)
    {
        osc_init.HSICalibrationValue =
            READ_BIT(RCC->HSICFGR, HAL_RCC_REV_Y_HSITRIM_Msk)
                >> HAL_RCC_REV_Y_HSITRIM_Pos;
    }
    else
    {
        osc_init.HSICalibrationValue =
            READ_BIT(RCC->HSICFGR, RCC_HSICFGR_HSITRIM_Msk)
                >> RCC_HSICFGR_HSITRIM_Pos;
    }

    if (HAL_RCC_OscConfig(&osc_init) != HAL_OK)
    {
        while (1);
    }

    RCC_ClkInitTypeDef clk_init = {};
    clk_init.ClockType =
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_D1PCLK1 |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2 |
        RCC_CLOCKTYPE_D3PCLK1;
    clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk_init.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk_init.AHBCLKDivider  = RCC_HCLK_DIV1;
    clk_init.APB1CLKDivider = RCC_APB1_DIV1;
    clk_init.APB2CLKDivider = RCC_APB2_DIV1;
    clk_init.APB3CLKDivider = RCC_APB3_DIV1;
    clk_init.APB4CLKDivider = RCC_APB4_DIV1;

    if (HAL_RCC_ClockConfig(&clk_init, flash_latency) != HAL_OK)
    {
        while (1);
    }

    RCC_PeriphCLKInitTypeDef periph_clk_init = {};
    periph_clk_init.PeriphClockSelection
        = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_QSPI;
    periph_clk_init.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    periph_clk_init.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL3;
    periph_clk_init.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
    periph_clk_init.PLL3 =
    {
        .PLL3M = 32,
        .PLL3N = 100,
        .PLL3P = 32,
        .PLL3Q = 32,
        .PLL3R = 32,
        .PLL3RGE = RCC_PLL3VCIRANGE_0,
        .PLL3VCOSEL = RCC_PLL3VCOMEDIUM,
        .PLL3FRACN = 0,
    };

    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    {
        while (1);
    }
}

extern "C"
void TickHandler(void)
{
    ScopedProfilingPin<PROFILE_TICK> profile;
    LL_TIM_ClearFlag_UPDATE(TIM7);
    LL_TIM_IsActiveFlag_UPDATE(TIM7);
    uint32_t ticks = ticks_.load(std::memory_order_relaxed);
    ticks_.store(ticks + 1, std::memory_order_relaxed);
}

extern "C"
HAL_StatusTypeDef HAL_InitTick(uint32_t)
{
    // This is called from HAL_RCC_ClockConfig() but we don't want to use
    // SysTick, so override it and do nothing.
    return HAL_OK;
}

static void InitTimer(uint32_t period)
{
    LL_TIM_InitTypeDef timer_init =
    {
        .Prescaler         = 0,
        .CounterMode       = LL_TIM_COUNTERMODE_DOWN,
        .Autoreload        = period - 1,
        .ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    };

    __HAL_RCC_TIM7_CLK_ENABLE();
    LL_TIM_Init(TIM7, &timer_init);
    LL_TIM_EnableIT_UPDATE(TIM7);
    LL_TIM_EnableCounter(TIM7);
    irq::RegisterHandler(TIM7_IRQn, TickHandler);
    irq::SetPriority(TIM7_IRQn, kTickIRQPriority);
    irq::Enable(TIM7_IRQn);
}

static void InitWatchdog(uint32_t timeout_ms)
{
    uint32_t mask =
        FLASH_OPTSR_FZ_IWDG_STOP |
        FLASH_OPTSR_FZ_IWDG_SDBY |
        FLASH_OPTSR_IWDG1_SW;
    uint32_t value = FLASH_OPTSR_IWDG1_SW;

    if ((FLASH->OPTSR_CUR & mask) != value)
    {
        FLASH->OPTKEYR = FLASH_OPT_KEY1;
        FLASH->OPTKEYR = FLASH_OPT_KEY2;
        MODIFY_REG(FLASH->OPTSR_PRG, mask, value);
        FLASH->OPTCR |= FLASH_OPTCR_OPTSTART;
        while (FLASH->OPTSR_CUR & FLASH_OPTSR_OPT_BUSY);
    }

    __HAL_DBGMCU_FREEZE_IWDG1();
    LL_IWDG_Enable(IWDG1);
    LL_IWDG_EnableWriteAccess(IWDG1);
    LL_IWDG_SetPrescaler(IWDG1, LL_IWDG_PRESCALER_32);
    LL_IWDG_SetReloadCounter(IWDG1, timeout_ms);
    while (!LL_IWDG_IsReady(IWDG1));
    ReloadWatchdog();
}

void ReloadWatchdog(void)
{
    ScopedProfilingPin<PROFILE_WATCHDOG> profile;
    LL_IWDG_ReloadCounter(IWDG1);
}

void Init(void)
{
    __disable_irq();

    InitFPU();

    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    SystemCoreClock = kSystemClock;
    SystemD2Clock = kSystemClock;

    ConfigureClocks();
    // Leave DCache disabled or else DMA won't work
    SCB_EnableICache();

    profiling::Init();
    ScopedProfilingPin<PROFILE_SYSTEM_INIT> profile;
    irq::Init();
    serial_.Init(115200);

    // 100us tick period
    ticks_.store(0, std::memory_order_relaxed);
    InitTimer(kSystemClock / 10000);

    InitWatchdog(100);
    __enable_irq();

    uint32_t reset_flags = RCC->RSR;
    LL_RCC_ClearResetFlags();
    printf("Reset source was ");

    switch (reset_flags >> 16)
    {
        case 0x00FE: printf("POR\n");   break;
        case 0x0046: printf("NRST\n");  break;
        case 0x0066: printf("BOR\n");   break;
        case 0x0146: printf("SFT\n");   break;
        case 0x0006: printf("CPU\n");   break;
        case 0x1046: printf("WWDG1\n"); break;
        case 0x0446: printf("IWDG1\n"); break;
        case 0x001E: printf("WAKE\n");  break;
        case 0x4046: printf("LPWR\n");  break;
        default: printf("unknown: 0x%08lX\n", reset_flags);
    }

    wakeup_flags_ = PWR->WKUPFR;

    if (wakeup_flags_ & PWR_WKUPFR_WKUPF1)
    {
        printf("Wakeup event was record button\n");
    }

    if (wakeup_flags_ & PWR_WKUPFR_WKUPF2)
    {
        printf("Wakeup event was play button\n");
    }

    LL_RCC_ClearResetFlags();
}

static uint32_t TickDelta(uint32_t start)
{
    return (ticks_.load(std::memory_order_acquire) - start) & 0xFFFFFFFF;
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = ticks_.load(std::memory_order_acquire);

    while (TickDelta(start) < ms * 10)
    {
        ScopedProfilingPin<PROFILE_SLEEP> profile;
        Sleep();
    }
}

uint32_t SerialBytesAvailable(void)
{
    return serial_.BytesAvailable();
}

uint8_t SerialGetByteBlocking(void)
{
    return serial_.GetByteBlocking();
}

void SerialFlushTx(bool discard)
{
    serial_.FlushTx(discard);
}

void Standby(void)
{
    ScopedProfilingPin<PROFILE_STANDBY> profile;

    __disable_fault_irq();

    // Disable and clear all interrupts
    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    LL_PWR_CPU_DisableD3RunInLowPowerMode();
    HAL_DisableDBGSleepMode();
    HAL_DisableDBGStopMode();
    HAL_DisableDBGStandbyMode();

    for (auto pin : {LL_PWR_WAKEUP_PIN1, LL_PWR_WAKEUP_PIN2})
    {
        LL_PWR_SetWakeUpPinPolarityLow(pin);
        LL_PWR_SetWakeUpPinPullUp(pin);
        LL_PWR_EnableWakeUpPin(pin);
    }

    // Clear all wakeup flags
    PWR->WKUPCR = 0xFFFFFFFF;

    // Enter standby mode
    LL_PWR_CPU_SetD1PowerMode(LL_PWR_CPU_MODE_D1STANDBY);
    LL_PWR_CPU_SetD2PowerMode(LL_PWR_CPU_MODE_D2STANDBY);
    LL_PWR_CPU_SetD3PowerMode(LL_PWR_CPU_MODE_D3STANDBY);
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();

    // If the system fails to enter standby for some reason, just reset it
    Reset();
}

bool WakeupWasPlayButton(void)
{
    return wakeup_flags_ & PWR_WKUPFR_WKUPF2;
}

void Sleep(void)
{
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}

void Reset(void)
{
    NVIC_SystemReset();
}

extern "C"
char* fgets(char* str, int count, std::FILE* stream)
{
    if (count < 2)
    {
        return nullptr;
    }

    int i;

    for (i = 0; i < count - 1; i++)
    {
        char byte = std::getc(stream);
        bool error = std::ferror(stream);
        bool eof = std::feof(stream);
        std::clearerr(stream);

        if (error || (eof && i == 0))
        {
            return nullptr;
        }
        else if (eof)
        {
            str[i] = '\0';
            return str;
        }
        else
        {
            str[i] = byte;

            if (byte == '\n')
            {
                break;
            }
        }
    }

    str[i + 1] = '\0';
    return str;
}

extern "C"
int _read(int file, char* ptr, int len)
{
    static bool prev_was_cr = false;

    if (file == STDIN_FILENO)
    {
        int i = 0;

        while (i < len && serial_.BytesAvailable())
        {
            char ch = serial_.GetByteBlocking();
            bool is_cr = (ch == '\r');
            bool is_lf = (ch == '\n');

            if (is_cr)
            {
                ch = '\n';
            }

            if (!(is_lf && prev_was_cr))
            {
                ptr[i++] = ch;
            }

            prev_was_cr = is_cr;
        }

        return i;
    }

    errno = ENOENT;
    return -1;
}

extern "C"
int _write(int file, char* ptr, int len)
{
    static char prev = '\0';

    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        for (int i = 0; i < len; i++)
        {
            if (ptr[i] == '\n' && prev != '\r')
            {
                serial_.Write('\r', true);
            }

            serial_.Write(ptr[i], true);
            prev = ptr[i];
        }

        return len;
    }

    errno = ENOENT;
    return -1;
}

}
