# Quickstart: Engine Event Routing and Application Boundary Cleanup

**Feature**: 009-engine-routing-refactor
**Branch**: `009-event-routing-refactor`
**Estimated Effort**: 4-6 hours

## Overview

This quickstart guide provides step-by-step implementation instructions for refactoring the engine event routing architecture. The refactor moves VFS configuration from Engine to Application and relocates Win32 message handling from Application to Win32Window.

## Prerequisites

- Current branch: `009-event-routing-refactor`
- Build environment: Visual Studio 2022, Premake5, MSVC v145
- Required specs:
  - [spec.md](./spec.md) - Functional requirements and success criteria
  - [research.md](./research.md) - Technical decisions and rationale
  - [data-model.md](./data-model.md) - Entity relationships and state machines
  - [contracts/win32window-callbacks.md](./contracts/win32window-callbacks.md) - Callback interface contracts

## Step 1: Add Callback Members to Win32Window (30 min)

**File**: `LongXi/LXEngine/Src/Window/Win32Window.h`

**Action**: Add public std::function callback members

```cpp
// In Win32Window class declaration, add after public members:
public:
    // Window event callbacks
    std::function<void(int, int)> OnResize;
    std::function<void(UINT, bool)> OnKeyDown;
    std::function<void(UINT)> OnKeyUp;
    std::function<void(int, int)> OnMouseMove;
    std::function<void(MouseButton)> OnMouseButtonDown;
    std::function<void(MouseButton)> OnMouseButtonUp;
    std::function<void(int)> OnMouseWheel;
```

**Verification**: Compile succeeds (Win32Window.cpp will use these in next step)

---

## Step 2: Move WindowProc to Win32Window (45 min)

**File**: `LongXi/LXEngine/Src/Window/Win32Window.cpp`

**Action**:

1. **Move** `Application::WindowProc` to `Win32Window::WindowProc` (static member)
2. **Modify** WindowProc to retrieve Win32Window* from GWLP_USERDATA instead of Application*
3. **Invoke callbacks** when corresponding Win32 messages received

```cpp
// In Win32Window.cpp
LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // Handle resize
    if (msg == WM_SIZE)
    {
        if (window && window->OnResize)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0)  // Guard against minimized window
            {
                LX_WINDOW_INFO("[Window] WM_SIZE received: {}x{}", width, height);
                window->OnResize(width, height);
            }
        }
        return 0;
    }

    // Handle keyboard
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        if (window && window->OnKeyDown)
        {
            UINT vk = static_cast<UINT>(wParam);
            bool isRepeat = (lParam & 0x40000000) != 0;
            window->OnKeyDown(vk, isRepeat);
        }
        return 0;
    }

    if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
    {
        if (window && window->OnKeyUp)
        {
            window->OnKeyUp(static_cast<UINT>(wParam));
        }
        return 0;
    }

    // Handle mouse
    if (msg == WM_MOUSEMOVE)
    {
        if (window && window->OnMouseMove)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            window->OnMouseMove(x, y);
        }
        return 0;
    }

    if (msg == WM_LBUTTONDOWN)
    {
        if (window && window->OnMouseButtonDown)
        {
            SetCapture(hwnd);
            window->OnMouseButtonDown(MouseButton::Left);
        }
        return 0;
    }

    if (msg == WM_LBUTTONUP)
    {
        if (window && window->OnMouseButtonUp)
        {
            window->OnMouseButtonUp(MouseButton::Left);
            ReleaseCapture();
        }
        return 0;
    }

    // ... similar for WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MOUSEWHEEL

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
```

**Remove from Application**:
- Delete `Application::WindowProc` from Application.cpp
- Remove WindowProc declaration from Application.h

**Verification**: Compile succeeds, Application has no WindowProc

---

## Step 3: Update Win32Window Initialization (15 min)

**File**: `LongXi/LXEngine/Src/Window/Win32Window.cpp`

**Action**: In Win32Window::Create(), store `this` pointer in GWLP_USERDATA (already done, just verify)

```cpp
bool Win32Window::Create(WNDPROC wndProc)
{
    // ... existing window creation code ...

    // Store Win32Window* in user data (already present)
    SetWindowLongPtr(m_WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // ... rest of creation ...
}
```

**Verification**: WindowProc can retrieve Win32Window* via GWLP_USERDATA

---

## Step 4: Remove VFS Mounting from Engine (20 min)

**File**: `LongXi/LXEngine/Src/Engine/Engine.cpp`

**Action**: Remove VFS mount configuration from `Engine::Initialize()`

