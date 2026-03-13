#pragma once

#include <cstdio>
#include <ios>
#include <windows.h>

namespace LXEngine
{

class DebugConsoleGuard
{
public:
    DebugConsoleGuard()
    {
#if defined(LX_DEBUG)
        AllocConsole();
        FILE* standardOut = nullptr;
        FILE* standardErr = nullptr;
        FILE* standardIn  = nullptr;
        freopen_s(&standardOut, "CONOUT$", "w", stdout);
        freopen_s(&standardErr, "CONOUT$", "w", stderr);
        freopen_s(&standardIn, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio(true);
#endif
    }

    ~DebugConsoleGuard()
    {
#if defined(LX_DEBUG)
        FreeConsole();
#endif
    }
};

} // namespace LXEngine
