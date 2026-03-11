# Research: Engine Event Routing and Application Boundary Cleanup

**Feature**: 009-engine-event-routing-refactor
**Date**: 2025-03-10
**Phase**: 0 - Outline & Research

## Overview

This document consolidates research findings for refactoring the engine event routing architecture. The refactor moves VFS configuration from Engine to Application and Win32 message handling from Application to Win32Window, introducing a callback-based event model.

## Research Areas

### 1. std::function Callback Best Practices

**Question**: What is the best practice for exposing std::function callbacks in C++ for event routing?

**Decision**: Use public std::function member variables with clear naming conventions

**Rationale**:
- std::function provides type-safe callback storage without requiring inheritance
- Member variables allow direct assignment from Application wiring code
- Clear naming (OnResize, OnKeyDown, etc.) makes intent explicit
- No performance overhead for single-threaded Phase 1 runtime
- Compatible with future MT model (callbacks can be synchronized later)

**Alternatives Considered**:
- Virtual interfaces (IWindowEventHandler): Overkill for this use case, adds unnecessary inheritance hierarchy
- Win32 message forwarding via direct function calls: Too tightly coupled, prevents platform abstraction
- Observer pattern: More complex than needed for single listener (Application)

**Code Example**:
```cpp
// Win32Window.h
class Win32Window
{
public:
    std::function<void(int, int)> OnResize;
    std::function<void(UINT, bool)> OnKeyDown;
    std::function<void(UINT)> OnKeyUp;
    std::function<void(int, int)> OnMouseMove;
    std::function<void(MouseButton)> OnMouseButtonDown;
    std::function<void(MouseButton)> OnMouseButtonUp;
    std::function<void(int)> OnMouseWheel;
};
```

### 2. HWND Parameter in Engine::Initialize()

**Question**: Should Engine::Initialize() accept HWND or should it be platform-agnostic?

**Decision**: Keep HWND in Engine::Initialize() for now, but isolate from public interface

**Rationale**:
- Engine needs window handle for DX11 renderer initialization (CreateSwapChain)
- HWND is Win32-specific but required by DirectX 11 on Windows
- Isolating HWND to Initialize() parameter keeps public interface otherwise clean
- Future platform ports would have different native handle types (Window for X11, etc.)
- Acceptable trade-off for Phase 1 Windows-only platform

**Alternatives Considered**:
- Use void* opaque handle: Loses type safety, no real benefit
- Create renderer externally: Violates subsystem ownership principle (Engine should own Renderer)
- Abstract window handle interface: Overkill for single-platform Phase 1

**Implementation**:
```cpp
// Engine.h - Public interface
class Engine
{
public:
    bool Initialize(void* nativeWindowHandle, int width, int height);  // Platform-agnostic type
    // OR
    bool Initialize(HWND hwnd, int width, int height);  // Explicit Win32, documented as Windows-only
};
```

**Chosen Approach**: Use HWND explicitly in Phase 1, document as Windows-only constraint. This matches constitutional requirements (Article III).

### 3. VFS Configuration Location

**Question**: Where should Application::ConfigureVirtualFileSystem() be implemented and called?

**Decision**: Implement as private method in Application, call after Engine::Initialize() succeeds

**Rationale**:
- Private method keeps VFS configuration encapsulated in Application
- Called after Engine initialization ensures Engine is ready to receive VFS operations
- Separation of concerns: Engine provides GetVFS() accessor, Application decides what to mount
- Testable: Different applications can override ConfigureVirtualFileSystem() or not call it

**Alternatives Considered**:
- Pass VFS config into Engine::Initialize(): Couples Engine to application-specific paths
- Separate VFSConfig class: Premature abstraction for single use case
- Global VFS configuration: Violates module boundaries and testability

**Code Example**:
```cpp
// Application.cpp
bool Application::CreateEngine()
{
    m_Engine = std::make_unique<Engine>();

    if (!m_Engine->Initialize(m_WindowHandle, width, height))
        return false;

    ConfigureVirtualFileSystem();  // Application owns mount decisions
    return true;
}

void Application::ConfigureVirtualFileSystem()
{
    auto& vfs = m_Engine->GetVFS();
    std::string exeDir = ResourceSystem::GetExecutableDirectory();

    if (!exeDir.empty())
    {
        vfs.MountDirectory(exeDir + "/Data/Patch");
        vfs.MountDirectory(exeDir + "/Data");
        vfs.MountDirectory(exeDir);
        vfs.MountWdf(exeDir + "/Data/C3.wdf");
        vfs.MountWdf(exeDir + "/Data/data.wdf");
    }
}
```

### 4. Win32Window::WindowProc Implementation

**Question**: Should Win32Window own the WindowProc static function, and how does it access callbacks?

**Decision**: Yes, Win32Window owns WindowProc and retrieves Application instance via GWLP_USERDATA

**Rationale**:
- Current pattern already uses GWLP_USERDATA to store Application pointer
- Win32Window is the natural owner of Win32 message processing
- Static WindowProc retrieves Application* from user data, then accesses callbacks
- Maintains existing Win32 message pump structure in Application::Run()

