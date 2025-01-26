#pragma once

#include <cstdint>

namespace recorder
{

template <typename T>
class Debouncer
{
public:
    void Init(uint32_t duration, bool initial_state = false)
    {
        duration_ = duration;
        count_ = 0;
        history_ = initial_state;
        state_ = initial_state;
    }

    T Process(T in)
    {
        if (in != history_)
        {
            count_ = 0;
        }
        else if (in != state_)
        {
            if (++count_ == duration_)
            {
                state_ = in;
            }
        }

        history_ = in;
        return state_;
    }

    T value(void)
    {
        return state_;
    }

protected:
    uint32_t duration_;
    uint32_t count_;
    T history_;
    T state_;
};

}