#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace LXEngine
{

class C3BinaryReader
{
public:
    C3BinaryReader(const uint8_t* data, size_t size)
        : m_Data(data)
        , m_Size(size)
        , m_Offset(0)
    {
    }

    explicit C3BinaryReader(const std::vector<uint8_t>& data)
        : m_Data(data.data())
        , m_Size(data.size())
        , m_Offset(0)
    {
    }

    size_t GetOffset() const
    {
        return m_Offset;
    }

    size_t GetSize() const
    {
        return m_Size;
    }

    bool Seek(size_t offset)
    {
        if (offset > m_Size)
        {
            return false;
        }
        m_Offset = offset;
        return true;
    }

    bool Skip(size_t bytes)
    {
        return Seek(m_Offset + bytes);
    }

    bool CanRead(size_t bytes) const
    {
        return m_Offset + bytes <= m_Size;
    }

    template <typename T> bool Read(T& out)
    {
        static_assert(std::is_trivially_copyable_v<T>, "C3BinaryReader requires trivially copyable types");
        if (!CanRead(sizeof(T)))
        {
            return false;
        }
        std::memcpy(&out, m_Data + m_Offset, sizeof(T));
        m_Offset += sizeof(T);
        return true;
    }

    template <typename T> bool ReadArray(size_t count, std::vector<T>& out)
    {
        static_assert(std::is_trivially_copyable_v<T>, "C3BinaryReader requires trivially copyable types");
        const size_t bytes = sizeof(T) * count;
        if (!CanRead(bytes))
        {
            return false;
        }
        out.resize(count);
        std::memcpy(out.data(), m_Data + m_Offset, bytes);
        m_Offset += bytes;
        return true;
    }

    bool ReadBytes(size_t count, std::vector<uint8_t>& out)
    {
        if (!CanRead(count))
        {
            return false;
        }
        out.resize(count);
        std::memcpy(out.data(), m_Data + m_Offset, count);
        m_Offset += count;
        return true;
    }

    bool ReadBytes(size_t count, uint8_t* out)
    {
        if (!CanRead(count))
        {
            return false;
        }
        std::memcpy(out, m_Data + m_Offset, count);
        m_Offset += count;
        return true;
    }

    template <typename T> bool ReadAt(size_t offset, T& out) const
    {
        static_assert(std::is_trivially_copyable_v<T>, "C3BinaryReader requires trivially copyable types");
        if (offset + sizeof(T) > m_Size)
        {
            return false;
        }
        std::memcpy(&out, m_Data + offset, sizeof(T));
        return true;
    }

    bool ReadBytesAt(size_t offset, size_t count, std::vector<uint8_t>& out) const
    {
        if (offset + count > m_Size)
        {
            return false;
        }
        out.resize(count);
        std::memcpy(out.data(), m_Data + offset, count);
        return true;
    }

private:
    const uint8_t* m_Data;
    size_t         m_Size;
    size_t         m_Offset;
};

} // namespace LXEngine
