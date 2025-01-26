#pragma once

#include <cstdint>
#include <cmath>
#include <limits>

namespace recorder
{

class Interpolator
{
public:
    void Init(uint32_t period)
    {
        step_ = 1.f / period;
        Reset();
    }

    void Reset(void)
    {
        history_ = 0;
        increment_ = 0;
    }

    void Sample(float sample)
    {
        increment_ = (sample - history_) * step_;

        if (std::fabs(increment_) <= std::numeric_limits<float>::epsilon())
        {
            history_ = sample;
            increment_ = 0.f;
        }
    }

    float Next(void)
    {
        history_ += increment_;
        return history_;
    }

protected:
    float history_;
    float step_;
    float increment_;
};

}
