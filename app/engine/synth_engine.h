#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib> // For std::rand() and RAND_MAX
#include <ctime>   // For time()

#include "common/config.h"
#include "app/engine/aafilter.h"
#include "app/engine/formant_filter.h"
#include "app/engine/one_pole.h"
#include "app/engine/pulse_generator.h"
#include "app/engine/delay_engine.h"
#include "app/engine/vibrato.h"

namespace recorder
{

    class SynthEngine
    {
    public:
        SynthEngine()
            : phase_(0.0f),
              currentFrequency_(130.81f), // Start at C3
              fundamentalFreq_(130.81f),  // Our new fundamental, default to C3
              targetFrequencyOffset_(0.0f),
              frequencyMargin_(0.05f),
              offsetCounter_(0),
              was_button_pressed_(false),
              was_freq_select_button_pressed_(false),
              vowel_set_(false),
              previousTargetIndex_(-1),
              adsr_state_(ADSRState::kIdle),
              adsr_value_(0.0f),
              freq_rate_(0.001f),
              freq_wobbliness_(0.0f),
              previous_formant_pot_val_(0.0f) // track the last pot value
        {
        }

        void Init()
        {
            // Seed random generator once at initialization
            std::srand(static_cast<unsigned>(std::time(nullptr)));

            // Initial parameters
            is_note_on_ = false;
            phase_ = 0.0f;
            currentFrequency_ = 130.81f; // Start at C3
            fundamentalFreq_ = 130.81f;  // Default fundamental is also C3
            targetFrequencyOffset_ = 0.0f;
            offsetCounter_ = 0;
            previousTargetIndex_ = -1;
            delay_.Init();

            // Set a local sample rate variable
            sample_rate_ = 16000.0f;

            // Initialize filters
            aa_filter_.Init();
            aa_filter_.Reset();

            // Example delay parameters
            delay_time_ = 0.1f;
            delay_feedback_ = 0.1f;

            // Initialize formant filter
            formant_filter_.Init(sample_rate_);
            freq_mult_ = 1.0f;
            formant_filter_.SetVoice(FormantFilter::VOICE_NEUTRAL);
            formant_filter_.setQMult(4.0f);
            formant_filter_.setFreqMult(0.75f);
            formant_filter_.SetMode(FormantFilter::FILTER_MODE_NORMAL);
            attack_formant_rate_ = 0.001f;
            lowpass_filter_.Init(20000.0f, sample_rate_, 0.0f);

            // Set up pulse generator
            pulse_generator_.SetBaseDutyCycle(0.01f);
            duty_gain_ = 3.8f;
            freq_wobbliness_ = 0.03f;
            pulse_generator_.SetDutyCycleRandomization(0.0f);

            // ADSR parameters
            adsr_attack_time_ = 0.05f; // seconds
            adsr_decay_time_ = 0.2f;
            adsr_sustain_level_ = 0.8f;
            adsr_release_time_ = 0.1f;

            // Set initial formant
            formant_filter_.SetFormantRate(0.0001f);

            // Initialize and set default vibrato parameters
            vibrato_.Init(sample_rate_);
            // Example: vibrato rate = 5 Hz, depth = 0.12, buildup = 1.8 seconds
            vibrato_.SetParameters(6.0f, 0.12f, 1.8f);
        }

