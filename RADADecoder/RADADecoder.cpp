#include <fstream>
#include <iostream>
#include <string>
#include <memory>

#include "FRadAudioInfo.h"
#include "FSoundQualityInfo.h"
#include "WaveHeader.h"

int main()
{
    bool verbose = true;

    //std::string inputFilePath = R"(D:\Programming\RadA-Decoder\Emote_GasStation_JunoVersion_Loop.rada)";
    //std::ifstream inputFile(inputFilePath, std::ios::binary);
    
    std::string inputFilePath = R"(D:\Leaking Tools\FModel\Output\Exports\FortniteGame\Plugins\GameFeatures\BRCosmetics\Content\Sounds\MusicPacks\BuffCat_Comic\Drop_In_MusicPack_Loop.rada)";
    std::ifstream inputFile(inputFilePath, std::ios::binary);

    std::string outFilePath = R"(D:\Programming\RadA-Decoder\Output.wav)";
    std::ofstream outFile(outFilePath, std::ios::binary);

    std::shared_ptr<uint8_t> inputData;
    
    inputFile.seekg(0, std::ios::end);
    size_t inputDataSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    if (inputDataSize == 0)
    {
        std::cerr << "Input file is empty." << '\n';
        return 0;
    }

    inputData = std::shared_ptr<uint8_t>(new uint8_t[inputDataSize]);
    inputFile.read(reinterpret_cast<char*>(inputData.get()), inputDataSize);
    
    FSoundQualityInfo audioInfo;
    if (!FRadAudioInfo::ParseHeader(inputData.get(), inputDataSize, &audioInfo))
    {
        std::cerr << "Failed to parse RADA header." << '\n';
        return 0;
    }
    
    uint8_t* rawMemory = nullptr;
    RadAudioDecoderHeader* decoder = nullptr;
    uint32_t srcBufferOffset = 0;
    RadAContainer* container = nullptr;

    if (!FRadAudioInfo::CreateDecoder(inputData.get(), inputDataSize, decoder, rawMemory, srcBufferOffset, container))
    {
        std::cerr << "Failed to create decoder." << '\n';
        return 0;
    }
    
    std::unique_ptr<uint8_t> compressedData = std::unique_ptr<uint8_t>(new uint8_t[inputDataSize - srcBufferOffset]);

    bool inSkip = false;
    size_t dstOffset = 0;
    
    for (size_t i = srcBufferOffset; i < inputDataSize; i++)
    {
        if (!inSkip && i + 4 < inputDataSize && std::memcmp(&inputData.get()[i], "SEEK", 4) == 0)
        {
            inSkip = true;
            i += 3;

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
    
    uint8_t* OutPCMData = (uint8_t*)malloc(audioInfo.SampleDataSize);
    auto result = FRadAudioInfo::Decode(compressedData.get(), dstOffset, OutPCMData, audioInfo.SampleDataSize, audioInfo.NumChannels, decoder);

    WaveHeader header = {0};

    memcpy(header.riff_tag, "RIFF", 4);
    memcpy(header.wave_tag, "WAVE", 4);
    memcpy(header.fmt_tag, "fmt ", 4);
    memcpy(header.data_tag, "data", 4);
    
    header.sample_rate = audioInfo.SampleRate;
    header.num_channels = audioInfo.NumChannels;
    header.riff_length = result.shii + sizeof(WaveHeader) - 8;
    header.data_length = result.shii;

    int bits_per_sample = 16;
    
    header.fmt_length = 16;
    header.audio_format = 1;
    header.byte_rate = header.num_channels * header.sample_rate * bits_per_sample / 8;
    header.block_align = header.num_channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    
    outFile.write(reinterpret_cast<char*>(&header), sizeof(header));
    outFile.write(reinterpret_cast<char*>(OutPCMData), result.shii);
    
    std::cout << result.NumAudioFramesProduced << '\n';
    std::cout << result.NumCompressedBytesConsumed << '\n';
    std::cout << result.NumPcmBytesProduced << '\n';
    
    inputFile.close();
    outFile.close();
    
    std::cout << "Shit worked" << '\n';
}
