# Data Model: Win32 Shell Bootstrap

**Feature**: Spec 001 — Win32 Shell Bootstrap
**Date**: 2026-03-10
**Phase**: Phase 1 Design

## Overview

This document defines the core data entities and their relationships for the Win32 Shell Bootstrap implementation. The design focuses on the Application class as the central lifecycle owner and the Win32Window class as the window management abstraction.

## Core Entities

### Application Class

**Purpose**: Central lifecycle owner that coordinates subsystem initialization, main runtime loop, and orderly shutdown.

**Location**: `LXEngine/Src/Application/Application.h` (in LXEngine module per dependency structure)

**Public Interface**:
```cpp
class Application
{
public:
    Application();
    ~Application();

    // Core lifecycle methods (per spec clarification Session 2026-03-10)
    bool Initialize();  // Setup console, logging, Win32 window
    int Run();          // Own message pump until shutdown
    void Shutdown();    // Teardown resources, exit cleanly

private:
    // Private members follow m_PascalCase naming convention (CLAUDE.md Section 16)
    HWND m_WindowHandle;           // Win32 window handle
    bool m_ShouldShutdown;         // Shutdown signal flag
    bool m_Initialized;           // Track initialization state

    // Window procedure (static member required by Win32)
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Internal helper methods
    bool CreateConsoleWindow();
    bool InitializeLogging();
    bool CreateMainWindow();
    void DestroyMainWindow();
};
```

**Member Variables**:

| Name | Type | Purpose | Naming Convention |
|------|------|---------|-------------------|
| m_WindowHandle | HWND | Win32 window handle | m_PascalCase (CLAUDE.md) |
| m_ShouldShutdown | bool | Flag indicating shutdown request | m_PascalCase |
| m_Initialized | bool | Track whether Initialize() succeeded | m_PascalCase |

**Lifecycle State Machine**:

```
[Constructed] → [Initialize() called] → [m_Initialized = true]
                                               ↓
                            [Window created, logging active]
                                               ↓
                            [Run() called] → [Message pump running]
                                               ↓
                            [Shutdown requested] → [Shutdown() called]
                                               ↓
                            [Resources released] → [Destructed]
```

**Error Handling**:
- **Initialize()**: Returns false on failure, logs error, must not call Run() or Shutdown()
- **Run()**: Returns exit code (0 for success, non-zero for errors)
- **Shutdown()**: Void return, best-effort cleanup (no failure mode in Phase 1)

### Win32Window Class

**Purpose**: RAII wrapper around Win32 window handle providing clean window creation and destruction.

**Location**: `LXEngine/Src/Window/Win32Window.h` (in LXEngine module)

**Public Interface**:
```cpp
class Win32Window
{
public:
    Win32Window(const std::wstring& title, int width, int height);
    ~Win32Window();

    // Disable copy (windows are not copyable)
    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool Create();
    void Show(int nCmdShow = SW_SHOW);
    void Destroy();

    HWND GetHandle() const { return m_WindowHandle; }
    bool IsValid() const { return m_WindowHandle != NULL; }

private:
    std::wstring m_Title;      // Window title
    int m_Width;               // Window width in pixels
    int m_Height;              // Window height in pixels
    HWND m_WindowHandle;       // Win32 window handle
    WNDCLASSEX m_WindowClass;   // Window class registration
};
```

**Member Variables**:

| Name | Type | Purpose | Naming Convention |
|------|------|---------|-------------------|
| m_Title | std::wstring | Window title ("Long Xi") | m_PascalCase |
| m_Width | int | Window width (1024) | m_PascalCase |
| m_Height | int | Window height (768) | m_PascalCase |
| m_WindowHandle | HWND | Win32 window handle | m_PascalCase |
| m_WindowClass | WNDCLASSEX | Window class registration | m_PascalCase |

**Window Properties** (per spec clarification Session 2026-03-10):
- **Title**: "Long Xi"
- **Style**: WS_OVERLAPPEDWINDOW (includes resize, system menu, etc.)
- **Size**: 1024x768 pixels
- **Show Mode**: Normal windowed (not fullscreen, not maximized, not minimized)
- **Resize Support**: Yes (as part of WS_OVERLAPPEDWINDOW)

**Lifecycle**:
```
[Constructed] → [Create() called] → [Window registered and created]
                                        ↓
                            [Show() called] → [Window visible]
                                        ↓
                            [Destroy() called] → [Window destroyed]
```

## Entity Relationships

### Dependency Graph

```
LXShell (Executable)
    ↓ (depends on)
LXEngine (Static Library)
    ↓ (contains)
Application Class
    ↓ (uses)
Win32Window Class
    ↓ (integrates)
Spdlog (Vendor Dependency)
```

### Ownership Model

**LXShell owns**:
- Application object (unique ownership via std::unique_ptr or direct instantiation)

**Application owns**:
- Win32Window object (unique ownership)
- Logging system setup (Spdlog integration)

**Win32Window owns**:
- Window handle (HWND) - wraps OS resource
- Window class registration (WNDCLASSEX)

### Cross-Module Dependencies

**LXShell → LXEngine**:
- Includes `LXEngine.h` (public entry point)
- Instantiates `Application` class
- Uses LXShell logging macros (LX_INFO) (fixes I2 - corrected macro reference)