        /**
         * @brief Main audio processing entry point.
         *
         * @param block                 Output buffer for oversampled audio frames
         * @param button_pressed        The "voice" button (for normal note on/off or hold logic)
         * @param pot_value             The main pitch knob (used either for scale or fundamental freq)
         * @param hold                  Whether we are in "hold" mode for the voice button
         * @param formant_pot_val       Some external pot controlling formant/WAH position
         * @param vibrato_pot_val       Pot controlling vibrato depth
         * @param freq_select_button    button for adjusting the fundamental frequency
         */
        void Process(float (&block)[kAudioOSFactor],
                     bool button_pressed,
                     float pot_value,
                     bool hold,
                     float formant_pot_val,
                     float vibrato_pot_val,
                     bool freq_select_button)
        {
            // Only do this "ROBOT TO MONK" mapping if the formant pot value has changed.
            // if (abs(formant_pot_val - previous_formant_pot_val_) > 0.01f)
            // {
            //     RobotToMonk(formant_pot_val);
            // }
            // previous_formant_pot_val_ = formant_pot_val;

            // Update formant/WAH parameters
            formant_filter_.UpdateParameters();
            formant_filter_.SetWahPosition(formant_pot_val);

            // Vibrato depth
            vibrato_.SetDepth(vibrato_pot_val);

            // Map vibrato pot to some delay parameters
          //  delay_feedback_ = mapFloat(vibrato_pot_val, 0.0f, 1.0f, 0.0f, .9f);
          //  delay_time_ = mapFloat(vibrato_pot_val, 0.0f, 1.0f, 0.8f, 0.1f);

            //------------------------------------------------------------------
            //  Handle fundamental-frequency selection button
            //------------------------------------------------------------------
            if (freq_select_button && !was_freq_select_button_pressed_)
            {
                // Just pressed: start envelope so we can hear the fundamental
                StartEnvelope();
            }
            else if (!freq_select_button && was_freq_select_button_pressed_)
            {
                // Just released: stop envelope for that mode
                StopEnvelope();
            }

            //------------------------------------------------------------------
            //  If NOT in freq-select mode => handle normal "voice" button logic
            //------------------------------------------------------------------
            if (!freq_select_button)
            {
                // Handle voice-button transitions (normal operation)
                if (button_pressed && !was_button_pressed_)
                {
                    if (hold)
                    {
                        // Toggle mode: flip the note state
                        is_note_on_ = !is_note_on_;
                        if (is_note_on_)
                        {
                            StartEnvelope();
                        }
                        else
                        {
                            StopEnvelope();
                        }
                    }
                    else
                    {
                        // Normal mode: just start the note
                        StartEnvelope();
                    }
                }
                else if (!hold && !button_pressed && was_button_pressed_)
                {
                    // Normal mode: release when button is released
                    StopEnvelope();
                }

                // Update pitch if the note is being played
                if ((hold && is_note_on_) || (!hold && button_pressed))
                {
                    UpdatePitchWithScale(pot_value);
                }
            }
            else
            {
                //------------------------------------------------------------------
                //  freq_select_button IS pressed => override pitch:
                //     1) Keep envelope open
                //     2) pot_value => fundamentalFreq_ (C1 .. C6)
                //     3) Let vibrato apply if desired
                //------------------------------------------------------------------
                if (!is_note_on_)
                {
                    // If we somehow got here with note off, force note on
                    StartEnvelope();
                }

                // Remap pot [0..1] to [C1..C6]
                fundamentalFreq_ = mapFloat(pot_value, 0.0f, 1.0f, kMinFundamental, kMaxFundamental);

                // Vibrato + smoothing
                float freqWithVibrato = vibrato_.Process(fundamentalFreq_);
                SmoothFrequencyToward(freqWithVibrato);
            }

            //------------------------------------------------------------------
            //  Generate the audio block (oversampled frames)
            //------------------------------------------------------------------
            float sample = RenderOneSample();
            sample *= (kAudioOSFactor * kAudioOutputLevel);

            for (uint32_t i = 0; i < kAudioOSFactor; i++)
            {
                float filtered = aa_filter_.Process((i == 0) ? sample : 0.0f);
                block[i] = filtered;
            }

            // Store button states for next iteration
            was_button_pressed_ = button_pressed;
            was_freq_select_button_pressed_ = freq_select_button;
        }

        /**
         * @brief Returns whether there's any active sound (ADSR not idle or delay not silent).
         */
        bool getActive()
        {
            if (adsr_state_ == ADSRState::kIdle && !delay_.audible())
            {
                return false;
            }
            return true;
        }

