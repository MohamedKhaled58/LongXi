#include "Engine/Engine.h"

#include <chrono>
#include <cmath>

#include "Assets/ResourceManager.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Profiling/IProfileSink.h"
#include "Core/Profiling/ProfileScope.h"
#include "Input/InputSystem.h"
#include "Map/MapSystem.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Scene.h"
#include "Texture/TextureManager.h"

namespace LXEngine
{

Engine::Engine() = default;

Engine::~Engine()
{
    Shutdown();
}

bool Engine::Initialize(HWND windowHandle, int width, int height)
{
    if (m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Already initialized");
        return false;
    }

    if (!windowHandle)
    {
        LX_ENGINE_ERROR("[Engine] Invalid window handle");
        return false;
    }

    const std::chrono::steady_clock::time_point startupStart = std::chrono::steady_clock::now();

    LX_ENGINE_INFO("[Engine] Initializing renderer");
    m_Renderer = CreateRenderer();
    if (!m_Renderer)
    {
        LX_ENGINE_ERROR("[Engine] Renderer backend factory returned null");
        return false;
    }

    if (!m_Renderer->Initialize(windowHandle, width, height))
    {
        LX_ENGINE_ERROR("[Engine] Renderer initialization failed");
        m_Renderer.reset();
        return false;
    }

    LX_ENGINE_INFO("[Engine] Initializing input system");
    m_Input = std::make_unique<InputSystem>();
    m_Input->Initialize();

    LX_ENGINE_INFO("[Engine] Initializing VFS");
    m_VFS = std::make_unique<CVirtualFileSystem>();

    LX_ENGINE_INFO("[Engine] Initializing texture manager");
    m_TextureManager = std::make_unique<TextureManager>(*m_Renderer, *m_VFS);

    // ResourceManager initialized later by Application after VFS is mounted
    // m_ResourceManager = std::make_unique<ResourceManager>();

    LX_ENGINE_INFO("[Engine] Initializing sprite renderer");
    m_SpriteRenderer = std::make_unique<SpriteRenderer>();
    if (!m_SpriteRenderer->Initialize(*m_Renderer, width, height))
    {
        LX_ENGINE_WARN("[Engine] SpriteRenderer initialization failed — sprite rendering disabled");
    }

    LX_ENGINE_INFO("[Engine] Initializing scene");
    m_Scene = std::make_unique<Scene>();
    if (!m_Scene->Initialize(*m_Renderer))
    {
        LX_ENGINE_WARN("[Engine] Scene initialization failed");
    }
    else
    {
        const LXCore::Vector3 cameraPosition = m_Scene->GetActiveCamera().GetPosition();
        m_LastSceneCameraX                   = cameraPosition.x;
        m_LastSceneCameraY                   = cameraPosition.y;
        m_LastSceneCameraZ                   = cameraPosition.z;
        m_HasLastSceneCameraState            = true;
    }

    LX_ENGINE_INFO("[Engine] Initializing map system");
    m_MapSystem = std::make_unique<MapSystem>();
    if (!m_MapSystem->Initialize(*m_Renderer, *m_VFS, *m_TextureManager))
    {
        LX_ENGINE_WARN("[Engine] MapSystem initialization failed — map rendering disabled");
        m_MapSystem.reset();
    }

    m_TimingService.Initialize();
    m_TimingService.SetFrameLimiterEnabled(true);
    m_TimingService.SetFrameRateLimit(60.0);
    m_TimingService.SetMaxDeltaTime(0.100);
    m_ProfilerCollector.Initialize();
    LXCore::IProfileSink::SetActive(IsProfilingEnabled() ? &m_ProfilerCollector : nullptr);

    m_Initialized = true;

    LX_ENGINE_INFO("[Timing] Frame limiter {} at {:.1f} FPS (max delta {:.1f} ms)",
                   m_TimingService.IsFrameLimiterEnabled() ? "enabled" : "disabled",
                   m_TimingService.GetFrameRateLimit(),
                   m_TimingService.GetMaxDeltaTime() * 1000.0);

    const double startupMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startupStart).count();
    LX_ENGINE_INFO("[Engine] Engine initialization complete ({:.2f} ms)", startupMs);

    return true;
}

