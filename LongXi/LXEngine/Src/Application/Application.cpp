#include "Application/Application.h"
#include "Window/Win32Window.h"
#include "Engine/Engine.h"
#include "Input/InputSystem.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>
#include <windowsx.h>
#include <memory>


namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

Application::Application() : m_WindowHandle(nullptr), m_ShouldShutdown(false), m_Initialized(false) {}

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

    if (!m_Window->Create(Application::WindowProc))
    {
        LX_ENGINE_ERROR("Failed to create main window");
        return false;
    }

    m_WindowHandle = m_Window->GetHandle();

    // Store Application pointer in window user data for WindowProc access
    SetWindowLongPtr(m_WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

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

    return true;
}

void Application::OnResize(int width, int height)
{
    // Guard against zero-area (minimized window)
    if (width <= 0 || height <= 0)
    {
        return;
    }

    // Forward to Engine
    if (m_Engine && m_Engine->IsInitialized())
    {
        m_Engine->OnResize(width, height);
    }
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
// WindowProc
// ============================================================================

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CLOSE:
        LX_ENGINE_INFO("WM_CLOSE received — destroying window");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        LX_ENGINE_INFO("WM_DESTROY received — posting quit message");
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (app)
        {
            int width = static_cast<int>(LOWORD(lParam));
            int height = static_cast<int>(HIWORD(lParam));
            if (app->m_Window)
            {
                app->m_Window->SetSize(width, height);
            }
            app->OnResize(width, height);
        }
        return 0;

        // ---- Keyboard ----

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            UINT vk = static_cast<UINT>(wParam);
            bool isRepeat = (lParam & 0x40000000) != 0;
            app->m_Engine->GetInput().OnKeyDown(vk, isRepeat);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnKeyUp(static_cast<UINT>(wParam));
        }
        return 0;

    case WM_KILLFOCUS:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnFocusLost();
        }
        return 0;

    case WM_ACTIVATEAPP:
        if (app && app->m_Engine && app->m_Engine->IsInitialized() && wParam == FALSE)
        {
            app->m_Engine->GetInput().OnFocusLost();
        }
        return 0;

        // ---- Mouse movement ----

    case WM_MOUSEMOVE:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

        // ---- Mouse buttons ----

    case WM_LBUTTONDOWN:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            SetCapture(hwnd);
            app->m_Engine->GetInput().OnMouseButtonDown(MouseButton::Left);
        }
        return 0;

    case WM_LBUTTONUP:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnMouseButtonUp(MouseButton::Left);
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            SetCapture(hwnd);
            app->m_Engine->GetInput().OnMouseButtonDown(MouseButton::Right);
        }
        return 0;

    case WM_RBUTTONUP:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnMouseButtonUp(MouseButton::Right);
            ReleaseCapture();
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            SetCapture(hwnd);
            app->m_Engine->GetInput().OnMouseButtonDown(MouseButton::Middle);
        }
        return 0;

    case WM_MBUTTONUP:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnMouseButtonUp(MouseButton::Middle);
            ReleaseCapture();
        }
        return 0;

        // ---- Mouse wheel ----

    case WM_MOUSEWHEEL:
        if (app && app->m_Engine && app->m_Engine->IsInitialized())
        {
            app->m_Engine->GetInput().OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
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
            m_Engine->Update();   // Advance input frame boundary
            m_Engine->Render();   // Render and present frame
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
