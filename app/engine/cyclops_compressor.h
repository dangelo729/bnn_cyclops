#pragma once

#include <cmath>
#include <algorithm>

namespace recorder
{

class CyclopsCompressor
{
public:
    void Init(float threshold, float ratio, float attack_time, float release_time, float sample_rate)
    {
        threshold_ = threshold;
        ratio_ = ratio;
        sample_rate_ = sample_rate;
        attack_coeff_ = std::exp(-1.0f / (attack_time * sample_rate_));
        release_coeff_ = std::exp(-1.0f / (release_time * sample_rate_));
        envelope_ = 0.0f;
    }

    void Reset()
    {
        envelope_ = 0.0f;
    }

    float Process(float input)
    {
        // Calculate the envelope of the input signal
        float rectified = std::fabs(input);
        if (rectified > envelope_)
        {
            envelope_ = attack_coeff_ * (envelope_ - rectified) + rectified;
        }
        else
        {
            envelope_ = release_coeff_ * (envelope_ - rectified) + rectified;
        }

        // Calculate gain reduction
        float gain = 1.0f;
        if (envelope_ > threshold_)
        {
            float over_threshold = envelope_ - threshold_;
            float compressed = threshold_ + (over_threshold / ratio_);
            gain = compressed / envelope_;
        }

        // Apply gain reduction to the input signal
        return input * gain;
    }

private:
    float threshold_;
    float ratio_;
    float attack_coeff_;
    float release_coeff_;
    float sample_rate_;
    float envelope_;
};

}
