#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "common/config.h"
#include "app/engine/compressor.h"
#include "app/engine/envelope_follower.h"
#include "app/engine/one_pole.h"

namespace recorder
{

class DelayEngine
{
public:
    void Init(void)
    {
        float threshold_dB = 1;
        float ratio = 1.05;
        float softness = 1.0;
        float attack_ms = 5;
        float decay_ms = 250;
        float hold_ms = 100;
        compressor_.Init(threshold_dB, ratio, softness,
            attack_ms, decay_ms, hold_ms, kAudioSampleRate);

        attack_ms = 10;
        decay_ms = kMinDelay * 1000;
        hold_ms = kMaxDelay * 1000;
        follower_.Init(attack_ms, decay_ms, hold_ms, kAudioSampleRate);

        delay_time_lpf_.Init(10, kAudioSampleRate);

        Reset();
    }

    void Reset(void)
    {
        for (uint32_t i = 0; i < kBufferSize; i++)
        {
            buffer_[i] = 0;
        }

        write_head_ = 0;
        compressor_.Reset();
        follower_.Reset();
        delay_time_lpf_.Reset();
        interpolator_history_ = 0;
    }

    float Process(float input, float delay, float feedback)
    {
        delay *= delay;
        delay = delay_time_lpf_.Process(delay);
        float time = kMinDelay + delay * (kMaxDelay - kMinDelay);
        time = std::clamp<float>(time, kMinDelay, kMaxDelay);
        float delay_samples = time * kAudioSampleRate;

        uint32_t i_a = ReadIndex(static_cast<uint32_t>(delay_samples));
        uint32_t i_b = ReadIndex(static_cast<uint32_t>(delay_samples + 1));
        float frac = delay_samples - static_cast<uint32_t>(delay_samples);
        float output = AllpassInterpolator(buffer_[i_a], buffer_[i_b], frac);

        feedback = kMaxFeedback * std::clamp<float>(feedback, 0, 1);
        output = std::clamp<float>(input + output * feedback, -2, 2);
        buffer_[write_head_] = compressor_.Process(output);
        write_head_ = (write_head_ + 1) % kBufferSize;

        output *= 0.5;
        follower_.Process(output);
        return output;
    }

    bool audible(void)
    {
        return follower_.level() > kTrailThreshold;
    }

protected:
    static constexpr float kMinDelay = 0.1;
    static constexpr float kMaxDelay = 1.0;
    static constexpr float kMaxFeedback = 1.0;
    static constexpr float kTrailThreshold = std::pow(10.0, -60.0 / 20.0);

    static constexpr uint32_t kBufferSize = std::round(std::exp2(std::ceil(
        std::log2(kMaxDelay * kAudioSampleRate + 1))));
    float buffer_[kBufferSize];
    uint32_t write_head_;
    Compressor compressor_;
    EnvelopeFollower follower_;
    OnePoleLowpass delay_time_lpf_;
    float interpolator_history_;

    uint32_t ReadIndex(uint32_t offset)
    {
        return (write_head_ + kBufferSize - offset) % kBufferSize;
    }

    float AllpassInterpolator(float a, float b, float t)
    {
        if (t == 0)
        {
            interpolator_history_ = a;
        }
        else
        {
            interpolator_history_ = (1 - t) * (a - interpolator_history_) + b;
        }

        return interpolator_history_;
    }
};

}
