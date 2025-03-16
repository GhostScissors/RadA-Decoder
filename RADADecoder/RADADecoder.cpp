#include <fstream>
#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "argparse.hpp"
#include "FRadAudioInfo.h"
#include "FSoundQualityInfo.h"
#include "WaveHeader.h"

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("RADADecoder");
    program.add_argument("-i", "--input-file").required().help("Input File to Decode");
    program.add_argument("-o", "--output-file").required().help("Path to Output File");
    program.add_argument("-v", "--verbose").default_value(false).implicit_value(true).help("Verbose Output");

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cout << err.what() << '\n';
        std::cout << program;
        return -1;
    }
    
    std::string inputFilePath = program.get<std::string>("-i");
    std::ifstream inputFile(inputFilePath, std::ios::binary);
    
    if (!inputFile)
    {
        std::cerr << "Failed to open input file: " << inputFilePath << '\n';
        return -1;
    }
    
    std::string outputFilePath = program.get<std::string>("-o");
    std::ofstream outFile(outputFilePath, std::ios::binary);
    
    if (!outFile)
    {
        std::cerr << "Failed to open output file: " << outputFilePath << '\n';
        return -1;
    }

    bool verbose = program.get<bool>("-v");

    if (verbose)
    {
        std::cout << "Input File: " << inputFilePath << '\n';
        std::cout << "Output File: " << outputFilePath << '\n';
    }
    
    inputFile.seekg(0, std::ios::end);
    size_t inputDataSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    if (inputDataSize == 0)
    {
        std::cerr << "Input file is empty." << '\n';
        return 0;
    }

    std::shared_ptr<uint8_t> inputData = std::shared_ptr<uint8_t>(new uint8_t[inputDataSize],
                                             std::default_delete<uint8_t[]>());
    inputFile.read((char *) inputData.get(), inputDataSize);
    
    FSoundQualityInfo audioInfo;
    if (!FRadAudioInfo::ParseHeader(inputData.get(), inputDataSize, &audioInfo))
    {
        std::cerr << "Failed to parse RADA header." << '\n';
        return -1;
    }

    if (verbose)
    {
        std::cout << "FSoundQualityInfo->SampleRate: " << audioInfo.SampleRate << '\n';
        std::cout << "FSoundQualityInfo->NumChannels: " << audioInfo.NumChannels << '\n';
        std::cout << "FSoundQualityInfo->SampleDataSize" << audioInfo.SampleDataSize << '\n';
        std::cout << "FSoundQualityInfo->Duration: " << audioInfo.Duration << '\n';
    }
    
    uint8_t* rawMemory = nullptr;
    RadAudioDecoderHeader* decoder = nullptr;
    uint32_t srcBufferOffset = 0;
    if (!FRadAudioInfo::CreateDecoder(inputData.get(), inputDataSize, decoder, rawMemory, srcBufferOffset))
    {
        std::cerr << "Failed to create decoder." << '\n';
        return -1;
    }

    if (verbose)
    {
        std::cout << "RadAudioDecoderHeader->SeekTableByteCount" << decoder->SeekTableByteCount << '\n';
        std::cout << "RadAudioDecoderHeader->ConsumeFrameCount" << decoder->ConsumeFrameCount << '\n';
        std::cout << "RadAudioDecoderHeader->OutputReservoirValidFrames" << decoder->OutputReservoirValidFrames << '\n';
        std::cout << "RadAudioDecoderHeader->OutputReservoirReadFrames" << decoder->OutputReservoirReadFrames << '\n';
    }
    
    std::unique_ptr<uint8_t> compressedData =
      std::unique_ptr<uint8_t>(new uint8_t[inputDataSize - srcBufferOffset]);
    bool inSkip = false;
    size_t dstOffset = 0;
    
    for (size_t i = srcBufferOffset; i < inputDataSize; i++)
    {
        if (!inSkip && i + 4 < inputDataSize && std::memcmp(&inputData.get()[i], "SEEK", 4) == 0)
        {
            inSkip = true;
            i += 1;
        }
        else if (inSkip && i + 2 < inputDataSize && inputData.get()[i] == 0x55)
        {
            inSkip = false;
            compressedData.get()[dstOffset++] = 0x55;
        }
        else if (!inSkip)
        {
            compressedData.get()[dstOffset++] = inputData.get()[i];
        }
    }

    if (verbose)
    {
        std::cout << "CompressedDataSize" << dstOffset << '\n';
    }
    
    uint8_t *OutPCMData = (uint8_t *) malloc(audioInfo.SampleDataSize);
    auto result = FRadAudioInfo::Decode(compressedData.get(), dstOffset, OutPCMData, audioInfo.SampleDataSize, audioInfo.NumChannels, decoder);

    if (result.NumPcmBytesProduced == 0)
    {
        std::cerr << "Failed to decode." << '\n';
        return -1;
    }
    
    WaveHeader header;

    memcpy(header.riff_tag, "RIFF", 4);
    memcpy(header.wave_tag, "WAVE", 4);
    memcpy(header.fmt_tag, "fmt ", 4);
    memcpy(header.data_tag, "data", 4);
    
    header.sample_rate = audioInfo.SampleRate;
    header.num_channels = audioInfo.NumChannels;
    header.riff_length = result.DataLength + sizeof(WaveHeader) - 8;
    header.data_length = result.DataLength;
    
    int bits_per_sample = 16;
    
    header.fmt_length = 16;
    header.audio_format = 1;
    header.byte_rate = header.num_channels * header.sample_rate * bits_per_sample / 8;
    header.block_align = header.num_channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    
    outFile.write(reinterpret_cast<char*>(&header), sizeof(header));
    outFile.write(reinterpret_cast<char*>(OutPCMData), result.DataLength);
    
    std::cout << "Audio Frames Produced: " << result.NumAudioFramesProduced << '\n';
    std::cout << "Compressed Bytes Consumed: " << result.NumCompressedBytesConsumed << '\n';
    std::cout << "PCM Bytes Produced: " << result.NumPcmBytesProduced << '\n';
    
    inputFile.close();
    outFile.close();
    
    std::cout << "Processing complete." << '\n';

    return 0;
}
