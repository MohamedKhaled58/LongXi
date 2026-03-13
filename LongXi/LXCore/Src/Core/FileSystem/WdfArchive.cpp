#include "Core/FileSystem/WdfArchive.h"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

#include "Core/Logging/LogMacros.h"

namespace LXCore
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
    if (str == nullptr || *str == '\0')
    {
        return 0;
    }

    char   buffer[256] = {};
    size_t len         = strlen(str);
    if (len >= sizeof(buffer))
    {
        len = sizeof(buffer) - 1;
    }

    for (size_t index = 0; index < len; ++index)
    {
        const char c = str[index];
        if (c >= 'A' && c <= 'Z')
        {
            buffer[index] = static_cast<char>(c + ('a' - 'A'));
        }
        else if (c == '\\')
        {
            buffer[index] = '/';
        }
        else
        {
            buffer[index] = c;
        }
    }
    buffer[len] = '\0';

    uint32_t m[0x46] = {};
    memcpy(m, buffer, (std::min)(sizeof(m) - sizeof(uint32_t) * 2, len + 1));

    size_t wordLen = (len + 3) / 4;
    m[wordLen++]   = 0x9BE74448u;
    m[wordLen++]   = 0x66F42C48u;

    uint32_t v   = 0xF4FA8928u;
    uint32_t esi = 0x37A8470Eu;
    uint32_t edi = 0x7758B42Bu;

    for (size_t ecx = 0; ecx < wordLen; ++ecx)
    {
        uint32_t ebx = 0x267B0B11u;
        v            = (v << 1) | (v >> 31);
        ebx ^= v;

        const uint32_t eax0 = m[ecx];
        esi ^= eax0;
        edi ^= eax0;

        uint32_t edx = ebx + edi;
        edx |= 0x02040801u;
        edx &= 0xBFEF7FDFu;

        uint64_t num = static_cast<uint64_t>(edx) * static_cast<uint64_t>(esi);
        uint32_t eax = static_cast<uint32_t>(num);
        edx          = static_cast<uint32_t>(num >> 32);
        if (edx != 0)
        {
            ++eax;
        }

        num = static_cast<uint64_t>(eax) + static_cast<uint64_t>(edx);
        eax = static_cast<uint32_t>(num);
        if ((num >> 32) != 0)
        {
            ++eax;
        }

        edx = ebx + esi;
        edx |= 0x00804021u;
        edx &= 0x7DFEFBFFu;
        esi = eax;

        num = static_cast<uint64_t>(edi) * static_cast<uint64_t>(edx);
        eax = static_cast<uint32_t>(num);
        edx = static_cast<uint32_t>(num >> 32);

        num = static_cast<uint64_t>(edx) * 2u;
        edx = static_cast<uint32_t>(num);
        if ((num >> 32) != 0)
        {
            ++eax;
        }

        num = static_cast<uint64_t>(eax) + static_cast<uint64_t>(edx);
        eax = static_cast<uint32_t>(num);
        if ((num >> 32) != 0)
        {
            eax += 2;
        }

        edi = eax;
    }

    return esi ^ edi;
}

static uint32_t NormalizeAndHash(const std::string& path)
{
    return stringtoid(path.c_str());
}

static const WdfIndexEntry* FindByUid(const std::vector<WdfIndexEntry>& index, uint32_t uid)
{
    auto it = std::lower_bound(index.begin(),
                               index.end(),
                               uid,
                               [](const WdfIndexEntry& entry, uint32_t value)
                               {
                                   return entry.Uid < value;
                               });

    if (it == index.end() || it->Uid != uid)
    {
        return nullptr;
    }

    return &(*it);
}

