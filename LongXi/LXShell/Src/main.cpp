// =============================================================================
// LXShell — Client Application
// Implements CreateApplication() as required by the engine entry point.
// The actual WinMain lives in LXEngine/Src/Application/EntryPoint.h
// =============================================================================

#include <LXEngine.h>
#include <Application/EntryPoint.h>

LongXi::Application* CreateApplication()
{
    return new LongXi::Application();
}
