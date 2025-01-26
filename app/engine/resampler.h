#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "util/fifo.h"

namespace recorder
{

// Ratio is output sampling rate divided by input sampling rate
template <uint32_t max_ratio>
class Resampler
{
public:
    void Init(void)
    {
        output_.Init();
    }

    void Reset(void)
    {
        output_.Flush();
        input_phase_ = 1;
        history_ = 0;
    }

    void Push(float sample, float ratio)
    {
        float speed = 1 / ratio;

        while (input_phase_ <= 1)
        {
            if (!output_.full())
            {
                output_.Push(std::lerp(history_, sample, input_phase_));
            }

            input_phase_ += speed;
        }

        input_phase_ -= 1;
        history_ = sample;
    }

    bool Pop(float& item)
    {
        return output_.Pop(item);
    }

protected:
    static constexpr uint32_t kFifoSize = std::round(std::exp2(std::ceil(
        std::log2(max_ratio + 1))));

    Fifo<float, kFifoSize> output_;
    float input_phase_;
    float history_;
};

}
