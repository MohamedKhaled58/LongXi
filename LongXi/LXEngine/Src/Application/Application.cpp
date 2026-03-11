#include "Application/Application.h"
#include "Window/Win32Window.h"
#include "Engine/Engine.h"
#include "Input/InputSystem.h"
#include "Core/FileSystem/ResourceSystem.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>
#include <memory>


namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

Application::Application() : m_WindowHandle(nullptr), m_Initialized(false) {}

Application::~Application()
{
    if (m_Initialized)
    {
        Shutdown();
    }
}

// ============================================================================
// Window
// ============================================================================

bool Application::CreateMainWindow()
{
    m_Window = std::make_unique<Win32Window>(L"LongXi", 1024, 768);

    if (!m_Window->Create(Win32Window::WindowProc))
    {
        LX_ENGINE_ERROR("Failed to create main window");
        return false;
    }

    m_WindowHandle = m_Window->GetHandle();

    m_Window->Show(SW_SHOW);

    LX_ENGINE_INFO("Main window created and shown (1024x768)");
    return true;
}

void Application::DestroyMainWindow()
{
    if (m_Window)
    {
        m_Window->Destroy();
        m_Window.reset();
        m_WindowHandle = nullptr;
    }
}

Engine& Application::GetEngine()
{
    return *m_Engine;
}

bool Application::CreateEngine()
{
    m_Engine = std::make_unique<Engine>();

    if (!m_Engine->Initialize(m_WindowHandle, m_Window->GetWidth(), m_Window->GetHeight()))
    {
        LX_ENGINE_ERROR("Engine initialization failed - aborting");
        m_Engine.reset();
        return false;
    }

    WireWindowCallbacks();
    ConfigureVirtualFileSystem();

    return true;
}

// ============================================================================
// VFS Configuration
// ============================================================================

void Application::ConfigureVirtualFileSystem()
{
    std::string exeDir = ResourceSystem::GetExecutableDirectory();

    if (!exeDir.empty())
    {
        LX_ENGINE_INFO("[Application] Configuring VFS mounts");

        // Mount directories first (highest priority)
        m_Engine->MountDirectory(exeDir + "/Data/Patch");
        m_Engine->MountDirectory(exeDir + "/Data");
        m_Engine->MountDirectory(exeDir);

        // Mount WDF archives after directories (lower priority)
        m_Engine->MountWdf(exeDir + "/Data/C3.wdf");
        m_Engine->MountWdf(exeDir + "/Data/data.wdf");

        LX_ENGINE_INFO("[Application] VFS configuration complete");
    }
    else
    {
        LX_ENGINE_WARN("[Application] Failed to get executable directory for VFS configuration");
    }
}

// ============================================================================
// Callback Wiring
// ============================================================================

void Application::WireWindowCallbacks()
{
    LX_ENGINE_INFO("[Application] Wiring window event callbacks");

    m_Window->OnResize = [this](int w, int h)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            LX_ENGINE_INFO("[Application] Resize forwarded to engine: {}x{}", w, h);
            m_Engine->OnResize(w, h);
        }
    };

    m_Window->OnKeyDown = [this](UINT key, bool repeat)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnKeyDown(key, repeat);
        }
    };

    m_Window->OnKeyUp = [this](UINT key)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnKeyUp(key);
        }
    };

    m_Window->OnMouseMove = [this](int x, int y)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseMove(x, y);
        }
    };

    m_Window->OnMouseButtonDown = [this](MouseButton button)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseButtonDown(button);
        }
    };

    m_Window->OnMouseButtonUp = [this](MouseButton button)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseButtonUp(button);
        }
    };

    m_Window->OnMouseWheel = [this](int delta)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseWheel(delta);
        }
    };

    m_Window->OnFocusLost = [this]()
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnFocusLost();
        }
    };
}

// ============================================================================
// Lifecycle: Initialize
// ============================================================================

bool Application::Initialize()
{
    LX_ENGINE_INFO("Application initializing...");

    if (!CreateMainWindow())
    {
        LX_ENGINE_ERROR("Window creation failed — aborting initialization");
        return false;
    }

    if (!CreateEngine())
    {
        LX_ENGINE_ERROR("Engine creation failed — aborting initialization");
        DestroyMainWindow();
        return false;
    }

    m_Initialized = true;
    LX_ENGINE_INFO("Application initialized successfully");
    return true;
}

// ============================================================================
// Lifecycle: Run (main loop)
// ============================================================================

int Application::Run()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("Cannot Run() — Initialize() did not succeed");
        return 1;
    }

    LX_ENGINE_INFO("Entering main loop");

    MSG msg = {};
    while (true)
    {
        // Drain all pending messages without blocking
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                LX_ENGINE_INFO("Exiting main loop");
                return static_cast<int>(msg.wParam);
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Engine frame
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->Update(); // Advance input frame boundary
            m_Engine->Render(); // Render and present frame
        }
    }
}

// ============================================================================
// Lifecycle: Shutdown
// ============================================================================

void Application::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    LX_ENGINE_INFO("Application shutting down...");

    // Destroy Engine (handles all subsystem shutdown internally)
    if (m_Engine)
    {
        m_Engine.reset();
    }

    // Window already destroyed by WM_CLOSE→DestroyWindow in WindowProc.
    // Clean up the wrapper only.
    if (m_Window)
    {
        m_Window.reset();
        m_WindowHandle = nullptr;
    }

    m_Initialized = false;
    LX_ENGINE_INFO("Application shutdown complete");
}

} // namespace LongXi
