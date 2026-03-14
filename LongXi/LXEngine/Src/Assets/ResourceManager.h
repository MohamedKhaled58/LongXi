#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Animation/AnimationClip.h"
#include "Assets/C3/C3RuntimeLoader.h"
#include "Assets/C3/Resources.h"
#include "Assets/C3/RuntimeMesh.h"
#include "Assets/ResourceCache.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Renderer/Renderer.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LXEngine
{

using LXCore::CVirtualFileSystem;

class ResourceManager
{
public:
    ResourceManager()  = default;
    ~ResourceManager() = default;

    // Non-copyable
    ResourceManager(const ResourceManager&)            = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    bool Initialize(CVirtualFileSystem& vfs, Renderer& renderer, TextureManager& textureManager);

    // Resource access by numeric ID
    RuntimeMesh*          GetMesh(uint64_t id);
    AnimationClip*        GetMotion(uint64_t id);
    RendererTextureHandle GetTexture(uint64_t id);
    ParticleResource*     GetEffect(uint64_t id);
    bool                  ResolveMeshPath(uint64_t id, std::string& outPath) const;
    bool                  TryResolveMotionPath(uint64_t id, std::string& outPath) const;

    // Fallback texture (white, used when texture loading fails)
    RendererTextureHandle GetFallbackTexture() const
    {
        return m_FallbackTexture;
    }

private:
    // INI file loading
    bool LoadINIFiles();
    bool CreateFallbackTexture();

    // Motion resolution with category precedence
    std::string ResolveMotionPath(uint64_t id) const;

    // Cache wrappers
    RuntimeMesh*          LoadMesh(uint64_t id, const std::string& path);
    AnimationClip*        LoadMotion(uint64_t id, const std::string& path);
    RendererTextureHandle LoadTexture(uint64_t id, const std::string& path);
    ParticleResource*     LoadEffect(uint64_t id, const std::string& path);

private:
    // INI mapping tables (id → filepath)
    std::unordered_map<uint64_t, std::string> m_MeshMap;
    std::unordered_map<uint64_t, std::string> m_TextureMap;
    std::unordered_map<uint64_t, std::string> m_MotionMap;
    std::unordered_map<uint64_t, std::string> m_EffectMap;
    std::unordered_map<uint64_t, std::string> m_WeaponMotionMap;
    // std::unordered_map<uint64_t, std::string> m_MountMotionMap;
    // std::unordered_map<uint64_t, std::string> m_MantleMotionMap;

    // Resource caches
    ResourceCache<RuntimeMesh>      m_MeshCache;
    ResourceCache<AnimationClip>    m_MotionCache;
    ResourceCache<Texture>          m_TextureCache;
    ResourceCache<ParticleResource> m_EffectCache;

    // Subsystems (non-owning)
    CVirtualFileSystem* m_VFS            = nullptr;
    Renderer*           m_Renderer       = nullptr;
    TextureManager*     m_TextureManager = nullptr;

    // C3 loader
    C3RuntimeLoader m_Loader;

    // Fallback white texture
    RendererTextureHandle m_FallbackTexture;

    // IDs for which loading has already failed — suppresses repeated warnings
    std::unordered_set<uint64_t> m_FailedTextureIds;
};

} // namespace LXEngine
