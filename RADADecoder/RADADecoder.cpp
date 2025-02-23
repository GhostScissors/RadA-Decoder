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
    
    uint8_t* compressedData = inputData.get() + srcBufferOffset;
    
    uint8_t* data = nullptr;
    std::cout << "Shit worked" << '\n';
}
