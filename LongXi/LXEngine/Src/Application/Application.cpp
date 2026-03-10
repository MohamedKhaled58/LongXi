#include "Application/Application.h"
#include "Window/Win32Window.h"
#include "Renderer/DX11Renderer.h"
#include "Input/InputSystem.h"
#include "Core/FileSystem/ResourceSystem.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>
#include <windowsx.h>
#include <memory>

namespace LongXi
{

// Global Win32Window pointer for WindowProc access
static std::unique_ptr<Win32Window> g_Window;

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
    g_Window = std::make_unique<Win32Window>(L"LongXi", 1024, 768);

    if (!g_Window->Create(Application::WindowProc))
    {
        LX_ENGINE_ERROR("Failed to create main window");
        return false;
    }

    m_WindowHandle = g_Window->GetHandle();

    // Store Application pointer in window user data for WindowProc access
    SetWindowLongPtr(m_WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    g_Window->Show(SW_SHOW);

    LX_ENGINE_INFO("Main window created and shown (1024x768)");
    return true;
}

void Application::DestroyMainWindow()
{
    if (g_Window)
    {
        g_Window->Destroy();
        g_Window.reset();
        m_WindowHandle = nullptr;
    }
}

bool Application::CreateRenderer()
{
    m_Renderer = std::make_unique<DX11Renderer>();

    if (!m_Renderer->Initialize(m_WindowHandle, g_Window->GetWidth(), g_Window->GetHeight()))
    {
        LX_ENGINE_ERROR("Failed to initialize DX11 renderer");
        m_Renderer.reset();
        return false;
    }

    return true;
}

bool Application::CreateInputSystem()
{
    m_InputSystem = std::make_unique<InputSystem>();
    m_InputSystem->Initialize();
    return true;
}

const InputSystem& Application::GetInput() const
{
    return *m_InputSystem;
}

bool Application::CreateResourceSystem()
{
    std::string exeDir = ResourceSystem::GetExecutableDirectory();

    std::vector<std::string> roots;
    if (!exeDir.empty())
    {
        roots.push_back(exeDir + "/Data");
        roots.push_back(exeDir);
    }

    m_ResourceSystem = std::make_unique<ResourceSystem>();
    m_ResourceSystem->Initialize(roots);
    return true;
}

const ResourceSystem& Application::GetResourceSystem() const
{
    return *m_ResourceSystem;
}

void Application::OnResize(int width, int height)
{
    // Guard against zero-area (minimized window)
    if (width <= 0 || height <= 0)
    {
        return;
    }

    // Forward to renderer
    if (m_Renderer && m_Renderer->IsInitialized())
    {
        m_Renderer->OnResize(width, height);
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

    if (!CreateRenderer())
    {
        LX_ENGINE_ERROR("Renderer creation failed — aborting initialization");
        DestroyMainWindow();
        return false;
    }

    if (!CreateInputSystem())
    {
        LX_ENGINE_ERROR("Input system creation failed — aborting initialization");
        m_Renderer->Shutdown();
        m_Renderer.reset();
        DestroyMainWindow();
        return false;
    }

    if (!CreateResourceSystem())
    {
        LX_ENGINE_ERROR("Resource system creation failed — aborting initialization");
        m_InputSystem->Shutdown();
        m_InputSystem.reset();
        m_Renderer->Shutdown();
        m_Renderer.reset();
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
            if (g_Window)
            {
                g_Window->SetSize(width, height);
            }
            app->OnResize(width, height);
        }
        return 0;

        // ---- Keyboard ----

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (app && app->m_InputSystem)
        {
            UINT vk = static_cast<UINT>(wParam);
            bool isRepeat = (lParam & 0x40000000) != 0;
            app->m_InputSystem->OnKeyDown(vk, isRepeat);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnKeyUp(static_cast<UINT>(wParam));
        }
        return 0;

    case WM_KILLFOCUS:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnFocusLost();
        }
        return 0;

    case WM_ACTIVATEAPP:
        if (app && app->m_InputSystem && wParam == FALSE)
        {
            app->m_InputSystem->OnFocusLost();
        }
        return 0;

        // ---- Mouse movement ----

    case WM_MOUSEMOVE:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

        // ---- Mouse buttons ----

    case WM_LBUTTONDOWN:
        if (app && app->m_InputSystem)
        {
            SetCapture(hwnd);
            app->m_InputSystem->OnMouseButtonDown(MouseButton::Left);
        }
        return 0;

    case WM_LBUTTONUP:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnMouseButtonUp(MouseButton::Left);
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (app && app->m_InputSystem)
        {
            SetCapture(hwnd);
            app->m_InputSystem->OnMouseButtonDown(MouseButton::Right);
        }
        return 0;

    case WM_RBUTTONUP:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnMouseButtonUp(MouseButton::Right);
            ReleaseCapture();
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (app && app->m_InputSystem)
        {
            SetCapture(hwnd);
            app->m_InputSystem->OnMouseButtonDown(MouseButton::Middle);
        }
        return 0;

    case WM_MBUTTONUP:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnMouseButtonUp(MouseButton::Middle);
            ReleaseCapture();
        }
        return 0;

        // ---- Mouse wheel ----

    case WM_MOUSEWHEEL:
        if (app && app->m_InputSystem)
        {
            app->m_InputSystem->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
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

        // Render frame continuously
        if (m_Renderer && m_Renderer->IsInitialized())
        {
            m_Renderer->BeginFrame();
            m_Renderer->EndFrame();
        }

        // Advance input frame boundary at END of frame so Pressed/Released
        // transitions are valid for the full frame before being cleared.
        if (m_InputSystem)
        {
            m_InputSystem->Update();
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

    // Shutdown resource system first
    if (m_ResourceSystem)
    {
        m_ResourceSystem->Shutdown();
        m_ResourceSystem.reset();
    }

    // Shutdown input before renderer
    if (m_InputSystem)
    {
        m_InputSystem->Shutdown();
        m_InputSystem.reset();
    }

    // Shutdown renderer before destroying window
    if (m_Renderer)
    {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }

    // Window already destroyed by WM_CLOSE→DestroyWindow in WindowProc.
    // Clean up the wrapper only.
    if (g_Window)
    {
        g_Window.reset();
        m_WindowHandle = nullptr;
    }

    m_Initialized = false;
    LX_ENGINE_INFO("Application shutdown complete");
}

} // namespace LongXi