    private:
        //--------------------------------------------------------------------------
        //                              CONSTANTS
        //--------------------------------------------------------------------------

        /**
         * @brief Diatonic scale ratios in Equal Temperament for a C-major scale
         *        (intervals: 0,2,4,5,7,9,11,12 semitones).
         */
        static constexpr float kDiatonicRatios[] = {
            1.0f,     // C  (0 semitones up from fundamental)
            1.12246f, // D  (2 semitones)
            1.25992f, // E  (4 semitones)
            1.33484f, // F  (5 semitones)
            1.49831f, // G  (7 semitones)
            1.68179f, // A  (9 semitones)
            1.88775f, // B  (11 semitones)
            2.0f      // C  (12 semitones, next octave)
        };
        static constexpr int kNumNotes = sizeof(kDiatonicRatios) / sizeof(kDiatonicRatios[0]);

        // Pot-value thresholds for snapping to the scale (like before)
        static constexpr float kThresholds[] = {
            0.125f, // between scale[0] and scale[1]
            0.25f,
            0.375f,
            0.5f,
            0.625f,
            0.75f,
            0.875f};
        static constexpr int kNumThresholds = sizeof(kThresholds) / sizeof(kThresholds[0]);

        // Range for adjusting fundamental frequency [C1..C6]
        static constexpr float kMinFundamental = 32.70f;   // ~C1
        static constexpr float kMaxFundamental = 1046.50f; // ~C6

        //--------------------------------------------------------------------------
        //                              ADSR STATE
        //--------------------------------------------------------------------------
        enum class ADSRState
        {
            kIdle,
            kAttack,
            kDecay,
            kSustain,
            kRelease
        };

        //--------------------------------------------------------------------------
        //                         PRIVATE MEMBER VARIABLES
        //--------------------------------------------------------------------------

        // Synth/frequency
        float phase_;
        float currentFrequency_;
        float fundamentalFreq_; // base frequency (e.g. C3), can be changed
        float targetFrequencyOffset_;
        float frequencyMargin_;
        int offsetCounter_;
        float duty_gain_;
        float freq_mult_;
        float attack_formant_rate_;
        bool is_note_on_;
        float target_duty_rand_;
        float duty_rand_;
        float target_formant_freq_mult_;
        float formant_freq_mult_;
        float delay_time_;
        float delay_feedback_;
        float freq_rate_;
        float freq_wobbliness_;

        // Button states
        bool was_button_pressed_;             // For the normal "voice" button
        bool was_freq_select_button_pressed_; // For the new fundamental-freq button
        bool vowel_set_;
        int previousTargetIndex_;

        // Track previous formant pot value
        float previous_formant_pot_val_;

        // DSP components
        AAFilter<float> aa_filter_;
        FormantFilter formant_filter_;
        OnePoleLowpass lowpass_filter_;
        PulseGenerator pulse_generator_;
        DelayEngine delay_;

        // Vibrato effect
        Vibrato vibrato_;

        // ADSR parameters
        float adsr_attack_time_;
        float adsr_decay_time_;
        float adsr_sustain_level_;
        float adsr_release_time_;
        float adsr_value_;
        ADSRState adsr_state_;

        // Misc
        float sample_rate_;

        //--------------------------------------------------------------------------
        //                              PRIVATE METHODS
        //--------------------------------------------------------------------------

