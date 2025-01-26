#pragma once

#include <cstdint>
#include <algorithm>

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_adc.h"

#include "common/io.h"
#include "util/hysteresis_filter.h"
#include "util/interpolator.h"

namespace recorder
{

class Adc
{
public:
    using Callback = void (*)(const AudioInput&, const PotInput& pot);

    void Init(Callback callback);
    void Start(void);
    void Stop(void);

protected:
    static inline Adc* instance_;
    Callback callback_;
    bool started_;

    struct PotFilter
    {
        Interpolator lerp_;
        HysteresisFilter hyst_;

        void Init(uint32_t period)
        {
            lerp_.Init(period);
            hyst_.Init(0, 1, 0.001);
        }

        void Reset(void)
        {
            lerp_.Reset();
        }

        void Sample(float sample)
        {
            // Expand range a little bit to compensate for resistive losses
            sample = hyst_.Process(0.5 + 1.0025f * (sample - 0.5f));
            lerp_.Sample(Position(sample));
        }

        float Next(void)
        {
            return lerp_.Next();
        }

        float Position(float sample)
        {
            // Correct for error caused by ADC input impedance
            sample = std::clamp(sample * 64, 0.f, 64.f);
            uint32_t index = std::clamp<int32_t>(sample + 0.5f, 0, 63);
            float frac = sample - index;
            float a = kPotCorrection[index];
            float b = kPotCorrection[index + 1];
            return a + (b - a) * frac;
        }

        static constexpr float kPotCorrection[65] =
        {
            /* [[[cog
            import math
            Q = 300e3 # ADC input impedance
            R = 50e3 # Pot value

            # Finds the pot position given a normalized ADC value
            # x on [0, 1]
            # return value on [0, 1]
            def pot_position(x):
                if x == 0:
                    return 0
                return (-Q + R*x) / (2*R*x) + 0.5*math.sqrt(
                    (Q*Q - 2*Q*R*x + 4*Q*R*x*x + R*R*x*x) / (R*R*x*x))

            size = 64
            table = [pot_position(x / size) for x in range(0, size + 1)]

            for i in range(0, size + 1, 4):
                values = table[i:i+4]
                line = ('{:.8f}, ' * len(values)).format(*values)
                cog.outl(line.strip())
            ]]]*/
            0.00000000, 0.01566516, 0.03140845, 0.04722653,
            0.06311596, 0.07907319, 0.09509456, 0.11117634,
            0.12731470, 0.14350575, 0.15974549, 0.17602989,
            0.19235483, 0.20871614, 0.22510962, 0.24153101,
            0.25797602, 0.27444033, 0.29091963, 0.30740956,
            0.32390577, 0.34040393, 0.35689972, 0.37338880,
            0.38986692, 0.40632981, 0.42277327, 0.43919315,
            0.45558533, 0.47194577, 0.48827050, 0.50455562,
            0.52079729, 0.53699178, 0.55313543, 0.56922468,
            0.58525607, 0.60122622, 0.61713189, 0.63296990,
            0.64873721, 0.66443089, 0.68004811, 0.69558615,
            0.71104242, 0.72641443, 0.74169983, 0.75689636,
            0.77200187, 0.78701436, 0.80193192, 0.81675274,
            0.83147516, 0.84609758, 0.86061857, 0.87503674,
            0.88935086, 0.90355977, 0.91766242, 0.93165786,
            0.94554523, 0.95932376, 0.97299278, 0.98655169,
            1.00000000,
            // [[[end]]]
        };
    };

    PotFilter pot_filter_[NUM_POTS];
    uint32_t current_pot_;
    static constexpr uint32_t kDMABufferSize =
        NUM_AUDIO_INS * kAudioOSFactor * 2;

    __attribute__ ((section (".dma")))
    static inline uint32_t dma_buffer_[kDMABufferSize];
    uint32_t read_index_;

    void InitGPIO(void);

    void InitDMA(void);
    void DMAService(void);
    static void DMAHandler(void);

    void InitADC(ADC_TypeDef* adc);
    void InitAudioSequence(void);
    void InitPotSequence(void);
    static void ADCHandler(void);

    void Reset(void);

    void PerformCallback(void)
    {
        PotInput pot;
        AudioInput audio;

        for (uint32_t i = 0; i < NUM_POTS; i++)
        {
            pot[i] = pot_filter_[i].Next();
        }

        for (uint32_t idx = 0; idx < kAudioOSFactor; idx++)
        {
            for (uint32_t ch = 0; ch < NUM_AUDIO_INS; ch++)
            {
                float sample = dma_buffer_[read_index_];
                read_index_ = (read_index_ + 1) % kDMABufferSize;
                audio[ch][idx] = (sample / 0xFFFF) * 2 - 1;
            }
        }

        callback_(audio, pot);
    }
};

}
