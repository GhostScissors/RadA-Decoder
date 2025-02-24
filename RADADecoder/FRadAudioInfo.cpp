#include "FRadAudioInfo.h"

#include <fstream>
#include <iostream>

#include "SDK/Include/rada_decode.h"

#include <random>

#include "FDecodeResult.h"

bool FRadAudioInfo::ParseHeader(const uint8_t* InSrcBufferData, uint32_t InSrcBufferDataSize, const FSoundQualityInfo* QualityInfo)
{
    const RadAFileHeader* FileHeader = RadAGetFileHeader(InSrcBufferData, InSrcBufferDataSize);
    if (FileHeader == nullptr)
        return false;

    uint32_t trueSampleCount = 0;
    if (MultiplyAndCheckForOverflow<uint32_t>(FileHeader->frame_count, FileHeader->channels, trueSampleCount) == false)
        return false;

    uint8_t NumChannels = FileHeader->channels;

    // seek_table_entry_count is 16 bit, no overflow possible
    uint32_t SeekTableSize = FileHeader->seek_table_entry_count * sizeof(uint16_t);
    if (sizeof(RadAFileHeader) + SeekTableSize > InSrcBufferDataSize)
        return false;

    // Store the offset to where the audio data begins.
    auto audioDataOffset = RadAGetBytesToOpen(FileHeader);

    // Check we have all headers and seek table data in memory.
    if (audioDataOffset > InSrcBufferDataSize)
        return false;

    // Write out the header info
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
    
    const RadAFileHeader* FileHeader = MIRARadAGetFileHeader_1(SrcBufferData, SrcBufferDataSize);
    if (FileHeader == nullptr)
        return false;
    
    uint32_t DecoderMemoryNeeded = 0;
    if (MIRARadAGetMemoryNeededToOpen_1(SrcBufferData, SrcBufferDataSize, &DecoderMemoryNeeded) != 0)
    {
        printf("Invalid/insufficient data in FRadAudioInfo::CreateDecoder - bad buffer passed / bad cook? Size = %d", SrcBufferDataSize);
        if (SrcBufferDataSize > 8)
            printf("First 8 bytes: 0x%llx", *(uint64_t*)SrcBufferData);

        return false;
    }

    uint32_t TotalMemory = DecoderMemoryNeeded + sizeof(RadAudioDecoderHeader);

    //
    // Allocate and save offsets
    //
    RawMemory = (uint8_t*)malloc(TotalMemory);
    memset(RawMemory, 0, TotalMemory);

    Decoder = (RadAudioDecoderHeader*)RawMemory;

    Container = Decoder->Container();

    if (MIRARadAOpenDecoder_1(SrcBufferData, SrcBufferDataSize, Container, DecoderMemoryNeeded) == 0)
    {
        printf("Failed to open decoder, likely corrupted data.");
        free(RawMemory);
        return false;
    }

    // Set our buffer at the start of the audio data.
    SrcBufferOffset = RadAGetBytesToOpen(FileHeader);
    return true;
}

