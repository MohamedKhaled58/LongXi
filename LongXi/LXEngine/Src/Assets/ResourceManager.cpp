#include "Assets/ResourceManager.h"

#include <algorithm>
#include <vector>

#include "Animation/AnimationClip.h"
#include "Assets/C3/C3MeshProcessor.h"
#include "Assets/C3/RuntimeMesh.h"
#include "Assets/INIFileParser.h"
#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

bool ResourceManager::Initialize(CVirtualFileSystem& vfs, Renderer& renderer, TextureManager& textureManager)
{
    m_VFS            = &vfs;
    m_Renderer       = &renderer;
    m_TextureManager = &textureManager;

    // Set cache sizes
    m_MeshCache.SetMaxSize(500);
    m_MotionCache.SetMaxSize(200);
    m_TextureCache.SetMaxSize(1000);
    m_EffectCache.SetMaxSize(100);

    // Load INI files
    if (!LoadINIFiles())
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to load one or more INI files");
        // Continue anyway - some mappings may have loaded
    }

    // Create fallback white texture
    if (!CreateFallbackTexture())
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to create fallback texture");
        return false;
    }

    LX_ENGINE_INFO("[ResourceManager] Initialized with {} mesh, {} texture, {} motion, {} effect mappings",
                   m_MeshMap.size(),
                   m_TextureMap.size(),
                   m_MotionMap.size(),
                   m_EffectMap.size());
    return true;
}

bool ResourceManager::LoadINIFiles()
{
    bool allSuccess = true;

    // Base resource mappings
    allSuccess &= INIFileParser::ParseFile(*m_VFS, "ini/3dobj.ini", m_MeshMap);
    allSuccess &= INIFileParser::ParseFile(*m_VFS, "ini/3dtexture.ini", m_TextureMap);
    allSuccess &= INIFileParser::ParseFile(*m_VFS, "ini/3dmotion.ini", m_MotionMap);
    allSuccess &= INIFileParser::ParseFile(*m_VFS, "ini/3DEffectObj.ini", m_EffectMap);

    // Specialized motion mappings - use correct case from actual filenames
    INIFileParser::ParseFile(*m_VFS, "ini/WeaponMotion.ini", m_WeaponMotionMap);
    // INIFileParser::ParseFile(*m_VFS, "ini/MountMotion.ini", m_MountMotionMap); // Legacy Not Working

    // Mantle motion (armet + misc + head combined)
    // INIFileParser::MappingTable armetMap, miscMap, headMap;
    // INIFileParser::ParseFile(*m_VFS, "ini/armetmotion.ini", armetMap);    //Legacy Not Working
    // INIFileParser::ParseFile(*m_VFS, "ini/miscmotion.ini", miscMap);      //Legacy Not Working
    // INIFileParser::ParseFile(*m_VFS, "ini/headmotion.ini", headMap);      //Legacy Not Working
    // m_MantleMotionMap = std::move(armetMap);
    // m_MantleMotionMap.insert(miscMap.begin(), miscMap.end());
    // m_MantleMotionMap.insert(headMap.begin(), headMap.end());

    return allSuccess;
}

bool ResourceManager::CreateFallbackTexture()
{
    constexpr uint32_t kWhitePixel = 0xFFFFFFFF;

    RendererTextureDesc desc = {};
    desc.Width               = 1;
    desc.Height              = 1;
    desc.Format              = LXCore::TextureFormat::RGBA8;
    desc.Usage               = RendererResourceUsage::Static;
    desc.CpuAccess           = RendererCpuAccessFlags::None;
    desc.BindFlags           = RendererBindFlags::ShaderResource;
    desc.InitialData         = &kWhitePixel;
    desc.InitialDataSize     = sizeof(kWhitePixel);

    m_FallbackTexture = m_Renderer->CreateTexture(desc);
    return m_FallbackTexture.IsValid();
}

