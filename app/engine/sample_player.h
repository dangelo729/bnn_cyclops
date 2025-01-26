#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <chrono>
#include "drivers/system.h"

namespace recorder
{

template <typename T>
class SamplePlayer
{
public:
    SamplePlayer(T& memory) : memory_{memory} {}

    void Init(void)
    {
        position_ = 0;
        fade_out_ = 0;
        Reset();
    }

    void Reset(void)
    {
        state_ = STATE_STOPPED;
    }

    void Play(void)
    {
        if (state_ == STATE_STOPPED)
        {
            state_ = STATE_STARTING;
        }
    }

    void Stop(void)
    {
        if (state_ == STATE_PLAYING)
        {
            state_ = STATE_STOPPING;
            fade_out_ = 0;
        }
    }

    bool ended(void)
    {
        return state_ == STATE_STOPPED;
    }

    void Scrub(float pot)
    {
 
        uint32_t length = memory_.length();
        uint32_t target = pot * length;

        // Calculate the difference between the target and current position.
        float diff = target - position_;

        // Calculate the rate of change based on the difference.
        // Here is a magic number determining how quickly position catches up to target.
        // You should adjust this value to fit your specific needs.
        float rate = 0.0005 * diff;

        // Update the position with the rate.
        position_ += rate;

        // Here, we make sure that the position does not exceed the bounds of the audio length.
        position_ = std::clamp(position_, 0.0f, static_cast<float>(length - 1));
        if(state_ != STATE_SCRUBBING){
            state_ = STATE_SCRUBBING;
         
        }
        

    }
    void StopScrub(void)
    {
        if (state_ == STATE_SCRUBBING)
        {
       
            state_ = STATE_PLAYING;
            
        }
    }

    void SetSpeedMultiplier(float current_knob_value)
     {
         // Calculate the speed multiplier from the rate of change of the knob value.
         float knob_delta = current_knob_value - previous_knob_value_;

         // Map the delta to a suitable speed multiplier.
         // If the knob is turned faster, knob_delta will be larger and speed_multiplier_ will be further from 1.
         // We can scale the delta by some factor to control the sensitivity of the speed change.
         speed_multiplier_target_ = 1.0f + knob_delta * sensitivity_factor;
         float diff =speed_multiplier_target_ - speed_multiplier_;
         float rate = 0.001 * diff;
         speed_multiplier_ += rate;
         // Update the previous knob value for the next call.
         previous_knob_value_ = current_knob_value;
     }
    
    float Process(float speed, bool loop, bool reverse)
    {
        uint32_t length = memory_.length();
        float sample = 0;
        float prev_position = position_;

        
        if (state_ == STATE_STARTING)
        {
            position_ = reverse ? length - 1 : 0;
            state_ = STATE_PLAYING;
        }

        if (state_ != STATE_STOPPED)
        {
            uint32_t index_a = position_;
            uint32_t index_b = index_a + 1;
            float sample_a = memory_[index_a];
            float sample_b = (index_b < length) ? memory_[index_b] : 0;

            float frac = position_ - index_a;
            sample = std::lerp(sample_a, sample_b, frac);

            bool fade_in = (position_ < kFadeDuration);
            bool fade_out = (length - 1 - position_ < kFadeDuration);
    
            if (fade_in)
            {
                sample *= FadeCurve(position_ / kFadeDuration);
            }

            if (fade_out)
            {
                sample *= FadeCurve((length - 1 - position_) / kFadeDuration);
            }
            if(state_ != STATE_SCRUBBING){
                position_ += (reverse ? -speed : speed) * speed_multiplier_;
            }
            

            if (position_ >= length)
            {
                if (!loop)
                {
                    state_ = STATE_STOPPED;
                }
                else
                {
                    position_ = 0;
                }
            }
            else if (position_ < 0)
            {
                if (!loop)
                {
                    state_ = STATE_STOPPED;
                }
                else
                {
                    position_ = length - 1;
                }
            }

            if (state_ == STATE_STOPPING)
            {
                fade_out_ += 1 / kFadeDuration;

                if (fade_out_ >= 1)
                {
                    fade_out_ = 1;
                    state_ = STATE_STOPPED;
                }

                sample *= FadeCurve(fade_out_);
            }
        }
    
        return sample;
    }

protected:
    static constexpr float kPi = 3.141592653589793;
    static constexpr float kFadeDuration = kAudioSampleRate * kAudioFadeTime;
    std::chrono::time_point<std::chrono::steady_clock> potTime;

    enum State
    {
        STATE_STOPPED,
        STATE_STOPPING,
        STATE_STARTING,
        STATE_PLAYING,
        STATE_SCRUBBING,
    };

    T& memory_;
    float position_;
    State state_;
    float speed_multiplier_ = 1.0;
    float speed_multiplier_target_= 1.0;
    float fade_out_;
    float previous_knob_value_{0.0f};
  
    static constexpr float sensitivity_factor = 180.0f;

    float FadeCurve(float tau)
    {
        tau = std::clamp<float>(tau, 0, 1);
        return 0.5 * (1 - std::cos(kPi * tau));
    }

};

}
