#pragma once
#include <cstdint>

struct FDecodeResult
{
    int32_t shii;
    
    // Number of bytes of compressed data consumed
    int32_t NumCompressedBytesConsumed;
    // Number of bytes produced
    int32_t NumPcmBytesProduced;
    // Number of frames produced.
    int32_t NumAudioFramesProduced;
};
