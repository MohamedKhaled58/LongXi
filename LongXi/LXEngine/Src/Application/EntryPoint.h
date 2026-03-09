#pragma once

// =============================================================================
// EntryPoint.h — Hazel-Style Engine-Owned Entry Point
//
// The engine defines WinMain here. The client (LXShell) implements
// CreateApplication() and includes this header exactly once.
// =============================================================================

#include "Application/Application.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/LogMacros.h"

#include <cstdio>
#include <windows.h>

// Client must implement this function
LongXi::Application* CreateApplication();

#ifdef LX_PLATFORM_WINDOWS

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Bootstrap: console + logging before anything else
    #ifdef LX_DEBUG
        AllocConsole();
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        std::ios::sync_with_stdio(true);
    #endif

    LongXi::Log::Initialize();
    LX_ENGINE_INFO("LongXi Engine starting...");

    // Client creates the application
    auto* app = CreateApplication();

    if (!app->Initialize())
    {
        LX_ENGINE_ERROR("Application initialization failed");
        delete app;
        LongXi::Log::Shutdown();
        return 1;
    }

    int exitCode = app->Run();
    app->Shutdown();

    LX_ENGINE_INFO("LongXi Engine exited with code {}", exitCode);

    delete app;
    LongXi::Log::Shutdown();

    #ifdef LX_DEBUG
        FreeConsole();
    #endif

    return exitCode;
}

#endif // LX_PLATFORM_WINDOWS
