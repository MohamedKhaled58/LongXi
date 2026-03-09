#pragma once

#include <Windows.h>

// =============================================================================
// Application — Central lifecycle owner
// Coordinates subsystem initialization, main runtime loop, and orderly shutdown.
// Clients subclass or instantiate directly via CreateApplication().
// =============================================================================

namespace LongXi
{

class Application
{
public:
    Application();
    virtual ~Application();

    // Core lifecycle methods
    bool Initialize();  // Setup Win32 window
    int Run();          // Own message pump until shutdown
    void Shutdown();    // Teardown resources, exit cleanly

private:
    HWND m_WindowHandle;
    bool m_ShouldShutdown;
    bool m_Initialized;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow();
    void DestroyMainWindow();
};

} // namespace LongXi