```cpp
// REMOVE THIS CODE from Engine::Initialize():
std::string exeDir = ResourceSystem::GetExecutableDirectory();
if (!exeDir.empty())
{
    m_VFS->MountDirectory(exeDir + "/Data/Patch");
    m_VFS->MountDirectory(exeDir + "/Data");
    m_VFS->MountDirectory(exeDir);
    m_VFS->MountWdf(exeDir + "/Data/C3.wdf");
    m_VFS->MountWdf(exeDir + "/Data/data.wdf");
}
```

**Keep**: VFS creation (`m_VFS = std::make_unique<CVirtualFileSystem>()`)

**Verification**: Engine::Initialize() creates empty VFS, no mounts

---

## Step 5: Add ConfigureVirtualFileSystem to Application (30 min)

**File**: `LongXi/LXEngine/Src/Application/Application.h`

**Action**: Add private method declaration

```cpp
private:
    void ConfigureVirtualFileSystem();
```

**File**: `LongXi/LXEngine/Src/Application/Application.cpp`

**Action**: Implement method with VFS mount configuration

```cpp
void Application::ConfigureVirtualFileSystem()
{
    auto& vfs = m_Engine->GetVFS();
    std::string exeDir = ResourceSystem::GetExecutableDirectory();

    if (!exeDir.empty())
    {
        LX_ENGINE_INFO("[Application] Configuring VFS mounts");

        // Mount directories first (highest priority)
        vfs.MountDirectory(exeDir + "/Data/Patch");
        vfs.MountDirectory(exeDir + "/Data");
        vfs.MountDirectory(exeDir);

        // Mount WDF archives after directories (lower priority)
        vfs.MountWdf(exeDir + "/Data/C3.wdf");
        vfs.MountWdf(exeDir + "/Data/data.wdf");

        LX_ENGINE_INFO("[Application] VFS configuration complete");
    }
    else
    {
        LX_ENGINE_WARN("[Application] Failed to get executable directory for VFS configuration");
    }
}
```

**Verification**: Application owns VFS mount configuration

---

## Step 6: Call ConfigureVirtualFileSystem from CreateEngine (10 min)

**File**: `LongXi/LXEngine/Src/Application/Application.cpp`

**Action**: Call ConfigureVirtualFileSystem() after Engine::Initialize() succeeds

```cpp
bool Application::CreateEngine()
{
    m_Engine = std::make_unique<Engine>();

    if (!m_Engine->Initialize(m_WindowHandle, m_Window->GetWidth(), m_Window->GetHeight()))
    {
        LX_ENGINE_ERROR("Engine initialization failed - aborting");
        m_Engine.reset();
        return false;
    }

    ConfigureVirtualFileSystem();  // ADD THIS LINE

    return true;
}
```

**Verification**: VFS configured after engine initialization

---

## Step 7: Wire Callbacks in Application (45 min)

**File**: `LongXi/LXEngine/Src/Application/Application.cpp`

**Action**: Wire Win32Window callbacks in `CreateEngine()` or `Initialize()`

```cpp
bool Application::CreateEngine()
{
    m_Engine = std::make_unique<Engine>();

    if (!m_Engine->Initialize(m_WindowHandle, m_Window->GetWidth(), m_Window->GetHeight()))
    {
        LX_ENGINE_ERROR("Engine initialization failed - aborting");
        m_Engine.reset();
        return false;
    }

    // Wire callbacks
    WireWindowCallbacks();

    ConfigureVirtualFileSystem();
    return true;
}

// NEW HELPER METHOD
void Application::WireWindowCallbacks()
{
    LX_ENGINE_INFO("[Application] Wiring window event callbacks");

    // Resize callback
    m_Window->OnResize = [this](int w, int h)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            LX_APPLICATION_INFO("[Application] Resize forwarded to engine: {}x{}", w, h);
            m_Engine->OnResize(w, h);
        }
    };

    // Keyboard callbacks
    m_Window->OnKeyDown = [this](UINT key, bool repeat)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnKeyDown(key, repeat);
        }
    };

    m_Window->OnKeyUp = [this](UINT key)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnKeyUp(key);
        }
    };

    // Mouse move callback
    m_Window->OnMouseMove = [this](int x, int y)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseMove(x, y);
        }
    };

    // Mouse button callbacks
    m_Window->OnMouseButtonDown = [this](MouseButton button)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseButtonDown(button);
        }
    };

    m_Window->OnMouseButtonUp = [this](MouseButton button)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseButtonUp(button);
        }
    };

    // Mouse wheel callback
    m_Window->OnMouseWheel = [this](int delta)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnMouseWheel(delta);
        }
    };
}
```

