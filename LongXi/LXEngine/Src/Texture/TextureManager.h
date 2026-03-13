#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Texture/Texture.h"

// =============================================================================
// TextureManager — Texture loading and caching subsystem
// Loads textures from VFS, decodes DDS/TGA, uploads to GPU, and caches by path
// =============================================================================

namespace LXEngine
{

class Renderer;
class CVirtualFileSystem;

class TextureManager
{
public:
    // Constructor — requires explicit subsystem dependencies
    TextureManager(Renderer& renderer, CVirtualFileSystem& vfs);
    ~TextureManager();

    // Non-copyable
    TextureManager(const TextureManager&)            = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Load texture from virtual path (returns cached shared_ptr if already loaded)
    // Returns nullptr on failure (logs error)
    std::shared_ptr<Texture> LoadTexture(const std::string& path);

    // Get cached texture without loading (returns nullptr if not cached)
    std::shared_ptr<Texture> GetTexture(const std::string& path);

    // Iterate all currently cached textures.
    void ForEachLoadedTexture(const std::function<void(const std::string& path, const Texture& texture)>& visitor) const;

    // Clear all cached textures (callers holding shared_ptr retain their instances)
    void ClearCache();

private:
    // Normalize path to lowercase forward-slash no-leading-slash form
    std::string Normalize(const std::string& path) const;

private:
    Renderer&                                                 m_Renderer;
    CVirtualFileSystem&                                       m_VFS;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Cache;
};

} // namespace LXEngine