**LXEngine → LXCore**:
- Includes `LXCore.h` (public entry point)
- Uses LXCore logging macros (LX_CORE_INFO)
- May use LXCore utilities (if needed)

**All Modules → Spdlog**:
- All modules include Spdlog headers via `Vendor/Spdlog/include`
- All modules use custom logging macros
- Spdlog linked as static library

## Validation Rules

### Application Class

**Preconditions**:
- `Initialize()`: Application object constructed, no prior initialization
- `Run()`: Initialize() returned true (m_Initialized == true)
- `Shutdown()`: Run() has completed (message pump exited)

**Postconditions**:
- `Initialize()`: Window created and visible, logging active, m_Initialized == true
- `Run()`: Message pump exited cleanly, window still valid (not yet destroyed)
- `Shutdown()`: All resources released, window destroyed, process ready to exit

**Invariants**:
- m_WindowHandle is valid only when m_Initialized is true
- Shutdown() can only be called once after Run() completes
- Initialize() can only be called once

### Win32Window Class

**Preconditions**:
- `Create()`: Object constructed, window not yet created
- `Show()`: Window created and valid (m_WindowHandle != NULL)
- `Destroy()`: Window currently valid (m_WindowHandle != NULL)

**Postconditions**:
- `Create()`: m_WindowHandle valid, window class registered
- `Show()`: Window visible on screen
- `Destroy()`: m_WindowHandle invalidated (set to NULL), window destroyed

**Invariants**:
- m_WindowHandle is NULL before Create() succeeds
- m_WindowHandle is non-NULL between Create() success and Destroy() call
- Window class registration valid for process lifetime

## Type Specifications

### Standard Library Types

**std::wstring**: Used for window titles and text strings (Windows Unicode support)
**bool**: Used for success/failure returns and state flags
**int**: Used for window dimensions and exit codes
**HWND**: Win32 window handle type (typedef void*)

### Win32 Types

**WNDCLASSEX**: Extended window class registration structure
**MSG**: Win32 message structure
**WPARAM/LPARAM**: Message parameter types (platform-dependent)
**LRESULT**: Window procedure return type

## Data Flow

### Startup Flow

```
[WinMain executes]
    ↓
[Application object created]
    ↓
[Application::Initialize() called]
    ↓
[AllocConsole()] → [Initialize Spdlog] → [Win32Window::Create()]
    ↓
[Window shown, logging active]
    ↓
[Return true to WinMain]
    ↓
[WinMain calls Application::Run()]
```

### Runtime Flow

```
[Application::Run() entered]
    ↓
[Message pump loop: GetMessage/DispatchMessage]
    ↓
[Process messages: WM_PAINT, WM_SIZE, etc.]
    ↓
[User closes window → WM_CLOSE → WM_DESTROY → PostQuitMessage]
    ↓
[GetMessage returns false]
    ↓
[Return exit code to WinMain]
    ↓
[WinMain calls Application::Shutdown()]
```

### Shutdown Flow

```
[User closes window → WM_CLOSE]
    ↓
[WindowProc handles WM_CLOSE → DestroyWindow()]
    ↓
[WM_DESTROY received → PostQuitMessage(0)]
    ↓
[GetMessage returns false → Run() exits]
    ↓
[WinMain calls Application::Shutdown()]
    ↓
[Flush Spdlog logs]
    ↓
[FreeConsole()] → [if allocated during Initialize()]
    ↓
[Return to WinMain]
    ↓
[WinMain exits, process terminates]
```
(fixes I3 - clarified that WM_CLOSE handler destroys window, Shutdown() only cleans up console/logging)

## Constraints and Validation

### Constitutional Compliance

**Article III (Non-Negotiable Technical Baseline)**:
- ✅ Windows-only platform
- ✅ Raw Win32 API
- ✅ Static library architecture (LXShell → LXEngine → LXCore)
- ✅ C++23 standard

**Article IV (Architectural Laws)**:
- ✅ Thin entrypoint (WinMain <20 lines)
- ✅ Centralized lifecycle (Application class)
- ✅ Module boundaries respected
- ✅ No god objects (Application has focused 3-method interface)

**Article IX (Threading and Runtime Discipline)**:
- ✅ Single-threaded execution (Phase 1)
- ✅ Message pump stability (no re-entry)
- ✅ MT-aware architecture (no design choices preventing future multithreading)

### Specification Compliance

**Clarified Requirements (Session 2026-03-10)**:
- ✅ Application interface: Initialize/Run/Shutdown
- ✅ Window properties: 1024x768, WS_OVERLAPPEDWINDOW, "Long Xi"
- ✅ Console approach: AllocConsole
- ✅ Logging: Spdlog with custom macros
- ✅ Shutdown signaling: WM_CLOSE → WM_DESTROY → PostQuitMessage

**Functional Requirements Coverage**:
- FR-001 to FR-021: All functional requirements mapped to class design
- SC-001 to SC-010: All success criteria supported by data model
- AC-001 to AC-010: All acceptance criteria can be validated against this design

## Summary

The data model provides:
- Clear entity definitions with member variables following m_PascalCase convention
- Explicit lifecycle state machines for both Application and Win32Window
- Well-defined ownership model respecting constitutional dependency direction
- Complete type specifications for all entities
- Validation rules ensuring constitutional and specification compliance

This design is ready for implementation with no ambiguity in entity relationships or lifecycle management.
