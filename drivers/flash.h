#pragma once

#include <cstdint>
#include <algorithm>

#include "drivers/profiling.h"
#include "drivers/system.h"

namespace recorder
{

class Flash
{
public:
    static constexpr uint32_t kSize = 8 * 1024 * 1024;
    static constexpr uint32_t kEraseGranularity = 4 * 1024;
    static constexpr uint32_t kWriteGranularity = 1;
    static constexpr uint8_t kFillByte = 0xFF;

    void Init(void);

    void PowerDown(void)
    {
        EnterPowerDown();
    }

    void ChipErase(void)
    {
        WriteEnable();
        SendCommand(CMD_CHIP_ERASE);
        WaitForWriteInProgress();
    }

    bool Read(void* dst, uint32_t location, uint32_t length)
    {
        WaitForWriteInProgress();
        ReadData(reinterpret_cast<uint8_t*>(dst), location, length);
        return true;
    }

    bool Writable(uint32_t location, uint32_t length)
    {
        WaitForWriteInProgress();

        uint8_t buffer[1024];

        while (length)
        {
            uint32_t len = std::min<uint32_t>(1024, length);

            if (!Read(buffer, location, len))
            {
                return false;
            }

            for (uint32_t i = 0; i < len; i++)
            {
                if (buffer[i] != kFillByte)
                {
                    return false;
                }
            }

            location += len;
            length -= len;
        }

        return true;
    }

    bool Write(uint32_t location, const void* src, uint32_t length)
    {
        if (!BeginWrite(location, src, length))
        {
            return false;
        }

        while (!FinishWrite());
        return true;
    }

    bool BeginWrite(uint32_t location, const void* src, uint32_t length)
    {
        if ((location % kWriteGranularity) || (length % kWriteGranularity))
        {
            return false;
        }

        state_ =
        {
            .location = location,
            .length = length,
            .bytes = reinterpret_cast<const uint8_t*>(src),
        };

        ProfilingPin<PROFILE_FLASH_WRITE>::Set();
        ProfilingPin<PROFILE_FLASH_ACCESS>::Set();
        return true;
    }

    bool FinishWrite(void)
    {
        if (state_.length == 0)
        {
            return true;
        }

        if (write_in_progress())
        {
            return false;
        }

        uint32_t offset_in_page = state_.location % kPageSize;
        uint32_t len = std::min(state_.length, kPageSize - offset_in_page);
        PageProgram(state_.bytes, state_.location, len, false);
        state_.bytes += len;
        state_.location += len;
        state_.length -= len;

        bool done = (state_.length == 0);
        ProfilingPin<PROFILE_FLASH_WRITE>::Write(!done);
        ProfilingPin<PROFILE_FLASH_ACCESS>::Write(!done);
        return done;
    }

    void AbortWrite(void)
    {
        ProfilingPin<PROFILE_FLASH_WRITE>::Clear();
        ProfilingPin<PROFILE_FLASH_ACCESS>::Clear();
    }

    bool Erase(uint32_t location, uint32_t length)
    {
        if (!BeginErase(location, length))
        {
            return false;
        }

        while (!FinishErase());
        return true;
    }

    bool BeginErase(uint32_t location, uint32_t length)
    {
        if ((location % kEraseGranularity) || (length % kEraseGranularity))
        {
            return false;
        }

        state_ =
        {
            .location = location,
            .length = length,
            .bytes = nullptr,
        };

        ProfilingPin<PROFILE_FLASH_ERASE>::Set();
        ProfilingPin<PROFILE_FLASH_ACCESS>::Set();
        return true;
    }

    bool FinishErase(void)
    {
        uint32_t length = state_.length;
        uint32_t location = state_.location;

        if (length == 0)
        {
            return true;
        }

        if (write_in_progress())
        {
            return false;
        }

        if ((location % kBlock64Size == 0) && (length >= kBlock64Size))
        {
            BlockErase64(location, false);
            location += kBlock64Size;
            length -= kBlock64Size;
        }
        else if ((location % kBlock32Size == 0) && (length >= kBlock32Size))
        {
            BlockErase32(location, false);
            location += kBlock32Size;
            length -= kBlock32Size;
        }
        else
        {
            SectorErase(location, false);
            location += kEraseGranularity;
            length -= kEraseGranularity;
        }

        state_.length = length;
        state_.location = location;

        bool done = (state_.length == 0);
        ProfilingPin<PROFILE_FLASH_ERASE>::Write(!done);
        ProfilingPin<PROFILE_FLASH_ACCESS>::Write(!done);
        return done;
    }

    void AbortErase(void)
    {
        ProfilingPin<PROFILE_FLASH_ERASE>::Clear();
        ProfilingPin<PROFILE_FLASH_ACCESS>::Clear();
    }

protected:
    static constexpr uint32_t kPageSize = 256;
    static constexpr uint32_t kBlock32Size = 32 * 1024;
    static constexpr uint32_t kBlock64Size = 64 * 1024;

    void InitPin(GPIO_TypeDef* base, uint32_t pin, uint32_t alternate);
    void InitDMA(void);

