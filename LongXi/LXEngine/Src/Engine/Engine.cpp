#include "Engine/Engine.h"

#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Input/InputSystem.h"
#include "Renderer/DX11Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Scene.h"
#include "Texture/TextureManager.h"

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

    LX_ENGINE_INFO("[Engine] Initializing renderer");
    m_Renderer = std::make_unique<DX11Renderer>();
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
    m_TextureManager = std::make_unique<TextureManager>(*this);

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

    m_LastFrameTime = std::chrono::steady_clock::now();
    m_FirstFrame = true;
    m_Initialized = true;
    LX_ENGINE_INFO("[Engine] Engine initialization complete");
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

    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = m_FirstFrame ? 0.0f : std::chrono::duration<float>(now - m_LastFrameTime).count();
    m_LastFrameTime = now;
    m_FirstFrame = false;

    if (m_Scene && m_Scene->IsInitialized())
    {
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

    m_Renderer->BeginFrame();

    if (m_Scene && m_Scene->IsInitialized() && m_Renderer->BeginPass(RenderPassType::Scene))
    {
        m_Scene->Render(*m_Renderer);
        m_Renderer->EndPass();
    }

    if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized() && m_Renderer->BeginPass(RenderPassType::Sprite))
    {
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

    m_Renderer->ExecuteExternalPass(callback);
}

void Engine::Present()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Present() called before Initialize()");
        return;
    }

    m_Renderer->EndFrame();
    m_Renderer->Present();

    if (m_Input)
    {
        m_Input->Update();
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

} // namespace LongXi
