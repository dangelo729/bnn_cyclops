#pragma once

#include <cstdint>

#include "common/config.h"
#include "app/engine/sample_player.h"
#include "app/engine/delay_engine.h"
#include "app/engine/aafilter.h"
#include "app/engine/resonant_filter.h"
#include "app/engine/ring_modulator.h"
#include "app/engine/biquad.h"

namespace recorder
{

    template <typename T>
    class PlaybackEngine
    {
    public:
        PlaybackEngine(T &memory) : memory_{memory} {}

        void Init(void)
        {
            sample_player_.Init();
            delay_.Init();
            aa_filter_.Init();
            res_filter_.Init(16000, 700, 10);
            ring_mod_.Init(16000, 400, .7);
            // below is the filter responsible for boosting level in certain freq ranges (vocals, kalimba, etc), currently commented out here and on line 184. Arguments are Init(samplerate, freq, Q, db boost) and SetParameters(freq, Q, dbBoost).
            main_filter_.Init(16000, 900, .5, 10);
            main_filter_.SetParameters(900, .5, 10);
            Reset();
        }

        void Reset(void)
        {
            state_ = STATE_STOPPED;
            cue_play_ = false;
            cue_stop_ = false;
            sample_player_.Reset();
            delay_.Reset();
            aa_filter_.Reset();
        }

        bool playing(void)
        {
            return state_ == STATE_PLAYING;
        }

        bool stopping(void)
        {
            return state_ == STATE_STOPPING;
        }

        bool ended(void)
        {
            return state_ == STATE_STOPPED;
        }

        void Play(void)
        {
            cue_play_ = true;
        }

        void Stop(void)
        {
            cue_stop_ = true;
        }

        void Scrub(float pot)
        {

            if (state_ != STATE_SCRUBBING)
            {
                state_ = STATE_SCRUBBING;
            }
            // Normalize the potentiometerValue between 0 and 1
            pot = std::clamp<float>(pot, 0, 1);
            sample_player_.Scrub(pot);
        }
        void StopScrub(void)
        {
            if (state_ == STATE_SCRUBBING)
            {
                sample_player_.StopScrub();
                state_ = STATE_PLAYING;
            }
        }
        void ScrubLive(float pot)
        {
            //  pot = std::clamp<float>(pot, 0, 1);
            sample_player_.SetSpeedMultiplier(pot);
        }

        void SetCutoffAndQ(float q, float cutoff)
        {
            res_filter_.SetQ(mapFloat(q, 0.0, 1.0, .5, 20));
            res_filter_.SetCutoffFrequency(mapFloat(cutoff, 0.0, 1.0, 30, 8000));
        }
        void SetRingMod(float freq, float mix)
        {
            ring_mod_.SetFrequency(mapFloat(freq, 0.0, 1.0, 100, 3000));
            ring_mod_.SetMix(mix);
        }
        void ringOn(bool ring)
        {
            ringModOn = ring;
        }
        void Process(float (&block)[kAudioOSFactor], bool loop, bool reverse,
                     const PotInput &pot)
        {
            float pitch;
            if (state_ != STATE_SCRUBBING)
            {
                pitch = (1 - pot[kPotPitch]) * 2 - 1; // pot values reversed due to wiring change
            }
            else
            {
                pitch = 1.0;
            }
            float speed = std::exp2(pitch);
            float sample = 0;

            if (state_ == STATE_STOPPED)
            {
                if (cue_play_)
                {
                    sample_player_.Reset();
                    sample_player_.Play();
                    state_ = STATE_PLAYING;
                    cue_play_ = false;
                    cue_stop_ = false;
                }
            }
            else
            {

                if (cue_stop_)
                {
                    state_ = STATE_STOPPING;
                    sample_player_.Stop();
                    cue_stop_ = false;
                }

                sample = sample_player_.Process(speed, loop, reverse);

                if (sample_player_.ended())
                {
                    state_ = STATE_STOPPING;
                }

                bool delay_is_quiet = !(kEnableDelay && delay_.audible());

                if (state_ == STATE_STOPPING)
                {
                    if (sample_player_.ended())
                    {
                        if (cue_play_)
                        {
                            sample_player_.Reset();
                            sample_player_.Play();
                            state_ = STATE_PLAYING;
                            cue_play_ = false;
                        }
                        else if (delay_is_quiet)
                        {
                            state_ = STATE_STOPPED;
                        }
                    }
                }

                if (kEnableDelay)
                {
                    float delay = pot[kPotDelayTime];
                    float feedback = pot[kPotDelayFeedback];
                    sample = delay_.Process(sample, delay, feedback);
                }
            }
            // sample = res_filter_.Process(sample);
            if (ringModOn)
            {
                // ring mod processing
                // sample = ring_mod_.Process(sample);
            }

            // sample = main_filter_.Process(sample); //main filter, used to boost output in certain frequency ranges for different units (vocal, kalimba, etc.)
            sample *= kAudioOSFactor * kAudioOutputLevel;

            for (uint32_t i = 0; i < kAudioOSFactor; i++)
            {
                sample = aa_filter_.Process((i == 0) ? sample : 0);
                block[i] = sample;
            }
        }

    protected:
        float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
        {
            return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
        }
        enum State
        {
            STATE_STOPPED,
            STATE_STOPPING,
            STATE_PLAYING,
            STATE_SCRUBBING,
        };

        T &memory_;
        SamplePlayer<T> sample_player_{memory_};
        State state_;
        bool cue_play_;
        bool cue_stop_;
        DelayEngine delay_;
        ResonantFilter res_filter_;
        RingModulator ring_mod_;
        Biquad main_filter_;
        AAFilter<float> aa_filter_;
        bool ringModOn = false;
        static constexpr PotID kPotPitch = POT_1;
        static constexpr PotID kPotDelayTime = POT_2;
        static constexpr PotID kPotDelayFeedback = POT_3;
    };

}
