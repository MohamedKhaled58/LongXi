#include "Application/Application.h"
#include "Window/Win32Window.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>
#include <memory>

namespace LongXi
{

// Global Win32Window pointer for WindowProc access
static std::unique_ptr<Win32Window> g_Window;

// ============================================================================
// Constructor / Destructor
// ============================================================================

Application::Application()
    : m_WindowHandle(nullptr)
    , m_ShouldShutdown(false)
    , m_Initialized(false)
{
}

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

    m_Initialized = true;
    LX_ENGINE_INFO("Application initialized successfully");
    return true;
}

// ============================================================================
// WindowProc
// ============================================================================

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Lifecycle: Run (message pump)
// ============================================================================

int Application::Run()
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("Cannot Run() — Initialize() did not succeed");
        return 1;
    }

    LX_ENGINE_INFO("Entering message pump");

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    LX_ENGINE_INFO("Exiting message pump");
    return static_cast<int>(msg.wParam);
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
