# Data Model: Engine Event Routing and Application Boundary Cleanup

**Feature**: 009-event-routing-refactor
**Date**: 2025-03-10
**Phase**: 1 - Design & Contracts

## Overview

This document describes the key entities, their relationships, and state transitions involved in the event routing refactor. This is an architectural refactor, so the "data model" focuses on component ownership and interface boundaries.

## Key Entities

### 1. Win32Window

**Purpose**: Platform-specific window wrapper that owns Win32 message processing and exposes platform-independent callbacks

**Location**: `LongXi/LXEngine/Src/Window/Win32Window.h`

**Owned State**:
- `HWND m_WindowHandle` - Native Win32 window handle
- `int m_Width`, `int m_Height` - Current window dimensions
- `bool m_Shown` - Window visibility state

**Callback Members** (public):
```cpp
std::function<void(int, int)> OnResize;
std::function<void(UINT, bool)> OnKeyDown;
std::function<void(UINT)> OnKeyUp;
std::function<void(int, int)> OnMouseMove;
std::function<void(MouseButton)> OnMouseButtonDown;
std::function<void(MouseButton)> OnMouseButtonUp;
std::function<void(int)> OnMouseWheel;
```

**Responsibilities**:
- Create and destroy Win32 window
- Own static WindowProc function for message handling
- Translate Win32 messages to callback invocations
- Store callbacks for Application to wire
- Validate callback state before invocation (null check)

**Lifecycle**:
```
Created → Create() → Message Loop (via WindowProc) → Destroy() → Destroyed
```

**Invariants**:
- Callbacks may be null (not yet wired or already disconnected)
- WindowHandle is valid between Create() and Destroy()
- WindowProc retrieves Win32Window* from GWLP_USERDATA

---

### 2. Application

**Purpose**: Coordinator that owns Engine and Win32Window instances, wires callbacks, and configures VFS mounts

**Location**: `LongXi/LXEngine/Src/Application/Application.h`

**Owned State**:
- `std::unique_ptr<Win32Window> m_Window` - Platform window wrapper
- `std::unique_ptr<Engine> m_Engine` - Engine runtime coordinator
- `HWND m_WindowHandle` - Cached window handle for Engine initialization
- `bool m_ShouldShutdown` - Runtime loop control flag
- `bool m_Initialized` - Application initialization state

**Methods**:
- `bool CreateMainWindow()` - Initialize Win32Window
- `bool CreateEngine()` - Initialize Engine, then call ConfigureVirtualFileSystem()
- `void ConfigureVirtualFileSystem()` - Configure VFS mounts (new method)
- `void DestroyMainWindow()` - Cleanup Win32Window
- `int Run()` - Main message pump
- `LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM)` - REMOVED (moved to Win32Window)

**Callback Wiring** (in CreateEngine or Initialize):
```cpp
// Wire Win32Window callbacks to Engine subsystems
m_Window->OnResize = [this](int w, int h)
{
    if (m_Engine && m_Engine->IsInitialized())
        m_Engine->OnResize(w, h);
};

m_Window->OnKeyDown = [this](UINT key, bool repeat)
{
    if (m_Engine && m_Engine->IsInitialized())
        m_Engine->GetInput().OnKeyDown(key, repeat);
};

// ... similar for other callbacks
```

**Responsibilities**:
- Own lifecycle of Engine and Win32Window
- Wire event callbacks between Win32Window and Engine
- Configure VFS mounts with application-specific paths
- Run main message loop (maintains ownership of PumpWin32Messages)
- Coordinate shutdown sequence

**Lifecycle**:
```
Constructor → Initialize() → Run() → [Message Loop] → Shutdown() → Destructor
```

**State Transitions**:
- `m_Initialized = false` → `CreateEngine()` → `m_Initialized = true`
- `m_ShouldShutdown = false` → [WM_CLOSE/WM_DESTROY] → `m_ShouldShutdown = true`

---

### 3. Engine

**Purpose**: Platform-agnostic runtime coordinator that owns subsystems (Renderer, InputSystem, VFS, TextureManager)

**Location**: `LongXi/LXEngine/Src/Engine/Engine.h`

**Owned State**:
- `std::unique_ptr<DX11Renderer> m_Renderer`
- `std::unique_ptr<InputSystem> m_Input`
- `std::unique_ptr<CVirtualFileSystem> m_VFS`
- `std::unique_ptr<TextureManager> m_TextureManager`
- `bool m_Initialized`

