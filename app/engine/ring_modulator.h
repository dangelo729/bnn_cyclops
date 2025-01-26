#pragma once
#include <cmath>
#include "app/engine/envelope_follower.h"
namespace recorder
{

class RingModulator
{
public:
    void Init(float sampleRate, float frequency, float mix)
    {
        sampleRate_ = sampleRate;
        phase_ = 0.0;
        twoPiOverSampleRate_ = 2.0 * M_PI / sampleRate_;
        SetFrequency(frequency);
        SetMix(mix);
        
        envFollower_.Init(50, 200, 500, 16000);
    }

    void SetFrequency(float frequency)
    {
        frequency_ = frequency;
        phaseIncrement_ = frequency_ * twoPiOverSampleRate_;
    }

    void SetMix(float mix)
    {
        mix_ = mix;
    }

    float Process(float input)
    {
        float oscillatorOutput = sin(phase_);

        // Update oscillator phase with envelope follower
        float envelope = envFollower_.Process(input);
        phase_ += envelope * phaseIncrement_;

        // Output is a mix of the dry signal and the modulated signal
        float output = (1.0 - mix_) * input + mix_ * (input * oscillatorOutput);

        // Keep phase within 0 to 2*pi range
        while (phase_ > 2.0 * M_PI)
        {
            phase_ -= 2.0 * M_PI;
        }

        return output;
    }

protected:
    float sampleRate_;
    float frequency_;
    float mix_;
    float phase_;
    float phaseIncrement_;
    float twoPiOverSampleRate_;
    EnvelopeFollower envFollower_;
};

}
