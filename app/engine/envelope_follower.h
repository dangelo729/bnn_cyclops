#pragma once

#include <cstdint>
#include <cmath>

namespace recorder
{

class EnvelopeFollower
{
public:
    void Init(float attack_ms, float decay_ms, float hold_ms, float sample_rate)
    {
        attack_rate_ = 1 - std::exp(-1000 / (attack_ms * sample_rate));
        decay_rate_ = 1 - std::exp(-1000 / (decay_ms * sample_rate));
        hold_samples_ = std::round(hold_ms * sample_rate / 1000);
        Reset();
    }

    void Reset(void)
    {
        hold_count_ = 0;
        envelope_ = 0;
    }

    float Process(float in)
    {
        in = std::abs(in);

        if (in >= envelope_)
        {
            envelope_ += attack_rate_ * (in - envelope_);
            hold_count_ = 0;
        }
        else if (hold_count_ < hold_samples_)
        {
            hold_count_++;
        }
        else
        {
            envelope_ += decay_rate_ * (in - envelope_);
        }

        return envelope_;
    }

    float level(void)
    {
        return envelope_;
    }

protected:
    float attack_rate_;
    float decay_rate_;
    uint32_t hold_samples_;
    uint32_t hold_count_;
    float envelope_;
};

}