**Public Interface**:
```cpp
class Engine
{
public:
    // Lifecycle
    bool Initialize(HWND windowHandle, int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    // Runtime
    void Update();
    void Render();
    void OnResize(int width, int height);  // NEW: Event entry point

    // Subsystem accessors
    DX11Renderer& GetRenderer();
    InputSystem& GetInput();
    CVirtualFileSystem& GetVFS();
    TextureManager& GetTextureManager();
};
```

**Removed from Engine**:
- VFS mount configuration (moved to Application::ConfigureVirtualFileSystem())
- Hardcoded asset paths ("Data/", "C3.wdf")

**Responsibilities**:
- Own and initialize all subsystems in dependency order
- Forward events to appropriate subsystems
- Provide accessor methods for Application to configure VFS
- Remain platform-independent (no Win32 types in public interface except HWND in Initialize)

**Lifecycle**:
```
Constructor → Initialize() → [Update/Render loop] → Shutdown() → Destructor
```

**Initialization Order**:
```
Initialize():
  1. Create Renderer (requires HWND)
  2. Create InputSystem
  3. Create VFS (empty, no mounts)
  4. Create TextureManager
  5. m_Initialized = true
```

**Shutdown Order** (reverse):
```
Shutdown():
  1. TextureManager.reset()
  2. VFS.reset()
  3. InputSystem->Shutdown(); InputSystem.reset()
  4. Renderer->Shutdown(); Renderer.reset()
  5. m_Initialized = false
```

---

### 4. CVirtualFileSystem

**Purpose**: File system abstraction that can mount directories and WDF archives

**Location**: `LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.h`

**Relationships**:
- **Owned by**: Engine (std::unique_ptr<CVirtualFileSystem>)
- **Configured by**: Application via Engine::GetVFS()
- **Mounts**: Directories and WDF archives

**Public Interface** (relevant methods):
```cpp
class CVirtualFileSystem
{
public:
    bool MountDirectory(const std::string& path);
    bool MountWdf(const std::string& wdfPath);
    std::vector<uint8_t> ReadAll(const std::string& relativePath);
    // ...
};
```

**Usage Pattern**:
```cpp
// In Application::ConfigureVirtualFileSystem()
auto& vfs = m_Engine->GetVFS();
vfs.MountDirectory(exeDir + "/Data/Patch");
vfs.MountDirectory(exeDir + "/Data");
vfs.MountWdf(exeDir + "/Data/C3.wdf");
```

---

### 5. InputSystem

**Purpose**: Input state manager that receives normalized input events

**Location**: `LongXi/LXEngine/Src/Input/InputSystem.h`

**Relationships**:
- **Owned by**: Engine
- **Receives events from**: Application (via callbacks)
- **Stores**: Current and previous input state

**Public Interface** (relevant methods):
```cpp
class InputSystem
{
public:
    void Initialize();
    void Shutdown();
    void Update();

    // Event handlers (called from Application callbacks)
    void OnKeyDown(UINT vk, bool isRepeat);
    void OnKeyUp(UINT vk);
    void OnMouseMove(int x, int y);
    void OnMouseButtonDown(MouseButton button);
    void OnMouseButtonUp(MouseButton button);
    void OnMouseWheel(int delta);
    void OnFocusLost();

    // State queries
    bool IsKeyDown(Key key) const;
    bool IsMouseButtonDown(MouseButton button) const;
    int GetMouseX() const;
    int GetMouseY() const;
};
```

**Event Flow**:
```
Win32Window::WindowProc (WM_KEYDOWN)
  → Win32Window.OnKeyDown callback
  → Application lambda (checks m_Engine->IsInitialized())
  → Engine::GetInput().OnKeyDown()
  → InputSystem state update
```

---

## Callback Type Definitions

### Resize Callback

**Type**: `std::function<void(int width, int height)>`

**Parameters**:
- `width`: New client area width in pixels
- `height`: New client area height in pixels

**Invoke Condition**: WM_SIZE received with valid dimensions (> 0)

**Routing Path**: `Win32Window → Application → Engine::OnResize → DX11Renderer::OnResize`

---

### Keyboard Callbacks

