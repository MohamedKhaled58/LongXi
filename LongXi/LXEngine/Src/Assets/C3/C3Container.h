#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Assets/C3/C3Types.h"

namespace LXEngine
{

class C3Container
{
public:
    struct ChunkInfo
    {
        FourCC   type   = 0;
        uint32_t offset = 0;
        uint32_t size   = 0;
    };

    struct RawChunk
    {
        ChunkInfo            info{};
        std::vector<uint8_t> data;
    };

    bool LoadFromMemory(const std::vector<uint8_t>& data);
    void Reset();

    bool IsValid() const
    {
        return m_IsValid;
    }

    const std::string& GetVersion() const
    {
        return m_Version;
    }

    const std::vector<ChunkInfo>& GetChunks() const
    {
        return m_Chunks;
    }

    const std::vector<RawChunk>& GetRawChunks() const
    {
        return m_RawChunks;
    }

    static bool IsChunkInBounds(size_t dataSize, uint32_t offset, uint32_t size);

private:
    bool ValidateHeader(const std::vector<uint8_t>& data);
    bool ParseChunks(const std::vector<uint8_t>& data, size_t startOffset);

private:
    bool                   m_IsValid = false;
    std::string            m_Version;
    std::vector<uint8_t>   m_Data;
    std::vector<ChunkInfo> m_Chunks;
    std::vector<RawChunk>  m_RawChunks;
};

} // namespace LXEngine
