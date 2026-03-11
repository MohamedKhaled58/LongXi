#include "Engine/Engine.h"
#include "Renderer/DX11Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Scene.h"
#include "Input/InputSystem.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Texture/TextureManager.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

Engine::Engine() : m_Initialized(false) {}

Engine::~Engine()
{
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool Engine::Initialize(HWND windowHandle, int width, int height)
{
    if (m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Already initialized - Initialize() can only succeed once");
        return false;
    }

    if (!windowHandle)
    {
        LX_ENGINE_ERROR("[Engine] Invalid window handle - cannot initialize");
        return false;
    }

    LX_ENGINE_INFO("[Engine] Initializing renderer");

    // Step 1: Initialize Renderer
    m_Renderer = std::make_unique<DX11Renderer>();
    if (!m_Renderer->Initialize(windowHandle, width, height))
    {
        LX_ENGINE_ERROR("[Engine] Renderer initialization failed - aborting");
        m_Renderer.reset();
        return false;
    }

    // Step 2: Initialize InputSystem
    LX_ENGINE_INFO("[Engine] Initializing input system");
    m_Input = std::make_unique<InputSystem>();
    m_Input->Initialize();

    // Step 3: Initialize VFS (empty — Application owns mount configuration)
    LX_ENGINE_INFO("[Engine] Initializing VFS");
    m_VFS = std::make_unique<CVirtualFileSystem>();

    // Step 4: Initialize TextureManager (depends on Renderer + VFS)
    LX_ENGINE_INFO("[Engine] Initializing texture manager");
    m_TextureManager = std::make_unique<TextureManager>(*this);

    // Step 5: Initialize SpriteRenderer (non-fatal — engine continues if it fails)
    LX_ENGINE_INFO("[Engine] Initializing sprite renderer");
    m_SpriteRenderer = std::make_unique<SpriteRenderer>();
    if (!m_SpriteRenderer->Initialize(*m_Renderer, width, height))
    {
        LX_ENGINE_WARN("[Engine] SpriteRenderer initialization failed — sprite rendering disabled");
    }

    // Step 6: Initialize Scene (non-fatal — engine continues if it fails)
    LX_ENGINE_INFO("[Engine] Initializing scene");
    m_LastFrameTime = std::chrono::steady_clock::now();
    m_FirstFrame = true;
    m_Scene = std::make_unique<Scene>();
    if (!m_Scene->Initialize(*m_Renderer))
    {
        LX_ENGINE_WARN("[Engine] Scene initialization failed");
    }

    m_Initialized = true;
    LX_ENGINE_INFO("[Engine] Engine initialization complete");
    return true;
}

void Engine::Shutdown()
{
    if (!m_Initialized)
    {
        return; // Idempotent - safe to call multiple times
    }

    LX_ENGINE_INFO("[Engine] Shutting down");

    // Shutdown in reverse dependency order
    // Scene FIRST — destroys all world objects while other subsystems still live
    if (m_Scene)
    {
        m_Scene->Shutdown();
        m_Scene.reset();
    }

    // SpriteRenderer — releases GPU sprite resources before device
    if (m_SpriteRenderer)
    {
        m_SpriteRenderer->Shutdown();
        m_SpriteRenderer.reset();
    }

    // TextureManager — releases GPU textures while device is alive
    if (m_TextureManager)
    {
        m_TextureManager.reset();
    }

    // VFS
    if (m_VFS)
    {
        m_VFS.reset();
    }

    // InputSystem
    if (m_Input)
    {
        m_Input->Shutdown();
        m_Input.reset();
    }

    // Renderer LAST - releases D3D11 device
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

// ============================================================================
// Runtime Loop
// ============================================================================

void Engine::Update()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Update() called before Initialize()");
        return;
    }

    // Advance input frame boundary
    m_Input->Update();

    // Measure deltaTime using steady_clock (0.0f on first frame to avoid large initial tick)
    auto now = std::chrono::steady_clock::now();
    float deltaTime = m_FirstFrame ? 0.0f : std::chrono::duration<float>(now - m_LastFrameTime).count();
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

    // Scene render pass — 3D world geometry before 2D screen-space sprites
    if (m_Scene && m_Scene->IsInitialized())
    {
        m_Scene->Render(*m_Renderer);
    }

    // Sprite rendering pass — runs after scene, before EndFrame (screen-space overlay)
    if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized())
    {
        m_SpriteRenderer->Begin();
        // DrawSprite calls from game/test code go here (future: virtual OnRender hook)
        m_SpriteRenderer->End();
    }

    m_Renderer->EndFrame();
}

void Engine::OnResize(int width, int height)
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] OnResize() called before Initialize()");
        return;
    }

    // Guard against zero-area (minimized window)
    if (width <= 0 || height <= 0)
    {
        return;
    }

    LX_ENGINE_INFO("[Engine] Resize forwarded to renderer: {}x{}", width, height);

    // Forward to renderer
    m_Renderer->OnResize(width, height);

    // Forward to sprite renderer (updates orthographic projection matrix)
    if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized())
    {
        m_SpriteRenderer->OnResize(width, height);
    }

    // Forward to scene
    if (m_Scene && m_Scene->IsInitialized())
    {
        m_Scene->OnResize(width, height);
    }
}

// ============================================================================
// VFS Configuration
// ============================================================================

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

// ============================================================================
// Subsystem Accessors
// ============================================================================

DX11Renderer& Engine::GetRenderer()
{
    // Assert in Debug builds would go here
    // For now, we'll rely on caller to check IsInitialized()
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

} // namespace LongXi
