#include "Engine/Engine.h"

#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Input/InputSystem.h"
#include "Profiling/ProfileScope.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Scene.h"
#include "Texture/TextureManager.h"

#include <chrono>

namespace LongXi
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

    m_TimingService.Initialize();
    m_ProfilerCollector.Initialize();
    ProfilerCollector::SetActiveCollector(IsProfilingEnabled() ? &m_ProfilerCollector : nullptr);

    m_Initialized = true;

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

    ProfilerCollector::SetActiveCollector(nullptr);
    m_ProfilerCollector.Shutdown();
    m_TimingService.Shutdown();

    m_Initialized = false;
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
    }

    LX_PROFILE_SCOPE("Engine.Update");

    const float deltaTime = static_cast<float>(m_TimingService.GetSnapshot().DeltaTimeSeconds);

    if (m_Scene && m_Scene->IsInitialized())
    {
        LX_PROFILE_SCOPE("Engine.SceneUpdate");
        m_Scene->Update(deltaTime);
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
    {
        LX_PROFILE_SCOPE("Engine.PresentCommands");

        m_Renderer->EndFrame();
        m_Renderer->Present();

        if (m_Input)
        {
            m_Input->Update();
        }
    }

    if (m_TimingService.IsFrameActive())
    {
        m_TimingService.EndFrame();

        const TimingSnapshot& timingSnapshot = m_TimingService.GetSnapshot();
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

const TimingSnapshot& Engine::GetTimingSnapshot() const
{
    return m_TimingService.GetSnapshot();
}

const FrameProfileSnapshot& Engine::GetLastFrameProfileSnapshot() const
{
    return m_ProfilerCollector.GetLastFrameSnapshot();
}

bool Engine::IsProfilingEnabled() const
{
#if defined(LX_DEBUG) || defined(LX_DEV)
    return true;
#else
    return false;
#endif
}

} // namespace LongXi
