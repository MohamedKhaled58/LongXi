#include "Map/SkyPuzzleSystem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Core/StringUtils.h"
#include "Core/Timing/TimingService.h"
#include "Map/MapBinaryReader.h"
#include "Map/MapCamera.h"
#include "Map/MapTypes.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/TextureManager.h"

namespace LXMap
{

namespace
{

bool IsTextureAssetPath(const std::string& path)
{
    return LXCore::EndsWithInsensitive(path, ".dds") || LXCore::EndsWithInsensitive(path, ".tga");
}

bool IsVirtualRootedPath(const std::string& path)
{
    const std::string normalizedPath = LXCore::ToLowerAscii(LXCore::NormalizeVirtualResourcePath(path, true));
    if (normalizedPath.empty())
    {
        return false;
    }

    if (normalizedPath.find(':') != std::string::npos || normalizedPath.front() == '/')
    {
        return true;
    }

    return normalizedPath.rfind("data/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("ani/", 0) == 0;
}

bool LooksLikeTexturePayload(const std::string& normalizedPath, const std::vector<uint8_t>& bytes)
{
    if (LXCore::EndsWithInsensitive(normalizedPath, ".dds"))
    {
        return bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ';
    }

    if (LXCore::EndsWithInsensitive(normalizedPath, ".tga"))
    {
        if (bytes.size() < 18)
        {
            return false;
        }

        const uint8_t imageType = bytes[2];
        switch (imageType)
        {
            case 1:
            case 2:
            case 3:
            case 9:
            case 10:
            case 11:
                return true;
            default:
                return false;
        }
    }

    return false;
}

std::string Trim(std::string value)
{
    const auto isWhitespace = [](unsigned char character)
    {
        return character == static_cast<unsigned char>(32) || character == static_cast<unsigned char>(9) ||
               character == static_cast<unsigned char>(13) || character == static_cast<unsigned char>(10);
    };

    const auto beginIt = std::find_if_not(value.begin(),
                                          value.end(),
                                          [&isWhitespace](char character)
                                          {
                                              return isWhitespace(static_cast<unsigned char>(character));
                                          });
    if (beginIt == value.end())
    {
        return {};
    }

    const auto endIt = std::find_if_not(value.rbegin(),
                                        value.rend(),
                                        [&isWhitespace](char character)
                                        {
                                            return isWhitespace(static_cast<unsigned char>(character));
                                        })
                           .base();
    return std::string(beginIt, endIt);
}

bool IsAllDigits(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    return std::all_of(value.begin(),
                       value.end(),
                       [](unsigned char character)
                       {
                           return std::isdigit(character) != 0;
                       });
}

int32_t WrapGridIndex(int32_t value, uint32_t gridSize)
{
    if (gridSize == 0)
    {
        return 0;
    }

    const int32_t signedGridSize = static_cast<int32_t>(gridSize);
    int32_t       wrappedValue   = value % signedGridSize;
    if (wrappedValue < 0)
    {
        wrappedValue += signedGridSize;
    }

    return wrappedValue;
}

bool ShouldRenderSkyPuzzleLayerInPass(int32_t sceneLayerIndex, uint32_t renderPass)
{
    switch (renderPass)
    {
        case 0:
            return sceneLayerIndex <= 0;
        case 1:
            return sceneLayerIndex > 0 && sceneLayerIndex < 4;
        default:
            return sceneLayerIndex >= 4;
    }
}

std::string JoinPath(const std::string& basePath, const std::string& relativePath)
{
    const std::string normalizedRelative = LXCore::NormalizeVirtualResourcePath(relativePath, true);
    if (normalizedRelative.empty())
    {
        return {};
    }

    if (normalizedRelative.find(':') != std::string::npos || normalizedRelative.front() == '/')
    {
        return normalizedRelative;
    }

    const size_t slashPos = basePath.find_last_of('/');
    if (slashPos == std::string::npos)
    {
        return normalizedRelative;
    }

    return LXCore::NormalizeVirtualResourcePath(basePath.substr(0, slashPos + 1) + normalizedRelative, true);
}

bool TryResolveTexturePath(const std::string&  framePath,
                           const std::string&  puzzleAssetPath,
                           CVirtualFileSystem& vfs,
                           std::string&        outResolvedPath)
{
    outResolvedPath.clear();

    const std::string normalizedFramePath = LXCore::NormalizeVirtualResourcePath(framePath, true);
    if (normalizedFramePath.empty())
    {
        return false;
    }

    std::vector<std::string> pathAttempts;
    auto                     pushPathAttempt = [&pathAttempts](const std::string& rawCandidate)
    {
        const std::string normalizedCandidate = LXCore::NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return;
        }

        if (std::find(pathAttempts.begin(), pathAttempts.end(), normalizedCandidate) != pathAttempts.end())
        {
            return;
        }

        pathAttempts.push_back(normalizedCandidate);
    };

    auto pushMapScopedPath = [&pushPathAttempt](const std::string& rawCandidate)
    {
        const std::string normalizedCandidate = LXCore::NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return;
        }

        const std::string loweredCandidate = LXCore::ToLowerAscii(normalizedCandidate);
        if (loweredCandidate.rfind("data/", 0) == 0)
        {
            pushPathAttempt(normalizedCandidate);
            return;
        }

        if (loweredCandidate.rfind("map/", 0) == 0)
        {
            pushPathAttempt(normalizedCandidate);
            pushPathAttempt("data/" + normalizedCandidate);
            return;
        }

        if (loweredCandidate.rfind("data/map/", 0) == 0)
        {
            pushPathAttempt(normalizedCandidate);
            pushPathAttempt(normalizedCandidate.substr(5));
            return;
        }

        pushPathAttempt("map/" + normalizedCandidate);
        pushPathAttempt("data/map/" + normalizedCandidate);
    };

