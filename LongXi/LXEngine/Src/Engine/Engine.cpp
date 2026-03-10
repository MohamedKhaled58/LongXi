#include "Engine/Engine.h"
#include "Renderer/DX11Renderer.h"
#include "Input/InputSystem.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Texture/TextureManager.h"
#include "Core/FileSystem/ResourceSystem.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

Engine::Engine()
    : m_Initialized(false)
{
}

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

    // Step 3: Initialize VFS
    LX_ENGINE_INFO("[Engine] Initializing VFS");
    m_VFS = std::make_unique<CVirtualFileSystem>();

    std::string exeDir = ResourceSystem::GetExecutableDirectory();
    if (!exeDir.empty())
    {
        // Mount directories first (highest priority)
        m_VFS->MountDirectory(exeDir + "/Data/Patch");
        m_VFS->MountDirectory(exeDir + "/Data");
        m_VFS->MountDirectory(exeDir);

        // Mount WDF archives after directories (lower priority)
        m_VFS->MountWdf(exeDir + "/Data/C3.wdf");
        m_VFS->MountWdf(exeDir + "/Data/data.wdf");
    }

    // Step 4: Initialize TextureManager (depends on Renderer + VFS)
    LX_ENGINE_INFO("[Engine] Initializing texture manager");
    m_TextureManager = std::make_unique<TextureManager>(*this);

    m_Initialized = true;
    LX_ENGINE_INFO("[Engine] Engine initialization complete");
    return true;
}

void Engine::Shutdown()
{
    if (!m_Initialized)
    {
        return;  // Idempotent - safe to call multiple times
    }

    LX_ENGINE_INFO("[Engine] Shutting down");

    // Shutdown in reverse dependency order
    // TextureManager FIRST - releases GPU textures while device is alive
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

    // TODO: Update Scene (future spec)
}

void Engine::Render()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] Render() called before Initialize()");
        return;
    }

    m_Renderer->BeginFrame();

    // TODO: Scene.Render() (future spec)

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

    // Forward to renderer
    m_Renderer->OnResize(width, height);
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

} // namespace LongXi
