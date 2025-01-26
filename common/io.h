#pragma once

#include <cstdint>
#include <array>
#include "common/config.h"

namespace recorder
{

enum PotID
{
    POT_1,
    POT_2,
    POT_3,
    POT_4,
    POT_5,
    POT_6,
    POT_7,
    POT_PHOTOCELL,
    NUM_POTS,
};

enum SwitchID
{
    SWITCH_PLAY,
    SWITCH_RECORD,
    SWITCH_LOOP,
    SWITCH_TUNE,
    SWITCH_SCRUB,
    SWITCH_EFFECT,
    SWITCH_REVERSE,
    NUM_SWITCHES,
};

enum DetectID
{
    DETECT_LINE_IN,
    NUM_DETECTS,
};

enum AudioInputID
{
    AUDIO_IN_MIC,
    AUDIO_IN_LINE,
    NUM_AUDIO_INS,
};

enum AudioOutputID
{
    AUDIO_OUT_LINE,
    NUM_AUDIO_OUTS,
};

using PotInput = std::array<float, NUM_POTS>;

struct HumanInput
{
    PotInput pot;
    bool sw[NUM_SWITCHES];
    bool detect[NUM_DETECTS];

    void Init(void)
    {
        *this = {};
    }
};

struct HumanIO
{
    HumanInput in;

    void Init(void)
    {
        in.Init();
    }
};

using AudioInput = std::array<float[kAudioOSFactor], NUM_AUDIO_INS>;
using AudioOutput = std::array<float[kAudioOSFactor], NUM_AUDIO_OUTS>;

struct AudioIO
{
    AudioInput in;
    AudioOutput out;

    void Init(void)
    {
        *this = {};
    }
};

struct DeviceIO
{
    HumanIO human;

    void Init(void)
    {
        human.Init();
    }
};

}
