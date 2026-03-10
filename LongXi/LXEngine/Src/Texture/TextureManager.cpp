#include "Texture/TextureManager.h"
#include "Texture/TextureLoader.h"
#include "Engine/Engine.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"

#include <algorithm>
#include <cstring>
#include <ctype.h>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

TextureManager::TextureManager(Engine& engine) : m_Engine(engine)
{
    LX_ENGINE_INFO("[Texture] TextureManager created");
}

TextureManager::~TextureManager()
{
    ClearCache();
}

// ============================================================================
// Normalize
// ============================================================================

std::string TextureManager::Normalize(const std::string& path) const
{
    if (path.empty())
    {
        return path;
    }

    std::string normalized;
    normalized.reserve(path.length());

    // Step 1: Convert backslash to forward slash
    // Step 2: Convert to lowercase
    // Step 3: Collapse duplicate slashes
    bool lastWasSlash = false;
    for (char c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }

        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        if (c == '/')
        {
            if (!lastWasSlash)
            {
                normalized += c;
                lastWasSlash = true;
            }
            // else: skip duplicate slash
        }
        else
        {
            normalized += c;
            lastWasSlash = false;
        }
    }

    // Step 4: Strip leading slash
    if (!normalized.empty() && normalized[0] == '/')
    {
        normalized.erase(0, 1);
    }

    // Step 5: Strip trailing slash
    if (!normalized.empty() && normalized[normalized.length() - 1] == '/')
    {
        normalized.erase(normalized.length() - 1, 1);
    }

    // Step 6: Reject ".." segments (security: prevent directory traversal)
    size_t dotDotPos = normalized.find("..");
    if (dotDotPos != std::string::npos)
    {
        // Check if it's a standalone ".." segment
        bool hasDotDotSegment = false;
        if (dotDotPos == 0)
        {
            hasDotDotSegment = true;
        }
        else if (normalized[dotDotPos - 1] == '/')
        {
            // Check if followed by slash or end of string
            if (dotDotPos + 2 >= normalized.length() || normalized[dotDotPos + 2] == '/')
            {
                hasDotDotSegment = true;
            }
        }

        if (hasDotDotSegment)
        {
            LX_ENGINE_WARN("[Texture] Path rejected (contains '..'): {}", path);
            return "";
        }
    }

    // Step 7: Collapse "." segments
    size_t dotSlashPos = 0;
    while ((dotSlashPos = normalized.find("./")) != std::string::npos)
    {
        normalized.erase(dotSlashPos, 2);
    }

    // Remove trailing "/." if present
    if (normalized.length() >= 2 && normalized[normalized.length() - 2] == '/' && normalized[normalized.length() - 1] == '.')
    {
        normalized.erase(normalized.length() - 2, 2);
    }

    // Standalone "." becomes empty
    if (normalized == "." || normalized == "/.")
    {
        return "";
    }

    // Step 8: Return empty if result is empty
    if (normalized.empty())
    {
        return "";
    }

    return normalized;
}

// ============================================================================
// LoadTexture
// ============================================================================

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& path)
{
    // Normalize path
    std::string normalized = Normalize(path);
    if (normalized.empty())
    {
        LX_ENGINE_WARN("[Texture] LoadTexture: empty or invalid path");
        return nullptr;
    }

    // Check cache
    auto it = m_Cache.find(normalized);
    if (it != m_Cache.end())
    {
        LX_ENGINE_INFO("[Texture] Cache hit: {}", normalized);
        return it->second;
    }

    // Load from VFS
    std::vector<uint8_t> fileData = m_Engine.GetVFS().ReadAll(normalized);
    if (fileData.empty())
    {
        LX_ENGINE_ERROR("[Texture] File not found: {}", normalized);
        return nullptr;
    }

    // Detect file extension from path
    std::string ext;
    size_t dotPos = normalized.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        ext = normalized.substr(dotPos);
        // Convert to lowercase
        for (char& c : ext)
        {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
    }

    // Decode based on extension
    TextureData texData;
    bool decodeSuccess = false;

    if (ext == ".dds")
    {
        decodeSuccess = TextureLoader::LoadDDS(fileData, texData);
    }
    else if (ext == ".tga")
    {
        decodeSuccess = TextureLoader::LoadTGA(fileData, texData);
    }
    else
    {
        LX_ENGINE_ERROR("[Texture] Unsupported format: {} (extension: {})", normalized, ext);
        return nullptr;
    }

    if (!decodeSuccess)
    {
        LX_ENGINE_ERROR("[Texture] Failed to decode: {}", normalized);
        return nullptr;
    }

    // Upload to GPU
    RendererTextureHandle handle = m_Engine.GetRenderer().CreateTexture(texData.Width, texData.Height, texData.Format, texData.Pixels.data());
    if (!handle.Get())
    {
        LX_ENGINE_ERROR("[Texture] GPU upload failed: {}", normalized);
        return nullptr;
    }

    // Create Texture object
    auto texture = std::shared_ptr<Texture>(new Texture(handle, texData.Width, texData.Height, texData.Format));

    // Insert into cache
    m_Cache[normalized] = texture;

    // Get format string for logging
    const char* formatStr = "Unknown";
    switch (texData.Format)
    {
    case TextureFormat::RGBA8:
        formatStr = "RGBA8";
        break;
    case TextureFormat::DXT1:
        formatStr = "DXT1";
        break;
    case TextureFormat::DXT3:
        formatStr = "DXT3";
        break;
    case TextureFormat::DXT5:
        formatStr = "DXT5";
        break;
    }

    LX_ENGINE_INFO("[Texture] Loaded: {} ({}x{} {})", normalized, texData.Width, texData.Height, formatStr);

    return texture;
}

// ============================================================================
// GetTexture
// ============================================================================

std::shared_ptr<Texture> TextureManager::GetTexture(const std::string& path)
{
    std::string normalized = Normalize(path);
    if (normalized.empty())
    {
        return nullptr;
    }

    auto it = m_Cache.find(normalized);
    if (it != m_Cache.end())
    {
        return it->second;
    }

    return nullptr;
}

// ============================================================================
// ClearCache
// ============================================================================

void TextureManager::ClearCache()
{
    size_t count = m_Cache.size();
    m_Cache.clear();
    LX_ENGINE_INFO("[Texture] Cache cleared ({} entries released)", count);
}

} // namespace LongXi
