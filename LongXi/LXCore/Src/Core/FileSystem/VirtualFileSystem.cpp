#include "Core/FileSystem/VirtualFileSystem.h"

#include <Windows.h>
#include <filesystem>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/WdfArchive.h"
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

static std::string GetArchiveRootLabel(const std::string& archivePath)
{
    const std::string normalizedPath = LXCore::NormalizeVirtualResourcePath(archivePath, true);
    if (normalizedPath.empty())
    {
        return {};
    }

    const size_t slashPos  = normalizedPath.find_last_of('/');
    const size_t nameStart = (slashPos == std::string::npos) ? 0 : slashPos + 1;
    const size_t dotPos    = normalizedPath.find_last_of('.');
    const size_t nameEnd   = (dotPos == std::string::npos || dotPos <= nameStart) ? normalizedPath.size() : dotPos;
    return normalizedPath.substr(nameStart, nameEnd - nameStart);
}

// ============================================================================
// CVirtualFileSystem
// ============================================================================

// Constructors defined here so unique_ptr<IMountPoint> destructor has complete type.
CVirtualFileSystem::CVirtualFileSystem()  = default;
CVirtualFileSystem::~CVirtualFileSystem() = default;

// ============================================================================
// Normalize
// ============================================================================

std::string CVirtualFileSystem::Normalize(const std::string& path) const
{
    std::string normalized = LXCore::NormalizeVirtualResourcePath(path, true);
    if (normalized.empty() && !path.empty())
        LX_CORE_WARN("CVirtualFileSystem: rejected invalid path: '{}'", path);
    return normalized;
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
    const std::string archiveRoot = GetArchiveRootLabel(path);
    if (archiveRoot.empty())
    {
        LX_CORE_INFO("[VFS] Mounted WDF archive: {}", path);
    }
    else
    {
        LX_CORE_INFO("[VFS] Mounted WDF archive: {} (root={})", path, archiveRoot);
    }
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

} // namespace LXCore
