#pragma once

#include <cstdint>

namespace recorder
{

template <typename T>
struct __attribute__ ((packed)) Packet
{
    uint8_t size;
    uint8_t checksum;
    T payload;

    void Init(void)
    {
        size = 0;
        checksum = 0;
    }

    bool Verify(void)
    {
        if (size > sizeof(T))
        {
            return false;
        }

        auto bytes = reinterpret_cast<uint8_t*>(&payload);
        uint8_t sum = 0;

        for (uint32_t i = 0; i < sizeof(T); i++)
        {
            sum += bytes[i];
        }

        return sum == checksum;
    }

    void Sign(void)
    {
        size = sizeof(T);
        auto bytes = reinterpret_cast<uint8_t*>(&(payload));
        checksum = 0;

        for (uint32_t i = 0; i < sizeof(T); i++)
        {
            checksum += bytes[i];
        }
    }
};

}
