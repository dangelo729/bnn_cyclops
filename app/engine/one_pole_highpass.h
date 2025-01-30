#pragma once

#include <cmath>
#include <limits>

namespace recorder
{

    class OnePoleHighpass
    {
    public:
        // Initialize the high-pass filter
        void Init(float cutoff, float sample_rate, float initial_value = 0.0f)
        {
            float omega = 2.0f * M_PI * cutoff / sample_rate;
            factor_ = omega / (1.0f + omega); // Proper bilinear transform coefficient
            Reset(initial_value);
        }

        // Reset the filter state
        void Reset(float initial_value = 0.0f)
        {
            history_ = initial_value; // Last output sample (y[n-1])
            lowpassHistory_ = initial_value; // Tracks DC content
        }

        // Process one sample through the high-pass filter
        float Process(float input)
        {
            // Track the low frequencies (DC removal)
            lowpassHistory_ += factor_ * (input - lowpassHistory_);
            
            // Subtract the low frequencies from the original signal
            float output = input - lowpassHistory_;

            history_ = output;
            return output;
        }

        // Return the most recent output sample
        float output() const
        {
            return history_;
        }

    protected:
        float factor_;         // Filter coefficient
        float history_;        // Last output sample (y[n-1])
        float lowpassHistory_; // Internal lowpass state for DC removal
    };

} // namespace recorder
