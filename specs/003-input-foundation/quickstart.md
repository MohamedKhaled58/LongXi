# Quickstart: Input Foundation

**Branch**: `003-input-foundation`
**Purpose**: Developer reference — how the input system is structured, how to add it, and how later systems will use it.

---

## What Gets Added

Two new source files in `LXEngine`:

```
LongXi/LXEngine/Src/Input/InputSystem.h
LongXi/LXEngine/Src/Input/InputSystem.cpp
```

Three existing files are modified:

```
LongXi/LXEngine/Src/Application/Application.h     ← add m_InputSystem, GetInput()
LongXi/LXEngine/Src/Application/Application.cpp   ← route messages, call Update()
```

No changes to `LXCore`, `LXShell`, the renderer, or the build script.

---

## Frame Lifecycle

Each frame in `Application::Run()`:

```
1. PeekMessage loop — dispatches Win32 messages to WindowProc
   └── WindowProc routes input messages → InputSystem message handlers
         (OnKeyDown, OnKeyUp, OnMouseMove, OnMouseButtonDown, ...)

2. Query input state — Pressed/Released transitions are valid here
   └── m_InputSystem->IsKeyPressed(...)   ← current=true, previous=false ✓
   └── m_InputSystem->IsKeyReleased(...)  ← current=false, previous=true ✓

3. m_Renderer->BeginFrame()
4. m_Renderer->EndFrame()

5. m_InputSystem->Update()  ← END of frame
   └── Copies current → previous state
   └── Resets wheel delta to 0
   └── Next frame's Pressed/Released start from this snapshot
```

**Critical**: `Update()` runs at the **end** of the frame, after all queries. Calling it before queries would make `current == previous`, causing `Pressed` and `Released` to always return false.

---

## Query API — How Later Systems Use It

```cpp
// Access via Application
const InputSystem& input = app->GetInput();

// Key held this frame
if (input.IsKeyDown(Key::W))   { /* move forward */ }
if (input.IsKeyDown(Key::S))   { /* move backward */ }

// Transition — true only on the first frame the key goes down
if (input.IsKeyPressed(Key::Space))   { /* jump, one-shot */ }
if (input.IsKeyPressed(Key::Escape))  { /* open menu, one-shot */ }

// Transition — true only on the first frame the key comes up
if (input.IsKeyReleased(Key::LControl)) { /* crouch release */ }

// Mouse buttons
if (input.IsMouseButtonDown(MouseButton::Left))      { /* attack held */ }
if (input.IsMouseButtonPressed(MouseButton::Right))  { /* aim start */ }
if (input.IsMouseButtonReleased(MouseButton::Right)) { /* aim end */ }

// Cursor position (client-space pixels, top-left = 0,0)
int mx = input.GetMouseX();
int my = input.GetMouseY();

// Wheel (accumulated this frame; 120 = one notch up, -120 = one notch down)
int wheel = input.GetWheelDelta();
if (wheel != 0) { /* zoom or scroll */ }
```

---

## Key Enum Coverage

The `Key` enum covers standard keyboard input needed for a MMORPG client:

| Category | Keys |
|----------|------|
| Letters | A–Z |
| Top-row digits | D0–D9 |
| Function | F1–F12 |
| Arrows | Up, Down, Left, Right |
| Navigation | Home, End, PageUp, PageDown, Insert, Delete |
| Common | Escape, Enter, Space, Tab, Backspace |
| Modifiers | LShift, RShift, LControl, RControl, LAlt, RAlt |
| Locks | CapsLock, NumLock, ScrollLock |
| Numpad | Numpad0–Numpad9, NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide, NumpadDecimal, NumpadEnter |
| Punctuation | Semicolon, Apostrophe, Comma, Period, Slash, Backslash, LeftBracket, RightBracket, Minus, Equal, Grave |

Keys not in the enum are silently ignored (translate to `Key::Unknown`). Side mouse buttons (X1/X2) are deferred to a later spec.

---

## Focus-Loss Behavior

When the application loses focus (alt-tab, task switch):

- `WM_KILLFOCUS` → `InputSystem::OnFocusLost()` → all key and mouse button current-states cleared
- `WM_ACTIVATEAPP` (deactivate) → same

On the next `Update()` after focus-loss, any keys that were `Down` will produce `Released() = true` for one frame, then transition to fully inactive. No keys remain stuck.

---

## Wheel Delta Semantics

- Raw Win32 unit: ±120 per scroll notch (high-resolution mice may deliver smaller increments)
- Positive = scroll up (toward user) = zoom in / scroll list up
- Accumulates across the frame: multiple quick scrolls in one pump cycle sum correctly
- Resets to 0 at `Update()` — always zero if no scroll occurred this frame

---

## Verification (Manual)

Add temporary diagnostic logging to `Application::Run()` to verify the system works:

```cpp
// After Update(), before BeginFrame()
const auto& input = *m_InputSystem;
if (input.IsKeyPressed(Key::Space))
    LX_ENGINE_INFO("Space PRESSED this frame");
if (input.IsKeyReleased(Key::Space))
    LX_ENGINE_INFO("Space RELEASED this frame");
if (input.GetWheelDelta() != 0)
    LX_ENGINE_INFO("Wheel delta: {}", input.GetWheelDelta());
```

Remove this diagnostic block before committing.

---

## What This Does NOT Do

- No action mapping (pressing Space does not "jump" — InputSystem has no gameplay knowledge)
- No UI event dispatch
- No controller/gamepad input
- No raw input (`WM_INPUT`)
- No text input / IME
- No side mouse buttons (X1, X2)
- No input recording or replay
- No thread safety (single-threaded Phase 1)
