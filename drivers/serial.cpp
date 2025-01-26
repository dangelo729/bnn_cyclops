#include "serial.h"
#include <cassert>
#include <cstdio>

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_usart.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_lpuart.h"
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_gpio.h"
#include "drivers/irq.h"
#include "drivers/profiling.h"

#include "common/config.h"

namespace recorder
{

void Serial::Init(uint32_t baud)
{
    instance_ = this;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    uint32_t pins = LL_GPIO_PIN_6 | LL_GPIO_PIN_7;

    while (pins != 0)
    {
        uint32_t pin = 1 << POSITION_VAL(pins);
        CLEAR_BIT(pins, pin);

        // Set alternate function first to avoid spurious events
        if (POSITION_VAL(pin) < 8)
        {
            LL_GPIO_SetAFPin_0_7(GPIOB, pin, GPIO_AF7_USART1);
        }
        else
        {
            LL_GPIO_SetAFPin_8_15(GPIOB, pin, GPIO_AF7_USART1);
        }

        LL_GPIO_SetPinMode      (GPIOB, pin, LL_GPIO_MODE_ALTERNATE);
        LL_GPIO_SetPinSpeed     (GPIOB, pin, LL_GPIO_SPEED_FREQ_LOW);
        LL_GPIO_SetPinPull      (GPIOB, pin, LL_GPIO_PULL_UP);
        LL_GPIO_SetPinOutputType(GPIOB, pin, LL_GPIO_OUTPUT_PUSHPULL);
    }

    rx_fifo_.Init();
    tx_fifo_.Init();

    LL_USART_InitTypeDef uart_init =
    {
        .PrescalerValue      = LL_USART_PRESCALER_DIV1,
        .BaudRate            = baud,
        .DataWidth           = LL_USART_DATAWIDTH_8B,
        .StopBits            = LL_USART_STOPBITS_1,
        .Parity              = LL_USART_PARITY_NONE,
        .TransferDirection   = LL_USART_DIRECTION_TX_RX,
        .HardwareFlowControl = LL_USART_HWCONTROL_NONE,
        .OverSampling        = LL_USART_OVERSAMPLING_16,
    };

    LL_USART_Init(USART1, &uart_init);
    LL_USART_DisableOverrunDetect(USART1);
    LL_USART_Enable(USART1);

    LL_USART_RequestRxDataFlush(USART1);
    LL_USART_EnableIT_RXNE(USART1);
    LL_USART_DisableIT_TXE(USART1);

    irq::RegisterHandler(USART1_IRQn, InterruptHandler);
    irq::SetPriority(USART1_IRQn, kSerialIRQPriority);
    irq::Enable(USART1_IRQn);
}

uint32_t Serial::BytesAvailable(void)
{
    return rx_fifo_.available();
}

uint8_t Serial::GetByteBlocking(void)
{
    uint8_t byte = 0;
    while (!rx_fifo_.Pop(byte));
    return byte;
}

uint32_t Serial::Write(uint8_t byte, bool blocking)
{
    return Write(&byte, 1, blocking);
}

uint32_t Serial::Write(const char* buffer, uint32_t length, bool blocking)
{
    return Write(reinterpret_cast<const uint8_t*>(buffer), length, blocking);
}

uint32_t Serial::Write(const uint8_t* buffer, uint32_t length, bool blocking)
{
    uint32_t i = 0;

    while (i < length && (!tx_fifo_.full() || blocking))
    {
        ScopedProfilingPin<PROFILE_SERIAL_TX_FIFO_PUSH> profile;
        while (!tx_fifo_.Push(buffer[i]));
        i++;
    }

    LL_USART_EnableIT_TXE(USART1);

    return i;
}

void Serial::FlushTx(bool discard)
{
    if (discard)
    {
        LL_USART_DisableIT_TXE(USART1);
        tx_fifo_.Init();
    }
    else
    {
        while (tx_fifo_.available());
        while (!LL_USART_IsActiveFlag_TXE(USART1));
        while (!LL_USART_IsActiveFlag_TC(USART1));
    }
}

void Serial::FlushRx(void)
{
    rx_fifo_.Flush();
}

void Serial::InterruptService(void)
{
    if (LL_USART_IsActiveFlag_RXNE(USART1))
    {
        ScopedProfilingPin<PROFILE_SERIAL_RX> profile;
        uint8_t byte = LL_USART_ReceiveData8(USART1);

        if (rx_fifo_.full())
        {
            rx_fifo_.Pop();
        }

        rx_fifo_.Push(byte);
    }

    if (LL_USART_IsEnabledIT_TXE(USART1) && LL_USART_IsActiveFlag_TXE(USART1))
    {
        ScopedProfilingPin<PROFILE_SERIAL_TX> profile;
        uint8_t byte = 0;

        if (tx_fifo_.Peek(byte))
        {
            ScopedProfilingPin<PROFILE_SERIAL_TX_FIFO_POP> profile;
            LL_USART_TransmitData8(USART1, byte);
            tx_fifo_.Pop();
        }
        else
        {
            LL_USART_DisableIT_TXE(USART1);
        }
    }
}

void Serial::InterruptHandler(void)
{
    ScopedProfilingPin<PROFILE_SERIAL_IRQ> profile;
    instance_->InterruptService();
}

}
