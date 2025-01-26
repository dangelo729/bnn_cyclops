#pragma once

#include <cstdint>

namespace recorder::system
{

static constexpr uint32_t kSystemClock = 64000000;

void Init(void);
void Delay_ms(uint32_t ms);

uint32_t SerialBytesAvailable(void);
uint8_t SerialGetByteBlocking(void);
void SerialFlushTx(bool discard = false);

void Standby(void);
bool WakeupWasPlayButton(void);
void Sleep(void);
void Reset(void);

void ReloadWatchdog(void);

}
