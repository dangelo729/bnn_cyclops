#pragma once

#include <cmath>
#include <algorithm> // for std::min, std::max

namespace recorder
{

class Vibrato
{
public:
    Vibrato()
        : sampleRate_(16000.0f),
          phase_(0.0f),
          rate_(5.0f),
          depth_(0.02f),
          targetDepth_(0.02f),
          buildupTime_(1.0f),
          currentDepth_(0.0f),
          buildingUp_(false)
    {
    }

    /**
     * @brief Initialize the vibrato with the given sample rate.
     * @param sampleRate The audio sample rate (e.g. 16000).
     */
    void Init(float sampleRate)
    {
        sampleRate_   = sampleRate;
        phase_        = 0.0f;
        currentDepth_ = 0.0f;
        buildingUp_   = false;
        // Ensure our internal "depth_" matches the initial target
        depth_        = targetDepth_;
    }

    /**
     * @brief Set vibrato parameters. 
     * @param rate        The LFO rate in Hz.
     * @param depth       Maximum vibrato depth (0.0 -> 0.25 in typical usage).
     * @param buildupTime Time in seconds to ramp from 0 -> depth after `Trigger()`.
     */
    void SetParameters(float rate, float depth, float buildupTime)
    {
        rate_        = rate;
        targetDepth_ = depth; // We'll still clamp and smooth this in Process().
        buildupTime_ = (buildupTime <= 0.0f) ? 0.01f : buildupTime;
    }

    /**
     * @brief Smoothly change vibrato depth in real-time.
     *        Incoming `newDepth` is mapped [0.0 .. 1.0] -> [0.0 .. 0.25].
     */
    void SetDepth(float newDepth)
    {
        // Map from [0,1] to [0,0.25]:
        float mapped = newDepth * 0.25f;
        targetDepth_ = mapped;
    }

    /**
     * @brief Trigger the vibrato buildup. This resets `currentDepth_` to 0
     *        and begins ramping up to `depth_` with the specified buildup time.
     */
    void Trigger()
    {
        buildingUp_   = true;
        currentDepth_ = 0.0f;
    }

    /**
     * @brief Process one sample of vibrato on the input frequency.
     * @param inputFreq The current "base" frequency (e.g. from the note/pitch).
     * @return `moddedFreq = inputFreq * (1 + sin(LFO) * finalDepth)`
     */
    float Process(float inputFreq)
    {
        //--------------------------------------------------
        // 1) Smooth "depth_" toward "targetDepth_"
        //--------------------------------------------------
        static constexpr float kDepthSmoothing = 0.02f;  // 2% approach each sample
        float dDiff = targetDepth_ - depth_;
        depth_ += dDiff * kDepthSmoothing;

        // Keep "depth_" within [0, 0.25] or whatever your max
        if (depth_ < 0.0f)
            depth_ = 0.0f;
        if (depth_ > 0.25f)
            depth_ = 0.25f;

        //--------------------------------------------------
        // 2) Move "currentDepth_" smoothly toward "depth_"
        //    respecting "buildupTime_"
        //--------------------------------------------------
        // We'll compute a per-sample increment factor based on buildupTime_:
        // If buildupTime_ is X seconds, we want to fully go from 0 to 'depth_'
        // in X * sampleRate_ samples => each sample we move an incremental fraction.
        float alpha = 1.0f / (buildupTime_ * sampleRate_);
        // We clamp alpha to something smaller if we want a minimum speed
        // but let's keep it as is for now.

        // If buildingUp_ is true, we definitely ramp up from currentDepth_ toward depth_.
        // But also if depth_ changes in the middle, we still approach it (up or down).
        float cdDiff = depth_ - currentDepth_;

        // Move a fraction of that difference
        currentDepth_ += cdDiff * alpha;

        // If we're close enough, or we've gone past, we can consider we've "caught up"
        if (std::fabs(cdDiff) < 0.0001f)
        {
            currentDepth_ = depth_;
            buildingUp_   = false; // We can consider the buildup complete
        }

        // Finally, clamp currentDepth_ so it never goes beyond depth_ in either direction
        // e.g. if depth_ decreased mid-buildup, we want currentDepth_ to smoothly go down.
        if (currentDepth_ < 0.0f)
            currentDepth_ = 0.0f;
        if (currentDepth_ > depth_)
            currentDepth_ = depth_;

        //--------------------------------------------------
        // 3) Increment the LFO phase
        //--------------------------------------------------
        float phaseIncrement = (2.0f * static_cast<float>(M_PI) * rate_) / sampleRate_;
        phase_ += phaseIncrement;
        if (phase_ >= 2.0f * static_cast<float>(M_PI))
        {
            phase_ -= 2.0f * static_cast<float>(M_PI);
        }

        //--------------------------------------------------
        // 4) Compute vibrato factor. Limit upward shift 
        //    so we don't boost volume too much
        //--------------------------------------------------
        // Basic vibrato factor = sin(phase_) * currentDepth_


        float vib = std::sin(phase_) * currentDepth_;
/*
        if (vib > 0.0f)
        {
            vib *= 0.05f;  // scale down upward range
        }
*/

        float moddedFreq = inputFreq * (1.0f + vib);

        return moddedFreq;
    }

private:
    float sampleRate_;   ///< The sample rate (e.g. 16000 Hz)
    float phase_;        ///< The current LFO phase
    float rate_;         ///< LFO rate in Hz

    float depth_;        ///< The "live" depth that is slowly approaching targetDepth_
    float targetDepth_;  ///< Where we eventually want depth_ to be

    float buildupTime_;  ///< Time in seconds to ramp from 0 to depth_ 
    float currentDepth_; ///< The per-note ramp that starts at 0 on Trigger() and moves toward depth_

    bool buildingUp_;    ///< True if we started from 0 and are still in the buildup phase
};

} // namespace recorder