    enum Command
    {
        CMD_WRITE_STATUS_REG    = 0x01,
        CMD_PAGE_PROGRAM        = 0x02,
        CMD_NORMAL_READ         = 0x03,
        CMD_FAST_READ           = 0x0B,
        CMD_READ_STATUS_REG     = 0x05,
        CMD_WRITE_ENABLE        = 0x06,
        CMD_ENABLE_QPI          = 0x35,
        CMD_DISABLE_QPI         = 0xF5,
        CMD_RESET_EN            = 0x66,
        CMD_FAST_READ_QUAD_OUT  = 0x6B,
        CMD_RESET               = 0x99,
        CMD_EXIT_POWER_DOWN     = 0xAB,
        CMD_ENTER_POWER_DOWN    = 0xB9,
        CMD_CHIP_ERASE          = 0xC7,
        CMD_SECTOR_ERASE        = 0xD7,
        CMD_BLOCK_ERASE_32K     = 0x52,
        CMD_BLOCK_ERASE_64K     = 0xD8,
    };

    enum Status
    {
        STATUS_WRITE_IN_PROGRESS = 0x01,
        STATUS_WRITE_ENABLE = 0x02,
        STATUS_BLOCK_PROTECTION = 0x3C,
        STATUS_QUAD_ENABLE = 0x40,
        STATUS_SR_WRITE_DISABLE = 0x80,
    };

    struct State
    {
        uint32_t location;
        uint32_t length;
        const uint8_t* bytes;
    };

    State state_;

    static constexpr uint32_t kIndirectWrite = 0;
    static constexpr uint32_t kIndirectRead = QUADSPI_CCR_FMODE_0;

    void SendCommand(Command cmd)
    {
        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        QUADSPI->CCR = QSPI_INSTRUCTION_1_LINE | kIndirectWrite | cmd;
        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;
    }

    bool write_in_progress(void)
    {
        return (ReadStatus() & STATUS_WRITE_IN_PROGRESS);
    }

    void WaitForWriteInProgress(void)
    {
        while (write_in_progress())
        {
            system::Delay_ms(1);
        }
    }

    uint8_t DataRead8(void)
    {
        return *reinterpret_cast<volatile uint8_t*>(&QUADSPI->DR);
    }

    void DataWrite8(uint8_t byte)
    {
        *reinterpret_cast<volatile uint8_t*>(&(QUADSPI->DR)) = byte;
    }

    void ReadData(uint8_t* buffer, uint32_t address, uint32_t count);

    void PageProgram(const uint8_t* buffer, uint32_t address, uint32_t count,
        bool blocking)
    {
        WriteEnable();
        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        QUADSPI->DLR = count - 1;
        QUADSPI->CCR =
            kIndirectWrite |
            QSPI_DATA_1_LINE |
            QSPI_ADDRESS_24_BITS |
            QSPI_ADDRESS_1_LINE |
            QSPI_INSTRUCTION_1_LINE |
            CMD_PAGE_PROGRAM;
        QUADSPI->AR = address;

        while (count--)
        {
            while (!(QUADSPI->SR & QUADSPI_SR_FTF));
            DataWrite8(*buffer++);
        }

        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;

        if (blocking)
        {
            WaitForWriteInProgress();
        }
    }

    void EraseCommand(Command command, uint32_t address,
        bool blocking)
    {
        WriteEnable();
        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        QUADSPI->CCR =
            kIndirectWrite |
            QSPI_ADDRESS_24_BITS |
            QSPI_ADDRESS_1_LINE |
            QSPI_INSTRUCTION_1_LINE |
            command;
        QUADSPI->AR = address;
        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;

        if (blocking)
        {
            WaitForWriteInProgress();
        }
    }

    void SectorErase(uint32_t address, bool blocking)
    {
        EraseCommand(CMD_SECTOR_ERASE, address, blocking);
    }

    void BlockErase32(uint32_t address, bool blocking)
    {
        EraseCommand(CMD_BLOCK_ERASE_32K, address, blocking);
    }

    void BlockErase64(uint32_t address, bool blocking)
    {
        EraseCommand(CMD_BLOCK_ERASE_64K, address, blocking);
    }

    void Reset(void)
    {
        SendCommand(CMD_RESET_EN);
        SendCommand(CMD_RESET);
    }

    void EnterPowerDown(void)
    {
        SendCommand(CMD_ENTER_POWER_DOWN);
    }

    void ExitPowerDown(void)
    {
        SendCommand(CMD_EXIT_POWER_DOWN);
    }

    void EnableQPI(void)
    {
        SendCommand(CMD_ENABLE_QPI);
    }

    void DisableQPI(void)
    {
        SendCommand(CMD_DISABLE_QPI);
    }

    void WriteEnable(void)
    {
        SendCommand(CMD_WRITE_ENABLE);
    }

    uint8_t ReadStatus(void)
    {
        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        QUADSPI->DLR = 0;
        QUADSPI->CCR =
            kIndirectRead |
            QSPI_DATA_1_LINE |
            QSPI_INSTRUCTION_1_LINE |
            CMD_READ_STATUS_REG;
        while (!(QUADSPI->SR & QUADSPI_SR_FLEVEL));
        uint8_t status = DataRead8();
        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;
        return status;
    }

    void WriteStatus(uint8_t status)
    {
        WriteEnable();

        while (QUADSPI->SR & QUADSPI_SR_BUSY);
        QUADSPI->DLR = 0;
        QUADSPI->CCR =
            kIndirectWrite |
            QSPI_DATA_1_LINE |
            QSPI_INSTRUCTION_1_LINE |
            CMD_WRITE_STATUS_REG;
        while (!(QUADSPI->SR & QUADSPI_SR_FTF));
        DataWrite8(status);
        while (!(QUADSPI->SR & QUADSPI_SR_TCF));
        QUADSPI->FCR = QUADSPI_FCR_CTCF;
        WaitForWriteInProgress();
    }
};

}
