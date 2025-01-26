#pragma once

#include <cstdint>
#include <cstring>

namespace a85
{

inline size_t Encode(char* buf, size_t buf_size, const void* data, size_t data_size)
{
    auto bytes = reinterpret_cast<const uint8_t*>(data);
    size_t num_chars = 0;

    while (data_size)
    {
        size_t padding = (data_size >= 4) ? 0 : (-data_size % 4) * 5 / 4;
        uint32_t word = 0;

        for (size_t i = 0; i < 4; i++)
        {
            word <<= 8;

            if (data_size)
            {
                word |= *bytes++;
                data_size--;
            }
        }

        uint8_t group[5];

        for (size_t i = 0; i < 5; i++)
        {
            group[4 - i] = (word % 85);
            word /= 85;
        }

        for (size_t i = 0; i < 5 - padding; i++)
        {
            if (num_chars < buf_size - 1)
            {
                buf[num_chars] = group[i] + 33;
                num_chars++;
            }
        }
    }

    buf[num_chars] = '\0';
    return num_chars;
}

inline size_t Decode(void* data, size_t size, const char* str)
{
    auto bytes = reinterpret_cast<uint8_t*>(data);
    size_t length = std::strlen(str);
    size_t num_bytes = 0;

    while (length)
    {
        size_t padding = (length >= 5) ? 0 : (-length % 5) * 4 / 5;
        uint8_t group[5];

        for (size_t i = 0; i < 5; i++)
        {
            group[i] = length ? *str - 33 : 84;

            if (length)
            {
                str++;
                length--;
            }
        }

        uint32_t word = 0;

        for (size_t i = 0; i < 5; i++)
        {
            word *= 85;
            word += group[i];
        }

        for (size_t i = 0; i < 4 - padding; i++)
        {
            if (num_bytes < size)
            {
                bytes[num_bytes] = word >> 24;
                num_bytes++;
                word <<= 8;
            }
        }
    }

    return num_bytes;
}

}
