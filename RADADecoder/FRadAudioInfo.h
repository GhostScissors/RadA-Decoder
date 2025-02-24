#pragma once
#include <cstdint>

#include "FDecodeResult.h"
#include "FSoundQualityInfo.h"
#include "RadAudioDecoderHeader.h"

class FRadAudioInfo
{
public:
    static bool ParseHeader(const uint8_t* InSrcBufferData, uint32_t InSrcBufferDataSize, const FSoundQualityInfo* QualityInfo);
    static bool CreateDecoder(const uint8_t* SrcBufferData, uint32_t SrcBufferDataSize, RadAudioDecoderHeader*& Decoder, uint8_t* RawMemory, uint32_t
                              & SrcBufferOffset);
    static FDecodeResult Decode(uint8_t* CompressedData, int32_t CompressedDataSize, uint8_t* OutPCMData, int32_t OutputPCMDataSize, uint32_t
                       NumChannels, RadAudioDecoderHeader* Decoder);

    static int32_t SeekTableSize(const uint8_t);

private:
    template <typename T>
    static bool MultiplyAndCheckForOverflow(T A, T B, T& OutResult);
};
