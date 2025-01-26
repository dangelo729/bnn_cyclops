#pragma once

#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <algorithm>

#include "drivers/system.h"
#include "drivers/flash.h"
#include "drivers/crc.h"
#include "drivers/save_data.h"
#include "common/config.h"
#include "util/buffer_chain.h"

namespace recorder
{

class SampleMemoryBase
{
protected:
    static constexpr uint32_t kBuffer1Size = 512 * 1024;
    static constexpr uint32_t kBuffer2Size = 288 * 1024;
    static constexpr uint32_t kBuffer3Size =  63 * 1024;

    __attribute__ ((section (".sram1")))
    static inline uint8_t buffer1_[kBuffer1Size];

    __attribute__ ((section (".sram2")))
    static inline uint8_t buffer2_[kBuffer2Size];

    __attribute__ ((section (".sram3")))
    static inline uint8_t buffer3_[kBuffer3Size];
};

template <typename T>
class SampleMemory : SampleMemoryBase
{
public:
    void Init(void)
    {
   
        dirty_ = false;
        buffer_index_ = 0;
        flash_.Init();
        crc_.Init();
        buffer_chain_.Init(link_info_);

        if (save_.Init(audio_info_))
        {
            printf("Save data found:\n");
            PrintInfo("    ");

            if (audio_info_.address < kAudioBufferAddress)
            {
                printf("Invalid address\n");
                audio_info_.address = kAudioBufferAddress;
                audio_info_.size = 0;
            }
            else
            {
                printf("Loading audio... ");
                crc_.Seed(0);
                uint32_t total_size = audio_info_.size;
                uint32_t address = audio_info_.address;

                for (auto link : buffer_chain_)
                {
                    uint32_t read_size = std::min(link.size(), total_size);
                    system::ReloadWatchdog();
                    flash_.Read(link.buffer, address, read_size);
                    system::ReloadWatchdog();
                    crc_.Process(link.buffer, read_size);
                    total_size -= read_size;
                    address += read_size;

                    if (total_size == 0)
                    {
                        break;
                    }

                }

                if (audio_info_.crc32 == crc_.value())
                {
                    printf("done\n");
                }
                else
                {
                    printf("invalid CRC32: 0x%08" PRIX32 "\n", crc_.value());
                    audio_info_.size = 0;
                }
            }
        }
        else
        {
            printf("No save data found\n");
            audio_info_.address = kAudioBufferAddress;
            audio_info_.size = 0;
        }
       
    }

    void StartRecording(void)
    {
        buffer_index_ = 0;
    }

    void StartPlayback(void)
    {
        buffer_index_ = 0;
    }

    const T& operator[](size_t index)
    {
        return buffer_chain_[index];
    }

    uint32_t length(void)
    {
        return audio_info_.size / sizeof(T);
    }

    void Append(T item)
    {
        if (buffer_index_ < buffer_chain_.length())
        {
            buffer_chain_[buffer_index_++] = item;
        }
    }

    void StopRecording(void)
    {
        uint32_t min_length =
            kAudioSampleRate * kButtonDebounceDuration_ms / 1000;

        if (buffer_index_ > min_length)
        {
            // Trim the end of the recording to remove the sound of the
            // button being released.
            buffer_index_ -= min_length;

            uint32_t address = audio_info_.address + audio_info_.size;
            uint32_t size = buffer_index_ * sizeof(T);

            uint32_t granularity = Flash::kEraseGranularity;
            address += granularity - 1;
            address -= (address % granularity);

            if (address + size > Flash::kSize)
            {
                address = kAudioBufferAddress;
            }

            crc_.Seed(0);
            uint32_t total_size = size;

            for (auto link : buffer_chain_)
            {
                system::ReloadWatchdog();
                uint32_t chunk_size = std::min(link.size(), total_size);
                crc_.Process(link.buffer, chunk_size);
                total_size -= chunk_size;

                if (total_size == 0)
                {
                    break;
                }
            }

            audio_info_ =
            {
                .address = address,
                .size    = size,
                .crc32   = crc_.value(),
            };

            dirty_ = true;
        }
    }
    T Read(size_t index)
    {
        return buffer_chain_[index];
    }
    bool dirty(void)
    {
        return dirty_ && audio_info_.size > 0;
    }

    bool BeginErase(void)
    {
        uint32_t granularity = Flash::kEraseGranularity;
        uint32_t erase_size = audio_info_.size + granularity - 1;
        erase_size -= (erase_size % granularity);
        return flash_.BeginErase(audio_info_.address, erase_size);
    }
    void Overwrite(T item, size_t index)
    {
        if (index < buffer_chain_.length())
        {
            buffer_chain_[index] = item;
        }
    }
    bool FinishErase(void)
    {
        if (flash_.FinishErase())
        {
            chain_iter_ = buffer_chain_.begin();
            return true;
        }
        else
        {
            return false;
        }
    }

    void AbortErase(void)
    {
        flash_.AbortErase();
    }

    bool write_complete(void)
    {
        bool end = (chain_iter_ == buffer_chain_.end());
        bool done = (audio_info_.size <= (*chain_iter_).offset);
        return end || done;
    }

    bool BeginWrite(void)
    {
        auto& link = *chain_iter_;
        uint32_t address = audio_info_.address + link.offset;
        uint32_t remaining = audio_info_.size - link.offset;
        uint32_t write_size = std::min(link.size(), remaining);
        return flash_.BeginWrite(address, link.buffer, write_size);
    }

    bool FinishWrite(void)
    {
        if (flash_.FinishWrite())
        {
            chain_iter_++;
            return true;
        }
        else
        {
            return false;
        }
    }

    void AbortWrite(void)
    {
        flash_.AbortWrite();
    }

    bool Commit(void)
    {
        return save_.Save(audio_info_);
    }

    void PrintInfo(const char* line_prefix)
    {
        printf("%sAddress: 0x%08" PRIX32 "\n", line_prefix, audio_info_.address);
        printf("%sSize:    0x%08" PRIX32 "\n", line_prefix, audio_info_.size);
        printf("%sCRC32:   0x%08" PRIX32 "\n", line_prefix, audio_info_.crc32);
    }

    void PowerDown(void)
    {
        flash_.PowerDown();
    }

    void Erase(void)
    {
        save_.Erase();
    }

protected:
    Flash flash_;
    Crc crc_;
    bool dirty_;
    uint32_t buffer_index_;

    struct AudioInfo
    {
        uint32_t address;
        uint32_t size;
        uint32_t crc32;
    };

    AudioInfo audio_info_;
    static constexpr uint32_t kSaveDataRegionSize = Flash::kEraseGranularity * 2;
    SaveData<Flash, AudioInfo, kSaveDataRegionSize> save_{flash_};

    static constexpr uint32_t kAudioBufferAddress = kSaveDataRegionSize;
    BufferChain<T> buffer_chain_;
    BufferChain<T>::iter chain_iter_;
    static inline BufferChain<T>::Link link_info_[] =
    {
        {reinterpret_cast<T*>(buffer1_), kBuffer1Size / sizeof(T), 0},
        {reinterpret_cast<T*>(buffer2_), kBuffer2Size / sizeof(T), 0},
        {reinterpret_cast<T*>(buffer3_), kBuffer3Size / sizeof(T), 0},
    };
};

}
