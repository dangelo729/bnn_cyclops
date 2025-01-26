#pragma once

#include <cstdint>
#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_crc.h"

namespace recorder
{

class Crc
{
public:
    void Init(void)
    {
        __HAL_RCC_CRC_CLK_ENABLE();
        LL_CRC_SetPolynomialCoef(CRC, LL_CRC_DEFAULT_CRC32_POLY);
        LL_CRC_SetOutputDataReverseMode(CRC, LL_CRC_OUTDATA_REVERSE_NONE);
        LL_CRC_SetInputDataReverseMode(CRC, LL_CRC_INDATA_REVERSE_NONE);
        Seed(0);
    }

    void Seed(uint32_t value)
    {
        LL_CRC_SetInitialData(CRC, ~value);
        LL_CRC_ResetCRCCalculationUnit(CRC);
    }

    uint32_t Process(const uint8_t* data, uint32_t size)
    {
        while (size >= 4)
        {
            auto word = *reinterpret_cast<const uint32_t*>(data);
            LL_CRC_FeedData32(CRC, word);
            size -= 4;
            data += 4;
        }

        while (size--)
        {
            LL_CRC_FeedData8(CRC, *data++);
        }

        return value();
    }

    template <typename T>
    uint32_t Process(const T* data, uint32_t size)
    {
        return Process(reinterpret_cast<const uint8_t*>(data), size);
    }

    uint32_t value(void) const
    {
        return ~LL_CRC_ReadData32(CRC);
    }
};

}
