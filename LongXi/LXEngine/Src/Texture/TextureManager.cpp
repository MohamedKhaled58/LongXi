#include "Texture/TextureManager.h"
#include "Texture/TextureLoader.h"
#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"

#include <ctype.h>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

TextureManager::TextureManager(Renderer& renderer, CVirtualFileSystem& vfs)
    : m_Renderer(renderer)
    , m_VFS(vfs)
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
    std::string normalized = NormalizeVirtualResourcePath(path, true);
    if (normalized.empty() && !path.empty())
        LX_ENGINE_WARN("[Texture] Path rejected during normalization: {}", path);
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
    std::vector<uint8_t> fileData = m_VFS.ReadAll(normalized);
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
    RendererTextureHandle handle = m_Renderer.CreateTexture(texData.Width, texData.Height, texData.Format, texData.Pixels.data());
    if (!handle.IsValid())
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

void TextureManager::ForEachLoadedTexture(const std::function<void(const std::string& path, const Texture& texture)>& visitor) const
{
    if (!visitor)
    {
        return;
    }

    for (const auto& [path, texture] : m_Cache)
    {
        if (texture)
        {
            visitor(path, *texture);
        }
    }
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
