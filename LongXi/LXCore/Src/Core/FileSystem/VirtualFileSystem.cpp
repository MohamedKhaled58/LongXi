#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/FileSystem/WdfArchive.h"
#include "Core/Logging/LogMacros.h"

#include <Windows.h>
#include <algorithm>
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
// CVirtualFileSystem
// ============================================================================

// Constructors defined here so unique_ptr<IMountPoint> destructor has complete type.
CVirtualFileSystem::CVirtualFileSystem() = default;
CVirtualFileSystem::~CVirtualFileSystem() = default;

// ============================================================================
// Normalize
// ============================================================================

std::string CVirtualFileSystem::Normalize(const std::string& path) const
{
    if (path.empty())
        return {};

    // Trim leading/trailing ASCII whitespace
    auto start = path.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    auto end = path.find_last_not_of(" \t\r\n");
    std::string s = path.substr(start, end - start + 1);

    // Step 1: Replace \ with /
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
    }

    // Step 2: Lowercase
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Step 3: Collapse consecutive / into one
    std::string collapsed;
    collapsed.reserve(s.size());
    bool lastSlash = false;
    for (char c : s)
    {
        if (c == '/')
        {
            if (!lastSlash)
                collapsed += c;
            lastSlash = true;
        }
        else
        {
            collapsed += c;
            lastSlash = false;
        }
    }
    s = std::move(collapsed);

    // Step 4: Strip leading /
    if (!s.empty() && s[0] == '/')
        s = s.substr(1);

    // Step 5: Strip trailing /
    if (!s.empty() && s.back() == '/')
        s.pop_back();

    // Steps 6–7: Split on /, reject .., collapse .
    std::vector<std::string> segments;
    std::string seg;
    auto flushSeg = [&]()
    {
        if (seg.empty())
            return;
        if (seg == "..")
        {
            LX_CORE_WARN("CVirtualFileSystem: rejected path with traversal '..': '{}'", path);
            segments.clear();
            seg.clear();
        }
        else if (seg != ".")
        {
            segments.push_back(seg);
            seg.clear();
        }
        else
        {
            seg.clear(); // collapse '.'
        }
    };

    bool rejected = false;
    for (char c : s)
    {
        if (c == '/')
        {
            if (!seg.empty() && seg == "..")
            {
                rejected = true;
                break;
            }
            flushSeg();
        }
        else
        {
            seg += c;
        }
    }
    if (!rejected && !seg.empty() && seg == "..")
        rejected = true;
    if (!rejected)
        flushSeg();

    if (rejected)
    {
        LX_CORE_WARN("CVirtualFileSystem: rejected path with traversal '..': '{}'", path);
        return {};
    }

    // Step 8: Return empty if result has no segments
    if (segments.empty())
        return {};

    // Rejoin with /
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i > 0)
            result += '/';
        result += segments[i];
    }
    return result;
}

// ============================================================================
// MountDirectory
// ============================================================================

bool CVirtualFileSystem::MountDirectory(const std::string& path)
{
    if (path.empty())
    {
        LX_CORE_ERROR("[VFS] MountDirectory called with empty path");
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(ToWide(path), ec))
    {
        LX_CORE_ERROR("[VFS] Failed to mount directory: {}", path);
        return false;
    }

    m_MountPoints.push_back(std::make_unique<CDirectoryMountPoint>(path));
    LX_CORE_INFO("[VFS] Mounted directory: {}", path);
    return true;
}

// ============================================================================
// MountWdf
// ============================================================================

bool CVirtualFileSystem::MountWdf(const std::string& path)
{
    if (path.empty())
    {
        LX_CORE_ERROR("[VFS] MountWdf called with empty path");
        return false;
    }

    auto archive = std::make_unique<WdfArchive>();
    if (!archive->Open(path))
    {
        // WdfArchive::Open already logs its own error; add VFS-level error for context
        LX_CORE_ERROR("[VFS] Failed to mount WDF: {}", path);
        return false;
    }

    m_MountPoints.push_back(std::make_unique<CWdfMountPoint>(std::move(archive)));
    LX_CORE_INFO("[VFS] Mounted WDF archive: {}", path);
    return true;
}

// ============================================================================
// Exists
// ============================================================================

bool CVirtualFileSystem::Exists(const std::string& path) const
{
    std::string normalized = Normalize(path);
    if (normalized.empty())
        return false;

    for (const auto& mp : m_MountPoints)
    {
        if (mp->Exists(normalized))
            return true;
    }
    return false;
}

// ============================================================================
// Open
// ============================================================================

std::unique_ptr<IFileStream> CVirtualFileSystem::Open(const std::string& path) const
{
    std::string normalized = Normalize(path);
    if (normalized.empty())
    {
        LX_CORE_WARN("[VFS] Open called with empty path");
        return nullptr;
    }

    // Search mount points front-to-back; return the first valid stream (first match wins).
    for (const auto& mp : m_MountPoints)
    {
        if (mp->Exists(normalized))
        {
            auto stream = mp->Open(normalized);
            if (stream)
                return stream; // first match — exit immediately
        }
    }

    LX_CORE_WARN("[VFS] Resource not found: {}", normalized);
    return nullptr;
}

// ============================================================================
// ReadAll
// ============================================================================

std::vector<uint8_t> CVirtualFileSystem::ReadAll(const std::string& path) const
{
    auto stream = Open(path);
    if (!stream)
        return {};

    std::vector<uint8_t> buffer(stream->Size());
    if (buffer.empty())
        return buffer;

    size_t bytesRead = stream->Read(buffer.data(), buffer.size());
    if (bytesRead != buffer.size())
    {
        LX_CORE_WARN("[VFS] Short read: requested {} bytes, read {}", buffer.size(), bytesRead);
        buffer.resize(bytesRead);
    }

    return buffer;
}

} // namespace LongXi
