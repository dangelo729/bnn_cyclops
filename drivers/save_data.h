#pragma once

#include <cstdint>
#include <cstring>

namespace recorder
{

struct NVMemInterface
{
    static constexpr uint32_t kSize = 0;
    static constexpr uint32_t kEraseGranularity = 0;
    static constexpr uint32_t kWriteGranularity = 0;
    static constexpr uint8_t kFillByte = 0;

    bool Read(void* dst, uint32_t location, uint32_t length);
    bool Writable(uint32_t location, uint32_t length);
    bool Write(uint32_t location, const void* src, uint32_t length);
    bool Erase(uint32_t location, uint32_t length);
};

template <typename NVMem, typename T, uint32_t region_size = NVMem::kSize>
class SaveData
{
public:
    SaveData(NVMem& nvmem) : nvmem_{nvmem} {}

    void Init(void)
    {
        active_block_n_ = FindFreshestBlock();

        if (!LoadBlock(active_block_n_))
        {
            // No valid blocks
        }

        if (kNumPages < 2)
        {
            // Region not fully fault-tolerant
        }
    }

    bool Init(T& data)
    {
        Init();
        return Load(data);
    }

    bool Load(T& data)
    {
        if (active_block_n_ != -1)
        {
            std::memcpy(&data, &block_.data, sizeof(T));
            return true;
        }
        else
        {
            // No valid blocks
            return false;
        }
    }

    bool Save(const T& data)
    {
        if (0 == std::memcmp(&block_.data, &data, sizeof(T)))
        {
            // Skipped write to NVM due to identical data
            return true;
        }

        int32_t next_block;

        if (active_block_n_ == -1)
        {
            next_block = NextWritableBlock(kNumBlocks - 1);
        }
        else
        {
            next_block = NextWritableBlock(active_block_n_);
        }

        sequence_++;

        if (next_block == -1)
        {
            if (active_block_n_ == -1)
            {
                if (!nvmem_.Erase(0, kRegionSize))
                {
                    // Failed to erase NVM
                    return false;
                }

                next_block = 0;
                sequence_ = 0;
            }
            else
            {
                uint32_t current_page = active_block_n_ / kBlocksPerPage;
                uint32_t next_page = (current_page + 1) % kNumPages;
                next_block = next_page * kBlocksPerPage;

                if (!nvmem_.Erase(next_page * kPageSize, kPageSize))
                {
                    // Failed to erase NVM
                    return false;
                }
            }
        }

        active_block_n_ = next_block;
        uint32_t location = BlockLocation(active_block_n_);

        std::memset(&block_, NVMem::kFillByte, kBlockSize);
        std::memcpy(&block_.data, &data, sizeof(T));
        block_.sequence_num = sequence_;
        block_.checksum = 0;
        block_.checksum = kChecksum - Checksum(block_);

        if (!nvmem_.Write(location, &block_, kBlockSize))
        {
            // Failed to write to NVM
            return false;
        }

        return true;
    }

    bool Erase(void)
    {
        return nvmem_.Erase(0, kRegionSize);
    }

protected:
    static constexpr uint32_t kRegionSize = region_size;

    static_assert(NVMem::kEraseGranularity <= kRegionSize);
    static_assert(NVMem::kWriteGranularity <= kRegionSize);

    static constexpr uint8_t kChecksum = 0xFF;

    static constexpr
    uint32_t PadSize(uint32_t unpadded_size, uint32_t granularity)
    {
        uint32_t rem = unpadded_size % granularity;
        return (granularity - rem) % granularity;
    }

    struct __attribute__ ((packed)) Block
    {
        T data;
        uint16_t sequence_num;
        uint8_t checksum;
        uint8_t padding[PadSize(sizeof(T) + 3, NVMem::kWriteGranularity)];
    };

    static constexpr uint32_t kBlockSize = sizeof(Block);
    static constexpr uint32_t kPageSize =
        kBlockSize + PadSize(kBlockSize, NVMem::kEraseGranularity);
    static constexpr uint32_t kBlocksPerPage = kPageSize / kBlockSize;
    static constexpr uint32_t kNumPages = kRegionSize / kPageSize;
    static constexpr uint32_t kNumBlocks = kNumPages * kBlocksPerPage;

    static_assert(kBlocksPerPage > 0);
    static_assert(kNumPages > 0);
    static_assert(kNumBlocks > 0);
    static_assert(kNumBlocks <= 0x8000);

    Block block_;
    int32_t active_block_n_;
    uint32_t sequence_;
    NVMem& nvmem_;

    uint8_t Checksum(Block& block)
    {
        auto bytes = reinterpret_cast<uint8_t*>(&block);
        uint32_t sum = 0;

        for (uint32_t i = 0; i < kBlockSize; i++)
        {
            sum += bytes[i];
        }

        return sum;
    }

    bool IsValid(Block& block)
    {
        uint8_t sum = Checksum(block);
        return (sum == kChecksum);
    }

    bool LoadBlock(int32_t block_n)
    {
        if (block_n == -1)
        {
            return false;
        }

        return nvmem_.Read(&block_, BlockLocation(block_n), kBlockSize);
    }

    int32_t FindFreshestBlock(void)
    {
        int32_t block = -1;

        for (uint32_t i = 0; i < kNumBlocks; i++)
        {
            if (LoadBlock(i) && IsValid(block_))
            {
                uint32_t sn = block_.sequence_num;

                if ((block == -1) ||
                    ((sn > sequence_) && (sn - sequence_ < kNumBlocks)) ||
                    ((sn < sequence_) && (sequence_ - sn >= kNumBlocks)))
                {
                    block = i;
                    sequence_ = sn;
                }
            }
        }

        return block;
    }

    uint32_t BlockLocation(uint32_t block_n)
    {
        uint32_t page_n = block_n / kBlocksPerPage;
        block_n -= page_n * kBlocksPerPage;
        return page_n * kPageSize + block_n * kBlockSize;
    }

    int32_t NextWritableBlock(int32_t current_block_n)
    {
        int32_t next_block_n = current_block_n;

        do
        {
            next_block_n = (next_block_n + 1) % kNumBlocks;

            if (nvmem_.Writable(BlockLocation(next_block_n), kBlockSize))
            {
                break;
            }
        }
        while (next_block_n != current_block_n);

        return (next_block_n == current_block_n) ? -1 : next_block_n;
    }
};

}