void Engine::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    LX_ENGINE_INFO("[Engine] Shutting down");

    if (m_Scene)
    {
        m_Scene->Shutdown();
        m_Scene.reset();
    }

    if (m_MapSystem)
    {
        m_MapSystem->Shutdown();
        m_MapSystem.reset();
    }

    if (m_ResourceManager)
    {
        m_ResourceManager.reset();
    }

    if (m_SpriteRenderer)
    {
        m_SpriteRenderer->Shutdown();
        m_SpriteRenderer.reset();
    }

    if (m_TextureManager)
    {
        m_TextureManager.reset();
    }

    if (m_VFS)
    {
        m_VFS.reset();
    }

    if (m_Input)
    {
        m_Input->Shutdown();
        m_Input.reset();
    }

    if (m_Renderer)
    {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }

    LXCore::IProfileSink::SetActive(nullptr);
    m_ProfilerCollector.Shutdown();
    m_TimingService.Shutdown();

    m_Initialized             = false;
    m_HasLastSceneCameraState = false;
    m_LastSceneCameraX        = 0.0f;
    m_LastSceneCameraY        = 0.0f;
    m_LastSceneCameraZ        = 0.0f;
}

bool Engine::IsInitialized() const
{
    return m_Initialized;
}

void Engine::Update()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Update() called before Initialize()");
        return;
    }

    if (!m_TimingService.IsFrameActive())
    {
        m_TimingService.BeginFrame();
        if (IsProfilingEnabled())
        {
            m_ProfilerCollector.BeginFrame(m_TimingService.GetSnapshot().FrameIndex);
        }

        if (m_MapSystem && m_MapSystem->IsInitialized())
        {
            m_MapSystem->BeginFrame(m_TimingService.GetSnapshot());
        }
    }

    LX_PROFILE_SCOPE("Engine.Update");

    const float deltaTime = static_cast<float>(m_TimingService.GetSnapshot().DeltaTimeSeconds);

    if (m_Scene && m_Scene->IsInitialized())
    {
        LX_PROFILE_SCOPE("Engine.SceneUpdate");
        m_Scene->Update(deltaTime);
    }

    if (m_MapSystem && m_MapSystem->IsInitialized() && m_MapSystem->IsMapReady())
    {
        float mapPanX = 0.0f;
        float mapPanY = 0.0f;

        if (m_Scene && m_Scene->IsInitialized())
        {
            const LXCore::Vector3 sceneCameraPosition = m_Scene->GetActiveCamera().GetPosition();
            if (!m_HasLastSceneCameraState)
            {
                m_LastSceneCameraX        = sceneCameraPosition.x;
                m_LastSceneCameraY        = sceneCameraPosition.y;
                m_LastSceneCameraZ        = sceneCameraPosition.z;
                m_HasLastSceneCameraState = true;
            }

            mapPanX += sceneCameraPosition.x - m_LastSceneCameraX;
            mapPanY += sceneCameraPosition.y - m_LastSceneCameraY;

            const float deltaSceneZ = sceneCameraPosition.z - m_LastSceneCameraZ;
            if (std::fabs(deltaSceneZ) > 0.0001f)
            {
                m_MapSystem->AdjustCameraZoom(-deltaSceneZ * 0.02f);
            }

            m_LastSceneCameraX = sceneCameraPosition.x;
            m_LastSceneCameraY = sceneCameraPosition.y;
            m_LastSceneCameraZ = sceneCameraPosition.z;
        }

        if (m_Input)
        {
            float panSpeed = 800.0f * deltaTime;
            if (m_Input->IsKeyDown(Key::LShift) || m_Input->IsKeyDown(Key::RShift))
            {
                panSpeed *= 2.0f;
            }

            if (m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::Left))
            {
                mapPanX -= panSpeed;
            }
            if (m_Input->IsKeyDown(Key::D) || m_Input->IsKeyDown(Key::Right))
            {
                mapPanX += panSpeed;
            }
            if (m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::Up))
            {
                mapPanY -= panSpeed;
            }
            if (m_Input->IsKeyDown(Key::S) || m_Input->IsKeyDown(Key::Down))
            {
                mapPanY += panSpeed;
            }

            if (m_Input->IsKeyDown(Key::Q))
            {
                m_MapSystem->AdjustCameraZoom(1.0f * deltaTime);
            }
            if (m_Input->IsKeyDown(Key::E))
            {
                m_MapSystem->AdjustCameraZoom(-1.0f * deltaTime);
            }

            const int wheelDelta = m_Input->GetWheelDelta();
            if (wheelDelta != 0)
            {
                m_MapSystem->AdjustCameraZoom(static_cast<float>(wheelDelta) / 120.0f * 0.08f);
            }
        }

        if (std::fabs(mapPanX) > 0.0001f || std::fabs(mapPanY) > 0.0001f)
        {
            m_MapSystem->PanCamera(mapPanX, mapPanY);
        }
    }
}

