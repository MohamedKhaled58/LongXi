#pragma once

#include "Core/FileSystem/FileStream.h"

#include <memory>
#include <string>

// =============================================================================
// MountPoint — abstract and concrete resource source types
//
// IMountPoint          : abstract source interface
// CDirectoryMountPoint : loose-file directory source (disk)
// CWdfMountPoint       : WDF archive source (Spec 005)
// =============================================================================

namespace LongXi
{

class WdfArchive; // forward declaration — full type required in .cpp only

// =============================================================================
// IMountPoint
// =============================================================================
class IMountPoint
{
  public:
    virtual ~IMountPoint() = default;

    IMountPoint(const IMountPoint&) = delete;
    IMountPoint& operator=(const IMountPoint&) = delete;

    // Returns true if this source contains a resource at the given normalized path.
    virtual bool Exists(const std::string& normalizedPath) const = 0;

    // Opens the resource at the given normalized path.
    // Returns nullptr if not found or on I/O error.
    virtual std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const = 0;

  protected:
    IMountPoint() = default;
};

// =============================================================================
// CDirectoryMountPoint
// =============================================================================
class CDirectoryMountPoint : public IMountPoint
{
  public:
    explicit CDirectoryMountPoint(const std::string& rootDirectory);

    bool Exists(const std::string& normalizedPath) const override;
    std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const override;

  private:
    std::string m_Root; // absolute directory path; no trailing slash
};

// =============================================================================
// CWdfMountPoint
// =============================================================================
class CWdfMountPoint : public IMountPoint
{
  public:
    // Takes ownership of the already-opened WdfArchive.
    explicit CWdfMountPoint(std::unique_ptr<WdfArchive> archive);

    // Defined in .cpp — WdfArchive full type required by unique_ptr destructor.
    ~CWdfMountPoint() override;

    CWdfMountPoint(const CWdfMountPoint&) = delete;
    CWdfMountPoint& operator=(const CWdfMountPoint&) = delete;

    bool Exists(const std::string& normalizedPath) const override;
    std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const override;

  private:
    std::unique_ptr<WdfArchive> m_Archive;
};

} // namespace LongXi
