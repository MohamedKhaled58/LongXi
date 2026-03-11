#pragma once

#include <cstdio>
#include <ios>
#include <windows.h>

namespace LongXi
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
        freopen_s(&standardOut, "CONOUT$", "w", stdout);
        freopen_s(&standardErr, "CONOUT$", "w", stderr);
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

} // namespace LongXi
