#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map/MapCamera.h"
#include "Map/MapLoader.h"
#include "Map/MapObjects.h"
#include "Map/MapTypes.h"
#include "Map/TileGrid.h"
#include "Map/TileRenderer.h"

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

    MapSystemState           GetState() const;
    const MapDescriptor*     GetMapDescriptor() const;
    const MapRenderSnapshot& GetRenderSnapshot() const;

private:
    struct SkyPuzzleLayer
    {
        uint32_t                                               SourceObjectId  = 0;
        int32_t                                                SceneLayerIndex = 0;
        std::string                                            PuzzlePath;
        uint32_t                                               GridWidth  = 0;
        uint32_t                                               GridHeight = 0;
        std::vector<uint16_t>                                  PuzzleIndices;
        std::vector<MapAnimationState>                         Animations;
        std::unordered_map<uint16_t, std::shared_ptr<Texture>> TextureRefs;
        int32_t                                                MoveRateX     = 100;
        int32_t                                                MoveRateY     = 100;
        int32_t                                                RollSpeedX    = 0;
        int32_t                                                RollSpeedY    = 0;
        float                                                  ScrollOffsetX = 0.0f;
        float                                                  ScrollOffsetY = 0.0f;
    };

    void UpdateAnimations(const TimingSnapshot& timingSnapshot);
    void UpdateObjectAnimations(const TimingSnapshot& timingSnapshot);
    void UpdatePuzzleScroll(const TimingSnapshot& timingSnapshot);
    void UpdateSkyPuzzleScroll(const TimingSnapshot& timingSnapshot);
    bool LoadSkyPuzzleLayer(const MapObjectRecord& skyPuzzleObject, SkyPuzzleLayer& outLayer, std::vector<std::string>& outWarnings);
    void RenderSkyPuzzleLayers(SpriteRenderer& spriteRenderer, uint32_t renderPass);
    void RenderWaterReflection(SpriteRenderer& spriteRenderer);
    void RenderMapObjects(SpriteRenderer& spriteRenderer);
    void ResetSnapshot(uint64_t frameIndex);

private:
    Renderer*           m_Renderer       = nullptr;
    CVirtualFileSystem* m_VFS            = nullptr;
    TextureManager*     m_TextureManager = nullptr;

    MapLoader    m_Loader;
    TileGrid     m_TileGrid;
    TileRenderer m_TileRenderer;
    MapObjects   m_MapObjects;
    MapCamera    m_MapCamera;

    MapDescriptor                                          m_MapDescriptor;
    std::unordered_map<uint16_t, std::shared_ptr<Texture>> m_TextureRefs;
    std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_ObjectTextureRefs;
    std::unordered_map<uint32_t, MapObjectAnimationState>  m_ObjectAnimations;
    std::vector<MapAnimationState>                         m_AnimationStates;
    std::vector<SkyPuzzleLayer>                            m_SkyPuzzleLayers;
    std::vector<const TileRecord*>                         m_VisibleTilesCache;
    std::vector<const MapObjectRecord*>                    m_VisibleObjectsCache;
    MapRenderSnapshot                                      m_RenderSnapshot;

    float m_PuzzleScrollOffsetX = 0.0f;
    float m_PuzzleScrollOffsetY = 0.0f;

    MapSystemState m_State       = MapSystemState::Uninitialized;
    bool           m_Initialized = false;
};

} // namespace LongXi
