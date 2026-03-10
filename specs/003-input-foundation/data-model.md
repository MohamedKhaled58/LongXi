# Data Model: Input Foundation

**Branch**: `003-input-foundation`
**Source**: Derived from `spec.md` requirements + `research.md` findings

---

## Key Entities

### `Key` Enum

Strongly typed engine-defined keyboard key identifier. Values do **not** map directly to Win32 VK codes — a static translation table converts between the two internally.

```
Key::Unknown    = 0     (sentinel — unmapped VK codes)

Letters:        A–Z     (26 values)
Digits:         D0–D9   (10 values, top-row digit keys)
Function keys:  F1–F12  (12 values)
Arrow keys:     Up, Down, Left, Right
Navigation:     Home, End, PageUp, PageDown, Insert, Delete
Common:         Escape, Enter, Space, Tab, Backspace
Modifiers:      LShift, RShift, LControl, RControl, LAlt, RAlt
Locks:          CapsLock, NumLock, ScrollLock
Numpad:         Numpad0–Numpad9, NumpadAdd, NumpadSubtract,
                NumpadMultiply, NumpadDivide, NumpadDecimal, NumpadEnter
Punctuation:    Semicolon, Apostrophe, Comma, Period, Slash,
                Backslash, LeftBracket, RightBracket, Minus, Equal, Grave

Key::Count      (sentinel — total key count, used as array size)
```

**Representation**: `enum class Key : uint8_t`

**Internal mapping**: A file-scope `static UINT s_KeyToVK[(size_t)Key::Count]` array maps each `Key` to its Win32 virtual key code. A file-scope `static Key s_VKToKey[256]` array maps each VK code (0–255) to the corresponding `Key` value (or `Key::Unknown` for unmapped codes). Both tables are initialized at `InputSystem::Initialize()`.

---

### `MouseButton` Enum

Strongly typed mouse button identifier for left, right, and middle buttons.

```
MouseButton::Left   = 0
MouseButton::Right  = 1
MouseButton::Middle = 2
MouseButton::Count  = 3  (sentinel)
```

**Representation**: `enum class MouseButton : uint8_t`

---

### `InputSystem` Class

Central engine object responsible for receiving Win32 input messages, maintaining per-frame state, advancing frame-boundary transitions, and exposing query operations.

#### State Fields

| Field | Type | Description |
|-------|------|-------------|
| `m_KeyCurrent` | `bool[(size_t)Key::Count]` | Key held-state this frame (set by message handlers) |
| `m_KeyPrevious` | `bool[(size_t)Key::Count]` | Key held-state last frame (copied from current at Update) |
| `m_MouseCurrent` | `bool[(size_t)MouseButton::Count]` | Mouse button held-state this frame |
| `m_MousePrevious` | `bool[(size_t)MouseButton::Count]` | Mouse button held-state last frame |
| `m_MouseX` | `int` | Most recent cursor X position in client-space pixels |
| `m_MouseY` | `int` | Most recent cursor Y position in client-space pixels |
| `m_WheelDelta` | `int` | Accumulated wheel delta this frame (raw Win32 units; positive = scroll up) |

#### Lifecycle Methods

| Method | Called By | Description |
|--------|-----------|-------------|
| `Initialize()` | `Application::Initialize()` | Zero-initializes all state; builds VK translation tables |
| `Shutdown()` | `Application::Shutdown()` | No-op for this implementation; included for lifecycle symmetry |
| `Update()` | `Application::Run()`, once per frame before render | Copies current → previous; resets `m_WheelDelta` to 0 |

#### Message Handler Methods

Called directly from `Application::WindowProc` on the matching Win32 message:

| Method | Win32 Message | Notes |
|--------|--------------|-------|
| `OnKeyDown(UINT vk, bool isRepeat)` | `WM_KEYDOWN`, `WM_SYSKEYDOWN` | Sets `m_KeyCurrent[mapped]` = true; ignores if `isRepeat` |
| `OnKeyUp(UINT vk)` | `WM_KEYUP`, `WM_SYSKEYUP` | Sets `m_KeyCurrent[mapped]` = false |
| `OnMouseMove(int x, int y)` | `WM_MOUSEMOVE` | Updates `m_MouseX`, `m_MouseY` from `GET_X_LPARAM` / `GET_Y_LPARAM` |
| `OnMouseButtonDown(MouseButton)` | `WM_LBUTTONDOWN`, `WM_RBUTTONDOWN`, `WM_MBUTTONDOWN` | Sets `m_MouseCurrent[button]` = true; calls `SetCapture(hwnd)` |
| `OnMouseButtonUp(MouseButton)` | `WM_LBUTTONUP`, `WM_RBUTTONUP`, `WM_MBUTTONUP` | Sets `m_MouseCurrent[button]` = false; calls `ReleaseCapture()` if no other buttons held |
| `OnMouseWheel(int delta)` | `WM_MOUSEWHEEL` | Accumulates `m_WheelDelta += delta` (raw Win32 delta, ±120 per notch) |
| `OnFocusLost()` | `WM_KILLFOCUS`, `WM_ACTIVATEAPP` (deactivate) | Zeroes `m_KeyCurrent`, `m_MouseCurrent`; does not clear position or delta |

#### Query Methods

