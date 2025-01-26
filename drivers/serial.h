#pragma once

#include <cstddef>
#include <cstdint>
#include "util/fifo.h"

namespace recorder
{

class Serial
{
public:
    void Init(uint32_t baud);
    uint32_t BytesAvailable(void);
    uint8_t GetByteBlocking(void);
    uint32_t Write(uint8_t byte, bool blocking = false);
    uint32_t Write(const char* buffer, uint32_t length, bool blocking = false);
    uint32_t Write(const uint8_t* buffer, uint32_t length,
        bool blocking = false);
    void FlushTx(bool discard = false);
    void FlushRx(void);

    template <size_t length>
    uint32_t Write(const char (&buffer)[length], bool blocking = false)
    {
        return Write(buffer, length - 1, blocking);
    }

protected:
    static constexpr uint32_t kRxFifoSize = 64;
    static constexpr uint32_t kTxFifoSize = 256;

    static inline Serial* instance_;

    Fifo<uint8_t, kRxFifoSize> rx_fifo_;
    Fifo<uint8_t, kTxFifoSize> tx_fifo_;

    void InterruptService(void);
    static void InterruptHandler(void);
};

}