void Engine::Render()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Render() called before Initialize()");
        return;
    }

    LX_PROFILE_SCOPE("Engine.Render");

    m_Renderer->BeginFrame();
    if (m_Renderer->GetLifecyclePhase() != FrameLifecyclePhase::InFrame || m_Renderer->GetActivePass() != RenderPassType::None)
    {
        // Renderer can intentionally skip frame work while in recovery mode.
        return;
    }

    if (m_Scene && m_Scene->IsInitialized() && m_Renderer->BeginPass(RenderPassType::Scene))
    {
        LX_PROFILE_SCOPE("Engine.ScenePass");

        m_Scene->Render(*m_Renderer);
        m_Renderer->EndPass();
    }

    if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized() && m_Renderer->BeginPass(RenderPassType::Sprite))
    {
        LX_PROFILE_SCOPE("Engine.SpritePass");
        m_SpriteRenderer->Begin();

        if (m_MapSystem && m_MapSystem->IsInitialized())
        {
            LX_PROFILE_SCOPE("Engine.MapPass");
            m_MapSystem->Render(*m_SpriteRenderer, m_TimingService.GetSnapshot());
        }

        m_SpriteRenderer->End();
        m_Renderer->EndPass();
    }
}

void Engine::ExecuteExternalRenderPass(const ExternalPassCallback& callback)
{
    if (!m_Initialized || !callback)
    {
        return;
    }

    if (m_Renderer->GetLifecyclePhase() != FrameLifecyclePhase::InFrame || m_Renderer->GetActivePass() != RenderPassType::None)
    {
        return;
    }

    LX_PROFILE_SCOPE("Engine.ExternalPass");
    m_Renderer->ExecuteExternalPass(callback);
}

void Engine::ExecuteSpritePass(const std::function<void(SpriteRenderer&)>& callback)
{
    if (!m_Initialized || !callback || !m_Renderer || !m_SpriteRenderer || !m_SpriteRenderer->IsInitialized())
    {
        return;
    }

    LX_PROFILE_SCOPE("Engine.SpritePass.External");

    if (m_Renderer->GetLifecyclePhase() != FrameLifecyclePhase::InFrame || m_Renderer->GetActivePass() != RenderPassType::None)
    {
        return;
    }

    if (!m_Renderer->BeginPass(RenderPassType::Sprite))
    {
        return;
    }

    m_SpriteRenderer->Begin();
    callback(*m_SpriteRenderer);
    m_SpriteRenderer->End();
    m_Renderer->EndPass();
}

void Engine::Present()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Present() called before Initialize()");
        return;
    }
    const bool canPresentFrame =
        m_Renderer->GetLifecyclePhase() == FrameLifecyclePhase::InFrame && m_Renderer->GetActivePass() == RenderPassType::None;
    {
        LX_PROFILE_SCOPE("Engine.PresentCommands");

        if (canPresentFrame)
        {
            m_Renderer->EndFrame();
            m_Renderer->Present();
        }

        if (m_Input)
        {
            m_Input->Update();
        }
    }

    if (m_TimingService.IsFrameActive())
    {
        m_TimingService.EndFrame();

        const LXCore::TimingSnapshot& timingSnapshot = m_TimingService.GetSnapshot();
        if (IsProfilingEnabled())
        {
            m_ProfilerCollector.EndFrame(timingSnapshot.FrameIndex, timingSnapshot.FrameTimeSeconds);
        }

        // Optional periodic diagnostics without per-frame log spam.
        if (timingSnapshot.FrameIndex > 0 && (timingSnapshot.FrameIndex % 600) == 0)
        {
            LX_ENGINE_INFO("[Timing] frame={} delta={:.3f} ms frame={:.3f} ms total={:.2f} s",
                           timingSnapshot.FrameIndex,
                           timingSnapshot.DeltaTimeSeconds * 1000.0,
                           timingSnapshot.FrameTimeSeconds * 1000.0,
                           timingSnapshot.TotalTimeSeconds);
        }
    }
}

void Engine::OnResize(int width, int height)
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] OnResize() called before Initialize()");
        return;
    }

    LX_ENGINE_INFO("[Engine] Resize forwarded to renderer: {}x{}", width, height);
    m_Renderer->OnResize(width, height);

    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized())
    {
        m_SpriteRenderer->OnResize(width, height);
    }

    if (m_Scene && m_Scene->IsInitialized())
    {
        m_Scene->OnResize(width, height);
    }

    if (m_MapSystem && m_MapSystem->IsInitialized())
    {
        m_MapSystem->OnResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
}

