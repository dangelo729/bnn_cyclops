#pragma once
#include <cmath>

namespace recorder
{

enum BiquadType {
    LOWPASS,
    HIGHPASS,
    BANDPASS,
    NOTCH,
    PEAK,
    LOWSHELF,
    HIGHSHELF
};

class FormantBiquad
{
public:
    void Init(BiquadType type, float sampleRate, float centerFrequency, float Q, float gainDB = 0.0f)
    {
        type_ = type;
        sampleRate_ = sampleRate;
        centerFrequency_ = centerFrequency;
        Q_ = Q;
        gain_ = std::pow(10.0f, gainDB / 40.0f); // For peak, lowshelf, highshelf
        x1_ = x2_ = y1_ = y2_ = 0.0f;
        UpdateFilter();
    }

    void SetParameters(float centerFrequency, float Q, float gainDB = 0.0f)
    {
        centerFrequency_ = centerFrequency;
        Q_ = Q;
        gain_ = std::pow(10.0f, gainDB / 40.0f); // For peak, lowshelf, highshelf
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
        float omega = 2.0f * M_PI * centerFrequency_ / sampleRate_;
        float sin_omega = sin(omega);
        float cos_omega = cos(omega);
        float alpha = sin_omega / (2.0f * Q_);
        float A = gain_;

        switch (type_)
        {
            case BANDPASS:
            {
                b0_ = alpha;
                b1_ = 0.0f;
                b2_ = -alpha;
                a0_ = 1.0f + alpha;
                a1_ = -2.0f * cos_omega;
                a2_ = 1.0f - alpha;
            }
            break;

            // You can add other filter types here if needed

            default:
                // Default to BANDPASS if type is not recognized
                b0_ = alpha;
                b1_ = 0.0f;
                b2_ = -alpha;
                a0_ = 1.0f + alpha;
                a1_ = -2.0f * cos_omega;
                a2_ = 1.0f - alpha;
                break;
        }

        // Normalize coefficients
        b0_ /= a0_;
        b1_ /= a0_;
        b2_ /= a0_;
        a1_ /= a0_;
        a2_ /= a0_;
    }

    BiquadType type_;
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
