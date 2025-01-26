#pragma once

#include "drivers/gpio.h"

namespace recorder
{

enum Profile
{
    PROFILE_MAIN,
    PROFILE_MAIN_LOOP,
    PROFILE_FLASH_READ,
    PROFILE_FLASH_WRITE,
    PROFILE_FLASH_ERASE,
    PROFILE_FLASH_ACCESS,
    PROFILE_SERIAL_IRQ,
    PROFILE_SERIAL_RX,
    PROFILE_SERIAL_TX,
    PROFILE_SERIAL_TX_FIFO_PUSH,
    PROFILE_SERIAL_TX_FIFO_POP,

    PROFILE_AUDIO_SAMPLING,
    PROFILE_POT_SAMPLING,
    PROFILE_POT_EOS,
    PROFILE_ADC_DMA_SERVICE,
    PROFILE_DAC_DMA_SERVICE,

    PROFILE_TICK,
    PROFILE_SLEEP,
    PROFILE_STANDBY,
    PROFILE_WATCHDOG,
    PROFILE_SYSTEM_INIT,

    PROFILE_PROCESS,

    DUMMY0,
    DUMMY1,
    DUMMY2,
    DUMMY3,
    DUMMY4,
};

namespace profiling
{

namespace impl
{
#include "profiling_impl.inc.h"
}

inline void Init(void)
{
    impl::Init();
}

}

template <Profile profile>
class ProfilingPin
{
public:
    static void Set(void)
    {
        profiling::impl::ProfilingPin<profile>::Set();
    }

    static void Clear(void)
    {
        profiling::impl::ProfilingPin<profile>::Clear();
    }

    static void Write(bool state)
    {
        profiling::impl::ProfilingPin<profile>::Write(state);
    }

    static void Toggle(void)
    {
        profiling::impl::ProfilingPin<profile>::Toggle();
    }

    static constexpr bool active(void)
    {
        return profiling::impl::ProfilingPin<profile>::active();
    }
};

template <Profile profile>
class ScopedProfilingPin
{
public:
    ScopedProfilingPin()
    {
        ProfilingPin<profile>::Set();
    }

    ~ScopedProfilingPin()
    {
        ProfilingPin<profile>::Clear();
    }
};

}
