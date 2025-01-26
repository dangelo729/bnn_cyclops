#pragma once

static constexpr Profile kConf[] =
{
    #if !defined(NDEBUG) && __has_include("profiling_conf.h")
    #include "profiling_conf.h"
    #endif
    DUMMY0,
    DUMMY1,
    DUMMY2,
    DUMMY3,
    DUMMY4,
};

template <bool value>
class ActiveType
{
public:
    static constexpr bool active(void)
    {
        return value;
    }
};

class DummyOutputPin : public GPIOPin
{
public:
    static void Init([[maybe_unused]] Speed speed = SPEED_LOW,
                     [[maybe_unused]] Type  type  = TYPE_PUSHPULL,
                     [[maybe_unused]] Pull  pull  = PULL_NONE) {};
    static void Set(void) {};
    static void Clear(void) {};
    static void Toggle(void) {};
    static void Write([[maybe_unused]] bool state) {};
};

class InactivePin : public ActiveType<false>, public DummyOutputPin {};

template <uint32_t base, uint32_t pin_n>
class ActivePin : public ActiveType<true>, public OutputPin<base, pin_n> {};

template <Profile profile>
class ProfilingPin : public InactivePin {};

template <> class ProfilingPin<kConf[0]> : public ActivePin<GPIOB_BASE, 14> {};
template <> class ProfilingPin<kConf[1]> : public ActivePin<GPIOB_BASE, 15> {};
template <> class ProfilingPin<kConf[2]> : public ActivePin<GPIOB_BASE,  9> {};
template <> class ProfilingPin<kConf[3]> : public ActivePin<GPIOB_BASE,  8> {};
template <> class ProfilingPin<kConf[4]> : public ActivePin<GPIOG_BASE, 10> {};

inline void Init(void)
{
    ProfilingPin<kConf[0]>::Init(GPIOPin::SPEED_MEDIUM);
    ProfilingPin<kConf[1]>::Init(GPIOPin::SPEED_MEDIUM);
    ProfilingPin<kConf[2]>::Init(GPIOPin::SPEED_MEDIUM);
    ProfilingPin<kConf[3]>::Init(GPIOPin::SPEED_MEDIUM);
    ProfilingPin<kConf[4]>::Init(GPIOPin::SPEED_MEDIUM);
}
