#pragma once

#include <cmath>
#include <cstdlib> // For rand()
#include <array>

//-------------------------------------------------------------------------------------------
// Example: Basic 2nd-order filter for shaping the burst noise according to place of articulation
//-------------------------------------------------------------------------------------------
class BiquadFilter
{
public:
    BiquadFilter()
    {
        Reset();
    }

    void SetCoefficients(float b0, float b1, float b2, float a1, float a2)
    {
        b0_ = b0; b1_ = b1; b2_ = b2; a1_ = a1; a2_ = a2;
    }

    void Reset()
    {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    float Process(float in)
    {
        // Direct form I
        float out = b0_ * in + z1_;
        z1_ = b1_ * in - a1_ * out + z2_;
        z2_ = b2_ * in - a2_ * out;
        return out;
    }

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

//-------------------------------------------------------------------------------------------
// Consonant type
//-------------------------------------------------------------------------------------------
enum class ConsonantType
{
    NONE,
    B, // bilabial
    D, // alveolar
    G  // velar
};

//-------------------------------------------------------------------------------------------
// ConsonantGenerator
//-------------------------------------------------------------------------------------------
class ConsonantGenerator
{
public:
    ConsonantGenerator() {}
    ~ConsonantGenerator() {}

    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    // Set up a new consonant to be generated
    // durations (in seconds), amplitude, fundamental frequency, etc.
    void Start(ConsonantType type,
               float f0,
               float amplitude,
               float closureDuration,
               float burstDuration,
               float transitionDuration)
    {
        Reset();
        type_                = type;
        f0_                  = f0;
        amplitude_           = amplitude;
        closureSamples_      = static_cast<int>(closureDuration   * sampleRate_);
        burstSamples_        = static_cast<int>(burstDuration     * sampleRate_);
        transitionSamples_   = static_cast<int>(transitionDuration* sampleRate_);
        totalSamples_        = closureSamples_ + burstSamples_ + transitionSamples_;
        state_               = State::CLOSURE;
        sampleCounter_       = 0;
        phase_               = 0.0f;

        // Prepare the place-of-articulation filter for the burst
        ConfigureBurstFilter(type);

        // A quick placeholder for transition filter. You could do
        // something more advanced with formants or partial filtering.
        // For now, we’ll just use a simple “tilt” factor that we apply
        // in the transition to differentiate /b/, /d/, /g/.
        transitionFilterFactor_ = GetTransitionTilt(type_);
    }

    // Returns true if generating a plosive
    bool IsActive() const
    {
        return (state_ != State::IDLE && state_ != State::DONE);
    }

    // Stream out one sample of the current consonant
    float Process()
    {
        if (state_ == State::IDLE || state_ == State::DONE || type_ == ConsonantType::NONE)
        {
            return 0.0f; // not active
        }

        float out = 0.0f;

        // 1. Generate samples depending on the state
        switch (state_)
        {
        case State::CLOSURE:
            // partial voicing or near silence
            out = GenerateClosureSample();
            // Move to next state if closure is done
            if (sampleCounter_ >= closureSamples_)
            {
                sampleCounter_ = 0;
                state_ = State::BURST;
            }
            break;

        case State::BURST:
            out = GenerateBurstSample();
            if (sampleCounter_ >= burstSamples_)
            {
                sampleCounter_ = 0;
                state_ = State::TRANSITION;
            }
            break;

        case State::TRANSITION:
            out = GenerateTransitionSample();
            if (sampleCounter_ >= transitionSamples_)
            {
                sampleCounter_ = 0;
                state_ = State::DONE;
            }
            break;

        default:
            break;
        }

        sampleCounter_++;
        return out;
    }

    // Call at the end or if you want to prematurely stop
    void Stop()
    {
        Reset();
    }

private:
    enum class State
    {
        IDLE,
        CLOSURE,
        BURST,
        TRANSITION,
        DONE
    };

    //---------------------------------------------------------------------------------------
    // Internal generator functions
    //---------------------------------------------------------------------------------------
    float GenerateClosureSample()
    {
        // partial voicing: small amplitude sine wave (or glottal model)
        float sample = amplitude_ * 0.1f * std::sin(2.0f * M_PI * f0_ * phase_);
        AdvancePhase();
        return sample;
    }

    float GenerateBurstSample()
    {
        // White noise filtered by place-of-articulation filter
        float rawNoise = static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f;
        float shapedNoise = burstFilter_.Process(rawNoise);
        // Envelope shape: quickly rise and fall. Here we do a simple linear fade
        float env = 1.0f - (static_cast<float>(sampleCounter_) / static_cast<float>(burstSamples_));
        float sample = amplitude_ * shapedNoise * env;
        return sample;
    }

    float GenerateTransitionSample()
    {
        // Voice with a simple tilt factor to simulate place-of-articulation formant transition
        float voice = amplitude_ * std::sin(2.0f * M_PI * f0_ * phase_);
        // apply a simple tilt or filter factor
        voice *= (1.0f + transitionFilterFactor_);
        // fade-out or fade-in approach over the transition
        float frac = static_cast<float>(sampleCounter_) / static_cast<float>(transitionSamples_);
        // you might ramp from slightly more consonant to a more open vowel sound
        // for simplicity, we just do a linear fade of amplitude
        float sample = voice * (1.0f - frac + 0.2f); // keep a bit of amplitude
        AdvancePhase();
        return sample;
    }

    //---------------------------------------------------------------------------------------
    // Helpers
    //---------------------------------------------------------------------------------------
    void AdvancePhase()
    {
        float inc = f0_ / sampleRate_;
        phase_ += inc;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;
    }

    void ConfigureBurstFilter(ConsonantType type)
    {
        burstFilter_.Reset();

        // A few example sets of Biquad coefficients for simple shaping:
        // These are not scientifically measured, just placeholders for demonstration.

        // b0, b1, b2, a1, a2
        if (type == ConsonantType::B)
        {
            // /b/ - emphasis on low frequencies
            burstFilter_.SetCoefficients(
                0.2f, 0.2f, 0.0f,
                -0.3f, 0.0f
            );
        }
        else if (type == ConsonantType::D)
        {
            // /d/ - emphasis on mid range
            burstFilter_.SetCoefficients(
                0.2f, 0.0f, -0.2f,
                -0.4f, 0.25f
            );
        }
        else if (type == ConsonantType::G)
        {
            // /g/ - emphasis on slightly higher frequencies
            burstFilter_.SetCoefficients(
                0.3f, 0.0f, -0.1f,
                -0.2f, 0.15f
            );
        }
        else
        {
            // fallback: no filter
            burstFilter_.SetCoefficients(
                1.0f, 0.0f, 0.0f,
                0.0f, 0.0f
            );
        }
    }

    float GetTransitionTilt(ConsonantType type)
    {
        switch (type)
        {
        case ConsonantType::B:
            return -0.2f; // slightly more low frequency
        case ConsonantType::D:
            return 0.0f;  // neutral
        case ConsonantType::G:
            return 0.2f;  // slightly more high frequency
        default:
            return 0.0f;
        }
    }

    void Reset()
    {
        type_              = ConsonantType::NONE;
        sampleCounter_     = 0;
        state_             = State::IDLE;
        phase_             = 0.0f;
        burstFilter_.Reset();
    }

    //---------------------------------------------------------------------------------------
    // Internal members
    //---------------------------------------------------------------------------------------
    float sampleRate_    = 44100.0f;
    ConsonantType type_  = ConsonantType::NONE;

    float f0_            = 100.0f; // fundamental frequency
    float amplitude_     = 0.5f;

    int closureSamples_    = 0;
    int burstSamples_      = 0;
    int transitionSamples_ = 0;
    int totalSamples_      = 0;

    int sampleCounter_   = 0;
    float phase_         = 0.0f;

    State state_ = State::IDLE;

    BiquadFilter burstFilter_;
    float transitionFilterFactor_ = 0.0f;
};
