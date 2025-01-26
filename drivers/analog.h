#pragma once

#include "drivers/gpio.h"
#include "drivers/adc.h"
#include "drivers/dac.h"

#include "common/io.h"
#include "common/config.h"

namespace recorder
{

class Analog
{
public:
    using Callback =
        const AudioOutput (*)(const AudioInput&, const PotInput& pot);

    void Init(Callback callback);

    void StartRecording(void)
    {
        Start(false);
    }

    void StartPlayback(void)
    {
        Start(true);
    }

    bool running(void)
    {
        return state_ == STATE_RUNNING;
    }

    bool stopped(void)
    {
        return state_ == STATE_STOPPED;
    }

    void Start(bool enable_amplifier)
    {
        if (state_ == STATE_STOPPED)
        {
            state_ = STATE_STARTING;
            fade_position_ = 0;
            cue_stop_ = false;

            boost_enable_.Set();
            amp_enable_.Write(enable_amplifier);

            dac_.Start();
            adc_.Start();
            StartTimer();
        }
    }

    void Stop(void)
    {
        if (state_ == STATE_RUNNING && !kADCAlwaysOn)
        {
            cue_stop_ = true;
        }
    }

protected:
    static inline Analog* instance_;
    static inline Callback callback_;
    OutputPin<GPIOB_BASE, 1> adc_enable_;
    OutputPin<GPIOG_BASE, 9> boost_enable_;
    OutputPin<GPIOB_BASE, 12> amp_enable_;
    Adc adc_;
    Dac dac_;

    enum State
    {
        STATE_STOPPED,
        STATE_STARTING,
        STATE_RUNNING,
        STATE_STOPPING,
    };

    static constexpr float kPi = 3.141592653589793;
    static constexpr float kFadeDuration = kAudioOSRate * 50e-3;
    float fade_position_;
    State state_;
    bool cue_stop_;

    void InitTimer(void);
    void StartTimer(void);
    void StopTimer(void);
    static void TimerHandler(void);

    static inline
    void AdcCallback(const AudioInput& in, const PotInput& pot)
    {
        instance_->Service(in, pot);
    }

    float FadeCurve(float tau)
    {
        tau = std::clamp<float>(tau, 0, 1);
        return 0.5 * (1 - std::cos(kPi * tau)) - 1;
    }

    void Service(const AudioInput& in, const PotInput& pot)
    {
        AudioOutput out;

        if (state_ == STATE_STARTING)
        {
            for (uint32_t i = 0; i < kAudioOSFactor; i++)
            {
                fade_position_ += 1 / kFadeDuration;

                for (uint32_t ch = 0; ch < NUM_AUDIO_OUTS; ch++)
                {
                    out[ch][i] = FadeCurve(fade_position_);
                }
            }

            if (fade_position_ >= 1)
            {
                state_ = STATE_RUNNING;
            }
        }
        else if (state_ == STATE_RUNNING)
        {
            out = callback_(in, pot);

            if (cue_stop_)
            {
                state_ = STATE_STOPPING;
                fade_position_ = 1;

                // Disable the amplifier at the start of the soft-off curve
                // instead of the end, otherwise the speaker will pop.
                amp_enable_.Clear();
            }
        }
        else if (state_ == STATE_STOPPING)
        {
            for (uint32_t i = 0; i < kAudioOSFactor; i++)
            {
                fade_position_ -= 1 / kFadeDuration;

                for (uint32_t ch = 0; ch < NUM_AUDIO_OUTS; ch++)
                {
                    out[ch][i] = FadeCurve(fade_position_);
                }
            }

            if (fade_position_ <= 0)
            {
                state_ = STATE_STOPPED;
                StopTimer();
                adc_.Stop();
                dac_.Stop();
                amp_enable_.Clear();
                boost_enable_.Clear();
            }
        }

        dac_.Process(out);
    }
};

}