static const WdfIndexEntry* FindEntryWithLegacyAliases(const std::vector<WdfIndexEntry>& index, const std::string& normalizedPath)
{
    const WdfIndexEntry* entry = FindByUid(index, NormalizeAndHash(normalizedPath));
    if (entry != nullptr)
    {
        return entry;
    }

    // Legacy CO archives often hash resource keys with a "./" prefix.
    if (normalizedPath.size() < 2 || normalizedPath[0] != '.' || normalizedPath[1] != '/')
    {
        entry = FindByUid(index, NormalizeAndHash("./" + normalizedPath));
        if (entry != nullptr)
        {
            return entry;
        }
    }

    return nullptr;
}

// ============================================================================
// WdfArchive
// ============================================================================

WdfArchive::WdfArchive()
    : m_IsOpen(false)
{
}

WdfArchive::~WdfArchive()
{
    if (m_IsOpen)
        Close();
}

bool WdfArchive::Open(const std::string& absolutePath)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
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
    bool wasSorted = std::is_sorted(m_Index.begin(),
                                    m_Index.end(),
                                    [](const WdfIndexEntry& a, const WdfIndexEntry& b)
                                    {
                                        return a.Uid < b.Uid;
                                    });
    if (!wasSorted)
    {
        LX_CORE_WARN("WdfArchive: index not pre-sorted in '{}' — sorting in memory", absolutePath);
        std::sort(m_Index.begin(),
                  m_Index.end(),
                  [](const WdfIndexEntry& a, const WdfIndexEntry& b)
                  {
                      return a.Uid < b.Uid;
                  });
    }

    m_IsOpen = true;
    LX_CORE_INFO("WdfArchive: opened '{}' ({} entries)", absolutePath, m_Index.size());
    return true;
}

void WdfArchive::Close()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_IsOpen)
        return false;

    const WdfIndexEntry* entry = FindEntryWithLegacyAliases(m_Index, normalizedPath);
    if (entry != nullptr)
    {
        return true;
    }

    static uint32_t s_WdfMissLogCount = 0;
    if (s_WdfMissLogCount < 8)
    {
        const uint32_t uid         = NormalizeAndHash(normalizedPath);
        bool           foundLinear = false;
        for (const WdfIndexEntry& candidate : m_Index)
        {
            if (candidate.Uid == uid)
            {
                foundLinear = true;
                break;
            }
        }

        bool     foundAliasLinear = false;
        uint32_t aliasUid         = 0;
        if (normalizedPath.size() < 2 || normalizedPath[0] != '.' || normalizedPath[1] != '/')
        {
            aliasUid = NormalizeAndHash("./" + normalizedPath);
            for (const WdfIndexEntry& candidate : m_Index)
            {
                if (candidate.Uid == aliasUid)
                {
                    foundAliasLinear = true;
                    break;
                }
            }
        }

        LX_CORE_WARN("WdfArchive: lookup miss archive='{}' path='{}' uid={:08X} linear={} aliasUid={:08X} aliasLinear={}",
                     m_Path,
                     normalizedPath,
                     uid,
                     foundLinear,
                     aliasUid,
                     foundAliasLinear);
        ++s_WdfMissLogCount;
    }

    return false;
}

std::optional<std::vector<uint8_t>> WdfArchive::ReadEntry(const std::string& normalizedPath) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_IsOpen)
        return std::nullopt;

    const WdfIndexEntry* entry = FindEntryWithLegacyAliases(m_Index, normalizedPath);
    if (entry == nullptr)
    {
        LX_CORE_ERROR("WdfArchive: entry not found: '{}' in '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    m_File.clear();
    m_File.seekg(static_cast<std::streamoff>(entry->Offset), std::ios::beg);
    if (!m_File)
    {
        LX_CORE_ERROR("WdfArchive: failed to seek to entry '{}' in '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(entry->Size);
    if (!m_File.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(entry->Size)))
    {
        LX_CORE_ERROR("WdfArchive: failed to read entry '{}' from '{}'", normalizedPath, m_Path);
        return std::nullopt;
    }

    // entry.Space: [Unknown — not used in any read operation]
    return buffer;
}

} // namespace LXCore
