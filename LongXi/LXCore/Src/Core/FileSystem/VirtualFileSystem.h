#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/FileSystem/MountPoint.h"

// =============================================================================
// CVirtualFileSystem — unified resource access layer
//
// Mount archives and directories in priority order (first mounted = highest
// priority). Use Exists / Open / ReadAll to access resources by virtual path.
// All paths are normalized (lowercase, forward-slash) before lookup.
//
// Threading:
//   Mount operations (MountWdf, MountDirectory) are single-threaded only.
//   Exists / Open / ReadAll are safe for concurrent use once all mounts complete.
// =============================================================================

namespace LongXi
{

class CVirtualFileSystem
{
public:
    CVirtualFileSystem();
    // Defined in .cpp — unique_ptr<IMountPoint> destructor requires complete type.
    ~CVirtualFileSystem();

    CVirtualFileSystem(const CVirtualFileSystem&)            = delete;
    CVirtualFileSystem& operator=(const CVirtualFileSystem&) = delete;

    // -------------------------------------------------------------------------
    // Mounting
    // -------------------------------------------------------------------------

    // Mount a directory as a resource source. First mounted = highest priority.
    // Returns false (and logs ERROR) if the directory is not found on disk.
    bool MountDirectory(const std::string& path);

    // Mount a WDF archive as a resource source.
    // Returns false (and logs ERROR) if the archive cannot be opened.
    bool MountWdf(const std::string& path);

    // -------------------------------------------------------------------------
    // Querying
    // -------------------------------------------------------------------------

    // Returns true if any mounted source contains a resource at the given path.
    bool Exists(const std::string& path) const;

    // Opens a resource by virtual path.
    // Returns nullptr (and logs WARN) if not found.
    // Caller owns the returned stream.
    std::unique_ptr<IFileStream> Open(const std::string& path) const;

    // Reads all bytes of a resource. Returns {} if not found.
    std::vector<uint8_t> ReadAll(const std::string& path) const;

private:
    // Normalize a caller-provided path: backslash→slash, lowercase, collapse //,
    // strip leading/trailing /, reject .., collapse . — returns "" on failure.
    std::string Normalize(const std::string& path) const;

    std::vector<std::unique_ptr<IMountPoint>> m_MountPoints;
};

} // namespace LongXi
