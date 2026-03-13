#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map/MapCamera.h"
#include "Map/MapLoader.h"
#include "Map/MapObjects.h"
#include "Map/MapTypes.h"
#include "Map/SkyPuzzleSystem.h"
#include "Map/TileGrid.h"
#include "Map/TileRenderer.h"

namespace LXCore
{
class CVirtualFileSystem;
struct TimingSnapshot;
} // namespace LXCore

namespace LXEngine
{
class Renderer;
class SpriteRenderer;
class Texture;
class TextureManager;
} // namespace LXEngine

namespace LXMap
{

using LXCore::CVirtualFileSystem;
using LXCore::TimingSnapshot;
using LXEngine::Renderer;
using LXEngine::SpriteRenderer;
using LXEngine::Texture;
using LXEngine::TextureManager;

class MapSystem
{
public:
    bool Initialize(Renderer& renderer, CVirtualFileSystem& vfs, TextureManager& textureManager);
    void Shutdown();

    bool IsInitialized() const;
    bool IsMapReady() const;

    bool LoadMap(const std::string& mapPath);
    void UnloadMap();

    void BeginFrame(const TimingSnapshot& timingSnapshot);
    void Render(SpriteRenderer& spriteRenderer, const TimingSnapshot& timingSnapshot);
    void OnResize(uint32_t width, uint32_t height);
    void PanCamera(float deltaWorldX, float deltaWorldY);
    void AdjustCameraZoom(float deltaZoom);

    MapSystemState           GetState() const;
    const MapDescriptor*     GetMapDescriptor() const;
    const MapRenderSnapshot& GetRenderSnapshot() const;

private:
    void UpdateAnimations(const TimingSnapshot& timingSnapshot);
    void UpdateObjectAnimations(const TimingSnapshot& timingSnapshot);
    void RenderMapObjects(SpriteRenderer& spriteRenderer);
    void ResetSnapshot(uint64_t frameIndex);

private:
    Renderer*           m_Renderer       = nullptr;
    CVirtualFileSystem* m_VFS            = nullptr;
    TextureManager*     m_TextureManager = nullptr;

    MapLoader       m_Loader;
    TileGrid        m_TileGrid;
    TileRenderer    m_TileRenderer;
    MapObjects      m_MapObjects;
    MapCamera       m_MapCamera;
    SkyPuzzleSystem m_SkyPuzzleSystem;

    MapDescriptor                                          m_MapDescriptor;
    std::unordered_map<uint16_t, std::shared_ptr<Texture>> m_TextureRefs;
    std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_ObjectTextureRefs;
    std::unordered_map<uint32_t, MapObjectAnimationState>  m_ObjectAnimations;
    std::vector<MapAnimationState>                         m_AnimationStates;
    std::vector<const TileRecord*>                         m_VisibleTilesCache;
    std::vector<const MapObjectRecord*>                    m_VisibleObjectsCache;
    MapRenderSnapshot                                      m_RenderSnapshot;

    MapSystemState m_State       = MapSystemState::Uninitialized;
    bool           m_Initialized = false;
};

} // namespace LXMap
