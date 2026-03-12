#include "Map/MapSystem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"
#include "Timing/TimingService.h"

namespace LongXi
{

namespace
{

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool EndsWithInsensitive(const std::string& value, std::string_view suffix)
{
    if (value.size() < suffix.size())
    {
        return false;
    }

    const size_t start = value.size() - suffix.size();
    for (size_t index = 0; index < suffix.size(); ++index)
    {
        const unsigned char left  = static_cast<unsigned char>(value[start + index]);
        const unsigned char right = static_cast<unsigned char>(suffix[index]);
        if (std::tolower(left) != std::tolower(right))
        {
            return false;
        }
    }

    return true;
}

bool IsTextureAssetPath(const std::string& path)
{
    return EndsWithInsensitive(path, ".dds") || EndsWithInsensitive(path, ".tga");
}

bool IsVirtualRootedPath(const std::string& path)
{
    const std::string normalizedPath = ToLowerAscii(NormalizeVirtualResourcePath(path, true));
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
    if (EndsWithInsensitive(normalizedPath, ".dds"))
    {
        return bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ';
    }

    if (EndsWithInsensitive(normalizedPath, ".tga"))
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

uint16_t ReadU16(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint16_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint32_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

int32_t ReadI32(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int32_t>(ReadU32(bytes, offset));
}

std::string ReadFixedCString(const std::vector<uint8_t>& bytes, size_t offset, size_t length)
{
    if (offset >= bytes.size() || length == 0)
    {
        return {};
    }

    const size_t end       = std::min(bytes.size(), offset + length);
    size_t       actualEnd = offset;
    while (actualEnd < end && bytes[actualEnd] != 0)
    {
        ++actualEnd;
    }

    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), actualEnd - offset);
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
    const std::string normalizedRelative = NormalizeVirtualResourcePath(relativePath, true);
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

    return NormalizeVirtualResourcePath(basePath.substr(0, slashPos + 1) + normalizedRelative, true);
}

bool TryResolveTexturePath(const std::string&  framePath,
                           const std::string&  puzzleAssetPath,
                           CVirtualFileSystem& vfs,
                           std::string&        outResolvedPath)
{
    outResolvedPath.clear();

    const std::string normalizedFramePath = NormalizeVirtualResourcePath(framePath, true);
    if (normalizedFramePath.empty())
    {
        return false;
    }

    std::vector<std::string> pathAttempts;
    auto                     pushPathAttempt = [&pathAttempts](const std::string& rawCandidate)
    {
        const std::string normalizedCandidate = NormalizeVirtualResourcePath(rawCandidate, true);
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
        const std::string normalizedCandidate = NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return;
        }

        const std::string loweredCandidate = ToLowerAscii(normalizedCandidate);
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

        if (loweredCandidate.rfind("data/", 0) == 0)
        {
            return;
        }

        pushPathAttempt("map/" + normalizedCandidate);
        pushPathAttempt("data/map/" + normalizedCandidate);
    };

    auto tryTexturePath = [&outResolvedPath, &vfs](const std::string& rawCandidate) -> bool
    {
        const std::string normalizedCandidate = NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return false;
        }

        const std::string loweredCandidate = ToLowerAscii(normalizedCandidate);
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

        const std::string extension = ToLowerAscii(attempt.substr(dotPos));
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
            std::string sectionLower = ToLowerAscii(section);
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

        std::string       key   = ToLowerAscii(Trim(trimmed.substr(0, equalsPos)));
        const std::string value = Trim(trimmed.substr(equalsPos + 1));
        if (value.empty() || key == "frameamount")
        {
            continue;
        }

        if (key.rfind("frame", 0) == 0)
        {
            outFramesByPuzzle[currentPuzzleIndex].push_back(value);
        }
    }

    return true;
}

} // namespace

bool MapSystem::Initialize(Renderer& renderer, CVirtualFileSystem& vfs, TextureManager& textureManager)
{
    if (m_Initialized)
    {
        return true;
    }

    m_Renderer       = &renderer;
    m_VFS            = &vfs;
    m_TextureManager = &textureManager;

    if (!m_TileRenderer.Initialize(renderer))
    {
        LX_MAP_ERROR("[Map] TileRenderer initialization failed");
        return false;
    }

    m_RenderSnapshot = {};
    m_MapDescriptor  = {};
    m_State          = MapSystemState::Uninitialized;
    m_Initialized    = true;

    LX_MAP_INFO("[Map] MapSystem initialized");
    return true;
}

