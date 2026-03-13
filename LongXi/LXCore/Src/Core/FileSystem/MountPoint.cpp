#include "Core/FileSystem/MountPoint.h"

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

static std::string ExtractArchiveRoot(const std::string& archivePath)
{
    const std::string normalizedArchivePath = LXCore::NormalizeVirtualResourcePath(archivePath, true);
    if (normalizedArchivePath.empty())
    {
        return {};
    }

    const size_t slashPos  = normalizedArchivePath.find_last_of('/');
    const size_t nameStart = (slashPos == std::string::npos) ? 0 : slashPos + 1;
    const size_t dotPos    = normalizedArchivePath.find_last_of('.');
    const size_t nameEnd   = (dotPos == std::string::npos || dotPos <= nameStart) ? normalizedArchivePath.size() : dotPos;
    return normalizedArchivePath.substr(nameStart, nameEnd - nameStart);
}

static bool PathMatchesArchiveRoot(const std::string& normalizedPath, const std::string& archiveRoot)
{
    if (archiveRoot.empty())
    {
        return true;
    }

    if (normalizedPath.size() < archiveRoot.size() || normalizedPath.compare(0, archiveRoot.size(), archiveRoot) != 0)
    {
        return false;
    }

    return normalizedPath.size() == archiveRoot.size() || normalizedPath[archiveRoot.size()] == '/';
}

// ============================================================================
// CDirectoryMountPoint
// ============================================================================

CDirectoryMountPoint::CDirectoryMountPoint(const std::string& rootDirectory)
    : m_Root(rootDirectory)
{
    // Convert all backslashes to forward slashes for consistency
    for (char& c : m_Root)
    {
        if (c == '\\')
            c = '/';
    }

    // Strip trailing slash(es)
    while (!m_Root.empty() && (m_Root.back() == '/' || m_Root.back() == '\\'))
        m_Root.pop_back();
}

bool CDirectoryMountPoint::Exists(const std::string& normalizedPath) const
{
    std::string     absPath = m_Root + "/" + normalizedPath;
    std::error_code ec;
    return std::filesystem::is_regular_file(ToWide(absPath), ec);
}

std::unique_ptr<IFileStream> CDirectoryMountPoint::Open(const std::string& normalizedPath) const
{
    std::string     absPath = m_Root + "/" + normalizedPath;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(ToWide(absPath), ec))
        return nullptr;
    return std::make_unique<CFileDiskStream>(absPath);
}

// ============================================================================
// CWdfMountPoint
// ============================================================================

CWdfMountPoint::CWdfMountPoint(std::unique_ptr<WdfArchive> archive)
    : m_Archive(std::move(archive))
    , m_ArchiveRoot(m_Archive ? ExtractArchiveRoot(m_Archive->GetPath()) : std::string())
{
}

// Destructor defined here so WdfArchive full type is visible for unique_ptr cleanup.
CWdfMountPoint::~CWdfMountPoint() = default;

bool CWdfMountPoint::Exists(const std::string& normalizedPath) const
{
    if (!m_Archive || !HandlesPath(normalizedPath))
        return false;
    return m_Archive->HasEntry(normalizedPath);
}

std::unique_ptr<IFileStream> CWdfMountPoint::Open(const std::string& normalizedPath) const
{
    if (!m_Archive || !HandlesPath(normalizedPath))
        return nullptr;
    auto result = m_Archive->ReadEntry(normalizedPath);
    if (!result.has_value())
        return nullptr;
    return std::make_unique<CFileWdfStream>(std::move(*result));
}

bool CWdfMountPoint::HandlesPath(const std::string& normalizedPath) const
{
    return PathMatchesArchiveRoot(normalizedPath, m_ArchiveRoot);
}

} // namespace LXCore
