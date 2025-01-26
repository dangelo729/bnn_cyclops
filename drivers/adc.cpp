#include "drivers/adc.h"

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_gpio.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_system.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dma.h"
#include "drivers/system.h"
#include "drivers/irq.h"
#include "drivers/profiling.h"

#include "common/config.h"

namespace recorder
{

static const uint32_t kPotChannels[] =
{
    ADC_CHANNEL_3,
    ADC_CHANNEL_10,
    ADC_CHANNEL_7,
    ADC_CHANNEL_4,
    ADC_CHANNEL_12,
    ADC_CHANNEL_13,
    ADC_CHANNEL_18,
    ADC_CHANNEL_17,
};

static const uint32_t kAudioChannels[] =
{
    LL_ADC_CHANNEL_15,
    LL_ADC_CHANNEL_11,
};

static const uint32_t kADCRegRank[] =
{
    LL_ADC_REG_RANK_1,
    LL_ADC_REG_RANK_2,
    LL_ADC_REG_RANK_3,
    LL_ADC_REG_RANK_4,
    LL_ADC_REG_RANK_5,
    LL_ADC_REG_RANK_6,
    LL_ADC_REG_RANK_7,
    LL_ADC_REG_RANK_8,
    LL_ADC_REG_RANK_9,
    LL_ADC_REG_RANK_10,
    LL_ADC_REG_RANK_11,
    LL_ADC_REG_RANK_12,
    LL_ADC_REG_RANK_13,
    LL_ADC_REG_RANK_14,
    LL_ADC_REG_RANK_15,
    LL_ADC_REG_RANK_16,
};

////////////////////////////////////////////////////////////////////////////////
// GPIO ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Adc::InitGPIO(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    LL_GPIO_InitTypeDef gpio_init;
    LL_GPIO_StructInit(&gpio_init);
    gpio_init.Mode = LL_GPIO_MODE_ANALOG;
    gpio_init.Pull = LL_GPIO_PULL_NO;

    gpio_init.Pin = LL_GPIO_PIN_1 | LL_GPIO_PIN_3 | LL_GPIO_PIN_6 |
        LL_GPIO_PIN_7;

    if (!kEnableReverse)
    {
        gpio_init.Pin |= LL_GPIO_PIN_4;
    }

    LL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 |
        LL_GPIO_PIN_3 | LL_GPIO_PIN_4;
    LL_GPIO_Init(GPIOC, &gpio_init);
}

////////////////////////////////////////////////////////////////////////////////
// DMA /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Adc::InitDMA(void)
{
    uint32_t periph_address =
        LL_ADC_DMA_GetRegAddr(ADC2, LL_ADC_DMA_REG_REGULAR_DATA);
    uint32_t memory_address = reinterpret_cast<uint32_t>(dma_buffer_);

    LL_DMA_InitTypeDef dma_init =
    {
        .PeriphOrM2MSrcAddress  = periph_address,
        .MemoryOrM2MDstAddress  = memory_address,
        .Direction              = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
        .Mode                   = LL_DMA_MODE_CIRCULAR,
        .PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT,
        .MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT,
        .PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD,
        .MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD,
        .NbData                 = kDMABufferSize,
        .PeriphRequest          = LL_DMAMUX1_REQ_ADC2,
        .Priority               = LL_DMA_PRIORITY_HIGH,
        .FIFOMode               = LL_DMA_FIFOMODE_DISABLE,
        .FIFOThreshold          = 0,
        .MemBurst               = LL_DMA_MBURST_SINGLE,
        .PeriphBurst            = LL_DMA_PBURST_SINGLE,
    };

    __HAL_RCC_DMA1_CLK_ENABLE();
    LL_DMA_Init(DMA1, LL_DMA_STREAM_1, &dma_init);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_1);
    LL_DMA_DisableIT_HT(DMA1, LL_DMA_STREAM_1);
}

void Adc::DMAService(void)
{
    ScopedProfilingPin<PROFILE_ADC_DMA_SERVICE> profile;
    LL_DMA_ClearFlag_TC1(DMA1);
    LL_DMA_ClearFlag_HT1(DMA1);

    if (!LL_ADC_REG_IsConversionOngoing(ADC1))
    {
        float pot = LL_ADC_REG_ReadConversionData16(ADC1);
        LL_ADC_REG_StartConversion(ADC1);
        ProfilingPin<PROFILE_POT_SAMPLING>::Set();

        if (kEnableReverse && current_pot_ == POT_7)
        {
            pot = 0;
        }

        pot_filter_[current_pot_].Sample(pot / 0xFFFF);
        current_pot_ = (current_pot_ + 1) % NUM_POTS;
    }

    PerformCallback();
}

