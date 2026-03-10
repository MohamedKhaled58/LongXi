# Research: Input Foundation

**Branch**: `003-input-foundation`
**Purpose**: Resolve Win32 input integration unknowns before design and implementation

---

## R1: WM_KEYDOWN Repeat Detection

**Decision**: Detect auto-repeat via lParam bit 30. Set key state to "down" only on first press (bit 30 = 0). Ignore subsequent WM_KEYDOWN messages while the key is held (bit 30 = 1).

**Rationale**: Win32 generates WM_KEYDOWN repeatedly while a key is held at the keyboard repeat rate. Without filtering, `Pressed()` would fire on every repeat message instead of only once. The lParam bit 30 (previous key-state flag) is the correct and documented mechanism for this.

**Implementation**: `bool isRepeat = (lParam & (1 << 30)) != 0;` — pass to `OnKeyDown(vk, isRepeat)` and skip setting state if `isRepeat` is true. Only update `m_KeyCurrent` on the first press.

**Alternatives considered**: Checking `m_KeyCurrent[key]` before setting it (self-guard). Rejected: the lParam bit is the authoritative signal and avoids any timing ambiguity.

---

## R2: WM_SYSKEYDOWN / WM_SYSKEYUP for Alt State

**Decision**: Handle `WM_SYSKEYDOWN` and `WM_SYSKEYUP` in the same way as `WM_KEYDOWN` / `WM_KEYUP` for the purposes of key state tracking.

**Rationale**: When the user holds Alt and presses another key, Windows delivers `WM_SYSKEYDOWN` instead of `WM_KEYDOWN`. If `WM_SYSKEYDOWN` is not handled, `LAlt` / `RAlt` will never be recorded as pressed, and combinations like Alt+F will be missed entirely. More critically, if only `WM_KEYDOWN` is handled for Alt, `WM_SYSKEYUP` will fire on release but `WM_KEYUP` will not — leaving Alt stuck as "down" in the state array.

**Implementation**: Route both `WM_KEYDOWN` + `WM_SYSKEYDOWN` to `InputSystem::OnKeyDown`, and both `WM_KEYUP` + `WM_SYSKEYUP` to `InputSystem::OnKeyUp`. After calling the input handler, still return `0` from `WindowProc` for `WM_SYSKEYDOWN` (or call `DefWindowProc`) to preserve default Alt behavior (activating menus etc.) where applicable. For a game, forwarding to `DefWindowProc` for `WM_SYSKEYDOWN` is safe.

**Alternatives considered**: Ignoring `WM_SYSKEYDOWN` entirely. Rejected: causes stuck Alt state and missed Alt-modified key presses.

---

## R3: WM_MOUSEWHEEL Delta Extraction and Coordinates

**Decision**: Use `GET_WHEEL_DELTA_WPARAM(wParam)` to extract the signed short delta. Sign convention: positive = scroll up (wheel rotated toward user). Ignore the lParam screen coordinates — mouse position is tracked separately via `WM_MOUSEMOVE` in client space.

**Rationale**: `GET_WHEEL_DELTA_WPARAM` is the correct Win32 macro for extracting wheel delta from wParam. One full notch = ±120 (`WHEEL_DELTA`). High-resolution mice may deliver fractional values. Accumulating per frame is correct because multiple small increments should not be dropped. The lParam of `WM_MOUSEWHEEL` contains screen-space coordinates, not client-space — and since we track cursor position separately via `WM_MOUSEMOVE`, we have no use for lParam here.

**Implementation**: `m_WheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);` in `OnMouseWheel`. Reset `m_WheelDelta = 0` in `Update()`.

**Alternatives considered**: Storing only the last received delta per frame (overwrite, not accumulate). Rejected by user — accumulation is required.

---

## R4: Mouse Capture (SetCapture / ReleaseCapture)

**Decision**: Call `SetCapture(hwnd)` on the first mouse button press and `ReleaseCapture()` when all mouse buttons are released.

**Rationale**: Without mouse capture, if the user presses a mouse button inside the client area and drags the cursor outside, `WM_LBUTTONUP` (or equivalent) is NOT delivered to the window. This causes the button to remain stuck as "down" in the state array. `SetCapture` routes all subsequent mouse messages to the capturing window regardless of cursor position until `ReleaseCapture` is called.

**Implementation**:
- `OnMouseButtonDown`: if no buttons were down before this press, call `SetCapture(hwnd)`
- `OnMouseButtonUp`: after clearing the button, if no buttons remain down, call `ReleaseCapture()`
- `hwnd` is passed to the `InputSystem` message handlers (or stored at initialization)

**Alternatives considered**: Not using capture. Rejected: leads to stuck button state during drag-outside-window scenarios. This is a known Win32 pitfall for drag operations.

---

## R5: Focus-Loss Clearing — Which Messages to Handle

**Decision**: Handle both `WM_KILLFOCUS` and `WM_ACTIVATEAPP` (deactivate path) to clear all key and mouse button current-state.