std::string ResourceManager::ResolveMotionPath(uint64_t id) const
{
    // Category precedence: weapon > mount > mantle > base
    auto it = m_WeaponMotionMap.find(id);
    if (it != m_WeaponMotionMap.end())
        return it->second;

    /* it = m_MountMotionMap.find(id);
       if (it != m_MountMotionMap.end())
           return it->second;

       it = m_MantleMotionMap.find(id);
       if (it != m_MantleMotionMap.end())
           return it->second;
   */
    it = m_MotionMap.find(id);
    if (it != m_MotionMap.end())
        return it->second;

    return {};
}

RuntimeMesh* ResourceManager::GetMesh(uint64_t id)
{
    // Check cache
    if (auto cached = m_MeshCache.Get(id))
    {
        LX_ENGINE_TRACE("[ResourceManager] Mesh cache HIT for ID {}", id);
        return cached.get();
    }

    LX_ENGINE_TRACE("[ResourceManager] Mesh cache MISS for ID {}", id);

    // Lookup path
    auto it = m_MeshMap.find(id);
    if (it == m_MeshMap.end())
    {
        LX_ENGINE_WARN("[ResourceManager] Mesh ID {} not found in INI", id);
        return nullptr;
    }

    return LoadMesh(id, it->second);
}

AnimationClip* ResourceManager::GetMotion(uint64_t id)
{
    // Check cache
    if (auto cached = m_MotionCache.Get(id))
    {
        LX_ENGINE_TRACE("[ResourceManager] Motion cache HIT for ID {}", id);
        return cached.get();
    }

    LX_ENGINE_TRACE("[ResourceManager] Motion cache MISS for ID {}", id);

    // Lookup path with category precedence
    std::string path = ResolveMotionPath(id);
    if (path.empty())
    {
        LX_ENGINE_WARN("[ResourceManager] Motion ID {} not found in any INI", id);
        return nullptr;
    }

    return LoadMotion(id, path);
}

bool ResourceManager::ResolveMeshPath(uint64_t id, std::string& outPath) const
{
    auto it = m_MeshMap.find(id);
    if (it == m_MeshMap.end())
    {
        outPath.clear();
        return false;
    }

    outPath = it->second;
    return true;
}

bool ResourceManager::TryResolveMotionPath(uint64_t id, std::string& outPath) const
{
    outPath = ResolveMotionPath(id);
    return !outPath.empty();
}

RendererTextureHandle ResourceManager::GetTexture(uint64_t id)
{
    // Check cache
    if (auto cached = m_TextureCache.Get(id))
    {
        LX_ENGINE_TRACE("[ResourceManager] Texture cache HIT for ID {}", id);
        return cached->GetHandle();
    }

    // Already known to fail — return fallback silently
    if (m_FailedTextureIds.count(id))
        return m_FallbackTexture;

    LX_ENGINE_TRACE("[ResourceManager] Texture cache MISS for ID {}", id);

    // Lookup path
    auto it = m_TextureMap.find(id);
    if (it == m_TextureMap.end())
    {
        LX_ENGINE_WARN("[ResourceManager] Texture ID {} not found in INI, using fallback", id);
        m_FailedTextureIds.insert(id);
        return m_FallbackTexture;
    }

    return LoadTexture(id, it->second);
}

ParticleResource* ResourceManager::GetEffect(uint64_t id)
{
    // Check cache
    if (auto cached = m_EffectCache.Get(id))
    {
        LX_ENGINE_TRACE("[ResourceManager] Effect cache HIT for ID {}", id);
        return cached.get();
    }

    LX_ENGINE_TRACE("[ResourceManager] Effect cache MISS for ID {}", id);

    // Lookup path
    auto it = m_EffectMap.find(id);
    if (it == m_EffectMap.end())
    {
        LX_ENGINE_WARN("[ResourceManager] Effect ID {} not found in INI", id);
        return nullptr;
    }

    return LoadEffect(id, it->second);
}