**OnKeyDown**: `std::function<void(UINT virtualKey, bool isRepeat)>`

**Parameters**:
- `virtualKey`: Win32 virtual key code (VK_A, VK_RETURN, etc.)
- `isRepeat`: True if auto-repeat key event, false for initial press

**OnKeyUp**: `std::function<void(UINT virtualKey)>`

**Parameters**:
- `virtualKey`: Win32 virtual key code

**Routing Path**: `Win32Window → Application → Engine::GetInput().OnKeyDown/OnKeyUp`

---

### Mouse Callbacks

**OnMouseMove**: `std::function<void(int x, int y)>`

**Parameters**:
- `x`: Client area X coordinate in pixels
- `y`: Client area Y coordinate in pixels

**OnMouseButtonDown**: `std::function<void(MouseButton button)>`

**OnMouseButtonUp**: `std::function<void(MouseButton button)>`

**Parameters**:
- `button`: MouseButton enum value (Left, Right, Middle)

**OnMouseWheel**: `std::function<void(int delta)>`

**Parameters**:
- `delta`: Wheel delta (±120 per notch)

**Routing Path**: `Win32Window → Application → Engine::GetInput().OnMouseMove/OnMouseButtonDown/etc`

---

## State Machine: Callback Safety

### Callback Null States

```
Callback State    | Can Invoke? | Behavior
------------------|-------------|----------
Null (not wired)  | No          | Message received but no action taken
Wired (non-null)  | Yes         | Callback invoked normally
```

### Engine Initialization States

```
Engine State       | Callback Behavior? | Reason
------------------|-------------------|----------
Not Initialized    | Guard with check   | Subsystems not ready
Initialized        | Forward normally   | Safe to access subsystems
Shutdown           | Guard with check   | Subsystems destroyed
```

### Validation Rules

1. **Win32Window::WindowProc**: Always check callback null before invocation
2. **Application callbacks**: Always check `m_Engine && m_Engine->IsInitialized()` before forwarding
3. **Engine::OnResize**: Always check `IsInitialized()` before forwarding to Renderer
4. **Engine::GetInput()**: Returns reference; caller must validate Engine state

---

## Ownership Summary

| Entity | Owned By | Lifecycle Owner | Configured By |
|--------|----------|-----------------|----------------|
| Win32Window | Application | Application | Application (callback wiring) |
| Engine | Application | Application | Application (VFS mounts) |
| VFS | Engine | Engine | Application (mount paths) |
| InputSystem | Engine | Engine | None (state-driven) |
| Renderer | Engine | Engine | None (event-driven) |

---

## Dependency Graph

```
Application (owns, coordinates)
  ├──> Win32Window (platform messages, callbacks)
  │     └──> WindowProc (static, owned by Win32Window)
  │           └──> Callbacks (wired by Application)
  └──> Engine (subsystems, event routing)
        ├──> Renderer (resize events)
        ├──> InputSystem (input events)
        ├──> VFS (configured by Application)
        └──> TextureManager (uses VFS)
```

**Arrows indicate**: "owns" or "sends events to"

---

## Edge Case State Handling

### 1. Callback During Engine Initialization

**Scenario**: Win32 message arrives before Engine::Initialize() completes

**Handling**:
- Application callbacks check `m_Engine && m_Engine->IsInitialized()`
- If false, event is silently dropped (acceptable during initialization window)

### 2. Null Callback After Engine Shutdown

**Scenario**: Callback invoked during Application destruction

**Handling**:
- Application destructor sets `m_Initialized = false` before destroying members
- Callbacks check initialization state, no-op if Engine shutting down
- Win32Window destroyed after Engine, so no callbacks possible

### 3. VFS Access Before Configuration

**Scenario**: Code attempts to access VFS before ConfigureVirtualFileSystem() called

**Handling**:
- VFS is created empty in Engine::Initialize()
- Mount operations happen after Initialize() in Application::ConfigureVirtualFileSystem()
- VFS operations before configuration simply return empty (no assets mounted)

### 4. Rapid Resize Events

**Scenario**: WM_SIZE messages arrive faster than Renderer can process

**Handling**:
- Each resize event forwarded synchronously via callback
- DX11Renderer::OnResize must handle rapid successive calls gracefully
- Target: < 1ms per event (SC-008) allows ~1000 resize events/second budget