        void RobotToMonk(float formant_pot_val)
        {
            freq_rate_ = mapFloat(formant_pot_val, 0.0f, 1.0f, 0.00001f, 0.008f);
            pulse_generator_.SetBaseDutyCycle(mapFloat(formant_pot_val, 0.0f, 1.0f, 0.0003f, 0.5f));
            formant_filter_.setFreqMult(mapFloat(formant_pot_val, 0.0f, 1.0f, 0.6f, 1.6f));
            freq_wobbliness_ = mapFloat(formant_pot_val, 0.0f, 1.0f, 0.03f, 0.0f);
            pulse_generator_.SetDutyCycleRandomization(mapFloat(formant_pot_val, 0.0f, 1.0f, 0.00f, 0.08f));
            formant_filter_.SetFormantRate(mapFloat(formant_pot_val, 0.0f, 1.0f, 0.000000001f, 0.008f));
            previous_formant_pot_val_ = formant_pot_val;
        }
        float RenderOneSample()
        {
            // If envelope is idle and the delay line is silent, output zero
            if (adsr_state_ == ADSRState::kIdle && !delay_.audible())
            {
                return 0.0f;
            }

            // Update the ADSR once per audio-frame
            UpdateEnvelope();

            // Advance oscillator
            float phaseIncrement = currentFrequency_ / sample_rate_;
            phase_ += phaseIncrement;
            if (phase_ >= 1.0f)
                phase_ -= 1.0f;

            // Generate one sample from the pulse oscillator
            float sample = pulse_generator_.GenerateSample(phase_, phaseIncrement);

            // Apply lowpass
            sample = lowpass_filter_.Process(sample);

            // Formant filter
            sample = formant_filter_.Process(sample);

            // Envelope
            sample *= adsr_value_;

            // Delay effect
            sample = delay_.Process(sample, delay_time_, delay_feedback_);

            // Additional gain for narrower pulses
            sample *= duty_gain_;

            return sample;
        }

        void StartEnvelope()
        {
            adsr_state_ = ADSRState::kAttack;
            adsr_value_ = 0.0f; // Start from 0
            is_note_on_ = true;

            // Set the formant to "A" to get that open mouth "WAH" sound.
            formant_filter_.SetVowel(FormantFilter::VOWEL_A);

            // Trigger vibrato buildup whenever we start the envelope
            vibrato_.Trigger();
        }

        void StopEnvelope()
        {
            // Only go to Release if we're not already idle
            if (adsr_state_ != ADSRState::kIdle)
            {
                adsr_state_ = ADSRState::kRelease;
            }
        }

        void UpdateEnvelope()
        {
            switch (adsr_state_)
            {
            case ADSRState::kAttack:
            {
                float increment = 1.0f / (adsr_attack_time_ * sample_rate_);
                adsr_value_ += increment;
                if (adsr_value_ >= 1.0f)
                {
                    adsr_value_ = 1.0f;
                    adsr_state_ = ADSRState::kDecay;
                }
            }
            break;

            case ADSRState::kDecay:
            {
                float decrement = (1.0f - adsr_sustain_level_) / (adsr_decay_time_ * sample_rate_);
                adsr_value_ -= decrement;
                if (adsr_value_ <= adsr_sustain_level_)
                {
                    adsr_value_ = adsr_sustain_level_;
                    adsr_state_ = ADSRState::kSustain;
                }
            }
            break;

            case ADSRState::kSustain:
                // Envelope stays at sustain level.
                adsr_value_ = adsr_sustain_level_;
                break;

            case ADSRState::kRelease:
            {
                float decrement = adsr_sustain_level_ / (adsr_release_time_ * sample_rate_);
                adsr_value_ -= decrement;

                // Morph back to "lips closed" OU
                formant_filter_.SetVowel(FormantFilter::VOWEL_OU);
                formant_filter_.SetFormantRate(0.001f);

                if (adsr_value_ <= 0.0f)
                {
                    adsr_value_ = 0.0f;
                    adsr_state_ = ADSRState::kIdle;
                    is_note_on_ = false;
                }
            }
            break;

            case ADSRState::kIdle:
            default:
                // Envelope is zero (no note playing).
                adsr_value_ = 0.0f;
                break;
            }
        }

