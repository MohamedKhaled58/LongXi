# Win32Window Callback Interface Contract

**Feature**: 009-engine-routing-refactor
**Component**: Win32Window
**Version**: 1.0
**Date**: 2025-03-10

## Purpose

This document defines the interface contract between Win32Window (callback provider) and Application (callback consumer). The contract specifies callback signatures, invocation guarantees, and behavioral expectations.

## Callback Signatures

### 1. Resize Callback

**Signature**: `std::function<void(int width, int height)> OnResize`

**Purpose**: Notifies listeners that the window client area has been resized

**Parameters**:
- `width` (int): New client area width in pixels. Guaranteed > 0.
- `height` (int): New client area height in pixels. Guaranteed > 0.

**Invocation Trigger**: WM_SIZE message received with valid dimensions

**Invocation Frequency**: Once per WM_SIZE message

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should forward resize event to appropriate subsystem
- Consumer should validate own initialization state before acting
- Consumer must not throw exceptions (std::function will terminate)

**Logging Requirement**: Consumer must log event receipt with component name and dimensions (per FR-017)

---

### 2. Key Down Callback

**Signature**: `std::function<void(UINT virtualKey, bool isRepeat)> OnKeyDown`

**Purpose**: Notifies listeners that a keyboard key has been pressed

**Parameters**:
- `virtualKey` (UINT): Win32 virtual key code (e.g., VK_A, VK_RETURN, VK_ESCAPE)
- `isRepeat` (bool): True if this is an auto-repeat event, false for initial key press

**Invocation Trigger**: WM_KEYDOWN or WM_SYSKEYDOWN message received

**Invocation Frequency**: Once per key press event (including repeats while held)

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should forward to input system for state tracking
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Logging Requirement**: Consumer must log event receipt with component name and key code

---

### 3. Key Up Callback

**Signature**: `std::function<void(UINT virtualKey)> OnKeyUp`

**Purpose**: Notifies listeners that a keyboard key has been released

**Parameters**:
- `virtualKey` (UINT): Win32 virtual key code

**Invocation Trigger**: WM_KEYUP or WM_SYSKEYUP message received

**Invocation Frequency**: Once per key release event

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should forward to input system for state tracking
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Logging Requirement**: Consumer must log event receipt with component name and key code

---

### 4. Mouse Move Callback

**Signature**: `std::function<void(int x, int y)> OnMouseMove`

**Purpose**: Notifies listeners that the mouse cursor has moved within the client area

**Parameters**:
- `x` (int): Client area X coordinate in pixels (relative to window origin)
- `y` (int): Client area Y coordinate in pixels (relative to window origin)

**Invocation Trigger**: WM_MOUSEMOVE message received

**Invocation Frequency**: High frequency (every mouse movement pixel)

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should forward to input system for cursor tracking
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Performance Requirement**: Target < 100μs per event (SC-009)

**Logging Requirement**: Debug logging only (too verbose for info level)

---

### 5. Mouse Button Down Callback

**Signature**: `std::function<void(MouseButton button)> OnMouseButtonDown`

**Purpose**: Notifies listeners that a mouse button has been pressed

**Parameters**:
- `button` (MouseButton): Enum value (Left, Right, Middle)

**Invocation Trigger**: WM_LBUTTONDOWN, WM_RBUTTONDOWN, or WM_MBUTTONDOWN received

**Invocation Frequency**: Once per button press

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should call SetCapture(hwnd) to receive mouse events outside window
- Consumer should forward to input system for button state tracking
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Logging Requirement**: Consumer must log event receipt with component name and button

---

### 6. Mouse Button Up Callback

**Signature**: `std::function<void(MouseButton button)> OnMouseButtonUp`

**Purpose**: Notifies listeners that a mouse button has been released

**Parameters**:
- `button` (MouseButton): Enum value (Left, Right, Middle)

**Invocation Trigger**: WM_LBUTTONUP, WM_RBUTTONUP, or WM_MBUTTONUP received

**Invocation Frequency**: Once per button release

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should call ReleaseCapture() after button up
- Consumer should forward to input system for button state tracking
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Logging Requirement**: Consumer must log event receipt with component name and button

---

### 7. Mouse Wheel Callback

**Signature**: `std::function<void(int delta)> OnMouseWheel`

**Purpose**: Notifies listeners that the mouse wheel has been rotated

**Parameters**:
- `delta` (int): Wheel rotation delta (±120 per notch, positive = away from user, negative = toward user)