void Engine::MountDirectory(const std::string& path)
{
    if (m_VFS)
    {
        m_VFS->MountDirectory(path);
    }
}

void Engine::MountWdf(const std::string& path)
{
    if (m_VFS)
    {
        m_VFS->MountWdf(path);
    }
}

bool Engine::InitializeResourceManager()
{
    if (m_ResourceManager)
    {
        return true; // Already initialized
    }

    m_ResourceManager = std::make_unique<ResourceManager>();
    if (!m_ResourceManager->Initialize(*m_VFS, *m_Renderer, *m_TextureManager))
    {
        LX_ENGINE_WARN("[Engine] ResourceManager initialization failed — asset loading may not work");
        return false;
    }

    return true;
}

Renderer& Engine::GetRenderer()
{
    return *m_Renderer;
}

InputSystem& Engine::GetInput()
{
    return *m_Input;
}

CVirtualFileSystem& Engine::GetVFS()
{
    return *m_VFS;
}

TextureManager& Engine::GetTextureManager()
{
    return *m_TextureManager;
}

SpriteRenderer& Engine::GetSpriteRenderer()
{
    return *m_SpriteRenderer;
}

Scene& Engine::GetScene()
{
    return *m_Scene;
}

ResourceManager& Engine::GetResourceManager()
{
    static ResourceManager s_FallbackResourceManager;
    return m_ResourceManager ? *m_ResourceManager : s_FallbackResourceManager;
}

MapSystem& Engine::GetMapSystem()
{
    static MapSystem s_FallbackMapSystem;
    if (!m_MapSystem)
    {
        LX_ENGINE_WARN("[Engine] GetMapSystem requested while map system is unavailable");
        return s_FallbackMapSystem;
    }

    return *m_MapSystem;
}

bool Engine::LoadMap(const std::string& mapPath)
{
    if (!m_MapSystem || !m_MapSystem->IsInitialized())
    {
        LX_ENGINE_WARN("[Engine] LoadMap requested but MapSystem is unavailable");
        return false;
    }

    return m_MapSystem->LoadMap(mapPath);
}

bool Engine::IsMapReady() const
{
    return m_MapSystem && m_MapSystem->IsMapReady();
}

const LXMap::MapRenderSnapshot& Engine::GetMapRenderSnapshot() const
{
    static const LXMap::MapRenderSnapshot kEmptySnapshot = {};
    if (!m_MapSystem)
    {
        return kEmptySnapshot;
    }

    return m_MapSystem->GetRenderSnapshot();
}

void* Engine::GetRendererDeviceHandle() const
{
    return m_Renderer ? m_Renderer->GetNativeDeviceHandle() : nullptr;
}

void* Engine::GetRendererContextHandle() const
{
    return m_Renderer ? m_Renderer->GetNativeContextHandle() : nullptr;
}

int Engine::GetRendererViewportWidth() const
{
    return m_Renderer ? m_Renderer->GetViewportWidth() : 0;
}

int Engine::GetRendererViewportHeight() const
{
    return m_Renderer ? m_Renderer->GetViewportHeight() : 0;
}

void Engine::SetFrameLimiterEnabled(bool enabled)
{
    m_TimingService.SetFrameLimiterEnabled(enabled);
}

bool Engine::IsFrameLimiterEnabled() const
{
    return m_TimingService.IsFrameLimiterEnabled();
}

void Engine::SetFrameRateLimit(double targetFramesPerSecond)
{
    m_TimingService.SetFrameRateLimit(targetFramesPerSecond);
}

double Engine::GetFrameRateLimit() const
{
    return m_TimingService.GetFrameRateLimit();
}

void Engine::SetMaxDeltaTime(double maxDeltaSeconds)
{
    m_TimingService.SetMaxDeltaTime(maxDeltaSeconds);
}

double Engine::GetMaxDeltaTime() const
{
    return m_TimingService.GetMaxDeltaTime();
}

const LXCore::TimingSnapshot& Engine::GetTimingSnapshot() const
{
    return m_TimingService.GetSnapshot();
}

const LXCore::FrameProfileSnapshot& Engine::GetLastFrameProfileSnapshot() const
{
    return m_ProfilerCollector.GetLastFrameSnapshot();
}

bool Engine::IsProfilingEnabled()
{
#if defined(LX_DEBUG) || defined(LX_DEV)
    return true;
#else
    return false;
#endif
}

} // namespace LXEngine
