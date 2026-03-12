#pragma once

#include "Map/MapCamera.h"
#include "Map/MapLoader.h"
#include "Map/MapObjects.h"
#include "Map/MapTypes.h"
#include "Map/TileGrid.h"
#include "Map/TileRenderer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace LongXi
{

class CVirtualFileSystem;
class Renderer;
class SpriteRenderer;
class Texture;
class TextureManager;
struct TimingSnapshot;

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

    MapSystemState GetState() const;
    const MapDescriptor* GetMapDescriptor() const;
    const MapRenderSnapshot& GetRenderSnapshot() const;

private:
    void UpdateAnimations(const TimingSnapshot& timingSnapshot);
    void UpdatePuzzleScroll(const TimingSnapshot& timingSnapshot);
    void RenderMapObjects(SpriteRenderer& spriteRenderer);
    void ResetSnapshot(uint64_t frameIndex);

private:
    Renderer* m_Renderer = nullptr;
    CVirtualFileSystem* m_VFS = nullptr;
    TextureManager* m_TextureManager = nullptr;

    MapLoader m_Loader;
    TileGrid m_TileGrid;
    TileRenderer m_TileRenderer;
    MapObjects m_MapObjects;
    MapCamera m_MapCamera;

    MapDescriptor m_MapDescriptor;
    std::unordered_map<uint16_t, std::shared_ptr<Texture>> m_TextureRefs;
    std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_ObjectTextureRefs;
    std::vector<MapAnimationState> m_AnimationStates;
    std::vector<const TileRecord*> m_VisibleTilesCache;
    std::vector<const MapObjectRecord*> m_VisibleObjectsCache;
    MapRenderSnapshot m_RenderSnapshot;

    float m_PuzzleScrollOffsetX = 0.0f;
    float m_PuzzleScrollOffsetY = 0.0f;

    MapSystemState m_State = MapSystemState::Uninitialized;
    bool m_Initialized = false;
};

} // namespace LongXi
