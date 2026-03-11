#include "Core/FileSystem/MountPoint.h"
#include "Core/FileSystem/WdfArchive.h"
#include "Core/Logging/LogMacros.h"

#include <Windows.h>
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
// CDirectoryMountPoint
// ============================================================================

CDirectoryMountPoint::CDirectoryMountPoint(const std::string& rootDirectory)
    : m_Root(rootDirectory)
{
    // Strip trailing slash(es)
    while (!m_Root.empty() && (m_Root.back() == '/' || m_Root.back() == '\\'))
        m_Root.pop_back();
}

bool CDirectoryMountPoint::Exists(const std::string& normalizedPath) const
{
    std::string absPath = m_Root + "/" + normalizedPath;
    std::error_code ec;
    return std::filesystem::is_regular_file(ToWide(absPath), ec);
}

std::unique_ptr<IFileStream> CDirectoryMountPoint::Open(const std::string& normalizedPath) const
{
    std::string absPath = m_Root + "/" + normalizedPath;
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
{
}

// Destructor defined here so WdfArchive full type is visible for unique_ptr cleanup.
CWdfMountPoint::~CWdfMountPoint() = default;

bool CWdfMountPoint::Exists(const std::string& normalizedPath) const
{
    if (!m_Archive)
        return false;
    return m_Archive->HasEntry(normalizedPath);
}

std::unique_ptr<IFileStream> CWdfMountPoint::Open(const std::string& normalizedPath) const
{
    if (!m_Archive)
        return nullptr;
    auto result = m_Archive->ReadEntry(normalizedPath);
    if (!result.has_value())
        return nullptr;
    return std::make_unique<CFileWdfStream>(std::move(*result));
}

} // namespace LongXi
