#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Math/Math.h"
#include "Renderer/RendererTypes.h"

namespace LongXi
{

class Texture;

inline constexpr uint16_t kInvalidPuzzleIndex = (std::numeric_limits<uint16_t>::max)();
inline constexpr float    kPuzzleGridSize     = 256.0f;

enum class MapSystemState : uint8_t
{
    Uninitialized = 0,
    Loading,
    Ready,
    Rendering,
    Failed,
    Unloaded,
};

enum class MapLayer : uint8_t
{
    Terrain = 0,
    Floor,
    Interactive,
    Sky,
};

struct MapPassageRecord
{
    int32_t TileX            = 0;
    int32_t TileY            = 0;
    int32_t DestinationMapId = 0;
};

struct MapDescriptor
{
    uint32_t    MapId = 0;
    std::string MapName;
    std::string SourcePath;
    uint32_t    WidthInTiles  = 0;
    uint32_t    HeightInTiles = 0;
    uint32_t    DmapVersion   = 0;

    uint32_t CellWidth  = 64;
    uint32_t CellHeight = 32;
    int32_t  OriginX    = 0;
    int32_t  OriginY    = 0;

    std::string                   PuzzleAssetPath;
    std::string                   PuzzleAniPath;
    uint32_t                      PuzzleGridWidth  = 0;
    uint32_t                      PuzzleGridHeight = 0;
    std::vector<uint16_t>         PuzzleIndices;
    std::vector<uint16_t>         MissingPuzzleIndices;
    int32_t                       PuzzleRollSpeedX = 0;
    int32_t                       PuzzleRollSpeedY = 0;
    std::vector<MapPassageRecord> Passages;
    uint32_t                      GameMapFlags           = 0;
    uint64_t                      LoadChecksum           = 0;
    uint32_t                      InteractiveObjectCount = 0;
    uint32_t                      SceneObjectCount       = 0;
    uint32_t                      UnknownObjectCount     = 0;
    uint32_t                      LoadedSceneFileCount   = 0;
    uint32_t                      LoadedScenePartCount   = 0;

    bool IsValid() const
    {
        return WidthInTiles > 0 && HeightInTiles > 0 && DmapVersion == 1004;
    }
};

struct TileRecord
{
    int32_t               TileX     = 0;
    int32_t               TileY     = 0;
    int16_t               Height    = 0;
    uint16_t              TextureId = kInvalidPuzzleIndex;
    uint32_t              Flags     = 0;
    uint16_t              TerrainId = 0;
    uint16_t              MaskId    = 0;
    std::vector<uint32_t> ObjectRefs;

    int32_t GetDepthKey() const
    {
        return TileX + TileY;
    }
};

struct MapObjectRecord
{
    uint32_t    ObjectId         = 0;
    uint16_t    ObjectType       = 0;
    int32_t     TileX            = 0;
    int32_t     TileY            = 0;
    int32_t     WorldX           = 0;
    int32_t     WorldY           = 0;
    bool        HasWorldPosition = false;
    int16_t     HeightOffset     = 0;
    MapLayer    RenderLayer      = MapLayer::Interactive;
    uint64_t    InsertionOrder   = 0;
    uint32_t    VisibilityFlags  = 0;
    std::string ResourcePath;
    std::string ResourceTag;

    // Original ANI file path and section name for animation frame loading.
    std::string AniSourcePath;
    std::string AniSectionName;

    // Legacy object footprint in tile-space for culling and ordering.
    int32_t BaseWidth  = 1;
    int32_t BaseHeight = 1;

    // Legacy 2D object fields (MAP_COVER / MAP_SCENE / MAP_TERRAIN_PART)
    int32_t OffsetX       = 0;
    int32_t OffsetY       = 0;
    int32_t FrameCount    = 0;
    int32_t FrameInterval = 0;
    int32_t ShowMode      = 0;

    // Sound object fields (MAP_SOUND)
    int32_t SoundRange    = 0;
    int32_t SoundVolume   = 0;
    int32_t SoundInterval = 0;

    // Scene layer fields (parallax)
    int32_t MoveRateX       = 0;
    int32_t MoveRateY       = 0;
    int32_t SceneLayerIndex = 0;

    // Legacy ARGB tint color (stored as 0xAARRGGBB in old client).
    uint32_t TintARGB = 0xFFFFFFFF;

    // 3D effect fields (MAP_3DEFFECTNEW rotation/scale)
    float EffectParams[6] = {};

    int32_t GetDepthKey() const
    {
        return TileX + TileY;
    }
};

struct MapAnimationState
{
    uint16_t                              AnimationId            = 0;
    uint32_t                              CurrentFrame           = 0;
    uint32_t                              FrameStepMilliseconds  = 160;
    bool                                  Loop                   = true;
    uint64_t                              LastStepFrameIndex     = 0;
    double                                AccumulatedTimeSeconds = 0.0;
    std::vector<std::shared_ptr<Texture>> Frames;
};

struct MapObjectAnimationState
{
    uint32_t                              ObjectId               = 0;
    uint32_t                              CurrentFrame           = 0;
    uint32_t                              FrameStepMilliseconds  = 160;
    bool                                  Loop                   = true;
    double                                AccumulatedTimeSeconds = 0.0;
    std::vector<std::shared_ptr<Texture>> Frames;
};

struct MapCatalogEntry
{
    uint32_t    MapId = 0;
    std::string Path;
    uint32_t    Flags = 0;
};

struct VisibleTileWindow
{
    int32_t StartTileX = 0;
    int32_t StartTileY = 0;
    int32_t EndTileX   = -1;
    int32_t EndTileY   = -1;

    bool IsEmpty() const
    {
        return StartTileX > EndTileX || StartTileY > EndTileY;
    }
};

struct MapRenderItem
{
    MapLayer                   Layer       = MapLayer::Terrain;
    int32_t                    DepthKey    = 0;
    uint64_t                   StableOrder = 0;
    RendererVertexBufferHandle GeometryHandle;
    RendererTextureHandle      TextureHandle;
    Matrix4                    Transform = {};
    Color                      TintColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct MapRenderSnapshot
{
    uint64_t                 FrameIndex     = 0;
    uint32_t                 VisibleTiles   = 0;
    uint32_t                 VisibleObjects = 0;
    uint32_t                 AnimatedTiles  = 0;
    uint32_t                 DrawCalls      = 0;
    std::vector<std::string> Warnings;
    bool                     IsValid = false;
};

} // namespace LongXi
