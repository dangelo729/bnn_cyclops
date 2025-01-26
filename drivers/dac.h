#pragma once

#include <algorithm>

#include "common/config.h"
#include "common/io.h"

namespace recorder
{

class Dac
{
public:
    void Init(void);

    void Process(const AudioOutput& audio)
    {
        for (uint32_t i = 0; i < kAudioOSFactor; i++)
        {
            float sample = audio[AUDIO_OUT_LINE][i];
            sample = std::clamp<float>(0.5 * (sample + 1), 0, 1);
            uint32_t code = 0.5 + 0xFFF * sample;
            dma_buffer_[write_index_] = code;
            write_index_ = (write_index_ + 1) % kDMABufferSize;
        }
    }

    void Start(void);
    void Stop(void);

protected:
    void InitGPIO(void);
    void InitDAC(void);
    void InitDMA(void);
    void Reset(void);

    static constexpr uint32_t kDMABufferSize = kAudioOSFactor * 2;
    __attribute__ ((section (".dma")))
    static inline uint32_t dma_buffer_[kDMABufferSize];
    uint32_t write_index_;
    bool started_;

    static void DMAHandler(void);
};

}