void Adc::DMAHandler(void)
{
    instance_->DMAService();
}

////////////////////////////////////////////////////////////////////////////////
// ADC /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Adc::InitADC(ADC_TypeDef* adc)
{
    LL_ADC_SetBoostMode(adc, LL_ADC_BOOST_MODE_50MHZ);
    LL_ADC_DisableDeepPowerDown(adc);
    LL_ADC_EnableInternalRegulator(adc);
    system::Delay_ms(1);

    LL_ADC_InitTypeDef adc_init =
    {
        .Resolution   = LL_ADC_RESOLUTION_16B,
        .LeftBitShift = LL_ADC_LEFT_BIT_SHIFT_NONE,
        .LowPowerMode = LL_ADC_LP_MODE_NONE,
    };

    LL_ADC_Init(adc, &adc_init);
    LL_ADC_StartCalibration(adc,
        LL_ADC_CALIB_OFFSET_LINEARITY, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(adc));
    system::Delay_ms(1);
}

void Adc::InitAudioSequence(void)
{
    auto adc = ADC2;
    auto trigger = LL_ADC_REG_TRIG_EXT_TIM15_TRGO;

    LL_ADC_REG_InitTypeDef reg_init =
    {
        .TriggerSource      = trigger,
        .SequencerLength    = LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS,
        .SequencerDiscont   = LL_ADC_REG_SEQ_DISCONT_DISABLE,
        .ContinuousMode     = LL_ADC_REG_CONV_SINGLE,
        .DataTransferMode   = LL_ADC_REG_DMA_TRANSFER_UNLIMITED,
        .Overrun            = LL_ADC_REG_OVR_DATA_OVERWRITTEN,
    };

    LL_ADC_REG_Init(adc, &reg_init);

    for (uint32_t i = 0; i < NUM_AUDIO_INS; i++)
    {
        uint32_t channel = kAudioChannels[i];
        uint32_t sampling_time = LL_ADC_SAMPLINGTIME_64CYCLES_5;
        adc->PCSEL |= (1UL << __LL_ADC_CHANNEL_TO_DECIMAL_NB(channel));
        LL_ADC_REG_SetSequencerRanks(adc, kADCRegRank[i], channel);
        LL_ADC_SetChannelSamplingTime(adc, channel, sampling_time);
    }

    LL_ADC_SetOverSamplingScope(adc, LL_ADC_OVS_GRP_INJ_REG_RESUMED);
    LL_ADC_SetOverSamplingDiscont(adc, LL_ADC_OVS_REG_CONT);
    LL_ADC_ConfigOverSamplingRatioShift(adc, 2, LL_ADC_OVS_SHIFT_RIGHT_1);
}

void Adc::InitPotSequence(void)
{
    auto adc = ADC1;
    auto trigger = LL_ADC_REG_TRIG_SOFTWARE;

    LL_ADC_REG_InitTypeDef reg_init =
    {
        .TriggerSource      = trigger,
        .SequencerLength    = (NUM_POTS - 1) << ADC_SQR1_L_Pos,
        .SequencerDiscont   = LL_ADC_REG_SEQ_DISCONT_1RANK,
        .ContinuousMode     = LL_ADC_REG_CONV_SINGLE,
        .DataTransferMode   = LL_ADC_REG_DR_TRANSFER,
        .Overrun            = LL_ADC_REG_OVR_DATA_OVERWRITTEN,
    };

    LL_ADC_REG_Init(adc, &reg_init);

    for (uint32_t i = 0; i < NUM_POTS; i++)
    {
        uint32_t channel = kPotChannels[i];
        uint32_t sampling_time = LL_ADC_SAMPLINGTIME_64CYCLES_5;
        adc->PCSEL |= (1UL << __LL_ADC_CHANNEL_TO_DECIMAL_NB(channel));
        LL_ADC_REG_SetSequencerRanks(adc, kADCRegRank[i], channel);
        LL_ADC_SetChannelSamplingTime(adc, channel, sampling_time);
    }

    LL_ADC_SetOverSamplingScope(adc, LL_ADC_OVS_GRP_INJ_REG_RESUMED);
    LL_ADC_SetOverSamplingDiscont(adc, LL_ADC_OVS_REG_CONT);
    LL_ADC_ConfigOverSamplingRatioShift(adc, 16, LL_ADC_OVS_SHIFT_RIGHT_4);
}

