#include "flash.h"

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_mdma.h"

namespace recorder
{

void Flash::Init(void)
{
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    InitPin(GPIOF, LL_GPIO_PIN_6, GPIO_AF9_QUADSPI);
    InitPin(GPIOF, LL_GPIO_PIN_7, GPIO_AF9_QUADSPI);
    InitPin(GPIOF, LL_GPIO_PIN_8, GPIO_AF10_QUADSPI);
    InitPin(GPIOF, LL_GPIO_PIN_9, GPIO_AF10_QUADSPI);
    InitPin(GPIOF, LL_GPIO_PIN_10, GPIO_AF9_QUADSPI);
    InitPin(GPIOG, LL_GPIO_PIN_6, GPIO_AF10_QUADSPI);

    InitDMA();

    __HAL_RCC_QSPI_CLK_ENABLE();
    while (QUADSPI->SR & QUADSPI_SR_BUSY);
    uint32_t prescaler = 1;
    uint32_t fifo_threshold = 1;
    QUADSPI->CR =
        ((prescaler - 1) << QUADSPI_CR_PRESCALER_Pos) |
        ((fifo_threshold - 1) << QUADSPI_CR_FTHRES_Pos) |
        QSPI_FLASH_ID_1 |
        QSPI_DUALFLASH_DISABLE |
        QSPI_SAMPLE_SHIFTING_NONE;
    QUADSPI->DCR =
        ((POSITION_VAL(kSize) - 1) << QUADSPI_DCR_FSIZE_Pos) |
        QSPI_CS_HIGH_TIME_2_CYCLE |
        QSPI_CLOCK_MODE_0;
    QUADSPI->CR |= QUADSPI_CR_EN;

    ExitPowerDown();
    Reset();

    if (ReadStatus() != STATUS_QUAD_ENABLE)
    {
        WriteStatus(STATUS_QUAD_ENABLE);
    }

    state_ =
    {
        .location = 0,
        .length = 0,
        .bytes = nullptr,
    };
}

void Flash::InitPin(GPIO_TypeDef* base, uint32_t pin, uint32_t alternate)
{
    if (POSITION_VAL(pin) < 8)
    {
        LL_GPIO_SetAFPin_0_7(base, pin, alternate);
    }
    else
    {
        LL_GPIO_SetAFPin_8_15(base, pin, alternate);
    }

    LL_GPIO_SetPinMode      (base, pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinSpeed     (base, pin, GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull      (base, pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinOutputType(base, pin, LL_GPIO_OUTPUT_PUSHPULL);
}

void Flash::InitDMA(void)
{
    __HAL_RCC_MDMA_CLK_ENABLE();

    LL_MDMA_DisableChannel(MDMA, LL_MDMA_CHANNEL_0);

    LL_MDMA_InitTypeDef mdma_init =
    {
        .SrcAddress                    = reinterpret_cast<uint32_t>(&(QUADSPI->DR)),
        .DstAddress                    = 0,
        .RequestMode                   = LL_MDMA_REQUEST_MODE_HW,
        .TriggerMode                   = LL_MDMA_REPEAT_BLOCK_TRANSFER,
        .HWTrigger                     = LL_MDMA_REQ_QUADSPI_FIFO_TH,
        .BlockDataLength               = 0,
        .BlockRepeatCount              = 0,
        .BlockRepeatDestAddrUpdateMode = LL_MDMA_BLK_RPT_DEST_ADDR_INCREMENT,
        .BlockRepeatSrcAddrUpdateMode  = LL_MDMA_BLK_RPT_SRC_ADDR_INCREMENT,
        .BlockRepeatDestAddrUpdateVal  = 0,
        .BlockRepeatSrcAddrUpdateVal   = 0,
        .LinkAddress                   = 0,
        .WordEndianess                 = LL_MDMA_WORD_ENDIANNESS_PRESERVE,
        .HalfWordEndianess             = LL_MDMA_HALFWORD_ENDIANNESS_PRESERVE,
        .ByteEndianess                 = LL_MDMA_BYTE_ENDIANNESS_PRESERVE,
        .Priority                      = LL_MDMA_PRIORITY_VERYHIGH,
        .BufferableWriteMode           = LL_MDMA_BUFF_WRITE_ENABLE,
        .PaddingAlignment              = LL_MDMA_DATAALIGN_RIGHT,
        .PackMode                      = LL_MDMA_PACK_DISABLE,
        .BufferTransferLength          = 127,
        .DestBurst                     = LL_MDMA_DEST_BURST_16BEATS,
        .SrctBurst                     = LL_MDMA_SRC_BURST_16BEATS,
        .DestIncSize                   = LL_MDMA_DEST_INC_OFFSET_BYTE,
        .SrcIncSize                    = LL_MDMA_SRC_INC_OFFSET_BYTE,
        .DestDataSize                  = LL_MDMA_DEST_DATA_SIZE_BYTE,
        .SrcDataSize                   = LL_MDMA_SRC_DATA_SIZE_BYTE,
        .DestIncMode                   = LL_MDMA_DEST_INCREMENT,
        .SrcIncMode                    = LL_MDMA_SRC_FIXED,
        .DestBus                       = LL_MDMA_DEST_BUS_SYSTEM_AXI,
        .SrcBus                        = LL_MDMA_SRC_BUS_SYSTEM_AXI,
        .MaskAddress                   = 0,
        .MaskData                      = 0,
    };

    LL_MDMA_Init(MDMA, LL_MDMA_CHANNEL_0, &mdma_init);
}

void Flash::ReadData(uint8_t* buffer, uint32_t address, uint32_t count)
{
    ScopedProfilingPin<PROFILE_FLASH_READ> profile1;
    ScopedProfilingPin<PROFILE_FLASH_ACCESS> profile2;

    while (count)
    {
        LL_MDMA_DisableChannel(MDMA, LL_MDMA_CHANNEL_0);
        uint32_t dest_addr = reinterpret_cast<uint32_t>(buffer);
        LL_MDMA_SetDestinationAddress(MDMA, LL_MDMA_CHANNEL_0, dest_addr);
        uint32_t bus = (dest_addr & 0xDF000000) ?
            LL_MDMA_DEST_BUS_SYSTEM_AXI :
            LL_MDMA_DEST_BUS_AHB_TCM;
        LL_MDMA_SetDestBusSelection(MDMA, LL_MDMA_CHANNEL_0, bus);
        uint32_t block_length = std::min<uint32_t>(count, 0x10000);
        LL_MDMA_SetBufferTransferLength(MDMA, LL_MDMA_CHANNEL_0,
            std::min<uint32_t>(128, block_length) - 1);
        LL_MDMA_SetBlkDataLength(MDMA, LL_MDMA_CHANNEL_0, block_length);

        if (block_length < 128)
        {
            LL_MDMA_SetSourceBurstSize(
                MDMA, LL_MDMA_CHANNEL_0, LL_MDMA_SRC_BURST_SINGLE);
            LL_MDMA_SetDestinationBurstSize(
                MDMA, LL_MDMA_CHANNEL_0, LL_MDMA_DEST_BURST_SINGLE);
        }
        else
        {
            LL_MDMA_SetSourceBurstSize(
                MDMA, LL_MDMA_CHANNEL_0, LL_MDMA_SRC_BURST_16BEATS);
            LL_MDMA_SetDestinationBurstSize(
                MDMA, LL_MDMA_CHANNEL_0, LL_MDMA_DEST_BURST_16BEATS);
        }

        LL_MDMA_EnableChannel(MDMA, LL_MDMA_CHANNEL_0);

        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        constexpr uint32_t dummy_cycles = 8;
        QUADSPI->DLR = block_length - 1;
        QUADSPI->CCR =
            kIndirectRead |
            QSPI_DATA_4_LINES |
            (dummy_cycles << QUADSPI_CCR_DCYC_Pos) |
            QSPI_ADDRESS_24_BITS |
            QSPI_ADDRESS_1_LINE |
            QSPI_INSTRUCTION_1_LINE |
            CMD_FAST_READ_QUAD_OUT;
        QUADSPI->AR = address;

        while (!LL_MDMA_IsActiveFlag_BT(MDMA, LL_MDMA_CHANNEL_0));
        LL_MDMA_ClearFlag_BT(MDMA, LL_MDMA_CHANNEL_0);
        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;

        count -= block_length;
        buffer += block_length;
        address += block_length;
    }
}

}
