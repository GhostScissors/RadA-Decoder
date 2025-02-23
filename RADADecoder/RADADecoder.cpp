#include <fstream>
#include <iostream>
#include <string>
#include <memory>

#include "FRadAudioInfo.h"
#include "FSoundQualityInfo.h"

int main()
{
    std::string inputFilePath = R"(D:\Programming\RADADecoder\Emote_GasStation_JunoVersion_Loop.rada)";
    std::ifstream inputFile(inputFilePath, std::ios::binary);

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

    for (size_t i = 0; i < inputDataSize; i++)
    {
        if (!inSkip && i + 4 < inputDataSize && std::memcmp(&inputData.get()[i], "SEEK", 4) == 0)
        {
            inSkip = true;
            i += 3;
        }
        else if (inSkip && i + 2 < inputDataSize && inputData.get()[i] == 0x99 && inputData.get()[i + 1] == 0x99)
        {
            inSkip = false;
            compressedData.get()[dstOffset++] = 0x99;
            compressedData.get()[dstOffset++] = 0x99;
            i += 1; // Skip 0x9999
        }
        else if (!inSkip)
        {
            compressedData.get()[dstOffset++] = inputData.get()[i];
        }
    }
    
    uint8_t* OutPCMData = (uint8_t*)malloc(audioInfo.SampleDataSize);
    if (!FRadAudioInfo::Decode(compressedData.get(), dstOffset, OutPCMData, audioInfo.SampleDataSize, audioInfo.NumChannels, decoder))
    {
        std::cerr << "Failed to decode RADA file" << '\n';
        return 0;
    }
    
    std::cout << "Shit worked" << '\n';
}