    auto tryTexturePath = [&outResolvedPath, &vfs](const std::string& rawCandidate) -> bool
    {
        const std::string normalizedCandidate = LXCore::NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return false;
        }

        const std::string loweredCandidate = LXCore::ToLowerAscii(normalizedCandidate);
        if (loweredCandidate.rfind("map/", 0) != 0 && loweredCandidate.rfind("data/map/", 0) != 0)
        {
            return false;
        }

        if (!IsTextureAssetPath(normalizedCandidate))
        {
            return false;
        }

        if (!vfs.Exists(normalizedCandidate))
        {
            return false;
        }

        const std::vector<uint8_t> bytes = vfs.ReadAll(normalizedCandidate);
        if (bytes.empty() || !LooksLikeTexturePayload(normalizedCandidate, bytes))
        {
            return false;
        }

        outResolvedPath = normalizedCandidate;
        return true;
    };

    pushMapScopedPath(normalizedFramePath);

    if (!IsVirtualRootedPath(normalizedFramePath))
    {
        const std::string puzzleRelativePath = JoinPath(puzzleAssetPath, normalizedFramePath);
        if (!puzzleRelativePath.empty())
        {
            pushMapScopedPath(puzzleRelativePath);
        }
    }

    for (const std::string& attempt : pathAttempts)
    {
        const size_t dotPos = attempt.find_last_of('.');
        if (dotPos == std::string::npos)
        {
            if (tryTexturePath(attempt + ".dds"))
            {
                return true;
            }

            if (tryTexturePath(attempt + ".tga"))
            {
                return true;
            }

            continue;
        }

        const std::string extension = LXCore::ToLowerAscii(attempt.substr(dotPos));
        if (extension == ".dds")
        {
            if (tryTexturePath(attempt))
            {
                return true;
            }

            if (tryTexturePath(attempt.substr(0, dotPos) + ".tga"))
            {
                return true;
            }

            continue;
        }

        if (extension == ".tga")
        {
            if (tryTexturePath(attempt))
            {
                return true;
            }

            if (tryTexturePath(attempt.substr(0, dotPos) + ".dds"))
            {
                return true;
            }

            continue;
        }

        const std::string stem = attempt.substr(0, dotPos);
        if (tryTexturePath(stem + ".dds"))
        {
            return true;
        }

        if (tryTexturePath(stem + ".tga"))
        {
            return true;
        }
    }

    return false;
}