**Add to Application.h**:
```cpp
private:
    void WireWindowCallbacks();
    void ConfigureVirtualFileSystem();
```

**Verification**: All 7 callbacks wired with null safety checks

---

## Step 8: Add Engine::OnResize Method (20 min)

**File**: `LongXi/LXEngine/Src/Engine/Engine.h`

**Action**: Add public method declaration

```cpp
public:
    void OnResize(int width, int height);
```

**File**: `LongXi/LXEngine/Src/Engine/Engine.cpp`

**Action**: Implement method

```cpp
void Engine::OnResize(int width, int height)
{
    if (!m_Initialized)
    {
        LX_ENGINE_ERROR("[Engine] OnResize() called before Initialize()");
        return;
    }

    // Guard against zero-area (minimized window)
    if (width <= 0 || height <= 0)
    {
        return;
    }

    LX_ENGINE_INFO("[Engine] Resize forwarded to renderer: {}x{}", width, height);

    // Forward to renderer
    m_Renderer->OnResize(width, height);
}
```

**Verification**: Resize events route from Win32Window → Application → Engine → Renderer

---

## Step 9: Update Application Constructor (10 min)

**File**: `LongXi/LXEngine/Src/Application/Application.cpp`

**Action**: Remove WindowProc from member initialization list (already deleted in Step 2)

**Current constructor** (unchanged):
```cpp
Application::Application()
    : m_WindowHandle(nullptr)
    , m_ShouldShutdown(false)
    , m_Initialized(false)
{
}
```

**Verification**: No WindowProc references remain

---

## Step 10: Build and Test (30 min)

**Action**: Build the solution and run the application

```bash
# Build
Win-Build Debug

# Run
Build/Debug/Executables/LXShell.exe
```

**Verification Checklist**:

- ✅ Application compiles without errors
- ✅ Application::WindowProc removed (search for "WindowProc" in Application.cpp)
- ✅ Win32Window::WindowProc exists (search in Win32Window.cpp)
- ✅ Engine.cpp contains no VFS mount paths (search for "MountDirectory" or "MountWdf")
- ✅ Application::ConfigureVirtualFileSystem() exists and is called
- ✅ All 7 callbacks defined in Win32Window.h
- ✅ Callbacks wired in Application::WireWindowCallbacks()
- ✅ Engine::OnResize() exists and forwards to Renderer
- ✅ Application runs successfully
- ✅ Window resize works (drag window edge)
- ✅ Keyboard input works (press keys, check console)
- ✅ Mouse input works (move mouse, click buttons, scroll wheel)
- ✅ VFS mounts succeed (check log for "[Application] VFS configuration complete")

---

## Step 11: Verify Logging (15 min)

**Action**: Run application and verify event routing logs

**Expected log output**:
```
[Window] WM_SIZE received: 1024x768
[Application] Resize forwarded to engine: 1024x768
[Engine] Resize forwarded to renderer: 1024x768
```

**Verification**: All three log lines appear for each event type

---

## Step 12: Code Review Checklist (30 min)

Verify architectural requirements met:

**FR-001**: Application owns VFS mount configuration via ConfigureVirtualFileSystem()
- ✅ Method exists in Application.cpp
- ✅ Called after Engine::Initialize()

**FR-002**: Engine contains no hardcoded asset paths
- ✅ Search Engine.cpp for "Data/" or ".wdf" - should find nothing

**FR-003**: Engine exposes GetVFS() accessor
- ✅ Already exists (unchanged)

**FR-004**: Win32Window owns WindowProc
- ✅ WindowProc is static member of Win32Window
- ✅ Application has no WindowProc

**FR-005**: Win32Window exposes std::function callbacks
- ✅ All 7 callbacks declared in Win32Window.h

**FR-006**: WindowProc translates messages to callbacks
- ✅ Each WM_* message invokes corresponding callback

**FR-007**: Application wires callbacks to Engine
- ✅ WireWindowCallbacks() connects all 7 callbacks

**FR-008**: Engine::OnResize() exists
- ✅ Method declared and implemented

**FR-009**: Engine::OnResize() forwards to Renderer
- ✅ Calls m_Renderer->OnResize()

**FR-010**: Application forwards input events to Engine::GetInput()
- ✅ All input callbacks call m_Engine->GetInput().On*()

**FR-011**: Engine public interface has no Win32 types
- ✅ Check Engine.h - only HWND in Initialize() parameter (acceptable)
- ✅ No WPARAM, LPARAM in public interface

**FR-012**: CreateEngine() calls ConfigureVirtualFileSystem() after Initialize()
- ✅ Verified in Step 6

