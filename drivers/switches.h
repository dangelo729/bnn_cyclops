#pragma once

#include <cstdint>

#include "drivers/gpio.h"
#include "drivers/system.h"
#include "common/config.h"
#include "common/io.h"
#include "util/debouncer.h"

namespace recorder
{

class Switches
{
public:
    void Init(void)
    {
        sw_[SWITCH_RECORD].Init(GPIOA_BASE, 0, true, GPIOPin::PULL_UP);
        sw_[SWITCH_PLAY].Init(GPIOA_BASE, 2, true, GPIOPin::PULL_UP);
        sw_[SWITCH_LOOP].Init(GPIOD_BASE, 11, true, GPIOPin::PULL_UP);
        sw_[SWITCH_TUNE].Init(GPIOC_BASE, 4, true, GPIOPin::PULL_UP);
        sw_[SWITCH_EFFECT].Init(GPIOA_BASE, 1, true, GPIOPin::PULL_UP);
        detect_[DETECT_LINE_IN].Init(GPIOC_BASE, 11, true, GPIOPin::PULL_DOWN);

        if (kEnableReverse)
        {
            sw_[SWITCH_REVERSE].Init(GPIOC_BASE, 4, false, GPIOPin::PULL_DOWN);
        }

        db_[SWITCH_RECORD].Init(kButtonDebounceDuration_ms);
        db_[SWITCH_PLAY].Init(kButtonDebounceDuration_ms,
            system::WakeupWasPlayButton());
        db_[SWITCH_LOOP].Init(kButtonDebounceDuration_ms);
        db_[SWITCH_EFFECT].Init(kButtonDebounceDuration_ms);
        db_[SWITCH_TUNE].Init(kButtonDebounceDuration_ms);
        db_[SWITCH_REVERSE].Init(kButtonDebounceDuration_ms);
        db_[NUM_SWITCHES + DETECT_LINE_IN].Init(kButtonDebounceDuration_ms);
    }

    void Process(HumanInput& in)
    {
        for (uint32_t i = 0; i < NUM_SWITCHES; i++)
        {
            if (kEnableReverse || i != SWITCH_REVERSE)
            {
                in.sw[i] = db_[i].Process(sw_[i].Read());
            }
            else
            {
                in.sw[i] = false;
            }
        }

        for (uint32_t i = 0; i < NUM_DETECTS; i++)
        {
            in.detect[i] = kEnableLineIn &&
                db_[NUM_SWITCHES + i].Process(detect_[i].Read());
        }
    }

protected:
    GenericInputPin sw_[NUM_SWITCHES];
    GenericInputPin detect_[NUM_DETECTS];
    Debouncer<bool> db_[NUM_SWITCHES + NUM_DETECTS];
};

}