        //--------------------------------------------------------------------------
        //  For normal (voice-button) operation: pick a diatonic note from pot_value
        //--------------------------------------------------------------------------
        void UpdatePitchWithScale(float pot_value)
        {
            int targetIndex = DetermineTargetIndex(pot_value);
            PossiblyUpdateVowel(targetIndex);

            float baseTargetFrequency = fundamentalFreq_ * kDiatonicRatios[targetIndex] * freq_mult_;

            PossiblyUpdateFrequencyOffset(baseTargetFrequency);

            float targetFrequency = baseTargetFrequency + targetFrequencyOffset_;

            // Apply vibrato
            float vibratoFreq = vibrato_.Process(targetFrequency);

            // Smooth toward final (vibrato) freq
            SmoothFrequencyToward(vibratoFreq);
        }

        int DetermineTargetIndex(float pot_value) const
        {
            // If it's below the first threshold, it's the lowest note
            if (pot_value < kThresholds[0])
            {
                return 0;
            }
            // If it's above the last threshold, it's the highest note
            else if (pot_value >= kThresholds[kNumThresholds - 1])
            {
                return kNumNotes - 1;
            }
            // Otherwise, find the correct threshold interval
            for (int i = 1; i < kNumThresholds; i++)
            {
                if (pot_value < kThresholds[i])
                {
                    return i;
                }
            }
            // Fallback
            return kNumNotes - 1;
        }

        void PossiblyUpdateVowel(int targetIndex)
        {
            if (targetIndex != previousTargetIndex_)
            {
                // Random voice from { NEUTRAL, NASAL, DARK }
                int randomVoice = std::rand() % 3;
                formant_filter_.SetVoice(static_cast<FormantFilter::VoiceType>(randomVoice));

                // Then pick a random vowel from the 10 available vowels
                FormantFilter::Vowel randomVowel = GetRandomVowel();
                formant_filter_.SetVowel(randomVowel);

                previousTargetIndex_ = targetIndex;
            }
        }

        void PossiblyUpdateFrequencyOffset(float baseTargetFrequency)
        {
            // If counter expired or frequency is near the old target, pick a new offset
            if (offsetCounter_ <= 0 ||
                std::abs(currentFrequency_ - (baseTargetFrequency + targetFrequencyOffset_)) < frequencyMargin_)
            {
                // freq_wobbliness_ is controlling the +/- offset range
                float maxOffset = baseTargetFrequency * freq_wobbliness_;
                targetFrequencyOffset_ =
                    ((static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f) * maxOffset;

                // Reset the offset counter (change offset every 1000 samples)
                offsetCounter_ = 1000;
            }
            // Decrement the counter
            offsetCounter_--;
        }

        void SmoothFrequencyToward(float targetFrequency)
        {
            // freq_rate_ controls how fast we move toward the target
            float diff = targetFrequency - currentFrequency_;
            currentFrequency_ += diff * freq_rate_;
        }

        FormantFilter::Vowel GetRandomVowel()
        {
            // 10 total vowels: VOWEL_I to VOWEL_SCHWA
            constexpr int numVowels = 10;
            int randomIndex = std::rand() % numVowels;
            return static_cast<FormantFilter::Vowel>(randomIndex);
        }

        // Optional stubs for voice/duty characteristics
        void setFormantMult(float mult)
        {
            target_formant_freq_mult_ = mapFloat(mult, 0.0f, 1.0f, 0.5f, 2.5f);
        }
        void setDutyRand(float rand)
        {
            target_duty_rand_ = mapFloat(rand, 0.0f, 1.0f, 0.0f, 0.95f);
        }
        float updateVoiceCharacteristics()
        {
            float dif1 = target_formant_freq_mult_ - formant_freq_mult_;
            formant_freq_mult_ += dif1 * 0.02f;
            float dif2 = target_duty_rand_ - duty_rand_;
            duty_rand_ += dif2 * 0.02f;
            return formant_freq_mult_;
        }

        float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
        {
            return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
        }
    };

} // namespace recorder
