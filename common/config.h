#pragma once

#include <cstdint>
#include <cmath>

namespace recorder
{

constexpr float kAudioSampleRate = 16000;
constexpr uint32_t kAudioOSFactor = std::ceil(48000.0 / kAudioSampleRate);
constexpr float kAudioOSRate = kAudioSampleRate * kAudioOSFactor;

constexpr float kAudioOutputLevel = 1.4;
constexpr float kAudioFadeTime = 20e-3;
constexpr uint32_t kButtonDebounceDuration_ms = 10;
constexpr float kIdleStandbyTime = 60;
constexpr float kPlaybackExpireTime = 60 * 5;

constexpr uint32_t kProfileIRQPriority = 0;
constexpr uint32_t kADCIRQPriority = 1;
constexpr uint32_t kTickIRQPriority = 10;
constexpr uint32_t kSerialIRQPriority = 11;

constexpr bool kEnableDelay = true;
constexpr bool kEnableLineIn = VARIANT_LINE_IN;
constexpr bool kEnableReverse = true;

#if __has_include("config.inc.h")
#include "config.inc.h"
#endif

#if !defined(NDEBUG) && defined(DISABLE_IDLE_STANDBY)
constexpr bool kEnableIdleStandby = false;
#else
constexpr bool kEnableIdleStandby = true;
#endif

#if !defined(NDEBUG) && defined(ADC_ALWAYS_ON)
constexpr bool kADCAlwaysOn = true;
#else
constexpr bool kADCAlwaysOn = false;
#endif

}
