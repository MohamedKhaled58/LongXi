#include "Texture/TextureManager.h"

#include <atomic>
#include <ctype.h>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Graphics/TextureFormat.h"
#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"
#include "Texture/TextureLoader.h"

namespace LXEngine
{

using LXCore::TextureFormat;

namespace
{
constexpr uint32_t kMaxTextureLogCount = 2;
std::atomic<uint32_t> s_TextureLogCount{0};

bool ShouldLogTextureEvent()
{
    uint32_t current = s_TextureLogCount.load(std::memory_order_relaxed);
    while (current < kMaxTextureLogCount)
    {
        if (s_TextureLogCount.compare_exchange_weak(current, current + 1u, std::memory_order_relaxed))
        {
            return true;
        }
    }
    return false;
}
} // namespace

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
    std::string normalized = LXCore::NormalizeVirtualResourcePath(path, true);
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
        if (ShouldLogTextureEvent())
        {
            LX_ENGINE_INFO("[Texture] Cache hit: {}", normalized);
        }
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
    size_t      dotPos = normalized.find_last_of('.');
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
    bool        decodeSuccess = false;

    if (ext == ".dds")
    {
        decodeSuccess = TextureLoader::LoadDDS(fileData, texData);
        if (!decodeSuccess)
        {
            // Legacy assets may be mislabeled; attempt TGA decode before failing.
            decodeSuccess = TextureLoader::LoadTGA(fileData, texData);
            if (decodeSuccess)
            {
                LX_ENGINE_WARN("[Texture] Decoded mislabeled texture as TGA: {}", normalized);
            }
        }
    }
    else if (ext == ".tga")
    {
        decodeSuccess = TextureLoader::LoadTGA(fileData, texData);
        if (!decodeSuccess)
        {
            // Legacy assets may be mislabeled; attempt DDS decode before failing.
            decodeSuccess = TextureLoader::LoadDDS(fileData, texData);
            if (decodeSuccess)
            {
                LX_ENGINE_WARN("[Texture] Decoded mislabeled texture as DDS: {}", normalized);
            }
        }
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

    // Check for corresponding .msk alpha mask file
    // MSK files are alpha masks that replace the texture's alpha channel
    if (texData.Format == TextureFormat::RGBA8)
    {
        // Build MSK path by replacing extension with .msk
        std::string mskPath = normalized;
        size_t      dotPos  = mskPath.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            mskPath = mskPath.substr(0, dotPos) + ".msk";

            // Try to load the MSK file directly from VFS (not through LoadTexture)
            std::vector<uint8_t> mskFileData = m_VFS.ReadAll(mskPath);
            if (!mskFileData.empty())
            {
                std::vector<uint8_t> alphaData;
                uint32_t             mskWidth, mskHeight;

                if (TextureLoader::LoadMSK(mskFileData, alphaData, mskWidth, mskHeight))
                {
                    // Verify dimensions match
                    if (mskWidth == texData.Width && mskHeight == texData.Height)
                    {
                        TextureLoader::ApplyMSKMask(texData, alphaData);
                        LX_ENGINE_INFO("[Texture] Applied MSK mask from: {}", mskPath);
                    }
                    else
                    {
                        LX_ENGINE_WARN("[Texture] MSK mask dimensions {}x{} don't match texture {}x{}, skipping",
                                       mskWidth,
                                       mskHeight,
                                       texData.Width,
                                       texData.Height);
                    }
                }
                else
                {
                    LX_ENGINE_WARN("[Texture] Failed to decode MSK file: {}", mskPath);
                }
            }
            // If MSK file doesn't exist, that's fine - silently continue
        }
    }

    // Upload to GPU via descriptor-based renderer API.
    RendererTextureDesc textureDesc = {};
    textureDesc.Width               = texData.Width;
    textureDesc.Height              = texData.Height;
    textureDesc.Format              = texData.Format;
    textureDesc.Usage               = RendererResourceUsage::Static;
    textureDesc.CpuAccess           = RendererCpuAccessFlags::None;
    textureDesc.BindFlags           = RendererBindFlags::ShaderResource;
    textureDesc.InitialData         = texData.Pixels.data();
    textureDesc.InitialDataSize     = static_cast<uint32_t>(texData.Pixels.size());

    RendererTextureHandle handle = m_Renderer.CreateTexture(textureDesc);
    if (!handle.IsValid())
    {
        LX_ENGINE_ERROR("[Texture] GPU upload failed: {}", normalized);
        return nullptr;
    }

    // Create Texture object with custom deleter that returns the GPU resource
    // to the renderer when the last CPU-side reference is released.
    Renderer* renderer = &m_Renderer;
    auto      texture  = std::shared_ptr<Texture>(new Texture(handle, texData.Width, texData.Height, texData.Format),
                                            [renderer](Texture* tex)
                                            {
                                                if (renderer && tex)
                                                {
                                                    const RendererTextureHandle& textureHandle = tex->GetHandle();
                                                    if (textureHandle.IsValid())
                                                    {
                                                        renderer->DestroyTexture(textureHandle);
                                                    }
                                                }
                                                delete tex;
                                            });

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

    if (ShouldLogTextureEvent())
    {
        LX_ENGINE_INFO("[Texture] Loaded: {} ({}x{} {})", normalized, texData.Width, texData.Height, formatStr);
    }

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

} // namespace LXEngine