void MapSystem::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    UnloadMap();
    m_TileRenderer.Shutdown();

    m_Renderer       = nullptr;
    m_VFS            = nullptr;
    m_TextureManager = nullptr;
    m_State          = MapSystemState::Uninitialized;
    m_Initialized    = false;

    LX_MAP_INFO("[Map] MapSystem shutdown");
}

bool MapSystem::IsInitialized() const
{
    return m_Initialized;
}

bool MapSystem::IsMapReady() const
{
    return m_State == MapSystemState::Ready || m_State == MapSystemState::Rendering;
}

bool MapSystem::LoadMap(const std::string& mapPath)
{
    if (!m_Initialized || !m_VFS || !m_TextureManager)
    {
        LX_MAP_ERROR("[Map] LoadMap called before MapSystem initialization");
        return false;
    }

    m_State = MapSystemState::Loading;
    LX_MAP_INFO("[Map] Map load started: {}", mapPath);

    std::vector<std::string>     warnings;
    std::vector<MapObjectRecord> loadedMapObjects;
    if (!m_Loader.LoadMap(
            mapPath, *m_VFS, *m_TextureManager, m_MapDescriptor, m_TileGrid, loadedMapObjects, m_AnimationStates, m_TextureRefs, warnings))
    {
        m_State                   = MapSystemState::Failed;
        m_RenderSnapshot.Warnings = std::move(warnings);
        LX_MAP_ERROR("[Map] Map load failed: {}", mapPath);
        return false;
    }

    m_MapObjects.Clear();
    m_ObjectTextureRefs.clear();
    m_SkyPuzzleLayers.clear();
    uint32_t loadedObjectTextures = 0;

    for (MapObjectRecord& mapObject : loadedMapObjects)
    {
        const uint32_t    objectId          = mapObject.ObjectId;
        const std::string resourcePath      = mapObject.ResourcePath;
        const bool        isSkyPuzzleObject = mapObject.RenderLayer == MapLayer::Sky && mapObject.ObjectType == 8u;
        if (isSkyPuzzleObject)
        {
            SkyPuzzleLayer skyLayer;
            if (LoadSkyPuzzleLayer(mapObject, skyLayer, warnings))
            {
                m_SkyPuzzleLayers.push_back(std::move(skyLayer));
            }
        }

        m_MapObjects.AddObject(std::move(mapObject));

        if (resourcePath.empty() || !IsTextureAssetPath(resourcePath))
        {
            continue;
        }

        std::shared_ptr<Texture> objectTexture = m_TextureManager->LoadTexture(resourcePath);
        if (objectTexture)
        {
            m_ObjectTextureRefs[objectId] = objectTexture;
            ++loadedObjectTextures;
        }
    }

    m_PuzzleScrollOffsetX = 0.0f;
    m_PuzzleScrollOffsetY = 0.0f;

    m_MapCamera.SetMapDescriptor(m_MapDescriptor);
    const uint32_t viewportWidth  = static_cast<uint32_t>(std::max(1, m_Renderer->GetViewportWidth()));
    const uint32_t viewportHeight = static_cast<uint32_t>(std::max(1, m_Renderer->GetViewportHeight()));
    m_MapCamera.OnResize(viewportWidth, viewportHeight);
    m_TileRenderer.OnResize(viewportWidth, viewportHeight);
    m_MapCamera.SetCullMargins(2, 2);

    m_State                   = MapSystemState::Ready;
    m_RenderSnapshot.Warnings = std::move(warnings);

    LX_MAP_INFO("[Map] Tile grid initialized ({} x {})", m_MapDescriptor.WidthInTiles, m_MapDescriptor.HeightInTiles);
    LX_MAP_INFO("[Map] Payload parsed: passages={}, interactiveObjects={}, sceneObjects={}, unknownObjects={}",
                m_MapDescriptor.Passages.size(),
                m_MapDescriptor.InteractiveObjectCount,
                m_MapDescriptor.SceneObjectCount,
                m_MapDescriptor.UnknownObjectCount);
    LX_MAP_INFO("[Map] Scene assets loaded: sceneFiles={}, sceneParts={}",
                m_MapDescriptor.LoadedSceneFileCount,
                m_MapDescriptor.LoadedScenePartCount);
    LX_MAP_INFO("[Map] Object textures loaded: {}/{}", loadedObjectTextures, m_MapObjects.GetObjects().size());
    LX_MAP_INFO("[Map] Sky puzzle layers loaded: {}", m_SkyPuzzleLayers.size());
    LX_MAP_INFO("[Map] Map load completed: {} (resolved={}, mapId={}, puzzle={})",
                mapPath,
                m_MapDescriptor.SourcePath,
                m_MapDescriptor.MapId,
                m_MapDescriptor.PuzzleAssetPath);
    return true;
}

