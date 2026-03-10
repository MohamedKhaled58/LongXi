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
class ResourceSystem;

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
    const ResourceSystem& GetResourceSystem() const;

  private:
    HWND m_WindowHandle;
    bool m_ShouldShutdown;
    bool m_Initialized;
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_InputSystem;
    std::unique_ptr<ResourceSystem> m_ResourceSystem;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow();
    void DestroyMainWindow();
    bool CreateRenderer();
    bool CreateInputSystem();
    bool CreateResourceSystem();
    void OnResize(int width, int height);
};

} // namespace LongXi