**Rationale**:
- `WM_KILLFOCUS`: fires when keyboard focus leaves the window (another window within the same app gets focus, or the user tabs away). Clears keyboard state.
- `WM_ACTIVATEAPP (wParam == FALSE)`: fires when the entire application loses activation (alt-tab to another app). Clears keyboard AND mouse button state.
- Handling both covers all focus-loss paths: keyboard-only focus change AND full application deactivation.
- `WM_ACTIVATE` with `WA_INACTIVE` is an alternative but is per-window (not per-app) and fires more frequently (e.g., when a child dialog activates). `WM_ACTIVATEAPP` is more appropriate for application-level deactivation.

**Implementation**: Both messages route to `InputSystem::OnFocusLost()` which zeroes `m_KeyCurrent[0..Count-1]` and `m_MouseCurrent[0..2]`. Does not touch `m_KeyPrevious` / `m_MousePrevious` — the next `Update()` will naturally produce `Released()` = true for previously held inputs.

**Alternatives considered**: Handling only `WM_KILLFOCUS`. Rejected: does not cover mouse button state or full app deactivation. Handling only `WM_ACTIVATEAPP`. Rejected: may miss keyboard focus changes within the app's own window hierarchy.

---

## R6: Virtual Key Code Range

**Decision**: Use `bool m_KeyCurrent[256]` and `bool m_KeyPrevious[256]` indexed by Win32 virtual key code directly as the internal storage arrays.

**Rationale**: Win32 virtual key codes are `UINT` values in the range 0–255. This is the complete and documented VK space — there are no VK codes outside this range. A `bool[256]` array indexed directly by VK code is memory-trivial (~256 bytes) and O(1) for all operations. The public `Key` enum translates to/from VK codes via a compile-time-initialized mapping table.

**Implementation**: Two static lookup tables initialized at startup:
- `static UINT s_KeyToVK[(size_t)Key::Count]` — maps `Key` → VK code
- `static Key s_VKToKey[256]` — maps VK code → `Key` (or `Key::Unknown` for unmapped codes)

Internal state arrays are indexed by VK code for O(1) message handling. Public queries translate `Key` to VK via `s_KeyToVK` before array access.

**Alternatives considered**: Indexing internal arrays by `Key` enum value. Rejected: requires VK→Key translation on every incoming message (O(1) but adds a table lookup on the hot path). The current approach moves the lookup to the query side where it matters less.

---

## R7: Alt Key Consistency (WM_SYSKEYDOWN/UP)

**Decision**: Covered by R2. The only additional note: `VK_MENU` (0x12) is the virtual key code for the generic Alt key. `VK_LMENU` (0xA4) and `VK_RMENU` (0xA5) are left/right variants. In practice, `WM_SYSKEYDOWN` delivers `VK_MENU`, not `VK_LMENU` or `VK_RMENU`, when Alt is pressed alone. For distinguishing left vs. right Alt, `GetKeyState(VK_LMENU)` / `GetKeyState(VK_RMENU)` can be called at WM_SYSKEYDOWN time. This is deferred — for the bootstrap, we map `VK_MENU` to `Key::LAlt` and separately handle `VK_LMENU` → `Key::LAlt`, `VK_RMENU` → `Key::RAlt` in the VK table.

---

## R8: Per-Frame Two-Array State Pattern

**Decision**: Use two bool arrays (current frame + previous frame) per input category. `Update()` copies current → previous and resets per-frame-only fields (wheel delta).

**Rationale**: This is the standard game engine input state pattern. It provides O(1) queries for all three states (Down, Pressed, Released) with no event queues, no heap allocation, and no per-frame iteration needed to compute transitions.

```
Down(k)     = m_Current[k]
Pressed(k)  = m_Current[k] && !m_Previous[k]
Released(k) = !m_Current[k] && m_Previous[k]
```

`Update()` MUST be called exactly once per frame at a well-defined boundary. Calling it more than once per frame would erase transition information. Calling it zero times per frame would cause `Pressed`/`Released` to never clear.

**Alternatives considered**: Single-array with a "changed this frame" bitset. Rejected: more complex with no meaningful benefit for Phase 1 scale. Event queue with per-frame drain. Rejected: unnecessary allocations and complexity for a single-threaded bootstrap.

---

## Summary of Key Decisions

| Topic | Decision |
|-------|----------|
| Repeat detection | lParam bit 30; ignore repeats in `OnKeyDown` |
| Alt key messages | Handle WM_SYSKEYDOWN/UP same as WM_KEYDOWN/UP |
| Wheel delta | Accumulate per frame; reset in `Update()`; positive = scroll up |
| Mouse capture | `SetCapture` on first button down; `ReleaseCapture` when all up |
| Focus loss | Handle both `WM_KILLFOCUS` and `WM_ACTIVATEAPP` |
| VK code range | 0–255 confirmed; `bool[256]` arrays are complete |
| Alt VK codes | `VK_MENU` → `Key::LAlt`; `VK_LMENU` / `VK_RMENU` mapped separately |
| State pattern | Two arrays (current + previous); `Update()` copies and resets |