**FR-013**: ConfigureVirtualFileSystem() mounts in correct order
- ✅ Patch → Data → root → WDF archives

**FR-014**: ConfigureVirtualFileSystem() mounts WDF archives after directories
- ✅ Verified in code order

**FR-015**: Engine initializes without VFS mounts
- ✅ Mount code removed from Engine::Initialize()

**FR-016**: Callbacks use engine-independent types
- ✅ int for dimensions, UINT for key codes, MouseButton enum

**FR-017**: Event routing logs with component name and event type
- ✅ Log statements added in WindowProc, callbacks, Engine::OnResize

---

## Common Issues and Solutions

### Issue: "unresolved external symbol Application::WindowProc"

**Cause**: Forgot to remove WindowProc declaration from Application.h

**Fix**: Remove `static LRESULT CALLBACK WindowProc(...)` from Application.h

---

### Issue: "nullptr assignment to std::function"

**Cause**: Callback invoked before being wired

**Fix**: Always check callback null before invocation in Win32Window::WindowProc

---

### Issue: VFS not mounting, log shows "Resource not found"

**Cause**: ConfigureVirtualFileSystem() not called or called before Engine::Initialize()

**Fix**: Ensure ConfigureVirtualFileSystem() called after Engine::Initialize() succeeds

---

### Issue: Callbacks not firing

**Cause**: GWLP_USERDATA not set correctly, or callbacks not wired

**Fix**: Verify SetWindowLongPtr(hwnd, GWLP_USERDATA, this) in Win32Window::Create()

---

### Issue: Engine::OnResize crashes

**Cause**: Renderer not initialized or null pointer

**Fix**: Check m_Initialized before accessing m_Renderer

---

## Success Criteria Verification

Run through success criteria from spec.md:

**SC-001**: Engine.h has no Win32 types in public interface
- Check: Only HWND in Initialize() parameter ✅

**SC-002**: Engine.cpp has no hardcoded VFS paths
- Check: Search for "Data/" and ".wdf" in Engine.cpp ✅

**SC-003**: Application::WindowProc removed
- Check: Search Application.cpp for "WindowProc" ✅

**SC-004**: Win32Window exposes 7 callback members
- Check: Count OnResize, OnKeyDown, OnKeyUp, OnMouseMove, OnMouseButtonDown, OnMouseButtonUp, OnMouseWheel ✅

**SC-005**: VFS config centralized in ConfigureVirtualFileSystem()
- Check: Single method contains all MountDirectory/MountWdf calls ✅

**SC-006**: Event routing ≤ 3 hops
- Check: Win32Window → Application callback → Engine subsystem ✅

**SC-007**: Engine initializes without VFS mounts
- Check: Engine::Initialize() creates empty VFS ✅

**SC-008**: Resize < 1ms
- Test: Rapid resize events, measure with profiler ✅

**SC-009**: Input < 100μs
- Test: Rapid input events, measure with profiler ✅

**SC-010**: Engine.h has no platform-specific includes
- Check: No #include <Windows.h> in Engine.h ✅

---

## Next Steps

After completing this quickstart:

1. **Run Tests**: Execute test application and verify all input works
2. **Profile Performance**: Measure event routing latency against SC-008/SC-009
3. **Code Review**: Have team review architectural changes
4. **Update Documentation**: Update any engine architecture diagrams
5. **Commit Changes**: `git add/commit/push` with descriptive message

## Estimated Timeline

| Step | Duration | Cumulative |
|------|----------|------------|
| Add callbacks to Win32Window | 30 min | 0:30 |
| Move WindowProc to Win32Window | 45 min | 1:15 |
| Update Win32Window initialization | 15 min | 1:30 |
| Remove VFS mounting from Engine | 20 min | 1:50 |
| Add ConfigureVirtualFileSystem | 30 min | 2:20 |
| Call ConfigureVirtualFileSystem | 10 min | 2:30 |
| Wire callbacks | 45 min | 3:15 |
| Add Engine::OnResize | 20 min | 3:35 |
| Update constructor | 10 min | 3:45 |
| Build and test | 30 min | 4:15 |
| Verify logging | 15 min | 4:30 |
| Code review checklist | 30 min | 5:00 |

**Total Estimated Time**: 4-6 hours (depending on familiarity with codebase)

---

## Support

For issues or questions:
- Review [research.md](./research.md) for technical decisions
- Review [data-model.md](./data-model.md) for entity relationships
- Review [contracts/win32window-callbacks.md](./contracts/win32window-callbacks.md) for callback contracts
- Check build logs in Visual Studio Output window
- Check runtime logs in console window

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
