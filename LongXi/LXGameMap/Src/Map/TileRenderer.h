#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Map/MapTypes.h"

namespace LXEngine
{
class Renderer;
class SpriteRenderer;
class Texture;
} // namespace LXEngine

namespace LXMap
{

class MapCamera;

using LXEngine::Renderer;
using LXEngine::SpriteRenderer;
using LXEngine::Texture;

class TileRenderer
{
public:
    bool Initialize(Renderer& renderer);
    void Shutdown();

    bool IsInitialized() const;

    void OnResize(uint32_t width, uint32_t height);

    bool RenderTiles(const MapDescriptor&                                          descriptor,
                     const MapCamera&                                              camera,
                     const std::vector<MapAnimationState>&                         animationStates,
                     const std::unordered_map<uint16_t, std::shared_ptr<Texture>>& textureRefs,
                     SpriteRenderer&                                               spriteRenderer,
                     float                                                         puzzleScrollOffsetX,
                     float                                                         puzzleScrollOffsetY,
                     MapRenderSnapshot&                                            inOutSnapshot);

private:
    bool m_Initialized = false;

    uint32_t m_ViewportWidth  = 1;
    uint32_t m_ViewportHeight = 1;
};

} // namespace LXMap
