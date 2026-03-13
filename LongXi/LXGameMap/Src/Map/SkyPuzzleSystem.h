#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map/MapTypes.h"
#include "Texture/Texture.h"

namespace LXMap
{

class CVirtualFileSystem;
class MapCamera;
class SpriteRenderer;
class TextureManager;
struct MapDescriptor;
struct MapObjectRecord;
struct TimingSnapshot;

class SkyPuzzleSystem
{
public:
    // Load all sky-puzzle layers found in mapObjects. Also captures the ground-puzzle scroll rates
    // from descriptor (PuzzleRollSpeedX/Y, PuzzleGridWidth/Height) for UpdateScroll.
    bool Load(const std::vector<MapObjectRecord>& mapObjects,
              const MapDescriptor&                descriptor,
              CVirtualFileSystem&                 vfs,
              TextureManager&                     textureManager,
              std::vector<std::string>&           outWarnings);

    void Unload();

    // Advances ground-puzzle scroll and all sky-layer scroll offsets by one frame delta.
    void UpdateScroll(const TimingSnapshot& timingSnapshot, const MapDescriptor& descriptor);

    // Renders all sky-puzzle layers for the given render pass. Returns the number of draw calls issued.
    uint32_t Render(SpriteRenderer&      spriteRenderer,
                    uint32_t             renderPass,
                    const MapCamera&     camera,
                    const MapDescriptor& descriptor,
                    uint32_t             viewportWidth,
                    uint32_t             viewportHeight);

    float GetGroundScrollOffsetX() const
    {
        return m_GroundScrollOffsetX;
    }

    float GetGroundScrollOffsetY() const
    {
        return m_GroundScrollOffsetY;
    }

    uint32_t GetLayerCount() const
    {
        return static_cast<uint32_t>(m_Layers.size());
    }

private:
    struct Layer
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

    bool LoadLayer(const MapObjectRecord&    sourceObject,
                   CVirtualFileSystem&       vfs,
                   TextureManager&           textureManager,
                   Layer&                    outLayer,
                   std::vector<std::string>& outWarnings);

private:
    std::vector<Layer> m_Layers;
    float              m_GroundScrollOffsetX = 0.0f;
    float              m_GroundScrollOffsetY = 0.0f;
};

} // namespace LXMap
