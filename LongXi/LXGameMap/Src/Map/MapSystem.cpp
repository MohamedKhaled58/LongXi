#include "Map/MapSystem.h"

#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Timing/TimingService.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

#include <algorithm>
#include <cmath>

namespace LongXi
{

bool MapSystem::Initialize(Renderer& renderer, CVirtualFileSystem& vfs, TextureManager& textureManager)
{
    if (m_Initialized)
    {
        return true;
    }

    m_Renderer = &renderer;
    m_VFS = &vfs;
    m_TextureManager = &textureManager;

    if (!m_TileRenderer.Initialize(renderer))
    {
        LX_ENGINE_ERROR("[Map] TileRenderer initialization failed");
        return false;
    }

    m_RenderSnapshot = {};
    m_MapDescriptor = {};
    m_State = MapSystemState::Uninitialized;
    m_Initialized = true;

    LX_ENGINE_INFO("[Map] MapSystem initialized");
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

    m_Renderer = nullptr;
    m_VFS = nullptr;
    m_TextureManager = nullptr;
    m_State = MapSystemState::Uninitialized;
    m_Initialized = false;

    LX_ENGINE_INFO("[Map] MapSystem shutdown");
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
        LX_ENGINE_ERROR("[Map] LoadMap called before MapSystem initialization");
        return false;
    }

    m_State = MapSystemState::Loading;
    LX_ENGINE_INFO("[Map] Map load started: {}", mapPath);

    std::vector<std::string> warnings;
    std::vector<MapObjectRecord> loadedMapObjects;
    if (!m_Loader.LoadMap(mapPath, *m_VFS, *m_TextureManager, m_MapDescriptor, m_TileGrid, loadedMapObjects, m_AnimationStates, m_TextureRefs, warnings))
    {
        m_State = MapSystemState::Failed;
        m_RenderSnapshot.Warnings = std::move(warnings);
        LX_ENGINE_ERROR("[Map] Map load failed: {}", mapPath);
        return false;
    }

    m_MapObjects.Clear();
    m_ObjectTextureRefs.clear();
    uint32_t loadedObjectTextures = 0;
    for (MapObjectRecord& mapObject : loadedMapObjects)
    {
        const uint32_t objectId = mapObject.ObjectId;
        const std::string& resourcePath = mapObject.ResourcePath;
        m_MapObjects.AddObject(std::move(mapObject));

        if (resourcePath.empty())
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
    const uint32_t viewportWidth = static_cast<uint32_t>(std::max(1, m_Renderer->GetViewportWidth()));
    const uint32_t viewportHeight = static_cast<uint32_t>(std::max(1, m_Renderer->GetViewportHeight()));
    m_MapCamera.OnResize(viewportWidth, viewportHeight);
    m_TileRenderer.OnResize(viewportWidth, viewportHeight);
    m_MapCamera.SetCullMargins(2, 2);

    m_State = MapSystemState::Ready;
    m_RenderSnapshot.Warnings = std::move(warnings);

    LX_ENGINE_INFO("[Map] Tile grid initialized ({} x {})", m_MapDescriptor.WidthInTiles, m_MapDescriptor.HeightInTiles);
    LX_ENGINE_INFO("[Map] Payload parsed: passages={}, interactiveObjects={}, sceneObjects={}, unknownObjects={}",
                   m_MapDescriptor.Passages.size(),
                   m_MapDescriptor.InteractiveObjectCount,
                   m_MapDescriptor.SceneObjectCount,
                   m_MapDescriptor.UnknownObjectCount);
    LX_ENGINE_INFO("[Map] Scene assets loaded: sceneFiles={}, sceneParts={}", m_MapDescriptor.LoadedSceneFileCount, m_MapDescriptor.LoadedScenePartCount);
    LX_ENGINE_INFO("[Map] Object textures loaded: {}/{}", loadedObjectTextures, m_MapObjects.GetObjects().size());
    LX_ENGINE_INFO("[Map] Map load completed: {} (resolved={}, mapId={}, puzzle={})", mapPath, m_MapDescriptor.SourcePath, m_MapDescriptor.MapId, m_MapDescriptor.PuzzleAssetPath);
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
    m_VisibleTilesCache.clear();
    m_VisibleObjectsCache.clear();
    m_RenderSnapshot = {};
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

    const bool renderSuccess =
        m_TileRenderer.RenderTiles(m_MapDescriptor, m_MapCamera, m_AnimationStates, m_TextureRefs, spriteRenderer, m_PuzzleScrollOffsetX, m_PuzzleScrollOffsetY, m_RenderSnapshot);
    if (!renderSuccess)
    {
        m_RenderSnapshot.Warnings.push_back("TileRenderer failed to submit map tiles");
    }

    RenderMapObjects(spriteRenderer);

    m_RenderSnapshot.FrameIndex = timingSnapshot.FrameIndex;
    m_RenderSnapshot.IsValid = renderSuccess;

    if (m_RenderSnapshot.FrameIndex > 0 && (m_RenderSnapshot.FrameIndex % 600) == 0)
    {
        LX_ENGINE_INFO("[Map] Visible tiles={}, drawCalls={}, objects={}, animated={}",
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
    const double deltaSeconds = std::max(0.0, timingSnapshot.DeltaTimeSeconds);
    for (MapAnimationState& animationState : m_AnimationStates)
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
    const float bgWorldWidth = static_cast<float>(m_MapDescriptor.PuzzleGridWidth) * kPuzzleGridSize;
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
        float worldZ = 0.0f;
        if (!m_MapCamera.TileToWorld(objectRecord->TileX, objectRecord->TileY, 0, worldX, worldY, worldZ))
        {
            continue;
        }

        worldX += static_cast<float>(objectRecord->OffsetX);
        worldY += static_cast<float>(objectRecord->OffsetY);
        worldZ = static_cast<float>(objectRecord->HeightOffset);

        float screenX = 0.0f;
        float screenY = 0.0f;
        m_MapCamera.WorldToScreen(worldX, worldY, worldZ, screenX, screenY);

        const Texture* texture = textureIt->second.get();
        const float drawWidth = static_cast<float>(texture->GetWidth()) * zoom;
        const float drawHeight = static_cast<float>(texture->GetHeight()) * zoom;

        const Vector2 drawPosition = {screenX - drawWidth * 0.5f, screenY - drawHeight};
        const Vector2 drawSize = {drawWidth, drawHeight};
        spriteRenderer.DrawSprite(texture, drawPosition, drawSize);
        ++m_RenderSnapshot.DrawCalls;
    }
}

void MapSystem::ResetSnapshot(uint64_t frameIndex)
{
    m_RenderSnapshot.FrameIndex = frameIndex;
    m_RenderSnapshot.VisibleTiles = 0;
    m_RenderSnapshot.VisibleObjects = 0;
    m_RenderSnapshot.AnimatedTiles = 0;
    m_RenderSnapshot.DrawCalls = 0;
    m_RenderSnapshot.IsValid = false;
    m_RenderSnapshot.Warnings.clear();
}

} // namespace LongXi