bool ParseAniFrames(const std::string&                                      aniPath,
                    CVirtualFileSystem&                                     vfs,
                    std::unordered_map<uint16_t, std::vector<std::string>>& outFramesByPuzzle,
                    std::vector<std::string>&                               outWarnings)
{
    const std::vector<uint8_t> bytes = vfs.ReadAll(aniPath);
    if (bytes.empty())
    {
        outWarnings.push_back("ANI file missing or empty: " + aniPath);
        return false;
    }

    std::string text(bytes.begin(), bytes.end());
    std::replace(text.begin(), text.end(), '\r', '\n');

    uint16_t           currentPuzzleIndex = kInvalidPuzzleIndex;
    std::istringstream stream(text);
    std::string        line;
    while (std::getline(stream, line))
    {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            currentPuzzleIndex       = kInvalidPuzzleIndex;
            std::string section      = Trim(trimmed.substr(1, trimmed.size() - 2));
            std::string sectionLower = LXCore::ToLowerAscii(section);
            if (sectionLower.rfind("puzzle", 0) == 0)
            {
                const std::string idPart = sectionLower.substr(6);
                if (IsAllDigits(idPart))
                {
                    try
                    {
                        const uint32_t value = static_cast<uint32_t>(std::stoul(idPart));
                        if (value <= (std::numeric_limits<uint16_t>::max)())
                        {
                            currentPuzzleIndex = static_cast<uint16_t>(value);
                        }
                    }
                    catch (const std::exception&)
                    {
                        currentPuzzleIndex = kInvalidPuzzleIndex;
                    }
                }
            }
            continue;
        }

        if (currentPuzzleIndex == kInvalidPuzzleIndex)
        {
            continue;
        }

        const size_t equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos)
        {
            continue;
        }

        std::string       key   = LXCore::ToLowerAscii(Trim(trimmed.substr(0, equalsPos)));
        const std::string value = Trim(trimmed.substr(equalsPos + 1));
        if (value.empty() || key == "frameamount")
        {
            continue;
        }

        if (key.rfind("frame", 0) == 0)
        {
            if (value.size() >= 4 && LXCore::ToLowerAscii(value.substr(value.size() - 4)) == ".msk")
            {
                continue;
            }
            outFramesByPuzzle[currentPuzzleIndex].push_back(value);
        }
    }

    return true;
}

} // namespace

bool SkyPuzzleSystem::Load(const std::vector<MapObjectRecord>& mapObjects,
                           const MapDescriptor&                descriptor,
                           CVirtualFileSystem&                 vfs,
                           TextureManager&                     textureManager,
                           std::vector<std::string>&           outWarnings)
{
    Unload();

    for (const MapObjectRecord& mapObject : mapObjects)
    {
        const bool isSkyPuzzleObject = mapObject.RenderLayer == MapLayer::Sky && mapObject.ObjectType == 8u;
        if (!isSkyPuzzleObject)
        {
            continue;
        }

        Layer layer;
        if (LoadLayer(mapObject, vfs, textureManager, layer, outWarnings))
        {
            m_Layers.push_back(std::move(layer));
        }
    }

    (void)descriptor;
    return true;
}

void SkyPuzzleSystem::Unload()
{
    m_Layers.clear();
    m_GroundScrollOffsetX = 0.0f;
    m_GroundScrollOffsetY = 0.0f;
}