RuntimeMesh* ResourceManager::LoadMesh(uint64_t id, const std::string& path)
{
    C3LoadRequest request;
    request.virtualPath    = path;
    request.requestedTypes = {C3ResourceType::Mesh};

    C3LoadResult result;
    if (!m_Loader.LoadFromVfs(*m_VFS, request, result))
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to load mesh {}: {}", id, path);
        return nullptr;
    }

    if (result.meshes.empty())
    {
        LX_ENGINE_WARN("[ResourceManager] No meshes in C3 file: {}", path);
        return nullptr;
    }

    const int selectedIndex = C3MeshProcessor::SelectBestMeshIndex(result.meshes);
    if (selectedIndex < 0)
    {
        LX_ENGINE_WARN("[ResourceManager] No usable meshes found in C3 file: {}", path);
        return nullptr;
    }

    const size_t    selectedIndexSize = static_cast<size_t>(selectedIndex);
    const MeshStats selectedStats     = C3MeshProcessor::ComputeMeshStats(result.meshes[selectedIndexSize]);

    if (result.meshes.size() > 1)
    {
        const auto& selectedMesh = result.meshes[selectedIndexSize];
        LX_ENGINE_INFO("[ResourceManager] Selected mesh {}/{} (name='{}', vertices={}, indices={}, bonesUsed={}, maxBoneIndex={})",
                       selectedIndex + 1,
                       result.meshes.size(),
                       selectedMesh.name,
                       selectedStats.vertexCount,
                       selectedStats.indexCount,
                       selectedStats.uniqueBones,
                       selectedStats.maxBoneIndex);
    }

    // Create RuntimeMesh from selected mesh in result
    auto mesh = std::make_shared<RuntimeMesh>();
    if (!mesh->Initialize(*m_Renderer, result.meshes[selectedIndexSize]))
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to initialize RuntimeMesh: {}", path);
        return nullptr;
    }

    // Cache and return
    m_MeshCache.Put(id, mesh);
    return mesh.get();
}

AnimationClip* ResourceManager::LoadMotion(uint64_t id, const std::string& path)
{
    C3LoadRequest request;
    request.virtualPath    = path;
    request.requestedTypes = {C3ResourceType::Animation};

    C3LoadResult result;
    if (!m_Loader.LoadFromVfs(*m_VFS, request, result))
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to load motion {}: {}", id, path);
        return nullptr;
    }

    if (result.animations.empty())
    {
        LX_ENGINE_WARN("[ResourceManager] No animations in C3 file: {}", path);
        return nullptr;
    }

    // Create AnimationClip from first animation in result
    auto clip = std::make_shared<AnimationClip>();
    if (!clip->Initialize(result.animations[0]))
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to initialize AnimationClip: {}", path);
        return nullptr;
    }

    // Cache and return
    m_MotionCache.Put(id, clip);
    return clip.get();
}

RendererTextureHandle ResourceManager::LoadTexture(uint64_t id, const std::string& path)
{
    std::shared_ptr<Texture> texture = m_TextureManager->LoadTexture(path);
    if (!texture)
    {
        LX_ENGINE_WARN("[ResourceManager] Texture ID {} failed to load ({}), using fallback", id, path);
        m_FailedTextureIds.insert(id);
        return m_FallbackTexture;
    }

    m_TextureCache.Put(id, texture);
    return texture->GetHandle();
}

ParticleResource* ResourceManager::LoadEffect(uint64_t id, const std::string& path)
{
    C3LoadRequest request;
    request.virtualPath    = path;
    request.requestedTypes = {C3ResourceType::Particle};

    C3LoadResult result;
    if (!m_Loader.LoadFromVfs(*m_VFS, request, result))
    {
        LX_ENGINE_ERROR("[ResourceManager] Failed to load effect {}: {}", id, path);
        return nullptr;
    }

    if (result.particles.empty())
    {
        LX_ENGINE_WARN("[ResourceManager] No particles in C3 file: {}", path);
        return nullptr;
    }

    // Return shared copy of first particle effect
    auto effect = std::make_shared<ParticleResource>(result.particles[0]);
    m_EffectCache.Put(id, effect);
    return effect.get();
}

} // namespace LXEngine
