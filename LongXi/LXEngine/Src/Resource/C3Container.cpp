#include "Resource/C3Container.h"

#include <algorithm>
#include <cstring>

#include "Resource/C3BinaryReader.h"
#include "Resource/C3Log.h"

namespace LongXi
{

namespace
{
constexpr size_t      kC3VersionSize = 16;
constexpr const char* kC3VersionTag  = "MAXFILE C3 00001";

#pragma pack(push, 1)

struct C3ChunkHeader
{
    uint8_t  id[4];
    uint32_t size;
};

#pragma pack(pop)
static_assert(sizeof(C3ChunkHeader) == 8, "C3ChunkHeader must be 8 bytes");

std::string TrimVersion(const std::string& input)
{
    std::string trimmed = input;
    while (!trimmed.empty())
    {
        char back = trimmed.back();
        if (back == '\0' || back == ' ')
        {
            trimmed.pop_back();
        }
        else
        {
            break;
        }
    }
    return trimmed;
}

} // namespace

bool C3Container::LoadFromMemory(const std::vector<uint8_t>& data)
{
    Reset();
    m_Data = data;

    if (!ValidateHeader(m_Data))
    {
        return false;
    }

    if (!ParseChunks(m_Data, kC3VersionSize))
    {
        return false;
    }

    m_IsValid = true;
    return true;
}

void C3Container::Reset()
{
    m_IsValid = false;
    m_Version.clear();
    m_Data.clear();
    m_Chunks.clear();
    m_RawChunks.clear();
}

bool C3Container::IsChunkInBounds(size_t dataSize, uint32_t offset, uint32_t size)
{
    const size_t end = static_cast<size_t>(offset) + static_cast<size_t>(size);
    return end <= dataSize;
}

bool C3Container::ValidateHeader(const std::vector<uint8_t>& data)
{
    if (data.size() < kC3VersionSize)
    {
        LX_C3_WARN("Container too small for C3 header (size={})", data.size());
        return false;
    }

    std::string version(reinterpret_cast<const char*>(data.data()), kC3VersionSize);
    version   = TrimVersion(version);
    m_Version = version;

    if (version.rfind("MAXFILE C3", 0) != 0)
    {
        LX_C3_WARN("Invalid C3 header signature: '{}'", version);
        return false;
    }

    if (version != kC3VersionTag)
    {
        LX_C3_WARN("Unexpected C3 version '{}' (expected '{}')", version, kC3VersionTag);
    }

    return true;
}

bool C3Container::ParseChunks(const std::vector<uint8_t>& data, size_t startOffset)
{
    C3BinaryReader reader(data);
    if (!reader.Seek(startOffset))
    {
        LX_C3_WARN("Failed to seek to chunk data (offset={})", startOffset);
        return false;
    }

    while (reader.CanRead(sizeof(C3ChunkHeader)))
    {
        C3ChunkHeader header{};
        if (!reader.Read(header))
        {
            LX_C3_WARN("Failed to read chunk header at offset {}", reader.GetOffset());
            return false;
        }

        const uint32_t dataOffset = static_cast<uint32_t>(reader.GetOffset());
        const uint32_t dataSize   = header.size;

        if (!IsChunkInBounds(data.size(), dataOffset, dataSize))
        {
            LX_C3_WARN("Invalid chunk bounds at offset {} (size={})", dataOffset, dataSize);
            break;
        }

        ChunkInfo info{};
        info.type   = MakeFourCC(static_cast<char>(header.id[0]),
                               static_cast<char>(header.id[1]),
                               static_cast<char>(header.id[2]),
                               static_cast<char>(header.id[3]));
        info.offset = dataOffset;
        info.size   = dataSize;
        m_Chunks.push_back(info);

        RawChunk raw{};
        raw.info = info;
        if (!reader.ReadBytes(dataSize, raw.data))
        {
            LX_C3_WARN("Failed to read chunk '{}' payload", FourCCToString(info.type));
            break;
        }
        m_RawChunks.push_back(std::move(raw));
    }

    return true;
}

} // namespace LongXi
