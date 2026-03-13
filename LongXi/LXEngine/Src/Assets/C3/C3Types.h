#pragma once

#include <cstdint>
#include <string>

namespace LXEngine
{

using FourCC = uint32_t;

constexpr FourCC MakeFourCC(char a, char b, char c, char d)
{
    return (static_cast<FourCC>(static_cast<uint8_t>(a))) | (static_cast<FourCC>(static_cast<uint8_t>(b)) << 8) |
           (static_cast<FourCC>(static_cast<uint8_t>(c)) << 16) | (static_cast<FourCC>(static_cast<uint8_t>(d)) << 24);
}

inline std::string FourCCToString(FourCC value)
{
    char text[5] = {static_cast<char>(value & 0xFF),
                    static_cast<char>((value >> 8) & 0xFF),
                    static_cast<char>((value >> 16) & 0xFF),
                    static_cast<char>((value >> 24) & 0xFF),
                    '\0'};
    return std::string(text);
}

constexpr FourCC C3ChunkPhy         = MakeFourCC('P', 'H', 'Y', ' ');
constexpr FourCC C3ChunkPhy4        = MakeFourCC('P', 'H', 'Y', '4');
constexpr FourCC C3ChunkMoti        = MakeFourCC('M', 'O', 'T', 'I');
constexpr FourCC C3ChunkPtcl        = MakeFourCC('P', 'T', 'C', 'L');
constexpr FourCC C3ChunkPtcl3       = MakeFourCC('P', 'T', 'C', '3');
constexpr FourCC C3ChunkCame        = MakeFourCC('C', 'A', 'M', 'E');
constexpr FourCC C3ChunkSecondaryId = MakeFourCC('2', 'S', 'I', 'D');
constexpr FourCC C3ChunkKKey        = MakeFourCC('K', 'K', 'E', 'Y');
constexpr FourCC C3ChunkZKey        = MakeFourCC('Z', 'K', 'E', 'Y');

} // namespace LXEngine
