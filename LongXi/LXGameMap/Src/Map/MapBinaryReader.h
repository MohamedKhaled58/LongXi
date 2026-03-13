#pragma once

// Header-only inline binary read helpers for the DMAP/ANI/puzzle binary formats.
// All functions read little-endian values from a bounds-checked byte buffer.
// Out-of-range accesses return a safe zero/empty default rather than asserting.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace LXMap
{

inline uint8_t ReadU8(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset >= bytes.size())
    {
        return 0;
    }

    return bytes[offset];
}

inline uint16_t ReadU16(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint16_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

inline int16_t ReadI16(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int16_t>(ReadU16(bytes, offset));
}

inline uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint32_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

inline int32_t ReadI32(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int32_t>(ReadU32(bytes, offset));
}

inline float ReadF32(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(float) > bytes.size())
    {
        return 0.0f;
    }

    float value = 0.0f;
    memcpy(&value, bytes.data() + offset, sizeof(float));
    return value;
}

inline std::string ReadFixedCString(const std::vector<uint8_t>& bytes, size_t offset, size_t length)
{
    if (offset >= bytes.size() || length == 0)
    {
        return {};
    }

    const size_t end       = std::min(bytes.size(), offset + length);
    size_t       actualEnd = offset;
    while (actualEnd < end && bytes[actualEnd] != 0)
    {
        ++actualEnd;
    }

    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), actualEnd - offset);
}

} // namespace LXMap
