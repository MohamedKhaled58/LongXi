# Research Findings: Win32 Shell Bootstrap

**Feature**: Spec 001 — Win32 Shell Bootstrap
**Date**: 2026-03-10
**Phase**: Phase 0 Research

## Overview

This document consolidates research findings for implementing the Win32 Shell Bootstrap specification. All technical decisions are documented with rationale and alternatives considered.

## Win32 Application Foundation Patterns

### Window Class Registration and Creation

**Decision**: Use standard Win32 window class registration with `RegisterClassEx` and window creation with `CreateWindowEx`.

**Rationale**:
- Idiomatic Win32 API usage, well-documented in MSDN
- Direct control over window properties and behavior
- No abstraction layer overhead
- Constitutinally required (raw Win32 API, no frameworks)

**Implementation Approach**:
```cpp
WNDCLASSEX wc = {0};
wc.cbSize = sizeof(WNDCLASSEX);
wc.style = CS_HREDRAW | CS_VREDRAW;
wc.lpfnWndProc = Application::WindowProc;
wc.hInstance = GetModuleHandle(NULL);
wc.hCursor = LoadCursor(NULL, IDC_ARROW);
wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
wc.lpszClassName = L"LongXiWindowClass";

RegisterClassEx(&wc);

HWND hwnd = CreateWindowEx(
    0,                              // dwExStyle
    L"LongXiWindowClass",          // lpClassName
    L"Long Xi",                      // lpWindowName
    WS_OVERLAPPEDWINDOW,            // style
    CW_USEDEFAULT, CW_USEDEFAULT,    // x, y
    1024, 768,                       // width, height
    NULL, NULL,                      // hWndParent, hMenu
    GetModuleHandle(NULL),           // hInstance
    NULL
);
```

**Alternatives Considered**:
- **ATL window classes**: More complex, adds dependency on ATL, overkill for simple window
- **Custom framework abstraction**: Violates constitutional requirement for raw Win32 API
- **SDL/GLFW**: Violates constitutional requirement, adds unnecessary abstraction layer

### Message Pump Implementation

**Decision**: Use standard `GetMessage/DispatchMessage` loop with `PostQuitMessage` for shutdown signaling.

**Rationale**:
- Standard Win32 message loop pattern
- Clean shutdown mechanism via WM_QUIT message
- Well-documented, idiomatic approach
- Supports constitutional requirement for message pump stability

**Implementation Approach**:
```cpp
MSG msg = {};
while (GetMessage(&msg, NULL, 0, 0) > 0)
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

**Shutdown Signaling**:
```cpp
// In WindowProc (WM_DESTROY handler):
PostQuitMessage(0);
```

**Alternatives Considered**:
- **PeekMessage loop**: More complex, not needed for simple message processing
- **Custom event system**: Overkill for bootstrap phase, adds complexity
- **Boolean flag with sleep**: Non-idiomatic, wastes CPU cycles

## Spdlog Integration for Native Applications

### Integration Patterns

**Decision**: Integrate Spdlog with custom module-specific macros using stdout_color_sink for console output.

**Rationale**:
- Provides structured logging from day one
- Validates vendor dependency early
- Custom macros provide module context (LXCore, LXEngine, LXShell)
- Colored console output improves debuggability
- Aligns with clarified requirements

**Implementation Approach**:
```cpp
// In LXCore setup (Application::Initialize):
spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] %v");
spdlog::set_level(spdlog::level::debug);
auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
spdlog::default_logger()->set_sink(console_sink);
```

**Custom Macros**:
```cpp
// Core logging macros (per spec clarification Session 2026-03-10):
#define LX_CORE_INFO(...) SPDLOG_LOGGER_INFO(&LXCoreLogger(), __VA_ARGS__)
#define LX_ENGINE_INFO(...) SPDLOG_LOGGER_INFO(&LXEngineLogger(), __VA_ARGS__)
#define LX_INFO(...) SPDLOG_LOGGER_INFO(&LXShellLogger(), __VA_ARGS__)
```

**Alternatives Considered**:
- **Direct printf/cout**: No structured logging, no timestamping, harder to debug
- **Custom logger implementation**: Reinventing the wheel, Spdlog already available
- **File logging only**: Not suitable for bootstrap phase where console visibility is key

## Application Class Design Patterns

### RAII Resource Management

**Decision**: Use RAII patterns for resource management with clear ownership in Application class.

**Rationale**:
- Automatic resource cleanup on destruction
- Clear ownership semantics
- Prevents resource leaks
- Supports constitutional requirement for controlled shutdown

**Implementation Approach**:
- Constructor: Lightweight initialization, no resource acquisition
- Initialize(): All resource acquisition (console, logging, window)
- Run(): Message pump ownership until shutdown
- Shutdown(): Resource release in reverse order of Initialize()
- Destructor: Final safety check (should be empty if Initialize failed)

**Error Handling Strategy**:
- Initialize() returns bool (true=success, false=failure)
- Run() returns int (exit code)
- Shutdown() is void (best-effort cleanup, cannot fail during Phase 1)

**Alternatives Considered**:
- **Constructor does everything**: Violates single-responsibility, error handling complicated
- **Exception-based**: Constitutional C++23 allows but not idiomatic for systems programming
- **Two-phase construction**: More complex, not needed for simple bootstrap

### Clean Lifecycle Separation

**Decision**: Three-method interface (Initialize/Run/Shutdown) with clear phase boundaries.

**Rationale**:
- Matches clarified requirements (Session 2026-03-10)
- Supports constitutional requirement for centralized lifecycle
- Clear separation of concerns
- Easy to test each phase independently
- Supports future extensibility without breaking existing interface

**Phase Contracts**:

**Initialize() Phase**:
- Input: None (member variables constructed)
- Output: bool (success/failure)
- Responsibilities: Console allocation, logging init, window creation
- State after: Window valid, logging functional, ready for Run()

**Run() Phase**:
- Input: None (assumes Initialize() succeeded)
- Output: int (exit code)
- Responsibilities: Message pump ownership, message dispatch
- State after: Message pump exited, resources still valid

**Shutdown() Phase**:
- Input: None (assumes Run() completed)
- Output: void (no return value, cleanup is best-effort)
- Responsibilities: Window destruction, console deallocation, log flush
- State after: All resources released

## Build System Integration

### Premake5 Configuration

**Decision**: Extend existing Premake5 configuration for Win32 linking.

**Rationale**:
- Constitutional requirement (Premake5 already configured)
- Minimal changes to existing build system
- Consistent with LXShell → LXEngine → LXCore dependency structure

**Implementation Requirements**:
```lua
-- LXShell configuration
links { "LXEngine", "LXCore" }
filter { "system:Windows" }
links { "user32", "gdi32" }  -- Win32 libraries

-- Include paths (already configured)
includedirs { "../LXEngine/Src", "../LXCore/Src" }
```

**Spdlog Integration**:
```lua
-- Workspace level (already configured)
includedirs { "Vendor/Spdlog/include" }

-- Static library linking
filter { "kind:StaticLib" }
links { "spdlog" }  -- Link Spdlog static library
```

## Summary

All technical decisions align with:
1. **Constitutional requirements**: Raw Win32 API, static libraries, single-threaded Phase 1
2. **Clarified requirements**: Spdlog integration, three-method Application interface, specific window properties
3. **CLAUDE.md policies**: m_PascalCase naming, centralized dependency management, public entry headers

**Research Complete**: All NEEDS CLARIFICATION items resolved. Ready for Phase 1 design and implementation.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