void SkyPuzzleSystem::UpdateScroll(const TimingSnapshot& timingSnapshot, const MapDescriptor& descriptor)
{
    const float deltaSeconds = static_cast<float>(std::max(0.0, timingSnapshot.DeltaTimeSeconds));

    // Ground puzzle scroll
    if (descriptor.PuzzleRollSpeedX != 0 || descriptor.PuzzleRollSpeedY != 0)
    {
        m_GroundScrollOffsetX += static_cast<float>(descriptor.PuzzleRollSpeedX) * deltaSeconds;
        m_GroundScrollOffsetY += static_cast<float>(descriptor.PuzzleRollSpeedY) * deltaSeconds;

        const float bgWorldWidth  = static_cast<float>(descriptor.PuzzleGridWidth) * kPuzzleGridSize;
        const float bgWorldHeight = static_cast<float>(descriptor.PuzzleGridHeight) * kPuzzleGridSize;
        if (bgWorldWidth > 0.0f)
        {
            m_GroundScrollOffsetX = std::fmod(m_GroundScrollOffsetX, bgWorldWidth);
        }
        if (bgWorldHeight > 0.0f)
        {
            m_GroundScrollOffsetY = std::fmod(m_GroundScrollOffsetY, bgWorldHeight);
        }
    }

    // Sky layer scrolls and animation advancement
    const double animDeltaSeconds = std::max(0.0, timingSnapshot.DeltaTimeSeconds);
    for (Layer& skyLayer : m_Layers)
    {
        if (skyLayer.RollSpeedX != 0 || skyLayer.RollSpeedY != 0)
        {
            const float moveRateXFactor = static_cast<float>(skyLayer.MoveRateX) / 100.0f;
            const float moveRateYFactor = static_cast<float>(skyLayer.MoveRateY) / 100.0f;
            skyLayer.ScrollOffsetX += static_cast<float>(skyLayer.RollSpeedX) * moveRateXFactor * deltaSeconds;
            skyLayer.ScrollOffsetY += static_cast<float>(skyLayer.RollSpeedY) * moveRateYFactor * deltaSeconds;

            const float bgWorldWidth  = static_cast<float>(skyLayer.GridWidth) * kPuzzleGridSize;
            const float bgWorldHeight = static_cast<float>(skyLayer.GridHeight) * kPuzzleGridSize;
            if (bgWorldWidth > 0.0f)
            {
                skyLayer.ScrollOffsetX = std::fmod(skyLayer.ScrollOffsetX, bgWorldWidth);
            }
            if (bgWorldHeight > 0.0f)
            {
                skyLayer.ScrollOffsetY = std::fmod(skyLayer.ScrollOffsetY, bgWorldHeight);
            }
        }

        for (MapAnimationState& animState : skyLayer.Animations)
        {
            if (animState.Frames.empty())
            {
                continue;
            }

            const double frameStepSeconds = static_cast<double>(animState.FrameStepMilliseconds) / 1000.0;
            if (frameStepSeconds <= 0.0)
            {
                continue;
            }

            animState.AccumulatedTimeSeconds += animDeltaSeconds;
            while (animState.AccumulatedTimeSeconds >= frameStepSeconds)
            {
                animState.AccumulatedTimeSeconds -= frameStepSeconds;
                animState.LastStepFrameIndex = timingSnapshot.FrameIndex;
                if (animState.Loop)
                {
                    animState.CurrentFrame = (animState.CurrentFrame + 1u) % static_cast<uint32_t>(animState.Frames.size());
                }
                else if (animState.CurrentFrame + 1u < animState.Frames.size())
                {
                    ++animState.CurrentFrame;
                }
                else
                {
                    animState.CurrentFrame = static_cast<uint32_t>(animState.Frames.size() - 1u);
                    break;
                }
            }
        }
    }
}

