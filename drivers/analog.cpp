#include "drivers/analog.h"

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_tim.h"
#include "drivers/system.h"
#include "drivers/profiling.h"
#include "drivers/irq.h"
#include "common/config.h"

namespace recorder
{

void Analog::Init(Callback callback)
{
    instance_ = this;
    callback_ = callback;

    adc_enable_.Init();
    adc_enable_.Set();
    boost_enable_.Init();
    amp_enable_.Init();

    adc_.Init(AdcCallback);
    dac_.Init();
    InitTimer();

    fade_position_ = 0;
    state_ = STATE_STOPPED;
    cue_stop_ = false;
    Stop();
}

void Analog::InitTimer(void)
{
    float period = system::kSystemClock / kAudioOSRate;

    LL_TIM_InitTypeDef timer_init =
    {
        .Prescaler         = 0,
        .CounterMode       = LL_TIM_COUNTERMODE_UP,
        .Autoreload        = static_cast<uint32_t>(period + 0.5) - 1,
        .ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    };

    __HAL_RCC_TIM15_CLK_ENABLE();
    LL_TIM_Init(TIM15, &timer_init);
    LL_TIM_SetTriggerOutput(TIM15, LL_TIM_TRGO_UPDATE);

    if (ProfilingPin<PROFILE_AUDIO_SAMPLING>::active())
    {
        irq::RegisterHandler(TIM15_IRQn, TimerHandler);
        irq::SetPriority(TIM15_IRQn, kProfileIRQPriority);
        irq::Enable(TIM15_IRQn);
    }
}

void Analog::StartTimer(void)
{
    if (ProfilingPin<PROFILE_AUDIO_SAMPLING>::active())
    {
        LL_TIM_EnableIT_UPDATE(TIM15);
    }

    LL_TIM_SetCounter(TIM15, 0);
    LL_TIM_EnableCounter(TIM15);
}

void Analog::StopTimer(void)
{
    LL_TIM_DisableCounter(TIM15);

    if (ProfilingPin<PROFILE_AUDIO_SAMPLING>::active())
    {
        LL_TIM_DisableIT_UPDATE(TIM15);
        LL_TIM_ClearFlag_UPDATE(TIM15);
    }
}

void Analog::TimerHandler(void)
{
    LL_TIM_ClearFlag_UPDATE(TIM15);
    LL_TIM_IsActiveFlag_UPDATE(TIM15);
    ProfilingPin<PROFILE_AUDIO_SAMPLING>::Set();
}

}
