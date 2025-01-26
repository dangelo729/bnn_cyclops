#pragma once
#include <cmath>

namespace recorder
{

class Biquad
{
public:
    void Init(float sampleRate, float centerFrequency, float Q, float gainDB)
    {
        sampleRate_ = sampleRate;
        SetParameters(centerFrequency, Q, gainDB);
    }

    void SetParameters(float centerFrequency, float Q, float gainDB)
    {
        centerFrequency_ = centerFrequency;
        Q_ = Q;
        gain_ = std::pow(10, gainDB / 20.0); // Convert gain from dB to linear scale

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
        float omega = 2 * M_PI * centerFrequency_ / sampleRate_;
        float alpha = sin(omega) / (2 * Q_);
        float A = gain_;

        b0_ = 1 + alpha * A;
        b1_ = -2 * cos(omega);
        b2_ = 1 - alpha * A;
        a0_ = 1 + alpha / A;
        a1_ = -2 * cos(omega);
        a2_ = 1 - alpha / A;

        // Scaling coefficients for unity gain at the center frequency
        b0_ /= a0_;
        b1_ /= a0_;
        b2_ /= a0_;
        a1_ /= a0_;
        a2_ /= a0_;
    }

    float sampleRate_;
    float centerFrequency_;
    float Q_;
    float gain_;

    float a0_, a1_, a2_;
    float b0_, b1_, b2_;
    float x1_, x2_;
    float y1_, y2_;
};

}