uint32_t SkyPuzzleSystem::Render(SpriteRenderer&      spriteRenderer,
                                 uint32_t             renderPass,
                                 const MapCamera&     camera,
                                 const MapDescriptor& descriptor,
                                 uint32_t             viewportWidth,
                                 uint32_t             viewportHeight)
{
    if (m_Layers.empty())
    {
        return 0;
    }

    uint32_t    drawCalls       = 0;
    const float zoom            = std::max(0.1f, camera.GetZoom());
    const float vpWidth         = static_cast<float>(std::max(1u, viewportWidth));
    const float vpHeight        = static_cast<float>(std::max(1u, viewportHeight));
    const float viewWorldWidth  = vpWidth / zoom;
    const float viewWorldHeight = vpHeight / zoom;

    const float mapCenterX    = static_cast<float>(descriptor.WidthInTiles) * static_cast<float>(descriptor.CellWidth) * 0.5f;
    const float mapCenterY    = static_cast<float>(descriptor.HeightInTiles) * static_cast<float>(descriptor.CellHeight) * 0.5f;
    const float cameraCenterX = camera.GetViewCenterWorldX();
    const float cameraCenterY = camera.GetViewCenterWorldY();

    for (const Layer& skyLayer : m_Layers)
    {
        if (!ShouldRenderSkyPuzzleLayerInPass(skyLayer.SceneLayerIndex, renderPass))
        {
            continue;
        }

        if (skyLayer.GridWidth == 0 || skyLayer.GridHeight == 0)
        {
            continue;
        }

        const size_t expectedPuzzleCount = static_cast<size_t>(skyLayer.GridWidth) * static_cast<size_t>(skyLayer.GridHeight);
        if (skyLayer.PuzzleIndices.size() < expectedPuzzleCount)
        {
            continue;
        }

        const float parallaxCenterX = mapCenterX + (cameraCenterX - mapCenterX) * (static_cast<float>(skyLayer.MoveRateX) / 100.0f);
        const float parallaxCenterY = mapCenterY + (cameraCenterY - mapCenterY) * (static_cast<float>(skyLayer.MoveRateY) / 100.0f);
        const float parallaxViewX   = parallaxCenterX - viewWorldWidth * 0.5f;
        const float parallaxViewY   = parallaxCenterY - viewWorldHeight * 0.5f;

        const float bgWorldWidth  = static_cast<float>(skyLayer.GridWidth) * kPuzzleGridSize;
        const float bgWorldHeight = static_cast<float>(skyLayer.GridHeight) * kPuzzleGridSize;
        const float bgWorldX      = mapCenterX - bgWorldWidth * 0.5f + skyLayer.ScrollOffsetX;
        const float bgWorldY      = mapCenterY - bgWorldHeight * 0.5f + skyLayer.ScrollOffsetY;

        const float bgViewportX = parallaxViewX - bgWorldX;
        const float bgViewportY = parallaxViewY - bgWorldY;

        const bool    wrapGrid   = skyLayer.RollSpeedX != 0 || skyLayer.RollSpeedY != 0;
        const int32_t startGridX = static_cast<int32_t>(std::floor(bgViewportX / kPuzzleGridSize)) - 1;
        const int32_t startGridY = static_cast<int32_t>(std::floor(bgViewportY / kPuzzleGridSize)) - 1;
        int32_t       endGridX   = static_cast<int32_t>(std::floor((bgViewportX + viewWorldWidth) / kPuzzleGridSize)) + 1;
        int32_t       endGridY   = static_cast<int32_t>(std::floor((bgViewportY + viewWorldHeight) / kPuzzleGridSize)) + 1;

        if (!wrapGrid)
        {
            endGridX = std::min<int32_t>(endGridX, static_cast<int32_t>(skyLayer.GridWidth) - 1);
            endGridY = std::min<int32_t>(endGridY, static_cast<int32_t>(skyLayer.GridHeight) - 1);
        }

        if (startGridX > endGridX || startGridY > endGridY)
        {
            continue;
        }

        std::unordered_map<uint16_t, const MapAnimationState*> animationLookup;
        animationLookup.reserve(skyLayer.Animations.size());
        for (const MapAnimationState& animationState : skyLayer.Animations)
        {
            if (!animationState.Frames.empty())
            {
                animationLookup[animationState.AnimationId] = &animationState;
            }
        }

        const float puzzleCellScreenSize = kPuzzleGridSize * zoom;
        for (int32_t gridY = startGridY; gridY <= endGridY; ++gridY)
        {
            for (int32_t gridX = startGridX; gridX <= endGridX; ++gridX)
            {
                if (!wrapGrid && (gridX < 0 || gridY < 0 || gridX >= static_cast<int32_t>(skyLayer.GridWidth) ||
                                  gridY >= static_cast<int32_t>(skyLayer.GridHeight)))
                {
                    continue;
                }

                const int32_t sampleGridX = wrapGrid ? WrapGridIndex(gridX, skyLayer.GridWidth) : gridX;
                const int32_t sampleGridY = wrapGrid ? WrapGridIndex(gridY, skyLayer.GridHeight) : gridY;
                const size_t  puzzleOffset =
                    static_cast<size_t>(sampleGridY) * static_cast<size_t>(skyLayer.GridWidth) + static_cast<size_t>(sampleGridX);
                const uint16_t puzzleIndex = skyLayer.PuzzleIndices[puzzleOffset];
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
                    const auto textureIt = skyLayer.TextureRefs.find(puzzleIndex);
                    if (textureIt != skyLayer.TextureRefs.end() && textureIt->second)
                    {
                        selectedTexture = textureIt->second.get();
                    }
                }

                if (selectedTexture == nullptr)
                {
                    continue;
                }

                const float screenX = (static_cast<float>(gridX) * kPuzzleGridSize - bgViewportX) * zoom;
                const float screenY = (static_cast<float>(gridY) * kPuzzleGridSize - bgViewportY) * zoom;
                if (screenX + puzzleCellScreenSize < 0.0f || screenY + puzzleCellScreenSize < 0.0f || screenX > vpWidth ||
                    screenY > vpHeight)
                {
                    continue;
                }

                const LXCore::Vector2 drawPosition = {screenX, screenY};
                const LXCore::Vector2 drawSize     = {puzzleCellScreenSize, puzzleCellScreenSize};
                spriteRenderer.DrawSprite(selectedTexture, drawPosition, drawSize);
                ++drawCalls;
            }
        }
    }

    return drawCalls;
}

