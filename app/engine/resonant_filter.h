#pragma once
#include <cmath>

namespace recorder
{

class ResonantFilter
{
public:
    void Init(float sampleRate, float cutoffFrequency, float Q)
    {
        sampleRate_ = sampleRate;
        SetCutoffFrequency(cutoffFrequency);
        SetQ(Q);
    }

    void SetCutoffFrequency(float cutoffFrequency)
    {
        cutoffFrequency_ = cutoffFrequency;
        UpdateFilter();
    }

    void SetQ(float Q)
    {
        Q_ = Q;
        UpdateFilter();
    }

    float Process(float input)
    {
        // Direct Form I implementation of the IIR filter
        float y0 = b0_ * input + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

        // Shift the delay line
        x2_ = x1_;
        x1_ = input;
        y2_ = y1_;
        y1_ = y0;

        return y0;
    }

protected:
    void UpdateFilter()
    {
        float omega = 2 * M_PI * cutoffFrequency_ / sampleRate_;
        float alpha = sin(omega) / (2 * Q_);

        a0_ = 1 + alpha;
        a1_ = -2 * cos(omega);
        a2_ = 1 - alpha;
        b0_ = (1 - cos(omega)) / 2;
        b1_ = 1 - cos(omega);
        b2_ = (1 - cos(omega)) / 2;

        // Scaling coefficients for unity gain at DC
        b0_ /= a0_;
        b1_ /= a0_;
        b2_ /= a0_;
        a1_ /= a0_;
        a2_ /= a0_;
    }

    float sampleRate_;
    float cutoffFrequency_;
    float Q_;

    float a0_, a1_, a2_;
    float b0_, b1_, b2_;
    float x1_, x2_;
    float y1_, y2_;
};

}
