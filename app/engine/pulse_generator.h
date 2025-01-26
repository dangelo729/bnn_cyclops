#pragma once
#include <cstdint>
#include <cmath>
#include <cstdlib> // For std::rand() and RAND_MAX
#include "common/config.h"
#include <ctime>
namespace recorder
{
    class PulseGenerator
    {
    public:
        PulseGenerator() : base_dutycycle(0.5f),
                           duty_cyclerandomization(0.0f),
                           current_dutycycle(0.5f),
                           randomizationcounter(0),
                           randomizationperiod(5) // Change random variation every 2 samples
        {
            std::srand(static_cast<unsigned>(std::time(0)));
        }
        void SetBaseDutyCycle(float duty_cycle)
        {
            // Clamp duty cycle between 0.0 and 1.0
            base_dutycycle = std::max(0.0f, std::min(1.0f, duty_cycle));
            UpdateDutyCycle();
        }
        void SetDutyCycleRandomization(float randomization)
        {
            // Clamp randomization between 0.0 and 1.0
            duty_cyclerandomization = std::max(0.0f, std::min(1.0f, randomization));
            UpdateDutyCycle();
        }
        float GenerateSample(float phase, float phase_increment)
        {
            // Update randomization if needed
            if (--randomizationcounter <= 0)
            {
                UpdateDutyCycle();
                randomizationcounter = randomizationperiod;
            }
            // Generate pulse wave with PolyBLEP antialiasing
            float sample = (phase < current_dutycycle) ? 1.0f : -1.0f;
            // Apply PolyBLEP at rising edge
            sample += PolyBlep(phase, phase_increment);
            // Apply PolyBLEP at falling edge
            float t = phase - current_dutycycle;
            if (t < 0.0f)
                t += 1.0f;
            sample -= PolyBlep(t, phase_increment);
            return sample;
        }

    private:
        float base_dutycycle;
        float duty_cyclerandomization;
        float current_dutycycle;
        int randomizationcounter;
        const int randomizationperiod;
        void UpdateDutyCycle()
        {
            if (duty_cyclerandomization > 0.0f)
            {
                // Generate random value between -1 and 1
                float random_offset = ((static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f);
                // Scale by randomization amount and clamp result
                float max_offset = 0.3f * duty_cyclerandomization; // Max 30% variation at full randomization
                float offset = random_offset * max_offset;
                current_dutycycle = std::max(0.1f, std::min(0.9f, base_dutycycle + offset));
            }
            else
            {
                current_dutycycle = base_dutycycle;
            }
        }
        float PolyBlep(float t, float dt)
        {
            if (dt == 0.0f)
                return 0.0f;
            // Normalize t to [0, 1)
            t = fmodf(t, 1.0f);
            if (t < dt)
            {
                t /= dt;
                return t + t - t * t - 1.0f;
            }
            else if (t > 1.0f - dt)
            {
                t = (t - 1.0f) / dt;
                return t * t + t + t + 1.0f;
            }
            else
            {
                return 0.0f;
            }
        }
    };
} // namespace recorder