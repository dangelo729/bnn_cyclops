#pragma once

#include "formant_biquad.h"
#include <cmath>

namespace recorder
{

    class FormantFilter
    {
    public:
        // Enumeration of vowels
        enum Vowel
        {
            VOWEL_I,    // /i/ ("see")
            VOWEL_IH,   // /ɪ/ ("sit")
            VOWEL_EH,   // /ɛ/ ("set")
            VOWEL_AE,   // /æ/ ("sat")
            VOWEL_A,    // /a/ ("father")
            VOWEL_O,    // /ɔ/ ("saw")
            VOWEL_OU,   // /o/ ("go")
            VOWEL_UH,   // /ʊ/ ("put")
            VOWEL_U,    // /u/ ("boot")
            VOWEL_SCHWA // /ə/ ("sofa")
        };

        // We only keep the NEUTRAL voice type
        enum VoiceType
        {
            VOICE_NEUTRAL = 0,
            VOICE_COUNT // == 1
        };

        // Add a filter mode enum
        enum FilterMode
        {
            FILTER_MODE_NORMAL = 0, // The original behavior
            FILTER_MODE_WAH         // Wah mode: interpolate between VOWEL_A and VOWEL_OU
        };

        // Structure to hold formant frequencies and Q values for a vowel
        struct VowelFormantData
        {
            float F1;
            float F2;
            float F3;
            float Q1;
            float Q2;
            float Q3;
        };

        // The 2D array of vowel data for our single (NEUTRAL) voice
        static const VowelFormantData vowelData[VOICE_COUNT][10];

        // Initialize the filter with a given sample rate
        void Init(float sampleRate)
        {
            sampleRate_ = sampleRate;

            // Default to neutral voice
            currentVoice_ = VOICE_NEUTRAL;

            // Default mode = NORMAL
            filterMode_ = FILTER_MODE_NORMAL;

            // Default wah position = 0.0 (fully "A" if we switch to wah)
            wahPosition_ = 0.0f;

            // Initialize to a default vowel (e.g., /a/)
            SetVowel(VOWEL_A);

            // Initialize the filters with the current frequencies and Qs
            for (int i = 0; i < 3; ++i)
            {
                currentFormantFreqs_[i] = targetFormantFreqs_[i];
                currentFormantQs_[i] = targetFormantQs_[i];
                filters_[i].Init(BANDPASS, sampleRate_, currentFormantFreqs_[i], currentFormantQs_[i]);
            }

            // Set a default formant rate
            formantRate_ = 0.002f;
        }

        // Set the filter mode (normal or wah)
        void SetMode(FilterMode mode)
        {
            filterMode_ = mode;
        }

        // Set the voice type. (In practice, we only have VOICE_NEUTRAL.)
        void SetVoice(VoiceType voice)
        {
            // Guard against invalid voice index
            if (voice < 0 || voice >= VOICE_COUNT)
                return;

            currentVoice_ = voice;
        }

        // Set how "far" along the wah we are (0.0 => VOWEL_A, 1.0 => VOWEL_OU)
        // You might want to clamp it just to be safe
        void SetWahPosition(float pos)
        {
            if (pos < 0.0f)
                pos = 0.0f;
            if (pos > 1.0f)
                pos = 1.0f;
            wahPosition_ = pos;
        }

        // Multipliers for final fine-tuning
        void setQMult(float qMult)
        {
            q_mult_ = qMult;
        }

        void setFreqMult(float freqMult)
        {
            freq_mult_ = freqMult;
        }

        // Function to set the target vowel (only effective in FILTER_MODE_NORMAL)
        void SetVowel(Vowel vowel)
        {
            // If we are in normal mode, we load from the array
            if (filterMode_ == FILTER_MODE_NORMAL)
            {
                targetFormantFreqs_[0] = vowelData[currentVoice_][vowel].F1;
                targetFormantFreqs_[1] = vowelData[currentVoice_][vowel].F2;
                targetFormantFreqs_[2] = vowelData[currentVoice_][vowel].F3;

                targetFormantQs_[0] = vowelData[currentVoice_][vowel].Q1;
                targetFormantQs_[1] = vowelData[currentVoice_][vowel].Q2;
                targetFormantQs_[2] = vowelData[currentVoice_][vowel].Q3;
            }
        }

        // Function to set the formant morphing rate
        void SetFormantRate(float rate)
        {
            formantRate_ = rate;
        }

