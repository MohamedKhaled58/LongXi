#include "Core/FileSystem/ResourceSystem.h"
#include "Core/Logging/LogMacros.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>

namespace LongXi
{

// ============================================================================
// File-scope helpers — not exposed in the public header
// ============================================================================

// Convert UTF-8 std::string to std::wstring for Win32 OS calls.
// Uses two-call MultiByteToWideChar pattern; does not appear in the public API.
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

// Return the directory containing the current executable as a UTF-8 string.
std::string ResourceSystem::GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    if (len == 0 || len == MAX_PATH)
    {
        LX_CORE_ERROR("GetModuleFileNameW failed (len={})", len);
        return {};
    }

    std::filesystem::path exePath(buffer);
    std::wstring wideDir = exePath.parent_path().wstring();

    // Convert back to UTF-8 for internal use
    int size = WideCharToMultiByte(CP_UTF8, 0, wideDir.data(), static_cast<int>(wideDir.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};

    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideDir.data(), static_cast<int>(wideDir.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// ============================================================================
// Normalize
// ============================================================================

std::string ResourceSystem::Normalize(const std::string& path) const
{
    if (path.empty())
        return {};

    std::string s = path;

    // 1. Trim leading/trailing ASCII whitespace
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    s = s.substr(start, end - start + 1);

    // 2. Replace all backslashes with forward slashes
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
    }

    // 3. Collapse consecutive '/' into a single '/'
    std::string collapsed;
    collapsed.reserve(s.size());
    bool lastWasSlash = false;
    for (char c : s)
    {
        if (c == '/')
        {
            if (!lastWasSlash)
                collapsed += c;
            lastWasSlash = true;
        }
        else
        {
            collapsed += c;
            lastWasSlash = false;
        }
    }
    s = std::move(collapsed);

    // 4. Strip a single leading '/' (make relative)
    if (!s.empty() && s[0] == '/')
        s = s.substr(1);

    // 5. Split on '/', check for '..' (reject) and '.' (collapse)
    std::vector<std::string> segments;
    std::string segment;
    for (char c : s)
    {
        if (c == '/')
        {
            if (!segment.empty())
            {
                if (segment == "..")
                {
                    LX_CORE_WARN("ResourceSystem: rejected path with traversal sequence: '{}'", path);
                    return {};
                }
                if (segment != ".")
                    segments.push_back(segment);
                segment.clear();
            }
        }
        else
        {
            segment += c;
        }
    }
    // Handle last segment
    if (!segment.empty())
    {
        if (segment == "..")
        {
            LX_CORE_WARN("ResourceSystem: rejected path with traversal sequence: '{}'", path);
            return {};
        }
        if (segment != ".")
            segments.push_back(segment);
    }

    if (segments.empty())
        return {};

    // 6. Rejoin with '/'
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
// Resolve
// ============================================================================

std::string ResourceSystem::Resolve(const std::string& normalizedPath) const
{
    // Extension boundary: archive-backed sources slot in here in a later spec.
    // Currently, only directory-backed roots are searched.
    for (const std::string& root : m_Roots)
    {
        std::string candidate = root + "/" + normalizedPath;
        std::error_code ec;
        if (std::filesystem::is_regular_file(ToWide(candidate), ec))
            return candidate;
    }
    return {};
}

// ============================================================================
// Lifecycle
// ============================================================================

void ResourceSystem::Initialize(const std::vector<std::string>& roots)
{
    m_Roots.clear();
    m_Roots.reserve(roots.size());

    for (const std::string& root : roots)
    {
        // Normalize the root path itself (replace backslashes, strip trailing slash)
        std::string normalized = root;
        for (char& c : normalized)
        {
            if (c == '\\')
                c = '/';
        }
        while (!normalized.empty() && normalized.back() == '/')
            normalized.pop_back();

        m_Roots.push_back(normalized);

        // Log registration — warn if root does not exist on disk
        std::error_code ec;
        if (std::filesystem::exists(ToWide(normalized), ec))
            LX_CORE_INFO("ResourceSystem: registered root '{}'", normalized);
        else
            LX_CORE_WARN("ResourceSystem: root '{}' does not exist — no files will resolve from it", normalized);
    }

    LX_CORE_INFO("Resource system initialized ({} root(s))", m_Roots.size());
}

void ResourceSystem::Shutdown()
{
    m_Roots.clear();
    LX_CORE_INFO("Resource system shutdown");
}

// ============================================================================
// Exists
// ============================================================================

bool ResourceSystem::Exists(const std::string& relativePath) const
{
    std::string normalized = Normalize(relativePath);
    if (normalized.empty())
        return false;

    return !Resolve(normalized).empty();
}

// ============================================================================
// ReadFile
// ============================================================================

std::optional<std::vector<uint8_t>> ResourceSystem::ReadFile(const std::string& relativePath) const
{
    // Normalize
    std::string normalized = Normalize(relativePath);
    if (normalized.empty())
    {
        // Normalize already logged WARN for traversal; log for empty-after-normalize
        if (!relativePath.empty())
            LX_CORE_WARN("ResourceSystem: path normalized to empty: '{}'", relativePath);
        return std::nullopt;
    }

    // Resolve to absolute path
    std::string resolved = Resolve(normalized);
    if (resolved.empty())
    {
        LX_CORE_ERROR("ResourceSystem: file not found: '{}'", relativePath);
        return std::nullopt;
    }

    // Open for binary read
    std::ifstream file(ToWide(resolved), std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LX_CORE_ERROR("ResourceSystem: failed to open: '{}'", resolved);
        return std::nullopt;
    }

    // Get file size
    auto fileSize = static_cast<std::streamsize>(file.tellg());

    // Zero-byte file is valid — return engaged optional with empty vector
    if (fileSize <= 0)
        return std::vector<uint8_t>{};

    // Read entire file into buffer
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        LX_CORE_ERROR("ResourceSystem: read failed for: '{}'", resolved);
        return std::nullopt;
    }

    return buffer;
}

} // namespace LongXi
