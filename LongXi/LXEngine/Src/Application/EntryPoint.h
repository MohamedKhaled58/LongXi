#pragma once

// =============================================================================
// EntryPoint.h — Hazel-Style Engine-Owned Entry Point
//
// The engine defines WinMain here. The client (LXShell) implements
// CreateApplication() and includes this header exactly once.
// =============================================================================

#include <windows.h>

#include "Application/Application.h"
#include "Application/DebugConsoleGuard.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

// Client must implement this function.
Application* CreateApplication();

} // namespace LXEngine

#ifdef LX_PLATFORM_WINDOWS

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Bootstrap: console + logging before anything else.
    LXEngine::DebugConsoleGuard debugConsoleGuard;

    LXCore::Log::Initialize();
    LX_ENGINE_INFO("LongXi Engine starting...");

    // Client creates the application
    auto* app = LXEngine::CreateApplication();
    if (!app)
    {
        LX_ENGINE_ERROR("CreateApplication() returned nullptr");
        LXCore::Log::Shutdown();
        return 1;
    }

    if (!app->Initialize())
    {
        LX_ENGINE_ERROR("Application initialization failed");
        delete app;
        LXCore::Log::Shutdown();
        return 1;
    }

    int exitCode = app->Run();

    LX_ENGINE_INFO("LongXi Engine exited with code {}", exitCode);

    delete app;
    LXCore::Log::Shutdown();

    return exitCode;
}

#endif // LX_PLATFORM_WINDOWS