        // Method to smoothly update parameters towards the target formant values
        void UpdateParameters()
        {
            // If in wah mode, first compute the new "target" by interpolating between /a/ and /ou/
            if (filterMode_ == FILTER_MODE_WAH)
            {
                const auto &vowelA = vowelData[currentVoice_][VOWEL_A];
                const auto &vowelOU = vowelData[currentVoice_][VOWEL_OU];

                // Interpolate F1, F2, F3
                targetFormantFreqs_[0] = vowelA.F1 + wahPosition_ * (vowelOU.F1 - vowelA.F1);
                targetFormantFreqs_[1] = vowelA.F2 + wahPosition_ * (vowelOU.F2 - vowelA.F2);
                targetFormantFreqs_[2] = vowelA.F3 + wahPosition_ * (vowelOU.F3 - vowelA.F3);

/*
                targetFormantFreqs_[0] = vowelA.F1 * (1 - wahPosition_) + (vowelOU.F1 - wahPosition_);
                targetFormantFreqs_[1] = vowelA.F2 * (1 - wahPosition_) + (vowelOU.F2 - wahPosition_);
                targetFormantFreqs_[2] = vowelA.F3 * (1 - wahPosition_) + (vowelOU.F3 - wahPosition_);
*/
                // Interpolate Q1, Q2, Q3
                targetFormantQs_[0] = vowelA.Q1 + wahPosition_ * (vowelOU.Q1 - vowelA.Q1);
                targetFormantQs_[1] = vowelA.Q2 + wahPosition_ * (vowelOU.Q2 - vowelA.Q2);
                targetFormantQs_[2] = vowelA.Q3 + wahPosition_ * (vowelOU.Q3 - vowelA.Q3);


/*
                targetFormantQs_[0] = vowelA.Q1 * (1 - wahPosition_) + (vowelOU.Q1 - wahPosition_);
                targetFormantQs_[1] = vowelA.Q2 * (1 - wahPosition_) + (vowelOU.Q2 - wahPosition_);
                targetFormantQs_[2] = vowelA.Q3 * (1 - wahPosition_) + (vowelOU.Q3 - wahPosition_);

                */
            }

            // Smoothly move current formants toward the target formants
            for (int i = 0; i < 3; ++i)
            {
                float freqDiff = targetFormantFreqs_[i] - currentFormantFreqs_[i];
                currentFormantFreqs_[i] += freqDiff * formantRate_;

                float qDiff = targetFormantQs_[i] - currentFormantQs_[i];
                currentFormantQs_[i] += qDiff * formantRate_;

                // Update the filter parameters
                filters_[i].SetParameters(
                    currentFormantFreqs_[i] * freq_mult_,
                    currentFormantQs_[i] * q_mult_);
            }
        }

        // Process a single audio sample
        float Process(float input)
        {
            float output1 = filters_[0].Process(input);
            float output2 = filters_[1].Process(input);
            float output3 = filters_[2].Process(input);

            // Sum the outputs and apply a gain factor
            float output = (output1 + output2 * 0.4f + output3 * 0.3f) * gainFactor_;
            return output;
        }

    private:
        float sampleRate_;
        FormantBiquad filters_[3];

        float currentFormantFreqs_[3];
        float targetFormantFreqs_[3];
        float currentFormantQs_[3];
        float targetFormantQs_[3];

        float formantRate_; // Rate for morphing toward target freq/Q

        // Multipliers for freq and Q
        float q_mult_ = 1.0f;
        float freq_mult_ = 1.0f;

        // Only one voice type is supported now (NEUTRAL)
        VoiceType currentVoice_;

        // Mode: normal or wah
        FilterMode filterMode_;

        // Wah position in [0.0 ... 1.0]
        // 0.0 => /a/, 1.0 => /ou/
        float wahPosition_;

        // Gain factor to prevent clipping or to adjust overall output level
        const float gainFactor_ = 1.7f;
    };

    // ----------------------------------------------------------------------------
    // Define the vowel data for the single (NEUTRAL) voice type.
    // vowelData[VoiceType=NEUTRAL][Vowel] = { F1, F2, F3, Q1, Q2, Q3 }
    // ----------------------------------------------------------------------------
    const FormantFilter::VowelFormantData
        FormantFilter::vowelData[FormantFilter::VOICE_COUNT][10] =
            {
                {// VOWEL_I  ("see")
                 {270.0f, 2290.0f, 3010.0f, 10.0f, 9.0f, 9.0f},
                 // VOWEL_IH ("sit")
                 {390.0f, 1990.0f, 2550.0f, 12.0f, 11.0f, 10.0f},
                 // VOWEL_EH ("set")
                 {530.0f, 1840.0f, 2480.0f, 11.0f, 11.0f, 10.0f},
                 // VOWEL_AE ("sat")
                 {660.0f, 1720.0f, 2410.0f, 11.0f, 11.0f, 10.0f},
                 // VOWEL_A  ("father")
                 {730.0f, 1090.0f, 2440.0f, 10.0f, 8.0f, 9.0f},
                 // VOWEL_O  ("saw")
                 {570.0f, 840.0f, 2410.0f, 11.0f, 10.0f, 10.0f},
                 // VOWEL_OU ("go")
                 {500.0f, 700.0f, 2450.0f, 11.0f, 10.0f, 10.0f},
                 // VOWEL_UH ("put")
                 {440.0f, 1020.0f, 2240.0f, 12.0f, 10.0f, 10.0f},
                 // VOWEL_U  ("boot")
                 {300.0f, 870.0f, 2240.0f, 10.0f, 9.0f, 9.0f},
                 // VOWEL_SCHWA ("sofa")
                 {500.0f, 1500.0f, 2400.0f, 12.0f, 11.0f, 10.0f}}};

} // namespace recorder
