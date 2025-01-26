#pragma once
#include <cmath>
#include "app/engine/biquad.h"
namespace recorder
{

class ThreeBandEq
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void SetBandParameters(int band, float centerFrequency, float Q, float gainDB)
    {
        filters_[band].Init(sampleRate_, centerFrequency, Q, gainDB);
    }

    float Process(float input)
    {
        float y0 = 0.0;
        for (int i = 0; i < 3; i++)
        {
            y0 += filters_[i].Process(input);
        }
        return y0;
    }

private:
    biquad filters_[3];
    float sampleRate_;
};

}
