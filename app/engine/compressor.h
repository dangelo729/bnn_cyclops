#pragma once

#include <cmath>
#include <algorithm>

#include "app/engine/envelope_follower.h"

namespace recorder
{

class Compressor
{
public:
    void Init(float threshold_dB, float ratio, float softness,
        float attack_ms, float decay_ms, float hold_ms, float sample_rate)
    {
        pregain_ = std::pow(10.0, -threshold_dB / 20);
        ratio_ = 1 / ratio - 1;
        softness_ = softness;
        t_scaler_ = 0.5 / softness;
        follower_.Init(attack_ms, decay_ms, hold_ms, sample_rate);
    }

    void Reset(void)
    {
        follower_.Reset();
    }

    float Process(float in)
    {
        float envelope = follower_.Process(in * pregain_);
        float sense = 20 * std::log10(envelope);

        float gain = Compression(sense);
        // TODO: can probably use a rougher exponential approximation since gain
        // is usually small
        return in * std::pow(10.0, gain / 20);
    }

protected:
    float pregain_;
    float ratio_;
    float softness_;
    float t_scaler_;
    EnvelopeFollower follower_;

    float Compression(float db)
    {
        // We use a cubic hermite spline to form a soft knee. The knee region
        // is the interval (-softness, softness) dB. To the left of this
        // region, gain = 0; to the right, gain = ratio * db. Since value and
        // slope are both 0 at the left of the region, we only need two terms
        // to calculate the spline.
        // First we map the interval (-softness, softness) onto t (0, 1).
        // Then we calculate the hermite terms:
        // h01 = -2t^3 + 3t^2
        // h11 = t^3 - t^2
        // We multiply h01 by A = ratio * softness to provide the step from 0
        // to A. We multiply h11 by 2A to make the ending slope equal to ratio.
        // Then, the sum of the terms reduces to A*t^2.

        if (db > softness_)
        {
            return ratio_ * db;
        }
        else
        {
            float t = std::max<float>(db * t_scaler_ + 0.5, 0);
            return ratio_ * softness_ * t * t;
        }
    }
};

}
