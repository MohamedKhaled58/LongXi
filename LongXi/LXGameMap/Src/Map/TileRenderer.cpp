#include "Map/TileRenderer.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "Core/Logging/LogMacros.h"
#include "Map/MapCamera.h"
#include "Renderer/Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/Texture.h"

namespace LongXi
{

bool TileRenderer::Initialize(Renderer& renderer)
{
    m_ViewportWidth  = static_cast<uint32_t>(std::max(1, renderer.GetViewportWidth()));
    m_ViewportHeight = static_cast<uint32_t>(std::max(1, renderer.GetViewportHeight()));
    m_Initialized    = true;
    return true;
}

void TileRenderer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    m_Initialized = false;
}

bool TileRenderer::IsInitialized() const
{
    return m_Initialized;
}

void TileRenderer::OnResize(uint32_t width, uint32_t height)
{
    m_ViewportWidth  = width > 0 ? width : 1;
    m_ViewportHeight = height > 0 ? height : 1;
}

bool TileRenderer::RenderTiles(const MapDescriptor&                                          descriptor,
                               const MapCamera&                                              camera,
                               const std::vector<MapAnimationState>&                         animationStates,
                               const std::unordered_map<uint16_t, std::shared_ptr<Texture>>& textureRefs,
                               SpriteRenderer&                                               spriteRenderer,
                               float                                                         puzzleScrollOffsetX,
                               float                                                         puzzleScrollOffsetY,
                               MapRenderSnapshot&                                            inOutSnapshot)
{
    if (!m_Initialized || !descriptor.IsValid())
    {
        return false;
    }

    if (descriptor.PuzzleGridWidth == 0 || descriptor.PuzzleGridHeight == 0)
    {
        return true;
    }

    const size_t expectedPuzzleCount = static_cast<size_t>(descriptor.PuzzleGridWidth) * static_cast<size_t>(descriptor.PuzzleGridHeight);
    if (descriptor.PuzzleIndices.size() < expectedPuzzleCount)
    {
        inOutSnapshot.Warnings.push_back("Puzzle index grid is incomplete for the loaded map");
        return false;
    }

    const float zoom            = std::max(0.1f, camera.GetZoom());
    const float viewportWidth   = static_cast<float>(std::max<uint32_t>(1u, m_ViewportWidth));
    const float viewportHeight  = static_cast<float>(std::max<uint32_t>(1u, m_ViewportHeight));
    const float viewWorldWidth  = viewportWidth / zoom;
    const float viewWorldHeight = viewportHeight / zoom;
    const float viewWorldMinX   = camera.GetViewCenterWorldX() - viewWorldWidth * 0.5f;
    const float viewWorldMinY   = camera.GetViewCenterWorldY() - viewWorldHeight * 0.5f;

    const float bgWorldWidth       = static_cast<float>(descriptor.PuzzleGridWidth) * kPuzzleGridSize;
    const float bgWorldHeight      = static_cast<float>(descriptor.PuzzleGridHeight) * kPuzzleGridSize;
    const float mapHalfHeightWorld = static_cast<float>(descriptor.CellHeight) * static_cast<float>(descriptor.HeightInTiles) * 0.5f;
    // Legacy map centering applies this half-cell shift when the terrain height is even.
    const float legacyEvenHeightOffset =
        ((descriptor.HeightInTiles + 1u) % 2u) != 0u ? static_cast<float>(descriptor.CellHeight) * 0.5f : 0.0f;
    const float bgWorldX = static_cast<float>(descriptor.OriginX) - bgWorldWidth * 0.5f + puzzleScrollOffsetX;
    const float bgWorldY =
        static_cast<float>(descriptor.OriginY) + mapHalfHeightWorld - bgWorldHeight * 0.5f - legacyEvenHeightOffset + puzzleScrollOffsetY;

    const float   bgViewportX = viewWorldMinX - bgWorldX;
    const float   bgViewportY = viewWorldMinY - bgWorldY;
    const int32_t startGridX  = std::max<int32_t>(0, static_cast<int32_t>(std::floor(bgViewportX / kPuzzleGridSize)) - 1);
    const int32_t startGridY  = std::max<int32_t>(0, static_cast<int32_t>(std::floor(bgViewportY / kPuzzleGridSize)) - 1);
    const int32_t endGridX    = std::min<int32_t>(static_cast<int32_t>(descriptor.PuzzleGridWidth) - 1,
                                               static_cast<int32_t>(std::floor((bgViewportX + viewWorldWidth) / kPuzzleGridSize)) + 1);
    const int32_t endGridY    = std::min<int32_t>(static_cast<int32_t>(descriptor.PuzzleGridHeight) - 1,
                                               static_cast<int32_t>(std::floor((bgViewportY + viewWorldHeight) / kPuzzleGridSize)) + 1);

    if (startGridX > endGridX || startGridY > endGridY)
    {
        return true;
    }

    std::unordered_map<uint16_t, const MapAnimationState*> animationLookup;
    animationLookup.reserve(animationStates.size());
    for (const MapAnimationState& animationState : animationStates)
    {
        if (animationState.Frames.empty())
        {
            continue;
        }

        animationLookup[animationState.AnimationId] = &animationState;
    }

    const float           puzzleCellScreenSize   = kPuzzleGridSize * zoom;
    uint32_t              skippedMissingTextures = 0;
    std::vector<uint16_t> skippedTextureSamples;
    skippedTextureSamples.reserve(6);
    for (int32_t gridY = startGridY; gridY <= endGridY; ++gridY)
    {
        for (int32_t gridX = startGridX; gridX <= endGridX; ++gridX)
        {
            const size_t puzzleOffset =
                static_cast<size_t>(gridY) * static_cast<size_t>(descriptor.PuzzleGridWidth) + static_cast<size_t>(gridX);
            const uint16_t puzzleIndex = descriptor.PuzzleIndices[puzzleOffset];
            if (puzzleIndex == kInvalidPuzzleIndex)
            {
                continue;
            }

            const Texture* selectedTexture = nullptr;
            const auto     animationIt     = animationLookup.find(puzzleIndex);
            if (animationIt != animationLookup.end() && animationIt->second != nullptr)
            {
                const MapAnimationState*        state        = animationIt->second;
                const std::shared_ptr<Texture>& frameTexture = state->Frames[state->CurrentFrame % state->Frames.size()];
                if (frameTexture)
                {
                    selectedTexture = frameTexture.get();
                }
            }
            else
            {
                const auto textureIt = textureRefs.find(puzzleIndex);
                if (textureIt != textureRefs.end() && textureIt->second)
                {
                    selectedTexture = textureIt->second.get();
                }
            }

            if (!selectedTexture)
            {
                ++skippedMissingTextures;
                if (skippedTextureSamples.size() < 6)
                {
                    skippedTextureSamples.push_back(puzzleIndex);
                }
                continue;
            }

            const float screenX = (static_cast<float>(gridX) * kPuzzleGridSize - bgViewportX) * zoom;
            const float screenY = (static_cast<float>(gridY) * kPuzzleGridSize - bgViewportY) * zoom;

            if (screenX + puzzleCellScreenSize < 0.0f || screenY + puzzleCellScreenSize < 0.0f || screenX > viewportWidth ||
                screenY > viewportHeight)
            {
                continue;
            }

            const Vector2 drawPosition = {screenX, screenY};
            const Vector2 drawSize     = {puzzleCellScreenSize, puzzleCellScreenSize};
            spriteRenderer.DrawSprite(selectedTexture, drawPosition, drawSize);
            ++inOutSnapshot.DrawCalls;
            ++inOutSnapshot.VisibleTiles;
        }
    }

    if (skippedMissingTextures > 0)
    {
        std::ostringstream warning;
        warning << "Skipped " << skippedMissingTextures << " visible puzzle tiles due to missing textures";
        if (!skippedTextureSamples.empty())
        {
            warning << " (sample indices: ";
            for (size_t sampleIndex = 0; sampleIndex < skippedTextureSamples.size(); ++sampleIndex)
            {
                if (sampleIndex > 0)
                {
                    warning << ", ";
                }

                warning << skippedTextureSamples[sampleIndex];
            }
            warning << ")";
        }

        inOutSnapshot.Warnings.push_back(warning.str());
    }

    return true;
}

} // namespace LongXi
