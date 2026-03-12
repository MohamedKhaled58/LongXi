#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// =============================================================================
// FileStream — abstract and concrete resource stream types
//
// IFileStream      : abstract seekable read-only stream; returned by CVirtualFileSystem::Open()
// CFileDiskStream  : disk-backed stream; wraps std::ifstream
// CFileWdfStream   : in-memory stream; holds extracted WDF entry bytes
// =============================================================================

namespace LongXi
{

// =============================================================================
// IFileStream
// =============================================================================
class IFileStream
{
public:
    virtual ~IFileStream() = default;

    IFileStream(const IFileStream&)            = delete;
    IFileStream& operator=(const IFileStream&) = delete;

    // Read up to `size` bytes into `buffer` from the current position.
    // Returns bytes actually read. Returns 0 at end-of-stream.
    virtual size_t Read(void* buffer, size_t size) = 0;

    // Seek to absolute byte offset from stream start.
    // Returns false if offset > Size(); position is unchanged on failure.
    virtual bool Seek(size_t offset) = 0;

    // Returns current byte position. Always in [0, Size()].
    virtual size_t Tell() const = 0;

    // Returns total stream size in bytes. Does not change after construction.
    virtual size_t Size() const = 0;

protected:
    IFileStream() = default;
};

// =============================================================================
// CFileDiskStream
// =============================================================================
class CFileDiskStream : public IFileStream
{
public:
    // Open absolutePath for binary reading. On failure, Size() == 0.
    explicit CFileDiskStream(const std::string& absolutePath);
    ~CFileDiskStream() override;

    CFileDiskStream(const CFileDiskStream&)            = delete;
    CFileDiskStream& operator=(const CFileDiskStream&) = delete;

    size_t Read(void* buffer, size_t size) override;
    bool   Seek(size_t offset) override;
    size_t Tell() const override;
    size_t Size() const override;

private:
    std::ifstream m_File;
    size_t        m_Size;
    size_t        m_Position;
};

// =============================================================================
// CFileWdfStream
// =============================================================================
class CFileWdfStream : public IFileStream
{
public:
    // Take ownership of the extracted WDF entry bytes by move.
    explicit CFileWdfStream(std::vector<uint8_t> data);
    ~CFileWdfStream() override = default;

    CFileWdfStream(const CFileWdfStream&)            = delete;
    CFileWdfStream& operator=(const CFileWdfStream&) = delete;

    size_t Read(void* buffer, size_t size) override;
    bool   Seek(size_t offset) override;
    size_t Tell() const override;
    size_t Size() const override;

private:
    std::vector<uint8_t> m_Data;
    size_t               m_Position;
};

} // namespace LongXi
