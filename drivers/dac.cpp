#include "drivers/dac.h"

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_gpio.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dac.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_dma.h"
#include "drivers/irq.h"
#include "drivers/profiling.h"

namespace recorder
{

void Dac::Init(void)
{
    started_ = false;
    Reset();
    InitGPIO();
    InitDAC();
    InitDMA();
}

void Dac::Reset(void)
{
    write_index_ = (kAudioOSFactor * 2) % kDMABufferSize;

    for (uint32_t i = 0; i < kDMABufferSize; i++)
    {
        dma_buffer_[i] = 0;
    }
}

void Dac::Start(void)
{
    if (!started_)
    {
        LL_DMA_ClearFlag_TC0(DMA1);
        LL_DMA_ClearFlag_HT0(DMA1);
        LL_DMA_EnableIT_TC(DMA1, LL_DMA_STREAM_0);
        LL_DMA_EnableIT_HT(DMA1, LL_DMA_STREAM_0);

        LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
        LL_DAC_SetTriggerSource(DAC1, LL_DAC_CHANNEL_2, LL_DAC_TRIG_SOFTWARE);
        LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_2, 0);
        LL_DAC_TrigSWConversion(DAC1, LL_DAC_CHANNEL_2);
        LL_DAC_SetTriggerSource(DAC1, LL_DAC_CHANNEL_2,
            LL_DAC_TRIG_EXT_TIM15_TRGO);

        DMA1->LIFCR = 0x7D << (8 * LL_DMA_STREAM_0);
        LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_0);

        started_ = true;
    }
}

void Dac::Stop(void)
{
    if (started_)
    {
        LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_0);
        while (LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_0));
        LL_DAC_Disable(DAC1, LL_DAC_CHANNEL_2);

        LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_0);
        LL_DMA_DisableIT_HT(DMA1, LL_DMA_STREAM_0);
        LL_DMA_ClearFlag_TC0(DMA1);
        LL_DMA_ClearFlag_HT0(DMA1);

        Reset();
        started_ = false;
    }
}

void Dac::InitGPIO(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    LL_GPIO_InitTypeDef gpio_init;
    LL_GPIO_StructInit(&gpio_init);
    gpio_init.Mode = LL_GPIO_MODE_ANALOG;
    gpio_init.Pull = LL_GPIO_PULL_NO;
    gpio_init.Pin = LL_GPIO_PIN_5;
    LL_GPIO_Init(GPIOA, &gpio_init);
}

void Dac::InitDAC(void)
{
    LL_DAC_InitTypeDef dac_init =
    {
        .TriggerSource            = LL_DAC_TRIG_EXT_TIM15_TRGO,
        .WaveAutoGeneration       = LL_DAC_WAVE_AUTO_GENERATION_NONE,
        .WaveAutoGenerationConfig = 0,
        .OutputBuffer             = LL_DAC_OUTPUT_BUFFER_DISABLE,
        .OutputConnection         = LL_DAC_OUTPUT_CONNECT_GPIO,
        .OutputMode               = LL_DAC_OUTPUT_MODE_NORMAL,
    };

    __HAL_RCC_DAC12_CLK_ENABLE();
    LL_DAC_Init(DAC1, LL_DAC_CHANNEL_2, &dac_init);
    LL_DAC_EnableDMAReq(DAC1, LL_DAC_CHANNEL_2);
    LL_DAC_EnableTrigger(DAC1, LL_DAC_CHANNEL_2);
}

void Dac::InitDMA(void)
{
    uint32_t periph_address = LL_DAC_DMA_GetRegAddr(DAC1, LL_DAC_CHANNEL_2,
        LL_DAC_DMA_REG_DATA_12BITS_RIGHT_ALIGNED);

    LL_DMA_InitTypeDef dma_init =
    {
        .PeriphOrM2MSrcAddress  = periph_address,
        .MemoryOrM2MDstAddress  = reinterpret_cast<uint32_t>(dma_buffer_),
        .Direction              = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
        .Mode                   = LL_DMA_MODE_CIRCULAR,
        .PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT,
        .MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT,
        .PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD,
        .MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD,
        .NbData                 = kDMABufferSize,
        .PeriphRequest          = LL_DMAMUX1_REQ_DAC1_CH2,
        .Priority               = LL_DMA_PRIORITY_MEDIUM,
        .FIFOMode               = LL_DMA_FIFOMODE_DISABLE,
        .FIFOThreshold          = 0,
        .MemBurst               = LL_DMA_MBURST_SINGLE,
        .PeriphBurst            = LL_DMA_PBURST_SINGLE,
    };

    __HAL_RCC_DMA1_CLK_ENABLE();
    LL_DMA_Init(DMA1, LL_DMA_STREAM_0, &dma_init);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_0);
    LL_DMA_DisableIT_HT(DMA1, LL_DMA_STREAM_0);

    irq::RegisterHandler(DMA1_Stream0_IRQn, DMAHandler);
    irq::SetPriority(DMA1_Stream0_IRQn, kProfileIRQPriority);
    irq::Enable(DMA1_Stream0_IRQn);
}

void Dac::DMAHandler(void)
{
    ScopedProfilingPin<PROFILE_DAC_DMA_SERVICE> profile;
    LL_DMA_ClearFlag_TC0(DMA1);
    LL_DMA_ClearFlag_HT0(DMA1);
    LL_DMA_IsActiveFlag_TC0(DMA1);
    LL_DMA_IsActiveFlag_HT0(DMA1);
}

}
