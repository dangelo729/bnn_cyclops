#pragma once

#include <cstdint>

namespace recorder
{

class EdgeDetector
{
public:
    void Init(bool initial_state = false)
    {
        history_ = initial_state ? ((1 << kShift) | 1) : 0;
    }

    void Process(bool level)
    {
        history_ = (history_ << kShift) | level;
    }

    bool rising(void)
    {
        return history_ == 1;
    }

    bool falling(void)
    {
        return history_ == (1 << kShift);
    }

    bool level(void)
    {
        return history_ & 1;
    }

    bool is_high(void)
    {
        return level();
    }

    bool is_low(void)
    {
        return !level();
    }

    bool was_high(void)
    {
        return (history_ & (1 << kShift));
    }

    bool was_low(void)
    {
        return !(history_ & (1 << kShift));
    }

    bool steady_high(void)
    {
        return history_ == ((1 << kShift) | 1);
    }

    bool steady_low(void)
    {
        return history_ == 0;
    }

protected:
    uint32_t history_;
    static constexpr uint32_t kShift = 16;
};

}