void MapSystem::UnloadMap()
{
    m_MapDescriptor = {};
    m_TileGrid.Clear();
    m_MapObjects.Clear();
    m_TextureRefs.clear();
    m_ObjectTextureRefs.clear();
    m_AnimationStates.clear();
    m_SkyPuzzleLayers.clear();
    m_VisibleTilesCache.clear();
    m_VisibleObjectsCache.clear();
    m_RenderSnapshot      = {};
    m_PuzzleScrollOffsetX = 0.0f;
    m_PuzzleScrollOffsetY = 0.0f;

    if (m_Initialized)
    {
        m_State = MapSystemState::Unloaded;
    }
}

void MapSystem::BeginFrame(const TimingSnapshot& timingSnapshot)
{
    if (!m_Initialized)
    {
        return;
    }

    ResetSnapshot(timingSnapshot.FrameIndex);
    if (!IsMapReady())
    {
        return;
    }

    UpdateAnimations(timingSnapshot);
    UpdatePuzzleScroll(timingSnapshot);
    UpdateSkyPuzzleScroll(timingSnapshot);

    uint32_t animatedTileCount = static_cast<uint32_t>(m_AnimationStates.size());
    for (const SkyPuzzleLayer& skyLayer : m_SkyPuzzleLayers)
    {
        animatedTileCount += static_cast<uint32_t>(skyLayer.Animations.size());
    }
    m_RenderSnapshot.AnimatedTiles = animatedTileCount;
}

void MapSystem::Render(SpriteRenderer& spriteRenderer, const TimingSnapshot& timingSnapshot)
{
    if (!m_Initialized)
    {
        return;
    }

    if (!IsMapReady())
    {
        if (m_State == MapSystemState::Failed)
        {
            m_RenderSnapshot.Warnings.push_back("Render skipped: map is in failed state");
        }
        return;
    }

    m_State = MapSystemState::Rendering;

    const VisibleTileWindow window = m_MapCamera.ComputeVisibleWindow();

    m_VisibleTilesCache.clear();
    m_TileGrid.EnumerateVisible(window, m_VisibleTilesCache);

    m_VisibleObjectsCache.clear();
    m_MapObjects.CollectVisible(window, m_VisibleObjectsCache);
    m_RenderSnapshot.VisibleObjects = static_cast<uint32_t>(m_VisibleObjectsCache.size());

    RenderSkyPuzzleLayers(spriteRenderer, 0);

    const bool renderSuccess = m_TileRenderer.RenderTiles(m_MapDescriptor,
                                                          m_MapCamera,
                                                          m_AnimationStates,
                                                          m_TextureRefs,
                                                          spriteRenderer,
                                                          m_PuzzleScrollOffsetX,
                                                          m_PuzzleScrollOffsetY,
                                                          m_RenderSnapshot);
    if (!renderSuccess)
    {
        m_RenderSnapshot.Warnings.push_back("TileRenderer failed to submit map tiles");
    }

    RenderSkyPuzzleLayers(spriteRenderer, 1);
    RenderMapObjects(spriteRenderer);
    RenderSkyPuzzleLayers(spriteRenderer, 2);

    m_RenderSnapshot.FrameIndex = timingSnapshot.FrameIndex;
    m_RenderSnapshot.IsValid    = renderSuccess;

    if (m_RenderSnapshot.FrameIndex > 0 && (m_RenderSnapshot.FrameIndex % 600) == 0)
    {
        LX_MAP_INFO("[Map] Visible tiles={}, drawCalls={}, objects={}, animated={}",
                    m_RenderSnapshot.VisibleTiles,
                    m_RenderSnapshot.DrawCalls,
                    m_RenderSnapshot.VisibleObjects,
                    m_RenderSnapshot.AnimatedTiles);
    }

    m_State = MapSystemState::Ready;
}