void Adc::ADCHandler(void)
{
    if (LL_ADC_IsActiveFlag_EOS(ADC2))
    {
        ProfilingPin<PROFILE_AUDIO_SAMPLING>::Clear();
        LL_ADC_ClearFlag_EOS(ADC2);
        LL_ADC_IsActiveFlag_EOS(ADC2);
    }

    if (LL_ADC_IsActiveFlag_EOC(ADC1))
    {
        ProfilingPin<PROFILE_POT_SAMPLING>::Clear();
        LL_ADC_ClearFlag_EOC(ADC1);
        LL_ADC_IsActiveFlag_EOC(ADC1);
    }

    if (LL_ADC_IsActiveFlag_EOS(ADC1))
    {
        ScopedProfilingPin<PROFILE_POT_EOS> profile;
        LL_ADC_ClearFlag_EOS(ADC1);
        LL_ADC_IsActiveFlag_EOS(ADC1);
    }
}

////////////////////////////////////////////////////////////////////////////////
// PUBLIC //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Adc::Init(Callback callback)
{
    instance_ = this;
    callback_ = callback;
    started_ = false;

    for (uint32_t i = 0; i < NUM_POTS; i++)
    {
        pot_filter_[i].Init(NUM_POTS);
    }

    Reset();

    InitGPIO();

    LL_SYSCFG_EnableAnalogBooster();
    __HAL_RCC_ADC12_CLK_ENABLE();
    LL_ADC_SetCommonClock(ADC12_COMMON, LL_ADC_CLOCK_SYNC_PCLK_DIV1);

    LL_ADC_Disable(ADC1);
    LL_ADC_Disable(ADC2);
    InitADC(ADC1);
    InitADC(ADC2);
    InitAudioSequence();
    InitPotSequence();
    LL_ADC_Enable(ADC1);
    LL_ADC_Enable(ADC2);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC2));

    InitDMA();

    bool adc_handler = false;

    if (ProfilingPin<PROFILE_AUDIO_SAMPLING>::active())
    {
        adc_handler = true;
        LL_ADC_EnableIT_EOS(ADC2);
        LL_ADC_ClearFlag_EOS(ADC2);
    }

    if (ProfilingPin<PROFILE_POT_SAMPLING>::active())
    {
        adc_handler = true;
        LL_ADC_EnableIT_EOC(ADC1);
        LL_ADC_ClearFlag_EOC(ADC1);
    }

    if (ProfilingPin<PROFILE_POT_EOS>::active())
    {
        adc_handler = true;
        LL_ADC_EnableIT_EOS(ADC1);
        LL_ADC_ClearFlag_EOS(ADC1);
    }

    if (adc_handler)
    {
        irq::RegisterHandler(ADC_IRQn, ADCHandler);
        irq::SetPriority(ADC_IRQn, kProfileIRQPriority);
        irq::Enable(ADC_IRQn);
    }

    irq::RegisterHandler(DMA1_Stream1_IRQn, DMAHandler);
    irq::SetPriority(DMA1_Stream1_IRQn, kADCIRQPriority);
    irq::Enable(DMA1_Stream1_IRQn);
}

void Adc::Reset(void)
{
    read_index_ = 0;
    current_pot_ = 0;

    for (uint32_t i = 0; i < NUM_POTS; i++)
    {
        pot_filter_[i].Reset();
    }
}

void Adc::Start(void)
{
    if (!started_)
    {
        LL_DMA_ClearFlag_TC1(DMA1);
        LL_DMA_ClearFlag_HT1(DMA1);
        LL_DMA_EnableIT_TC(DMA1, LL_DMA_STREAM_1);
        LL_DMA_EnableIT_HT(DMA1, LL_DMA_STREAM_1);

        while (LL_ADC_REG_IsConversionOngoing(ADC1));
        ProfilingPin<PROFILE_POT_SAMPLING>::Set();
        LL_ADC_REG_StartConversion(ADC1);
        LL_ADC_REG_StartConversion(ADC2);

        DMA1->LIFCR = 0x7D << (8 * LL_DMA_STREAM_1);
        LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_1);

        started_ = true;
    }
}

void Adc::Stop(void)
{
    if (started_)
    {
        LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_1);
        while (LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_1));

        LL_ADC_REG_StopConversion(ADC1);
        LL_ADC_REG_StopConversion(ADC2);
        while (LL_ADC_REG_IsConversionOngoing(ADC1));
        ProfilingPin<PROFILE_POT_SAMPLING>::Clear();
        while (LL_ADC_REG_IsConversionOngoing(ADC2));
        ProfilingPin<PROFILE_AUDIO_SAMPLING>::Clear();

        LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_1);
        LL_DMA_DisableIT_HT(DMA1, LL_DMA_STREAM_1);
        LL_DMA_ClearFlag_TC1(DMA1);
        LL_DMA_ClearFlag_HT1(DMA1);

        Reset();
        started_ = false;
    }
}

}
