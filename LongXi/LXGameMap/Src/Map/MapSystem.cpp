#include "Map/MapSystem.h"

#include <algorithm>
#include <sstream>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Core/StringUtils.h"
#include "Core/Timing/TimingService.h"
#include "Renderer/Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LXMap
{

namespace
{

bool IsTextureAssetPath(const std::string& path)
{
    return LXCore::EndsWithInsensitive(path, ".dds") || LXCore::EndsWithInsensitive(path, ".tga");
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
    m_ObjectAnimations.clear();
    uint32_t loadedObjectTextures   = 0;
    uint32_t loadedObjectAnimations = 0;

    // Load sky puzzle layers before the object loop moves the records out of loadedMapObjects.
    m_SkyPuzzleSystem.Load(loadedMapObjects, m_MapDescriptor, *m_VFS, *m_TextureManager, warnings);

    // Cache parsed ANI sections so we don't re-parse the same .ani file for each object.
    using AniSectionFrames = std::unordered_map<std::string, std::vector<std::string>>;
    std::unordered_map<std::string, AniSectionFrames> aniCache;

    auto loadAniSectionsForObject = [this, &aniCache](const std::string& aniPath) -> const AniSectionFrames*
    {
        const auto cacheIt = aniCache.find(aniPath);
        if (cacheIt != aniCache.end())
        {
            return &cacheIt->second;
        }

        AniSectionFrames           sections;
        const std::vector<uint8_t> aniBytes = m_VFS->ReadAll(aniPath);
        if (!aniBytes.empty())
        {
            std::string text(aniBytes.begin(), aniBytes.end());
            std::replace(text.begin(), text.end(), '\r', '\n');

            std::istringstream stream(text);
            std::string        line;
            std::string        currentSection;
            while (std::getline(stream, line))
            {
                size_t start = 0;
                size_t end   = line.size();
                while (start < end && (line[start] == ' ' || line[start] == '\t'))
                {
                    ++start;
                }
                while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\n' || line[end - 1] == '\r'))
                {
                    --end;
                }
                const std::string trimmed = line.substr(start, end - start);
                if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
                {
                    continue;
                }

                if (trimmed.front() == '[' && trimmed.back() == ']')
                {
                    currentSection = LXCore::ToLowerAscii(trimmed.substr(1, trimmed.size() - 2));
                    continue;
                }

                if (currentSection.empty())
                {
                    continue;
                }

                const size_t equalsPos = trimmed.find('=');
                if (equalsPos == std::string::npos)
                {
                    continue;
                }

                const std::string key   = LXCore::ToLowerAscii(trimmed.substr(0, equalsPos));
                const std::string value = trimmed.substr(equalsPos + 1);
                if (value.empty() || key == "frameamount")
                {
                    continue;
                }

                if (key.rfind("frame", 0) == 0)
                {
                    // Skip .msk files - they're alpha masks that will be loaded automatically
                    // when the corresponding .dds texture is loaded
                    if (value.size() >= 4 && LXCore::ToLowerAscii(value.substr(value.size() - 4)) == ".msk")
                    {
                        continue;
                    }
                    sections[currentSection].push_back(value);
                }
            }
        }

        const auto [insertedIt, _] = aniCache.emplace(aniPath, std::move(sections));
        return &insertedIt->second;
    };

    for (MapObjectRecord& mapObject : loadedMapObjects)
    {
        const uint32_t    objectId     = mapObject.ObjectId;
        const std::string resourcePath = mapObject.ResourcePath;

        // Try to load all animation frames for objects that reference an ANI file.
        if (!mapObject.AniSourcePath.empty() && m_TextureManager != nullptr)
        {
            const AniSectionFrames* sections = loadAniSectionsForObject(mapObject.AniSourcePath);
            if (sections != nullptr && !sections->empty())
            {
                // Build section name candidates (same logic as MapLoader).
                std::vector<std::string> sectionCandidates;
                auto                     pushCandidate = [&sectionCandidates](const std::string& raw)
                {
                    std::string normalized = LXCore::ToLowerAscii(raw);
                    if (!normalized.empty() &&
                        std::find(sectionCandidates.begin(), sectionCandidates.end(), normalized) == sectionCandidates.end())
                    {
                        sectionCandidates.push_back(normalized);
                    }
                };
                pushCandidate(mapObject.AniSectionName);
                const size_t colonPos = mapObject.AniSectionName.find_last_of(':');
                if (colonPos != std::string::npos && colonPos + 1 < mapObject.AniSectionName.size())
                {
                    pushCandidate(mapObject.AniSectionName.substr(colonPos + 1));
                }

                for (const std::string& sectionName : sectionCandidates)
                {
                    const auto sectionIt = sections->find(sectionName);
                    if (sectionIt == sections->end())
                    {
                        continue;
                    }

                    std::vector<std::shared_ptr<Texture>> frames;
                    for (const std::string& framePath : sectionIt->second)
                    {
                        // Skip .msk files - they're alpha masks that will be loaded automatically
                        // (This is a safety check in case the ANI parsing filter is bypassed)
                        if (framePath.size() >= 4 && LXCore::ToLowerAscii(framePath.substr(framePath.size() - 4)) == ".msk")
                        {
                            continue;
                        }

                        std::shared_ptr<Texture> frame = m_TextureManager->LoadTexture(framePath);
                        if (!frame)
                        {
                            // Try with map/ prefix variations.
                            const std::string normalizedFrame = LXCore::NormalizeVirtualResourcePath(framePath, true);
                            if (!normalizedFrame.empty())
                            {
                                frame = m_TextureManager->LoadTexture("map/" + normalizedFrame);
                            }
                        }
                        if (frame)
                        {
                            frames.push_back(std::move(frame));
                        }
                    }

                    if (frames.size() > 1)
                    {
                        MapObjectAnimationState animState;
                        animState.ObjectId              = objectId;
                        animState.CurrentFrame          = 0;
                        animState.FrameStepMilliseconds = static_cast<uint32_t>(std::max(1, mapObject.FrameInterval));
                        animState.Loop                  = true;
                        animState.Frames                = std::move(frames);
                        m_ObjectAnimations[objectId]    = std::move(animState);
                        ++loadedObjectAnimations;
                    }
                    break;
                }
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
    LX_MAP_INFO("[Map] Object textures loaded: {}/{}, animated: {}",
                loadedObjectTextures,
                m_MapObjects.GetObjects().size(),
                loadedObjectAnimations);
    LX_MAP_INFO("[Map] Sky puzzle layers loaded: {}", m_SkyPuzzleSystem.GetLayerCount());
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
    m_ObjectAnimations.clear();
    m_AnimationStates.clear();
    m_SkyPuzzleSystem.Unload();
    m_VisibleTilesCache.clear();
    m_VisibleObjectsCache.clear();
    m_RenderSnapshot = {};

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
    UpdateObjectAnimations(timingSnapshot);
    m_SkyPuzzleSystem.UpdateScroll(timingSnapshot, m_MapDescriptor);

    m_RenderSnapshot.AnimatedTiles = static_cast<uint32_t>(m_AnimationStates.size());
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

    const uint32_t vpWidth  = static_cast<uint32_t>(std::max(1, m_Renderer != nullptr ? m_Renderer->GetViewportWidth() : 1));
    const uint32_t vpHeight = static_cast<uint32_t>(std::max(1, m_Renderer != nullptr ? m_Renderer->GetViewportHeight() : 1));
    m_RenderSnapshot.DrawCalls += m_SkyPuzzleSystem.Render(spriteRenderer, 0, m_MapCamera, m_MapDescriptor, vpWidth, vpHeight);

    const bool renderSuccess = m_TileRenderer.RenderTiles(m_MapDescriptor,
                                                          m_MapCamera,
                                                          m_AnimationStates,
                                                          m_TextureRefs,
                                                          spriteRenderer,
                                                          m_SkyPuzzleSystem.GetGroundScrollOffsetX(),
                                                          m_SkyPuzzleSystem.GetGroundScrollOffsetY(),
                                                          m_RenderSnapshot);
    if (!renderSuccess)
    {
        m_RenderSnapshot.Warnings.push_back("TileRenderer failed to submit map tiles");
    }

    // Render water reflection (sky mirror effect on water tiles)
    // DISABLED: Current implementation causes visual artifacts. Needs proper implementation.
    // TODO: Rewrite RenderWaterReflection using correct isometric projection and render-target sampling.
    // RenderWaterReflection(spriteRenderer);

    m_RenderSnapshot.DrawCalls += m_SkyPuzzleSystem.Render(spriteRenderer, 1, m_MapCamera, m_MapDescriptor, vpWidth, vpHeight);
    RenderMapObjects(spriteRenderer);
    m_RenderSnapshot.DrawCalls += m_SkyPuzzleSystem.Render(spriteRenderer, 2, m_MapCamera, m_MapDescriptor, vpWidth, vpHeight);

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
}

void MapSystem::UpdateObjectAnimations(const TimingSnapshot& timingSnapshot)
{
    if (m_ObjectAnimations.empty())
    {
        return;
    }

    const double deltaSeconds = std::max(0.0, timingSnapshot.DeltaTimeSeconds);
    for (auto& [objectId, animState] : m_ObjectAnimations)
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

        animState.AccumulatedTimeSeconds += deltaSeconds;
        while (animState.AccumulatedTimeSeconds >= frameStepSeconds)
        {
            animState.AccumulatedTimeSeconds -= frameStepSeconds;
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

        // Determine the texture to render: animated frame or static texture.
        const Texture* texture = nullptr;
        const auto     animIt  = m_ObjectAnimations.find(objectRecord->ObjectId);
        if (animIt != m_ObjectAnimations.end() && !animIt->second.Frames.empty())
        {
            const MapObjectAnimationState& animState = animIt->second;
            const uint32_t                 frameIdx  = animState.CurrentFrame % static_cast<uint32_t>(animState.Frames.size());
            texture                                  = animState.Frames[frameIdx].get();
        }

        if (texture == nullptr)
        {
            const auto textureIt = m_ObjectTextureRefs.find(objectRecord->ObjectId);
            if (textureIt == m_ObjectTextureRefs.end() || !textureIt->second)
            {
                continue;
            }
            texture = textureIt->second.get();
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

        // Project cell world position to screen, then apply sprite anchor offset.
        // TerrainPart (.scene): SpriteOffset is negative (top-left relative to cell center) → add.
        // Cover/Scene (DMAP): Offset is positive anchor point → subtract.
        float screenX = 0.0f;
        float screenY = 0.0f;
        m_MapCamera.WorldToScreen(worldX, worldY, 0.0f, screenX, screenY);

        const float drawWidth  = static_cast<float>(texture->GetWidth()) * zoom;
        const float drawHeight = static_cast<float>(texture->GetHeight()) * zoom;

        const float offsetX = static_cast<float>(objectRecord->OffsetX) * zoom;
        const float offsetY = static_cast<float>(objectRecord->OffsetY) * zoom;

        float drawX = 0.0f;
        float drawY = 0.0f;
        if (objectRecord->ObjectType == 2u)
        {
            // TerrainPart: .scene SpriteOffset is signed offset from cell center to sprite top-left.
            drawX = screenX + offsetX;
            drawY = screenY + offsetY;
        }
        else
        {
            // MAP_COVER, MAP_SCENE and others: DMAP offset is positive anchor → subtract.
            drawX = screenX - offsetX;
            drawY = screenY - offsetY;
        }

        const LXCore::Vector2 drawPosition = LXCore::Vector2{drawX, drawY};
        const LXCore::Vector2 drawSize     = {drawWidth, drawHeight};

        // Apply legacy ARGB tint color.
        const uint32_t      argb      = objectRecord->TintARGB;
        const float         tintA     = static_cast<float>((argb >> 24) & 0xFF) / 255.0f;
        const float         tintR     = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
        const float         tintG     = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
        const float         tintB     = static_cast<float>(argb & 0xFF) / 255.0f;
        const LXCore::Color tintColor = {tintR, tintG, tintB, tintA};
        spriteRenderer.DrawSprite(texture, drawPosition, drawSize, {0.0f, 0.0f}, {1.0f, 1.0f}, tintColor);
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

} // namespace LXMap