void MapSystem::OnResize(uint32_t width, uint32_t height)
{
    if (!m_Initialized)
    {
        return;
    }

    m_MapCamera.OnResize(width, height);
    m_TileRenderer.OnResize(width, height);
}

void MapSystem::PanCamera(float deltaWorldX, float deltaWorldY)
{
    if (!m_Initialized || !IsMapReady())
    {
        return;
    }

    const float newCenterX = m_MapCamera.GetViewCenterWorldX() + deltaWorldX;
    const float newCenterY = m_MapCamera.GetViewCenterWorldY() + deltaWorldY;
    m_MapCamera.SetViewCenter(newCenterX, newCenterY);
}

void MapSystem::AdjustCameraZoom(float deltaZoom)
{
    if (!m_Initialized || !IsMapReady())
    {
        return;
    }

    m_MapCamera.SetZoom(m_MapCamera.GetZoom() + deltaZoom);
}

MapSystemState MapSystem::GetState() const
{
    return m_State;
}

const MapDescriptor* MapSystem::GetMapDescriptor() const
{
    return m_MapDescriptor.IsValid() ? &m_MapDescriptor : nullptr;
}

const MapRenderSnapshot& MapSystem::GetRenderSnapshot() const
{
    return m_RenderSnapshot;
}

void MapSystem::UpdateAnimations(const TimingSnapshot& timingSnapshot)
{
    const double deltaSeconds      = std::max(0.0, timingSnapshot.DeltaTimeSeconds);
    auto         advanceAnimations = [&timingSnapshot, deltaSeconds](std::vector<MapAnimationState>& animations)
    {
        for (MapAnimationState& animationState : animations)
        {
            if (animationState.Frames.empty())
            {
                continue;
            }

            const double frameStepSeconds = static_cast<double>(animationState.FrameStepMilliseconds) / 1000.0;
            if (frameStepSeconds <= 0.0)
            {
                continue;
            }

            animationState.AccumulatedTimeSeconds += deltaSeconds;
            while (animationState.AccumulatedTimeSeconds >= frameStepSeconds)
            {
                animationState.AccumulatedTimeSeconds -= frameStepSeconds;
                animationState.LastStepFrameIndex = timingSnapshot.FrameIndex;
                if (animationState.Loop)
                {
                    animationState.CurrentFrame = (animationState.CurrentFrame + 1u) % static_cast<uint32_t>(animationState.Frames.size());
                }
                else if (animationState.CurrentFrame + 1u < animationState.Frames.size())
                {
                    ++animationState.CurrentFrame;
                }
                else
                {
                    animationState.CurrentFrame = static_cast<uint32_t>(animationState.Frames.size() - 1u);
                    break;
                }
            }
        }
    };

    advanceAnimations(m_AnimationStates);
    for (SkyPuzzleLayer& skyLayer : m_SkyPuzzleLayers)
    {
        advanceAnimations(skyLayer.Animations);
    }
}

void MapSystem::UpdatePuzzleScroll(const TimingSnapshot& timingSnapshot)
{
    if (m_MapDescriptor.PuzzleRollSpeedX == 0 && m_MapDescriptor.PuzzleRollSpeedY == 0)
    {
        return;
    }

    const float deltaSeconds = static_cast<float>(std::max(0.0, timingSnapshot.DeltaTimeSeconds));
    m_PuzzleScrollOffsetX += static_cast<float>(m_MapDescriptor.PuzzleRollSpeedX) * deltaSeconds;
    m_PuzzleScrollOffsetY += static_cast<float>(m_MapDescriptor.PuzzleRollSpeedY) * deltaSeconds;

    // Wrap the scroll offset to avoid floating point drift over time.
    const float bgWorldWidth  = static_cast<float>(m_MapDescriptor.PuzzleGridWidth) * kPuzzleGridSize;
    const float bgWorldHeight = static_cast<float>(m_MapDescriptor.PuzzleGridHeight) * kPuzzleGridSize;
    if (bgWorldWidth > 0.0f)
    {
        m_PuzzleScrollOffsetX = std::fmod(m_PuzzleScrollOffsetX, bgWorldWidth);
    }
    if (bgWorldHeight > 0.0f)
    {
        m_PuzzleScrollOffsetY = std::fmod(m_PuzzleScrollOffsetY, bgWorldHeight);
    }
}

