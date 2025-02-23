#pragma once

#define RADA_WRAP MIRA

#include <cstdint>
#include "SDK/Include/rada_decode.h"

struct RadAudioDecoderHeader
{
    uint32_t SeekTableByteCount;

    // # of frames we need to eat before outputting data due to a sample
    // accurate seek.
    int32_t ConsumeFrameCount;
    uint16_t OutputReservoirValidFrames;
    uint16_t OutputReservoirReadFrames;

    RadAContainer* Container()
    {
        return reinterpret_cast<RadAContainer*>(this + 1);
    }
};
