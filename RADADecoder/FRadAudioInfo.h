#pragma once
#include <cstdint>

#include "FSoundQualityInfo.h"
#include "RadAudioDecoderHeader.h"

class FRadAudioInfo
{
public:
    static bool ParseHeader(const uint8_t* InSrcBufferData, uint32_t InSrcBufferDataSize, const FSoundQualityInfo* QualityInfo);
    static bool CreateDecoder(const uint8_t* SrcBufferData, uint32_t SrcBufferDataSize, RadAudioDecoderHeader*& Decoder, uint8_t*& RawMemory, uint32_t&
                              SrcBufferOffset, RadAContainer*& Container);
    static void Decode(const uint8_t* compressedData, int32_t compressedDataSize, uint8_t* outPCMData, int32_t
                       outputPCMDataSize, RadAudioDecoderHeader
                       * decoder, int32_t numChannels);

private:
    template <typename T>
    static bool MultiplyAndCheckForOverflow(T A, T B, T& OutResult);
};