| Method | Signature | Returns |
|--------|-----------|---------|
| `IsKeyDown` | `bool IsKeyDown(Key key) const` | True if key is held this frame |
| `IsKeyPressed` | `bool IsKeyPressed(Key key) const` | True only on the frame the key transitions up → down |
| `IsKeyReleased` | `bool IsKeyReleased(Key key) const` | True only on the frame the key transitions down → up |
| `IsMouseButtonDown` | `bool IsMouseButtonDown(MouseButton) const` | True if button is held this frame |
| `IsMouseButtonPressed` | `bool IsMouseButtonPressed(MouseButton) const` | True only on the frame the button transitions up → down |
| `IsMouseButtonReleased` | `bool IsMouseButtonReleased(MouseButton) const` | True only on the frame the button transitions down → up |
| `GetMouseX` | `int GetMouseX() const` | Current client-space cursor X |
| `GetMouseY` | `int GetMouseY() const` | Current client-space cursor Y |
| `GetWheelDelta` | `int GetWheelDelta() const` | Accumulated wheel delta this frame (0 if no scroll) |

#### Query Implementation Contract

```
Down(key)     = m_KeyCurrent[key]
Pressed(key)  = m_KeyCurrent[key] && !m_KeyPrevious[key]
Released(key) = !m_KeyCurrent[key] && m_KeyPrevious[key]
```

Same contract applies to `MouseButton` fields. Queries are only valid after `Update()` has been called at least once.

---

## State Transition Diagram

```
Key/Button:

    [Up] ──WM_KEYDOWN──▶ [Down]
    [Up] ◀──WM_KEYUP──── [Down]
    [Up] ◀──OnFocusLost── [Down]   (force cleared)

Per-frame transitions (at Update() boundary):

    Frame N message:  Down → sets m_KeyCurrent = true
    Frame N Update(): m_KeyPrevious = false, m_KeyCurrent = true
                      → Pressed() returns true this frame

    Frame N+1:        no change
    Frame N+1 Update(): m_KeyPrevious = true, m_KeyCurrent = true
                        → Down() = true, Pressed() = false

    Frame N+k (release): m_KeyCurrent = false
    Frame N+k Update(): m_KeyPrevious = true, m_KeyCurrent = false
                        → Released() returns true this frame

    Frame N+k+1 Update(): m_KeyPrevious = false, m_KeyCurrent = false
                           → no active transitions
```

---

## Application Integration Points

### `Application.h` changes

```
+ #include "Input/InputSystem.h"  (or forward-declare and include in .cpp)
+ std::unique_ptr<InputSystem> m_InputSystem;
+ const InputSystem& GetInput() const;   // accessor for later systems
```

### `Application.cpp` changes

```
Initialize():
  + CreateInputSystem()  ← new private method, called after CreateMainWindow()

WindowProc():
  WM_KEYDOWN / WM_SYSKEYDOWN  → m_InputSystem->OnKeyDown(vk, repeat)
  WM_KEYUP   / WM_SYSKEYUP    → m_InputSystem->OnKeyUp(vk)
  WM_MOUSEMOVE                → m_InputSystem->OnMouseMove(x, y)
  WM_LBUTTONDOWN              → m_InputSystem->OnMouseButtonDown(MouseButton::Left)
  WM_LBUTTONUP                → m_InputSystem->OnMouseButtonUp(MouseButton::Left)
  WM_RBUTTONDOWN              → m_InputSystem->OnMouseButtonDown(MouseButton::Right)
  WM_RBUTTONUP                → m_InputSystem->OnMouseButtonUp(MouseButton::Right)
  WM_MBUTTONDOWN              → m_InputSystem->OnMouseButtonDown(MouseButton::Middle)
  WM_MBUTTONUP                → m_InputSystem->OnMouseButtonUp(MouseButton::Middle)
  WM_MOUSEWHEEL               → m_InputSystem->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam))
  WM_KILLFOCUS                → m_InputSystem->OnFocusLost()
  WM_ACTIVATEAPP (wParam==0)  → m_InputSystem->OnFocusLost()

Run():
  Each frame iteration:
  + m_InputSystem->Update()   ← called BEFORE render (BeginFrame / EndFrame)

Shutdown():
  + m_InputSystem->Shutdown()  ← called before renderer shutdown
  + m_InputSystem.reset()
```

---

## Constraints and Invariants

- `Update()` MUST be called exactly once per frame, before any system queries input that frame
- `OnFocusLost()` zeroes current state but does NOT reset previous-frame state — the next `Update()` will naturally produce `Released()` = true for any keys that were held at focus-loss time
- `SetCapture` / `ReleaseCapture` ensure `WM_MOUSEBUTTONUP` is received even when the cursor leaves the client area during a drag
- `WM_SYSKEYDOWN` must be handled (not forwarded to `DefWindowProc` blindly) to capture `LAlt` / `RAlt` state; `WM_SYSKEYUP` clears it
- Wheel delta raw units: Win32 delivers ±120 per notch (or fractional for high-resolution wheels). Consumers of `GetWheelDelta()` should divide by `WHEEL_DELTA` (120) to get notch count, or treat raw accumulated value as scroll intensity
- VK code range: 0–255 is the complete Win32 virtual key space. The `s_VKToKey[256]` table covers all possible values
