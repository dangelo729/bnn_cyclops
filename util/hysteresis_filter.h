#pragma once

#include <algorithm>

namespace recorder
{

class HysteresisFilter
{
public:
    void Init(float min, float max, float threshold, float initial_value = 0)
    {
        out_min_ = min;
        out_max_ = max;
        threshold_ = threshold;
        position_ = initial_value;

        out_range_ = out_max_ - out_min_;
        in_min_ = out_min_ + threshold_;
        in_range_inv_ = 1 / (out_range_ - 2 * threshold_);
    }

    float Process(float input)
    {
        float delta = input - position_;

        if (delta >= threshold_)
        {
            position_ = input - threshold_;
        }
        else if (delta <= -threshold_)
        {
            position_ = input + threshold_;
        }

        // Rescale to full range
        float x = position_;
        x = out_min_ + out_range_ * (x - in_min_) * in_range_inv_;
        return std::clamp(x, out_min_, out_max_);
    }

protected:
    float out_min_;
    float out_max_;
    float threshold_;
    float position_;

    float out_range_;
    float in_min_;
    float in_range_inv_;
};

}