void MapSystem::UpdateSkyPuzzleScroll(const TimingSnapshot& timingSnapshot)
{
    if (m_SkyPuzzleLayers.empty())
    {
        return;
    }

    const float deltaSeconds = static_cast<float>(std::max(0.0, timingSnapshot.DeltaTimeSeconds));
    for (SkyPuzzleLayer& skyLayer : m_SkyPuzzleLayers)
    {
        if (skyLayer.RollSpeedX == 0 && skyLayer.RollSpeedY == 0)
        {
            continue;
        }

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
}

bool MapSystem::LoadSkyPuzzleLayer(const MapObjectRecord& skyPuzzleObject, SkyPuzzleLayer& outLayer, std::vector<std::string>& outWarnings)
{
    outLayer                  = {};
    outLayer.SourceObjectId   = skyPuzzleObject.ObjectId;
    outLayer.SceneLayerIndex  = skyPuzzleObject.SceneLayerIndex;
    outLayer.MoveRateX        = skyPuzzleObject.MoveRateX;
    outLayer.MoveRateY        = skyPuzzleObject.MoveRateY;

    if (m_VFS == nullptr || m_TextureManager == nullptr)
    {
        return false;
    }

    std::string resolvedPuzzlePath = NormalizeVirtualResourcePath(skyPuzzleObject.ResourcePath, true);
    if (resolvedPuzzlePath.empty())
    {
        outWarnings.push_back("Scene-layer puzzle resource path is empty");
        return false;
    }

    if (!m_VFS->Exists(resolvedPuzzlePath))
    {
        if (resolvedPuzzlePath.rfind("data/", 0) != 0)
        {
            const std::string withDataPrefix = "data/" + resolvedPuzzlePath;
            if (m_VFS->Exists(withDataPrefix))
            {
                resolvedPuzzlePath = withDataPrefix;
            }
        }
    }

    if (!m_VFS->Exists(resolvedPuzzlePath))
    {
        outWarnings.push_back("Scene-layer puzzle file not found: " + skyPuzzleObject.ResourcePath);
        return false;
    }

    const std::vector<uint8_t> bytes = m_VFS->ReadAll(resolvedPuzzlePath);
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

    std::string resolvedAniPath = NormalizeVirtualResourcePath(aniPathRaw, true);
    if (!resolvedAniPath.empty() && !m_VFS->Exists(resolvedAniPath))
    {
        const std::string aniRelativeToPuzzle = JoinPath(resolvedPuzzlePath, resolvedAniPath);
        if (!aniRelativeToPuzzle.empty() && m_VFS->Exists(aniRelativeToPuzzle))
        {
            resolvedAniPath = aniRelativeToPuzzle;
        }
    }

    std::unordered_map<uint16_t, std::vector<std::string>> aniFramesByPuzzle;
    if (!resolvedAniPath.empty() && m_VFS->Exists(resolvedAniPath))
    {
        ParseAniFrames(resolvedAniPath, *m_VFS, aniFramesByPuzzle, outWarnings);
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
                if (TryResolveTexturePath(framePathRaw, resolvedPuzzlePath, *m_VFS, resolvedFramePath))
                {
                    std::shared_ptr<Texture> frame = m_TextureManager->LoadTexture(resolvedFramePath);
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

void MapSystem::RenderSkyPuzzleLayers(SpriteRenderer& spriteRenderer, uint32_t renderPass)
{
    if (m_SkyPuzzleLayers.empty())
    {
        return;
    }

    const float zoom            = std::max(0.1f, m_MapCamera.GetZoom());
    const float viewportWidth   = static_cast<float>(std::max(1, m_Renderer != nullptr ? m_Renderer->GetViewportWidth() : 1));
    const float viewportHeight  = static_cast<float>(std::max(1, m_Renderer != nullptr ? m_Renderer->GetViewportHeight() : 1));
    const float viewWorldWidth  = viewportWidth / zoom;
    const float viewWorldHeight = viewportHeight / zoom;

    const float mapCenterX    = static_cast<float>(m_MapDescriptor.WidthInTiles) * static_cast<float>(m_MapDescriptor.CellWidth) * 0.5f;
    const float mapCenterY    = static_cast<float>(m_MapDescriptor.HeightInTiles) * static_cast<float>(m_MapDescriptor.CellHeight) * 0.5f;
    const float cameraCenterX = m_MapCamera.GetViewCenterWorldX();
    const float cameraCenterY = m_MapCamera.GetViewCenterWorldY();

    for (const SkyPuzzleLayer& skyLayer : m_SkyPuzzleLayers)
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
                if (!wrapGrid &&
                    (gridX < 0 || gridY < 0 || gridX >= static_cast<int32_t>(skyLayer.GridWidth) || gridY >= static_cast<int32_t>(skyLayer.GridHeight)))
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
                if (screenX + puzzleCellScreenSize < 0.0f || screenY + puzzleCellScreenSize < 0.0f || screenX > viewportWidth ||
                    screenY > viewportHeight)
                {
                    continue;
                }

                const Vector2 drawPosition = {screenX, screenY};
                const Vector2 drawSize     = {puzzleCellScreenSize, puzzleCellScreenSize};
                spriteRenderer.DrawSprite(selectedTexture, drawPosition, drawSize);
                ++m_RenderSnapshot.DrawCalls;
            }
        }
    }
}

void MapSystem::RenderMapObjects(SpriteRenderer& spriteRenderer)
{
    if (m_VisibleObjectsCache.empty())
    {
        return;
    }

    const float zoom = m_MapCamera.GetZoom();

    for (const MapObjectRecord* objectRecord : m_VisibleObjectsCache)
    {
        if (objectRecord == nullptr)
        {
            continue;
        }

        const auto textureIt = m_ObjectTextureRefs.find(objectRecord->ObjectId);
        if (textureIt == m_ObjectTextureRefs.end() || !textureIt->second)
        {
            continue;
        }

        float worldX = 0.0f;
        float worldY = 0.0f;
        if (objectRecord->HasWorldPosition)
        {
            worldX = static_cast<float>(objectRecord->WorldX);
            worldY = static_cast<float>(objectRecord->WorldY);
        }
        else
        {
            const float cellWidth  = static_cast<float>(m_MapDescriptor.CellWidth);
            const float cellHeight = static_cast<float>(m_MapDescriptor.CellHeight);
            const float originX    = static_cast<float>(m_MapDescriptor.OriginX);
            const float originY    = static_cast<float>(m_MapDescriptor.OriginY);
            worldX                 = cellWidth * static_cast<float>(objectRecord->TileX - objectRecord->TileY) * 0.5f + originX;
            worldY                 = cellHeight * static_cast<float>(objectRecord->TileX + objectRecord->TileY) * 0.5f + originY;
        }

        // Legacy cover/scene/terrain-part objects project from cell position + sprite offsets.
        // Their serialized "height" field is not a direct screen-space Z displacement.
        float screenX = 0.0f;
        float screenY = 0.0f;
        m_MapCamera.WorldToScreen(worldX, worldY, 0.0f, screenX, screenY);

        const Texture* texture    = textureIt->second.get();
        const float    drawWidth  = static_cast<float>(texture->GetWidth()) * zoom;
        const float    drawHeight = static_cast<float>(texture->GetHeight()) * zoom;

        const bool  isTerrainPartObject = objectRecord->ObjectType == 2u;
        const float offsetX             = static_cast<float>(objectRecord->OffsetX) * zoom;
        const float offsetY             = static_cast<float>(objectRecord->OffsetY) * zoom;
        // MAP_COVER / MAP_SCENE use screen - offset while MAP_TERRAIN_PART uses screen + offset.
        const Vector2 drawPosition =
            isTerrainPartObject ? Vector2{screenX + offsetX, screenY + offsetY} : Vector2{screenX - offsetX, screenY - offsetY};
        const Vector2 drawSize = {drawWidth, drawHeight};
        spriteRenderer.DrawSprite(texture, drawPosition, drawSize);
        ++m_RenderSnapshot.DrawCalls;
    }
}

void MapSystem::ResetSnapshot(uint64_t frameIndex)
{
    m_RenderSnapshot.FrameIndex     = frameIndex;
    m_RenderSnapshot.VisibleTiles   = 0;
    m_RenderSnapshot.VisibleObjects = 0;
    m_RenderSnapshot.AnimatedTiles  = 0;
    m_RenderSnapshot.DrawCalls      = 0;
    m_RenderSnapshot.IsValid        = false;
    m_RenderSnapshot.Warnings.clear();
}

} // namespace LongXi
