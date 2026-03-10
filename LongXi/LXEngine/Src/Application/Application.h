#pragma once

#include <Windows.h>
#include <memory>

// =============================================================================
// Application — Central lifecycle owner
// Coordinates subsystem initialization, main runtime loop, and orderly shutdown.
// Clients subclass or instantiate directly via CreateApplication().
// =============================================================================

namespace LongXi
{

class DX11Renderer;
class InputSystem;

class Application
{
  public:
    Application();
    virtual ~Application();

    // Core lifecycle methods
    bool Initialize(); // Setup Win32 window, renderer, and input
    int Run();         // Own message pump until shutdown
    void Shutdown();   // Teardown resources, exit cleanly

    // Input access for engine systems
    const InputSystem& GetInput() const;

  private:
    HWND m_WindowHandle;
    bool m_ShouldShutdown;
    bool m_Initialized;
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_InputSystem;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow();
    void DestroyMainWindow();
    bool CreateRenderer();
    bool CreateInputSystem();
    void OnResize(int width, int height);
};

} // namespace LongXi
