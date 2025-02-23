#include "FRadAudioInfo.h"

#include <random>

bool FRadAudioInfo::ParseHeader(const uint8_t* InSrcBufferData, uint32_t InSrcBufferDataSize, const FSoundQualityInfo* QualityInfo)
{
    const RadAFileHeader* FileHeader = RadAGetFileHeader(InSrcBufferData, InSrcBufferDataSize);
    if (FileHeader == nullptr)
        return false;

    uint32_t trueSampleCount = 0;
    if (MultiplyAndCheckForOverflow<uint32_t>(FileHeader->frame_count, FileHeader->channels, trueSampleCount) == false)
        return false;

    uint8_t NumChannels = FileHeader->channels;

    uint32_t SeekTableSize = FileHeader->seek_table_entry_count * sizeof(uint16_t);
    if (sizeof(RadAFileHeader) + SeekTableSize > InSrcBufferDataSize)
        return false;

    auto audioDataOffset = RadAGetBytesToOpen(FileHeader);
    if (audioDataOffset > InSrcBufferDataSize)
        return false;

    if (QualityInfo)
    {
        QualityInfo->SampleRate = RadASampleRateFromEnum(FileHeader->sample_rate);
        QualityInfo->NumChannels = FileHeader->channels;
        QualityInfo->SampleDataSize = FileHeader->frame_count * QualityInfo->NumChannels * sizeof(int16_t);
        if (QualityInfo->SampleRate)
        {
            QualityInfo->Duration = (float)FileHeader->frame_count / QualityInfo->SampleRate;
        }
    }

    return true;
}

bool FRadAudioInfo::CreateDecoder(const uint8_t* SrcBufferData, uint32_t SrcBufferDataSize, RadAudioDecoderHeader*& Decoder, uint8_t*& RawMemory, uint32_t& SrcBufferOffset, RadAContainer*& Container)
{
    if (SrcBufferOffset != 0)
        return false;

    const RadAFileHeader* FileHeader = RadAGetFileHeader(SrcBufferData, SrcBufferDataSize);
    if (FileHeader == nullptr)
        return false;

    uint32_t DecoderMemoryNeeded = 0;
    if (RadAGetMemoryNeededToOpen(SrcBufferData, SrcBufferDataSize, &DecoderMemoryNeeded) != 0)
    {
        printf("Invalid/insufficient data in FRadAudioInfo::CreateDecoder - bad buffer passed / bad cook? Size = %d", SrcBufferDataSize);
        if (SrcBufferDataSize > 8)
            printf("First 8 bytes: 0x%llx", *(uint64_t*)SrcBufferData);

        return false;
    }

    uint32_t TotalMemory = DecoderMemoryNeeded + sizeof(RadAudioDecoderHeader);
}


template <typename T>
bool FRadAudioInfo::MultiplyAndCheckForOverflow(T A, T B, T& OutResult)
{
    if (std::is_integral_v<T>)
    {
        if (A > 0)
        {
            if (B > 0)
            {
                if (A > (std::numeric_limits<T>::max() / B))
                    return false;
            }
            else // B < 0
            {
                if (B < (std::numeric_limits<T>::min() / A))
                    return false;
            }
        }
        else
        {
            if (B > 0)
            {
                if (A < std::numeric_limits<T>::min() / B)
                    return false;
            }
            else
            {
                if (A < std::numeric_limits<T>::max() / B)
                    return false;
            }
        }
    }

    OutResult = A * B;
    return true;
}