FDecodeResult FRadAudioInfo::Decode(uint8_t* CompressedData, const int32_t CompressedDataSize, uint8_t* OutPCMData, const int32_t OutputPCMDataSize, uint32_t NumChannels, RadAudioDecoderHeader* Decoder)
{
    uint32_t SampleStride = NumChannels * sizeof(int16_t); // constexpr uint32 MONO_PCM_SAMPLE_SIZE = sizeof(int16);
    uint32_t RemnOutputFrames = OutputPCMDataSize / SampleStride;
    uint32_t RemnCompressedDataSize = CompressedDataSize;
    const uint8_t* CompressedDataEnd = CompressedData + CompressedDataSize;

    std::vector<float> DeinterleavedDecodeBuffer;
    std::vector<uint8_t> OutputReservoir;

    while (RemnOutputFrames)
    {
        // Drain the output reservoir before attempting a decode.
        if (Decoder->OutputReservoirReadFrames < Decoder->OutputReservoirValidFrames)
        {
            uint32_t AvailableFrames = Decoder->OutputReservoirReadFrames - Decoder->OutputReservoirValidFrames;
            uint32_t CopyFrames = std::min(AvailableFrames, RemnOutputFrames);

            uint32_t CopyByteCount = SampleStride * CopyFrames;
            uint32_t CopyOffset = SampleStride * Decoder->OutputReservoirReadFrames;
            memcpy(OutPCMData, OutputReservoir.data() + CopyOffset, CopyByteCount);

            Decoder->OutputReservoirReadFrames += CopyFrames;
            RemnOutputFrames -= CopyFrames;
            OutPCMData += CopyByteCount;

            if (RemnOutputFrames == 0)
                break;
        }

        if (RemnCompressedDataSize == 0)
            break;
        
        uint32_t CompressedBytesNeeded = 0;
        RadAExamineBlockResult BlockResult = RadAExamineBlock(Decoder->Container(), CompressedData, RemnCompressedDataSize, &CompressedBytesNeeded);
        
        if (BlockResult != RadAExamineBlockResult::Valid)
        {
            uint32_t dstOffset = 0;
            bool bFound = false;

            for (int i = 1; i < RemnCompressedDataSize; i++)
            {
                if (CompressedData[i] == 0x55)
                {
                    dstOffset = i;
                    bFound = true;
                    break;
                }
            }

            if (bFound)
            {
                size_t newSize = RemnCompressedDataSize - dstOffset;
                std::memmove(CompressedData, CompressedData + dstOffset, newSize);
                RemnCompressedDataSize = newSize; // Update the new size of compressed data
            }

            RadAExamineBlockResult newBlockResult = RadAExamineBlock(Decoder->Container(), CompressedData, RemnCompressedDataSize, &CompressedBytesNeeded);

            if (newBlockResult != RadAExamineBlockResult::Valid || !bFound)
            {
                printf("Invalid block in FRadAudioInfo::Decode: Result = %d, RemnSize = %d \n", BlockResult, RemnCompressedDataSize);
                if (RemnCompressedDataSize >= 8)
                    printf("First 8 bytes of buffer: 0x%02x 0x%02x 0x%02x 0x%02x:0x%02x 0x%02x 0x%02x 0x%02x \n", CompressedData[0], CompressedData[1], CompressedData[2], CompressedData[3], CompressedData[4], CompressedData[5], CompressedData[6], CompressedData[7]);

                break;
            }
        }

        // RadAudio outputs deinterleaved 32-bit float buffers - we don't want to carry around
        // those buffers all the time and rad audio uses a pretty healthy amount of stack already,
        // so we drop these in the temp buffers.
        if (DeinterleavedDecodeBuffer.empty())
            DeinterleavedDecodeBuffer = std::vector<float>(RadADecodeBlock_MaxOutputFrames * NumChannels);
        
        size_t CompressedDataConsumed = 0;
        int16_t DecodeResult = RadADecodeBlock(Decoder->Container(), CompressedData, RemnCompressedDataSize, DeinterleavedDecodeBuffer.data(), RadADecodeBlock_MaxOutputFrames, &CompressedDataConsumed);

        if (DecodeResult == RadADecodeBlock_Error)
        {
            printf("Failed to decode block that passed validation checks, corrupted buffer? \n");
            return FDecodeResult();
        }
        else if (DecodeResult == RadADecodeBlock_Done)
        {
            break;
        }
        
        CompressedData += CompressedDataConsumed;
        RemnCompressedDataSize -= CompressedDataConsumed;

        int16_t DecodeResultOffset = 0;

        if (Decoder->ConsumeFrameCount)
        {
            int16_t ConsumedThisTime = DecodeResult;
            if (Decoder->ConsumeFrameCount < DecodeResult)
            {
                ConsumedThisTime = (int16_t)Decoder->ConsumeFrameCount;
            }

            if (ConsumedThisTime)
            {
                DecodeResultOffset = ConsumedThisTime;
                DecodeResult -= ConsumedThisTime;
            }
            Decoder->ConsumeFrameCount -= ConsumedThisTime;
        }

        if (DecodeResult == 0)
            continue;

        int16_t* InterleaveDestination = (int16_t*)OutPCMData;
        if (RemnOutputFrames < RadADecodeBlock_MaxOutputFrames)
        {
            if (OutputReservoir.empty())
                OutputReservoir.reserve(RadADecodeBlock_MaxOutputFrames * SampleStride);

            InterleaveDestination = (int16_t*)OutputReservoir.data();
        }

        for (int32_t ChannelIdx = 0; ChannelIdx < NumChannels; ChannelIdx++)
        {
            const float* InBuffer = DeinterleavedDecodeBuffer.data() + RadADecodeBlock_MaxOutputFrames*ChannelIdx + DecodeResultOffset;
			
            for (int32_t SampleIdx = 0; SampleIdx < DecodeResult; SampleIdx++)
            {
                float InBufferFloat = InBuffer[SampleIdx] * 32768.0f;
                if (InBufferFloat > 32767)
                {
                    InBufferFloat = 32767;
                }
                else if (InBufferFloat < -32768)
                {
                    InBufferFloat = -32768;
                }

                InterleaveDestination[ChannelIdx + (SampleIdx * NumChannels)] = (int16_t)InBufferFloat;
            }
        }

        if (InterleaveDestination == (int16_t*)OutputReservoir.data())
        {
            Decoder->OutputReservoirValidFrames = DecodeResult;
            Decoder->OutputReservoirReadFrames = 0;
        }
        else
        {
            RemnOutputFrames -= DecodeResult;
            OutPCMData += DecodeResult * SampleStride;
        }
    }
    
    FDecodeResult Result;

    Result.DataLength = OutputPCMDataSize - RemnOutputFrames;
    Result.NumPcmBytesProduced = OutputPCMDataSize - (RemnOutputFrames * SampleStride);
    Result.NumAudioFramesProduced = Result.NumPcmBytesProduced / SampleStride;
    Result.NumCompressedBytesConsumed = CompressedDataSize - RemnCompressedDataSize;
    
    return Result;
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