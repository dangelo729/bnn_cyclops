#pragma once

#include <cmath>
#include <limits>

namespace recorder
{

    class OnePoleLowpass
    {
    public:
        void Init(float cutoff, float sample_rate, float initial_value = 0)
        {
            float omega = 2.0f * M_PI * cutoff / sample_rate;
            factor_ = omega / (1.0f + omega);
            Reset(initial_value);
        }

        void Reset(float initial_value = 0)
        {
            history_ = initial_value;
        }

        float Process(float input)
        {
            history_ += factor_ * (input - history_);
            return history_;
        }

        float output(void)
        {
            return history_;
        }

    protected:
        float factor_;
        float history_;
    };

}