**Invocation Trigger**: WM_MOUSEWHEEL message received

**Invocation Frequency**: Once per wheel notch event

**Thread Safety**: Single-threaded (Phase 1 runtime)

**Null Safety**: Win32Window checks for null before invocation

**Expected Consumer Behavior**:
- Consumer should forward to input system for wheel delta accumulation
- Consumer must validate own initialization state before acting
- Consumer must not throw exceptions

**Logging Requirement**: Consumer must log event receipt with component name and delta

---

## General Contract Terms

### Lifetime Guarantees

**Win32Window Guarantees**:
- Callbacks remain valid for the lifetime of the Win32Window instance
- Callbacks are not invoked after Win32Window destruction begins
- Callback members are public and can be assigned at any time
- Previous callback values are overwritten on assignment (no chaining)

**Application Responsibilities**:
- Wire callbacks before entering message loop
- Do not destroy Win32Window while callbacks are active
- Ensure callback lambdas capture valid pointers (check m_Engine validity)
- Do not assign null callbacks if message handling is required

### Exception Safety

**Contract**: Callbacks must not throw exceptions

**Rationale**: std::function invocation will call std::terminate() on exception, crashing the application

**Consumer Requirements**:
- Wrap any exception-prone operations in try-catch blocks
- Log errors and return gracefully if exceptions occur
- Do not let exceptions propagate through callback boundary

### Performance Expectations

**Resize Callback**: Target < 1ms from Win32Window to Renderer (SC-008)

**Input Callbacks**: Target < 100μs from Win32Window to InputSystem (SC-009)

**Measurement**: Includes callback overhead, Application lambda, and Engine subsystem access

### Null Safety Contract

**Win32Window Obligations**:
- Always check callback for null before invocation
- Never invoke null callback (would crash)
- Log warning if callback expected but null (debug builds only)

**Application Obligations**:
- Always check `m_Engine && m_Engine->IsInitialized()` in callback lambdas
- Never dereference null or uninitialized Engine
- Return gracefully if Engine not ready

### Concurrency Contract (Phase 1)

**Current**: Single-threaded runtime, no concurrent callback invocation

**Future MT Consideration**:
- Callbacks may be invoked from different threads in future multithreaded runtime
- Callback implementations must be ready for synchronization (mutexes, atomic flags)
- Current single-threaded implementation must not introduce threading pitfalls

---

## Example Usage

### Correct Implementation

```cpp
// Application.cpp
bool Application::CreateEngine()
{
    m_Engine = std::make_unique<Engine>();

    if (!m_Engine->Initialize(m_WindowHandle, width, height))
        return false;

    // Wire callbacks with null safety checks
    m_Window->OnResize = [this](int w, int h)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            LX_APPLICATION_INFO("[Application] Resize forwarded to engine: {}x{}", w, h);
            m_Engine->OnResize(w, h);
        }
    };

    m_Window->OnKeyDown = [this](UINT key, bool repeat)
    {
        if (m_Engine && m_Engine->IsInitialized())
        {
            m_Engine->GetInput().OnKeyDown(key, repeat);
        }
    };

    ConfigureVirtualFileSystem();
    return true;
}
```

### Incorrect Implementation (Do Not Do This)

```cpp
// WRONG: No null safety check
m_Window->OnResize = [this](int w, int h)
{
    m_Engine->OnResize(w, h);  // Crashes if m_Engine null or not initialized
};

// WRONG: Capturing raw pointer without lifetime management
Engine* rawEngine = m_Engine.get();
m_Window->OnResize = [rawEngine](int w, int h)
{
    rawEngine->OnResize(w, h);  // Dangling pointer if Engine destroyed
};

// WRONG: Throwing exceptions from callback
m_Window->OnResize = [this](int w, int h)
{
    if (w < 0 || h < 0)
        throw std::runtime_error("Invalid dimensions");  // Terminates app
};
```

---

## Testing Expectations

When testing callback contracts, verify:

1. **Null Callback Safety**: Win32Window does not crash when callbacks are null
2. **Engine Not Ready**: Application callbacks do nothing when Engine uninitialized
3. **Exception Safety**: Callbacks handle errors without throwing
4. **Performance**: Event routing meets timing budgets (1ms resize, 100μs input)
5. **Logging**: All logged events include component name and event details

---

## Version History

- **1.0** (2025-03-10): Initial callback interface contract for Spec 009
