#include "Core/FileSystem/FileStream.h"

#include <Windows.h>
#include <algorithm>
#include <cstring>

#include "Core/Logging/LogMacros.h"

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
// CFileDiskStream
// ============================================================================

CFileDiskStream::CFileDiskStream(const std::string& absolutePath)
    : m_Size(0)
    , m_Position(0)
{
    m_File.open(ToWide(absolutePath), std::ios::binary | std::ios::ate);
    if (!m_File.is_open())
    {
        LX_CORE_ERROR("CFileDiskStream: failed to open '{}'", absolutePath);
        return;
    }

    auto fileSize = m_File.tellg();
    if (fileSize < 0)
    {
        LX_CORE_ERROR("CFileDiskStream: tellg failed for '{}'", absolutePath);
        m_File.close();
        return;
    }

    m_Size = static_cast<size_t>(fileSize);
    m_File.seekg(0, std::ios::beg);
}

CFileDiskStream::~CFileDiskStream()
{
    if (m_File.is_open())
        m_File.close();
}

size_t CFileDiskStream::Read(void* buffer, size_t size)
{
    if (!m_File.is_open() || size == 0)
        return 0;

    size_t remaining = m_Size - m_Position;
    size_t toRead    = std::min(size, remaining);
    if (toRead == 0)
        return 0;

    m_File.read(static_cast<char*>(buffer), static_cast<std::streamsize>(toRead));
    size_t bytesRead = static_cast<size_t>(m_File.gcount());
    m_Position += bytesRead;
    return bytesRead;
}

bool CFileDiskStream::Seek(size_t offset)
{
    if (!m_File.is_open())
        return false;
    if (offset > m_Size)
        return false;

    // Clear EOF/fail flags so seek works after prior full reads.
    m_File.clear();
    m_File.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!m_File)
        return false;

    m_Position = offset;
    return true;
}

size_t CFileDiskStream::Tell() const
{
    return m_Position;
}

size_t CFileDiskStream::Size() const
{
    return m_Size;
}

// ============================================================================
// CFileWdfStream
// ============================================================================

CFileWdfStream::CFileWdfStream(std::vector<uint8_t> data)
    : m_Data(std::move(data))
    , m_Position(0)
{
}

size_t CFileWdfStream::Read(void* buffer, size_t size)
{
    if (size == 0 || m_Position >= m_Data.size())
        return 0;

    size_t remaining = m_Data.size() - m_Position;
    size_t toRead    = std::min(size, remaining);
    std::memcpy(buffer, m_Data.data() + m_Position, toRead);
    m_Position += toRead;
    return toRead;
}

bool CFileWdfStream::Seek(size_t offset)
{
    if (offset > m_Data.size())
        return false;
    m_Position = offset;
    return true;
}

size_t CFileWdfStream::Tell() const
{
    return m_Position;
}

size_t CFileWdfStream::Size() const
{
    return m_Data.size();
}

} // namespace LongXi
