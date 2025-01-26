#pragma once

#include <cstdint>

namespace recorder
{

struct __attribute__ ((packed)) Message
{
    enum Type
    {
        TYPE_NONE,
        TYPE_TEXT,
        TYPE_PING = 'p',
        TYPE_RESET = 'r',
        TYPE_QUERY = 'q',
        TYPE_STANDBY = 's',
        TYPE_ERASE = 'e',
        TYPE_WATCHDOG = 'w',
    };

    uint8_t type;

    union
    {
        char text[128];
    };
};

}
