# Tasks: Input Foundation

**Input**: Design documents from `/specs/003-input-foundation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: No automated tests requested. Verification is manual (build, launch, diagnostic log inspection, focus-loss test, shutdown).

**Organization**: Two user stories (US1 — Keyboard State Query, P1; US2 — Mouse State Query, P2). Phase 2 foundational tasks (header + Application.h changes) block both stories.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2)
- Exact file paths included in descriptions

---

## Phase 1: Setup (Build System)

**Purpose**: Confirm no build system changes are required before adding source files

- [x] T001 Verify `LongXi/LXEngine/premake5.lua` — confirm `Src/**.h` and `Src/**.cpp` globs already cover the new `Input/` subdirectory; no edits needed

**Checkpoint**: Build system confirmed. New files placed under `LongXi/LXEngine/Src/Input/` will be auto-included on next project regeneration.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish the complete `InputSystem` header and `Application.h` changes — required by both user stories before any `.cpp` work begins

**⚠️ CRITICAL**: US1 and US2 implementation cannot begin until this phase is complete

- [x] T002 [P] Create `LongXi/LXEngine/Src/Input/InputSystem.h` — declare `Key` enum (`enum class Key : uint8_t`, full set: A–Z, D0–D9, F1–F12, arrows, navigation, common, modifiers, locks, numpad, punctuation, `Count` sentinel), `MouseButton` enum (`enum class MouseButton : uint8_t` with Left/Right/Middle/Count), and full `InputSystem` class with all member variable declarations (`m_KeyCurrent[256]`, `m_KeyPrevious[256]`, `m_MouseCurrent[3]`, `m_MousePrevious[3]`, `m_MouseX`, `m_MouseY`, `m_WheelDelta`) and all method signatures (lifecycle, message handlers, query methods)
- [x] T003 [P] Modify `LongXi/LXEngine/Src/Application/Application.h` — add `#include "Input/InputSystem.h"`, add `std::unique_ptr<InputSystem> m_InputSystem` private member, add `const InputSystem& GetInput() const` public accessor, add `bool CreateInputSystem()` private method declaration

**Checkpoint**: Header and Application.h compile cleanly. Both user stories can now begin independently.

---

## Phase 3: User Story 1 — Keyboard State Query (Priority: P1) 🎯 MVP

**Goal**: Capture keyboard key state through Win32 message integration, expose Down/Pressed/Released transition queries via typed `Key` enum, integrate into Application lifecycle, handle focus-loss clearing.

**Independent Test**: Launch `LXShell.exe`. Press Space — verify `Space PRESSED` appears in console. Hold Space — verify no repeated `PRESSED` lines (auto-repeat filtered). Release Space — verify `Space RELEASED` appears. Alt-tab away and return — verify no keys stuck. Close window — verify clean exit.

### Implementation for User Story 1

- [x] T004 [US1] Implement `InputSystem::Initialize()` in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — zero-initialize `m_KeyCurrent`, `m_KeyPrevious`, `m_MouseCurrent`, `m_MousePrevious`, `m_MouseX`, `m_MouseY`, `m_WheelDelta`; build `s_KeyToVK[(size_t)Key::Count]` and `s_VKToKey[256]` static translation tables mapping every `Key` enum value to its Win32 VK code and back (unmapped VK codes → `Key::Unknown`)
- [x] T005 [US1] Implement `InputSystem::OnKeyDown(UINT vk, bool isRepeat)` and `InputSystem::OnKeyUp(UINT vk)` in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — `OnKeyDown` maps VK via `s_VKToKey`, sets `m_KeyCurrent[vk] = true` only when `!isRepeat`; `OnKeyUp` maps VK and sets `m_KeyCurrent[vk] = false`
- [x] T006 [US1] Implement `IsKeyDown(Key)`, `IsKeyPressed(Key)`, `IsKeyReleased(Key)` query methods in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — translate `Key` to VK via `s_KeyToVK`; `Down = m_KeyCurrent[vk]`; `Pressed = m_KeyCurrent[vk] && !m_KeyPrevious[vk]`; `Released = !m_KeyCurrent[vk] && m_KeyPrevious[vk]`
- [x] T007 [US1] Implement `InputSystem::Update()` in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — `memcpy(m_KeyPrevious, m_KeyCurrent, sizeof(m_KeyCurrent))`; `memcpy(m_MousePrevious, m_MouseCurrent, sizeof(m_MouseCurrent))`; `m_WheelDelta = 0`
- [x] T008 [US1] Implement `InputSystem::OnFocusLost()` in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — `memset(m_KeyCurrent, 0, sizeof(m_KeyCurrent))`; `memset(m_MouseCurrent, 0, sizeof(m_MouseCurrent))`; and `InputSystem::Shutdown()` as a no-op with an `LX_ENGINE_INFO("Input system shutdown")` log
- [x] T009 [US1] Implement `Application::CreateInputSystem()` in `LongXi/LXEngine/Src/Application/Application.cpp` — instantiate `m_InputSystem = std::make_unique<InputSystem>()`, call `m_InputSystem->Initialize()`, log `LX_ENGINE_INFO("Input system initialized")`; call `CreateInputSystem()` from `Application::Initialize()` after `CreateRenderer()`
- [x] T010 [US1] Add keyboard message cases to `Application::WindowProc` in `LongXi/LXEngine/Src/Application/Application.cpp` — handle `WM_KEYDOWN` and `WM_SYSKEYDOWN` (extract vk = `(UINT)wParam`, isRepeat = `(lParam & 0x40000000) != 0`, call `app->m_InputSystem->OnKeyDown(vk, isRepeat)`, return 0); handle `WM_KEYUP` and `WM_SYSKEYUP` (call `app->m_InputSystem->OnKeyUp((UINT)wParam)`, return 0); handle `WM_KILLFOCUS` and `WM_ACTIVATEAPP` (call `OnFocusLost()` when `wParam == 0` for ACTIVATEAPP)
- [x] T011 [US1] Integrate `InputSystem` into `Application::Run()` and `Application::Shutdown()` in `LongXi/LXEngine/Src/Application/Application.cpp` — call `m_InputSystem->Update()` at the start of each frame iteration before `BeginFrame()`; call `m_InputSystem->Shutdown()` and `m_InputSystem.reset()` in `Shutdown()` before renderer shutdown
- [x] T012 [US1] Add temporary diagnostic log lines to `Application::Run()` in `LongXi/LXEngine/Src/Application/Application.cpp` — after `Update()`, log `LX_ENGINE_INFO("Space PRESSED")` when `m_InputSystem->IsKeyPressed(Key::Space)`, `LX_ENGINE_INFO("Space RELEASED")` when `m_InputSystem->IsKeyReleased(Key::Space)`, to enable US1 manual verification

**Checkpoint**: Keyboard input fully integrated. Application clears to cornflower blue, keyboard events appear in console log, focus-loss clears state, auto-repeat filtered.

---

## Phase 4: User Story 2 — Mouse State Query (Priority: P2)

**Goal**: Capture mouse button state, client-space cursor position, and wheel delta through Win32 message integration, expose Down/Pressed/Released queries for buttons and position/delta accessors, integrate into existing Application::WindowProc.

**Independent Test**: Launch `LXShell.exe`. Click left mouse button — verify `LMB PRESSED` then `LMB RELEASED` in console. Move mouse — verify X/Y position updates in console. Scroll wheel — verify non-zero delta this frame then zero on next frame. Alt-tab with left button held — verify button unsticks on return.

### Implementation for User Story 2

- [x] T013 [US2] Implement mouse message handlers in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — `OnMouseMove(int x, int y)`: update `m_MouseX`, `m_MouseY`; `OnMouseButtonDown(MouseButton button)`: set `m_MouseCurrent[(size_t)button] = true`; `OnMouseButtonUp(MouseButton button)`: set `m_MouseCurrent[(size_t)button] = false`; `OnMouseWheel(int delta)`: `m_WheelDelta += delta`
- [x] T014 [US2] Implement mouse query methods in `LongXi/LXEngine/Src/Input/InputSystem.cpp` — `IsMouseButtonDown`, `IsMouseButtonPressed`, `IsMouseButtonReleased` (same Down/Pressed/Released pattern as keys); `GetMouseX()`, `GetMouseY()`, `GetWheelDelta()` returning member values
- [x] T015 [US2] Add mouse message cases to `Application::WindowProc` in `LongXi/LXEngine/Src/Application/Application.cpp` — `WM_MOUSEMOVE`: call `app->m_InputSystem->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))`; `WM_LBUTTONDOWN/RBUTTONDOWN/MBUTTONDOWN`: call `SetCapture(hwnd)`, call `OnMouseButtonDown(MouseButton::Left/Right/Middle)`; `WM_LBUTTONUP/RBUTTONUP/MBUTTONUP`: call `OnMouseButtonUp(MouseButton::Left/Right/Middle)`, call `ReleaseCapture()`; `WM_MOUSEWHEEL`: call `OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam))`; add `#include <windowsx.h>` for `GET_X_LPARAM`, `GET_Y_LPARAM`, `GET_WHEEL_DELTA_WPARAM` macros
- [x] T016 [US2] Add temporary diagnostic log lines to `Application::Run()` in `LongXi/LXEngine/Src/Application/Application.cpp` — log `LMB PRESSED/RELEASED`, mouse X/Y on move (throttled by change), and wheel delta when non-zero, to enable US2 manual verification

**Checkpoint**: Full input foundation integrated. Keyboard and mouse state both queryable. Focus-loss clears all state. Application exits cleanly.

---

## Phase 5: Verification & Polish

**Purpose**: Build all configurations, manual verification, cleanup, and documentation

- [x] T017 Regenerate Visual Studio project files via `Win-Generate Project.bat` to add `LongXi/LXEngine/Src/Input/InputSystem.h` and `InputSystem.cpp` to `LongXi/LXEngine/LXEngine.vcxproj`
- [x] T018 Build Debug|x64 — verify 0 errors, 0 warnings across LXCore, LXEngine, LXShell
- [x] T019 Build Release|x64 — verify 0 errors
- [x] T020 Build Dist|x64 — verify 0 errors
- [x] T021 Manual test: launch LXShell.exe (Debug), press and release Space — verify `Space PRESSED` then `Space RELEASED` in console
- [x] T022 Manual test: hold Space — verify `PRESSED` fires only once on first frame (auto-repeat bit-30 filtering confirmed), subsequent frames show no repeat `PRESSED`
- [x] T023 Manual test: alt-tab away and return — verify no key or button remains stuck as active
- [x] T024 Manual test: click left mouse button, release — verify `LMB PRESSED` → `LMB RELEASED` in console
- [x] T025 Manual test: move mouse within window — verify X/Y position updates appear in console
- [x] T026 Manual test: scroll mouse wheel — verify non-zero wheel delta this frame, zero delta on next frame
- [x] T027 Manual test: press left mouse button, drag cursor outside window, release outside — verify `LMB RELEASED` is still received (SetCapture confirmed working)
- [x] T028 Manual test: close window — verify clean exit with no DX11 debug layer warnings and no input-related errors in console
- [x] T029 Verify `LXShell/Src/main.cpp` remains unchanged from Spec 002 (still only `CreateApplication()`)
- [x] T030 Remove all diagnostic keyboard and mouse log lines added in T012 and T016 from `LongXi/LXEngine/Src/Application/Application.cpp`
- [x] T031 Run `Win-Format Code.bat` to format all C++ source files
- [x] T032 Update `specs/003-input-foundation/spec.md` status from `Draft` to `Accepted`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — header and Application.h changes block both user stories
- **User Story 1 (Phase 3)**: Depends on Phase 2 — keyboard methods + Application.cpp integration
- **User Story 2 (Phase 4)**: Depends on Phase 2 — can technically begin independently of US1 for InputSystem.cpp mouse methods, but Application.cpp changes are sequential
- **Verification (Phase 5)**: Depends on Phases 3 and 4 — all code must be written before build/test

### Within User Story 1

```text
T002 (InputSystem.h) ────────┐
T003 (Application.h) ────────┤
                              ├── T004 (Initialize + VK tables)
                              ├── T005 (OnKeyDown/OnKeyUp)
                              ├── T006 (IsKeyDown/Pressed/Released)
                              ├── T007 (Update)
                              ├── T008 (OnFocusLost + Shutdown)
                              ├── T009 (Application::CreateInputSystem)
                              ├── T010 (Application::WindowProc keyboard cases)
                              ├── T011 (Run + Shutdown integration)
                              └── T012 (diagnostic logging)
```

### Within User Story 2

```text
T002 (InputSystem.h) ────────┐
T003 (Application.h) ────────┤
                              ├── T013 (mouse message handlers in InputSystem.cpp)
                              ├── T014 (mouse query methods in InputSystem.cpp)
                              ├── T015 (Application::WindowProc mouse cases)
                              └── T016 (diagnostic logging)
```

### Parallel Opportunities

- T002 (InputSystem.h) and T003 (Application.h) can run in parallel — different files
- T004–T008 (InputSystem.cpp keyboard methods) are sequential — same file
- T009–T012 (Application.cpp integration) are sequential — same file
- T013–T014 (InputSystem.cpp mouse methods) are sequential — same file, but can overlap with T009–T012 on Application.cpp if needed since they edit different functions
- T017 (project regen) must come after all new files are created
- T018–T020 (three build configs) are sequential
- T021–T029 (manual tests) can be run as a single session

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001)
2. Complete Phase 2: Foundational (T002–T003)
3. Complete Phase 3: User Story 1 (T004–T012)
4. **STOP and VALIDATE**: Build, launch, test keyboard (T017–T023)
5. Proceed to User Story 2 if keyboard verification passes

### Single Developer Execution

All phases are sequential for a single developer. Estimated scope: T001–T016 implementation, T017–T032 verification and polish. Total: 32 tasks.

---

## Notes

- [P] tasks = different files, no dependencies
- [US1] / [US2] labels map tasks to specific user stories for traceability
- No automated test tasks — spec does not request automated tests
- `SetCapture` / `ReleaseCapture` are called in `Application::WindowProc`, not in `InputSystem` — keeps Win32 message routing concerns in Application, state management in InputSystem
- `#include <windowsx.h>` required in `Application.cpp` for `GET_X_LPARAM`, `GET_Y_LPARAM`, `GET_WHEEL_DELTA_WPARAM`
- Diagnostic log lines (T012, T016) are temporary — must be removed in T030 before committing
- Commit after each logical group (Phase 2, Phase 3, Phase 4, Phase 5)
- All DX11 Spec 002 behavior (cornflower blue clear, resize, VSync) must remain unaffected