**Alternatives Considered**:
- Keep WindowProc in Application: Violates architectural goal (centralize Win32 handling)
- Use per-window subclassing: Overkill for single-window application
- Message-only window: Unnecessary complexity for current use case

**Implementation**:
```cpp
// Win32Window.h
class Win32Window
{
public:
    // Callback members
    std::function<void(int, int)> OnResize;
    std::function<void(UINT, bool)> OnKeyDown;
    // ... other callbacks

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Win32Window.cpp
LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (window && msg == WM_SIZE)
    {
        if (window->OnResize)
            window->OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }

    // ... other message handling

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
```

### 5. Callback Lifetime and Null Safety

**Question**: What happens when callbacks are null or Engine is not yet initialized?

**Decision**: Callbacks check for null before invocation; Engine methods check initialization state

**Rationale**:
- Callbacks are optional (may not be wired during early initialization)
- Null checks prevent crashes during shutdown or edge cases
- Engine::IsInitialized() provides guard for subsystem access
- Matches existing error handling patterns in codebase

**Implementation**:
```cpp
// Win32Window::WindowProc
if (window && window->OnResize)
    window->OnResize(width, height);

// Application callback wiring
m_Window->OnResize = [this](int w, int h)
{
    if (m_Engine && m_Engine->IsInitialized())
        m_Engine->OnResize(w, h);
};
```

### 6. Event Routing Performance

**Question**: Can callback-based routing meet performance targets (1ms resize, 100μs input)?

**Decision**: Yes, std::function indirection is negligible for single-threaded Phase 1 runtime

**Rationale**:
- std::function call overhead is ~5-10 nanoseconds on modern compilers
- Target budgets: 1ms = 1,000,000ns (resize), 100μs = 100,000ns (input)
- Callback overhead is < 0.01% of budget
- Actual cost is in subsystem operations (DX11 resize, InputSystem state update), not routing
- Measured performance will validate during implementation

**Performance Estimate**:
```
Win32Message → WindowProc → std::function::operator() → lambda → Engine::OnResize → Renderer::OnResize
Overhead breakdown:
- WindowProc dispatch: ~100ns
- std::function invoke: ~10ns
- Lambda capture access: ~5ns
- Total routing: ~115ns (< 0.1% of 1ms budget)
```

### 7. MouseButton Enum Location

**Question**: Should MouseButton enum be defined in InputSystem.h or a shared location?

**Decision**: Keep MouseButton enum in InputSystem.h where it currently resides

**Rationale**:
- MouseButton is part of InputSystem's public API
- InputSystem.h is already included by both Application.cpp and Win32Window.cpp (via Engine.h)
- No need for additional shared header
- Follows existing project organization (namespace-scope enums with owning system)

**Implementation**:
```cpp
// InputSystem.h (existing)
namespace LongXi
{
enum class MouseButton : uint8_t
{
    Left = 0,
    Right = 1,
    Middle = 2,
    Count = 3
};

class InputSystem { ... };
}
```

### 8. Logging Integration

**Question**: How should event routing logging integrate with existing logging infrastructure?

**Decision**: Use existing LX_ENGINE_INFO/LX_ENGINE_ERROR macros from LogMacros.h

**Rationale**:
- Consistent with existing codebase logging patterns
- Component prefixes ([Window], [Application], [Engine]) already established
- No new logging infrastructure needed
- Supports FR-017 requirement for comprehensive event logging

**Implementation**:
```cpp
// Win32Window.cpp
LX_WINDOW_INFO("[Window] WM_SIZE received: {}x{}", width, height);
if (OnResize) OnResize(width, height);

// Application.cpp (callback wiring)
m_Window->OnResize = [this](int w, int h)
{
    LX_APPLICATION_INFO("[Application] Resize forwarded to engine: {}x{}", w, h);
    m_Engine->OnResize(w, h);
};

// Engine.cpp
void Engine::OnResize(int width, int height)
{
    LX_ENGINE_INFO("[Engine] Resize forwarded to renderer: {}x{}", width, height);
    m_Renderer->OnResize(width, height);
}
```

## Summary of Decisions

| Area | Decision | Key Rationale |
|------|----------|---------------|
| Callback mechanism | std::function member variables | Type-safe, simple, no inheritance overhead |
| HWND parameter | Keep in Initialize(), Windows-only | DX11 requires it, isolated from public interface |
| VFS configuration | Application::ConfigureVirtualFileSystem() | Application owns mount decisions, Engine provides accessor |
| WindowProc ownership | Move to Win32Window | Centralize Win32 handling, retrieve Application via GWLP_USERDATA |
| Null safety | Null checks on callbacks and Engine::IsInitialized() | Prevent crashes during initialization/shutdown edge cases |
| Performance | std::function overhead negligible | < 0.1% of budget, actual cost in subsystem operations |
| MouseButton enum | Keep in InputSystem.h | Part of InputSystem API, already included where needed |
| Logging | Use existing LX_ENGINE_* macros | Consistent with codebase, supports FR-017 |

## Next Steps

Phase 1 will generate:
- **data-model.md**: Document callback interfaces, ownership relationships, and state transitions
- **contracts/**: Define Win32Window callback signatures and Engine event handler interfaces
- **quickstart.md**: Provide step-by-step implementation guide

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
