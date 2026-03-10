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

class CVirtualFileSystem;
class DX11Renderer;
class InputSystem;
class Win32Window;

class Application
{
  public:
    Application();
    virtual ~Application();

    // Core lifecycle methods
    bool Initialize(); // Setup Win32 window, renderer, input, and resources
    int Run();         // Own message pump until shutdown
    void Shutdown();   // Teardown resources, exit cleanly

    // Subsystem accessors for engine systems
    const InputSystem& GetInput() const;
    const CVirtualFileSystem& GetVirtualFileSystem() const;

  private:
    HWND m_WindowHandle;
    bool m_ShouldShutdown;
    bool m_Initialized;
    std::unique_ptr<Win32Window>        m_Window;
    std::unique_ptr<DX11Renderer>       m_Renderer;
    std::unique_ptr<InputSystem>        m_InputSystem;
    std::unique_ptr<CVirtualFileSystem> m_VirtualFileSystem;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow();
    void DestroyMainWindow();
    bool CreateRenderer();
    bool CreateInputSystem();
    bool CreateVirtualFileSystem();
    void OnResize(int width, int height);
};

} // namespace LongXi
