#include "Core/FileSystem/WdfArchive.h"
#include "Core/Logging/LogMacros.h"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace LongXi
{

// ============================================================================
// File-scope helpers
// ============================================================================

static std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

// ============================================================================
// stringtoid — custom 32-bit hash
//
// [Partially Confirmed — reimplemented from x86 inline assembly in the reference
// client (C3_CORE_DLL/c3_datafile.cpp). Magic constants preserved exactly.
// Adopted — verify: must be tested against known archive entry UIDs.]
// ============================================================================
static uint32_t stringtoid(const char* str)
{
    uint32_t m[70] = {};
    strncpy_s(reinterpret_cast<char*>(m), sizeof(m), str, 256);

    int i;
    for (i = 0; i < 64 && m[i]; i++)
        ;
    m[i++] = 0x9BE74448u;
    m[i++] = 0x66F42C48u;

    uint32_t v = 0xF4FA8928u;
    uint32_t x = 0x37A8470Eu;
    uint32_t y = 0x7758B42Bu;

    for (int j = 0; j < i; j++)
    {
        v = (v << 1) | (v >> 31);
        uint32_t w = 0x267B0B11u ^ v;

        x ^= m[j];
        y ^= m[j]; // y used in its ORIGINAL value for both multipliers

        // First multiply
        uint32_t mult1 = ((w + y) | 0x02040801u) & 0xBFEF7FDFu;
        uint64_t p1 = static_cast<uint64_t>(x) * mult1;
        uint32_t lo1 = static_cast<uint32_t>(p1);
        uint32_t hi1 = static_cast<uint32_t>(p1 >> 32);
        uint32_t cf1 = (hi1 != 0) ? 1u : 0u;
        uint64_t adc1 = static_cast<uint64_t>(lo1) + hi1 + cf1;
        uint32_t cf2 = (adc1 > 0xFFFFFFFFull) ? 1u : 0u;
        uint32_t new_x = static_cast<uint32_t>(adc1) + cf2;

        // Second multiply (uses ORIGINAL y)
        uint32_t mult2 = ((w + y) | 0x00804021u) & 0x7DFEFBFFu;
        uint64_t p2 = static_cast<uint64_t>(y) * mult2;
        uint32_t lo2 = static_cast<uint32_t>(p2);
        uint32_t hi2 = static_cast<uint32_t>(p2 >> 32);
        uint64_t hi2x2 = static_cast<uint64_t>(hi2) * 2;
        uint32_t hi2d = static_cast<uint32_t>(hi2x2);
        uint32_t cf_d = (hi2x2 > 0xFFFFFFFFull) ? 1u : 0u;
        uint64_t adc2 = static_cast<uint64_t>(lo2) + hi2d + cf_d;
        uint32_t cf3 = (adc2 > 0xFFFFFFFFull) ? 1u : 0u;
        uint32_t new_y = static_cast<uint32_t>(adc2);
        if (cf3)
            new_y += 2;

        x = new_x;
        y = new_y;
    }

    return x ^ y;
}

static uint32_t NormalizeAndHash(const std::string& path)
{
    std::string n;
    n.reserve(path.size());
    for (char c : path)
    {
        if (c >= 'A' && c <= 'Z')
            n += static_cast<char>(c + ('a' - 'A'));
        else if (c == '\\')
            n += '/';
        else
            n += c;
    }
    return stringtoid(n.c_str());
}

// ============================================================================
// WdfArchive
// ============================================================================

WdfArchive::WdfArchive() : m_IsOpen(false) {}

WdfArchive::~WdfArchive()
{
    if (m_IsOpen)
        Close();
}

bool WdfArchive::Open(const std::string& absolutePath)
{
    m_Path = absolutePath;

    m_File.open(ToWide(absolutePath), std::ios::binary);
    if (!m_File.is_open())
    {
        LX_CORE_ERROR("WdfArchive: failed to open '{}'", absolutePath);
        return false;
    }

    WdfHeader header;
    if (!m_File.read(reinterpret_cast<char*>(&header), sizeof(WdfHeader)))
    {
        LX_CORE_ERROR("WdfArchive: failed to read header from '{}'", absolutePath);
        m_File.close();
        return false;
    }

    // header.Id: [Partially Confirmed — NOT validated]
    // header.Count: [Confirmed]
    // header.Offset: [Confirmed]

    if (header.Count < 0)
    {
        LX_CORE_ERROR("WdfArchive: invalid entry count ({}) in '{}'", header.Count, absolutePath);
        m_File.close();
        return false;
    }

    if (header.Count == 0)
    {
        LX_CORE_WARN("WdfArchive: archive '{}' has 0 entries", absolutePath);
        m_IsOpen = true;
        LX_CORE_INFO("WdfArchive: opened '{}' (0 entries)", absolutePath);
        return true;
    }

    m_File.seekg(static_cast<std::streamoff>(header.Offset), std::ios::beg);
    if (!m_File)
    {
        LX_CORE_ERROR("WdfArchive: failed to seek to index in '{}'", absolutePath);
        m_File.close();
        return false;
    }

    m_Index.resize(static_cast<size_t>(header.Count));
    if (!m_File.read(reinterpret_cast<char*>(m_Index.data()), static_cast<std::streamsize>(sizeof(WdfIndexEntry) * m_Index.size())))
    {
        LX_CORE_ERROR("WdfArchive: failed to read index from '{}'", absolutePath);
        m_Index.clear();
        m_File.close();
        return false;
    }

    // [Adopted — verify] Sort defensively; warn if not pre-sorted.
    bool wasSorted = std::is_sorted(m_Index.begin(), m_Index.end(), [](const WdfIndexEntry& a, const WdfIndexEntry& b) { return a.Uid < b.Uid; });
    if (!wasSorted)
    {
        LX_CORE_WARN("WdfArchive: index not pre-sorted in '{}' — sorting in memory", absolutePath);
        std::sort(m_Index.begin(), m_Index.end(), [](const WdfIndexEntry& a, const WdfIndexEntry& b) { return a.Uid < b.Uid; });
    }

    m_IsOpen = true;
    LX_CORE_INFO("WdfArchive: opened '{}' ({} entries)", absolutePath, m_Index.size());
    return true;
}

void WdfArchive::Close()
{
    if (m_File.is_open())
        m_File.close();
    m_Index.clear();
    m_IsOpen = false;
}

bool WdfArchive::IsOpen() const
{
    return m_IsOpen;
}
const std::string& WdfArchive::GetPath() const
{
    return m_Path;
}
int WdfArchive::GetEntryCount() const
{
    return static_cast<int>(m_Index.size());
}

uint32_t WdfArchive::ComputeUid(const std::string& path) const
{
    return NormalizeAndHash(path);
}

bool WdfArchive::HasEntry(const std::string& normalizedPath) const
{
    if (!m_IsOpen)
        return false;
    uint32_t uid = ComputeUid(normalizedPath);
    auto it = std::lower_bound(m_Index.begin(), m_Index.end(), uid, [](const WdfIndexEntry& e, uint32_t val) { return e.Uid < val; });
    return it != m_Index.end() && it->Uid == uid;
}

std::optional<std::vector<uint8_t>> WdfArchive::ReadEntry(const std::string& normalizedPath) const
{
    if (!m_IsOpen)
        return std::nullopt;

    uint32_t uid = ComputeUid(normalizedPath);
    auto it = std::lower_bound(m_Index.begin(), m_Index.end(), uid, [](const WdfIndexEntry& e, uint32_t val) { return e.Uid < val; });

    if (it == m_Index.end() || it->Uid != uid)
    {
        LX_CORE_ERROR("WdfArchive: entry not found: '{}' in '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    const WdfIndexEntry& entry = *it;

    m_File.seekg(static_cast<std::streamoff>(entry.Offset), std::ios::beg);
    if (!m_File)
    {
        LX_CORE_ERROR("WdfArchive: failed to seek to entry '{}' in '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(entry.Size);
    if (!m_File.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(entry.Size)))
    {
        LX_CORE_ERROR("WdfArchive: failed to read entry '{}' from '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    // entry.Space: [Unknown — not used in any read operation]
    return buffer;
}

} // namespace LongXi