bool SkyPuzzleSystem::LoadLayer(const MapObjectRecord&    sourceObject,
                                CVirtualFileSystem&       vfs,
                                TextureManager&           textureManager,
                                Layer&                    outLayer,
                                std::vector<std::string>& outWarnings)
{
    outLayer                 = {};
    outLayer.SourceObjectId  = sourceObject.ObjectId;
    outLayer.SceneLayerIndex = sourceObject.SceneLayerIndex;
    outLayer.MoveRateX       = sourceObject.MoveRateX;
    outLayer.MoveRateY       = sourceObject.MoveRateY;

    std::string resolvedPuzzlePath = LXCore::NormalizeVirtualResourcePath(sourceObject.ResourcePath, true);
    if (resolvedPuzzlePath.empty())
    {
        outWarnings.push_back("Scene-layer puzzle resource path is empty");
        return false;
    }

    if (!vfs.Exists(resolvedPuzzlePath))
    {
        if (resolvedPuzzlePath.rfind("data/", 0) != 0)
        {
            const std::string withDataPrefix = "data/" + resolvedPuzzlePath;
            if (vfs.Exists(withDataPrefix))
            {
                resolvedPuzzlePath = withDataPrefix;
            }
        }
    }

    if (!vfs.Exists(resolvedPuzzlePath))
    {
        outWarnings.push_back("Scene-layer puzzle file not found: " + sourceObject.ResourcePath);
        return false;
    }

    const std::vector<uint8_t> bytes = vfs.ReadAll(resolvedPuzzlePath);
    if (bytes.size() < 8 + 256 + sizeof(uint32_t) * 2)
    {
        outWarnings.push_back("Scene-layer puzzle file too small: " + resolvedPuzzlePath);
        return false;
    }

    const std::string signature(reinterpret_cast<const char*>(bytes.data()), 8);
    const bool        isPuzzleV2 = signature.rfind("PUZZLE2", 0) == 0;
    const bool        isPuzzleV1 = !isPuzzleV2 && signature.rfind("PUZZLE", 0) == 0;
    if (!isPuzzleV1 && !isPuzzleV2)
    {
        outWarnings.push_back("Unsupported scene-layer puzzle signature: " + resolvedPuzzlePath);
        return false;
    }

    size_t            cursor     = 8;
    const std::string aniPathRaw = ReadFixedCString(bytes, cursor, 256);
    cursor += 256;

    const uint32_t gridWidth  = ReadU32(bytes, cursor);
    const uint32_t gridHeight = ReadU32(bytes, cursor + sizeof(uint32_t));
    cursor += sizeof(uint32_t) * 2;
    if (gridWidth == 0 || gridHeight == 0 || gridWidth > 4096 || gridHeight > 4096)
    {
        outWarnings.push_back("Scene-layer puzzle has invalid grid dimensions: " + resolvedPuzzlePath);
        return false;
    }

    const uint64_t puzzleCount64 = static_cast<uint64_t>(gridWidth) * static_cast<uint64_t>(gridHeight);
    if (puzzleCount64 > (std::numeric_limits<size_t>::max)())
    {
        outWarnings.push_back("Scene-layer puzzle grid dimensions overflow host limits: " + resolvedPuzzlePath);
        return false;
    }

    const size_t puzzleCount = static_cast<size_t>(puzzleCount64);
    if (cursor + puzzleCount * sizeof(uint16_t) > bytes.size())
    {
        outWarnings.push_back("Scene-layer puzzle index payload is incomplete: " + resolvedPuzzlePath);
        return false;
    }

    std::vector<uint16_t> puzzleIndices(puzzleCount, kInvalidPuzzleIndex);
    for (size_t index = 0; index < puzzleCount; ++index)
    {
        puzzleIndices[index] = ReadU16(bytes, cursor + index * sizeof(uint16_t));
    }
    cursor += puzzleCount * sizeof(uint16_t);

    int32_t rollSpeedX = 0;
    int32_t rollSpeedY = 0;
    if (isPuzzleV2)
    {
        if (cursor + sizeof(int32_t) * 2 <= bytes.size())
        {
            rollSpeedX = ReadI32(bytes, cursor);
            rollSpeedY = ReadI32(bytes, cursor + sizeof(int32_t));
        }
        else
        {
            outWarnings.push_back("Scene-layer puzzle v2 is missing roll-speed fields: " + resolvedPuzzlePath);
        }
    }

    outLayer.PuzzlePath    = resolvedPuzzlePath;
    outLayer.GridWidth     = gridWidth;
    outLayer.GridHeight    = gridHeight;
    outLayer.PuzzleIndices = std::move(puzzleIndices);
    outLayer.RollSpeedX    = rollSpeedX;
    outLayer.RollSpeedY    = rollSpeedY;

    std::string resolvedAniPath = LXCore::NormalizeVirtualResourcePath(aniPathRaw, true);
    if (!resolvedAniPath.empty() && !vfs.Exists(resolvedAniPath))
    {
        const std::string aniRelativeToPuzzle = JoinPath(resolvedPuzzlePath, resolvedAniPath);
        if (!aniRelativeToPuzzle.empty() && vfs.Exists(aniRelativeToPuzzle))
        {
            resolvedAniPath = aniRelativeToPuzzle;
        }
    }

    std::unordered_map<uint16_t, std::vector<std::string>> aniFramesByPuzzle;
    if (!resolvedAniPath.empty() && vfs.Exists(resolvedAniPath))
    {
        ParseAniFrames(resolvedAniPath, vfs, aniFramesByPuzzle, outWarnings);
    }

    std::unordered_set<uint16_t> uniqueIndices;
    uniqueIndices.reserve(outLayer.PuzzleIndices.size());
    for (uint16_t value : outLayer.PuzzleIndices)
    {
        if (value != kInvalidPuzzleIndex)
        {
            uniqueIndices.insert(value);
        }
    }

    std::vector<uint16_t> unresolvedPuzzleIndices;
    unresolvedPuzzleIndices.reserve(uniqueIndices.size());

    for (uint16_t puzzleIndex : uniqueIndices)
    {
        std::vector<std::shared_ptr<Texture>> frames;

        const auto framesIt = aniFramesByPuzzle.find(puzzleIndex);
        if (framesIt != aniFramesByPuzzle.end())
        {
            for (const std::string& framePathRaw : framesIt->second)
            {
                std::string resolvedFramePath;
                if (TryResolveTexturePath(framePathRaw, resolvedPuzzlePath, vfs, resolvedFramePath))
                {
                    std::shared_ptr<Texture> frame = textureManager.LoadTexture(resolvedFramePath);
                    if (frame)
                    {
                        frames.push_back(frame);
                    }
                }
            }
        }

        if (frames.empty())
        {
            unresolvedPuzzleIndices.push_back(puzzleIndex);
            continue;
        }

        outLayer.TextureRefs[puzzleIndex] = frames.front();

        if (frames.size() > 1)
        {
            MapAnimationState state;
            state.AnimationId           = puzzleIndex;
            state.CurrentFrame          = 0;
            state.FrameStepMilliseconds = 160;
            state.Loop                  = true;
            state.Frames                = std::move(frames);
            outLayer.Animations.push_back(std::move(state));
        }
    }

    if (!unresolvedPuzzleIndices.empty())
    {
        std::ostringstream warningMessage;
        warningMessage << "Missing scene-layer puzzle texture frames for " << unresolvedPuzzleIndices.size() << " puzzle indices";
        warningMessage << " (sample indices: ";
        const size_t sampleCount = std::min<size_t>(unresolvedPuzzleIndices.size(), 8);
        for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            if (sampleIndex > 0)
            {
                warningMessage << ", ";
            }

            warningMessage << unresolvedPuzzleIndices[sampleIndex];
        }

        if (unresolvedPuzzleIndices.size() > sampleCount)
        {
            warningMessage << ", ...";
        }
        warningMessage << ")";

        outWarnings.push_back(warningMessage.str());
    }

    if (outLayer.TextureRefs.empty())
    {
        outWarnings.push_back("Scene-layer puzzle resolved no textures: " + resolvedPuzzlePath);
        return false;
    }

    return true;
}

} // namespace LXMap
