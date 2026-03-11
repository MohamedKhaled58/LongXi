#pragma once

#include <Windows.h>
#include <memory>

// =============================================================================
// Application — Central lifecycle owner
// Coordinates window creation and owns Engine instance.
// Clients subclass or instantiate directly via CreateApplication().
// =============================================================================

namespace LongXi
{

class Engine;
class Win32Window;

class Application
{
  public:
    Application();
    virtual ~Application();

    // Core lifecycle methods
    virtual bool Initialize(); // Setup Win32 window and engine
    int Run();                 // Own message pump until shutdown
    void Shutdown();           // Teardown resources, exit cleanly

  protected:
    // Protected accessor for subclasses to access Engine
    Engine& GetEngine();

  private:
    HWND m_WindowHandle;
    bool m_Initialized;
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Engine> m_Engine;

    bool CreateMainWindow();
    void DestroyMainWindow();
    bool CreateEngine();
    void ConfigureVirtualFileSystem();
    void WireWindowCallbacks();
};

} // namespace LongXi
