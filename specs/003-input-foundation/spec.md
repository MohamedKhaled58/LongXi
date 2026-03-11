# Spec 003 — Input Foundation

**Feature Branch**: `003-input-foundation`
**Created**: 2026-03-10
**Status**: Accepted
**Input**: User description: "Input Foundation"

## Objective

Establish a minimal, stable Win32-integrated input foundation that captures keyboard and mouse state, updates deterministically within the existing application loop, and exposes a queryable runtime input state for later systems.

## Problem Statement

The shell and renderer are complete, but no formal input system exists. Win32 input messages are currently handled ad hoc within the message pump without any centralized capture or query surface. Later systems — camera, UI navigation, character movement, gameplay logic — cannot safely depend on scattered, unstructured Win32 message handling. A centralized input foundation must be established before any of those systems can be added.

## Scope

This specification includes only:

- Keyboard state capture through Win32 message integration
- Mouse button state capture through Win32 message integration
- Mouse position tracking in window/client space
- Mouse wheel delta capture
- Deterministic per-frame input state update and reset behavior
- A centralized engine-owned input system
- Query access for current input state suitable for later systems
- Focus-loss handling sufficient to avoid stuck input state
- Logging and diagnostic hooks only where needed for bootstrap visibility

## Out of Scope

This specification explicitly excludes:

- Gameplay action mapping
- Keybinding and remapping UI or configuration
- Controller and gamepad support
- Raw input (`WM_INPUT`)
- Text input and IME handling
- Character movement logic
- Camera control logic
- UI widgets and UI navigation
- Input recording and replay
- Network input transmission
- Multithreaded input handling

## User Scenarios & Testing

### User Story 1 — Keyboard State Query (Priority: P1)

A developer queries whether a specific keyboard key is currently held down, was pressed this frame, or was released this frame. The input foundation captures `WM_KEYDOWN` and `WM_KEYUP` messages through the existing Win32 message path and exposes a stable per-frame query interface that distinguishes held, pressed, and released states.

**Why this priority**: Keyboard state is the primary dependency for all movement, camera, and gameplay systems. Nothing can build on the input foundation without this.

**Independent Test**: Launch `LXShell.exe`. Add temporary diagnostic logging that queries a key state each frame. Press and release a key and verify that state transitions (pressed → held → released) appear in the log output.

**Acceptance Scenarios**:

1. **Given** the application is running, **When** a keyboard key is pressed, **Then** the input system reflects the key as pressed this frame and held on subsequent frames until released
2. **Given** a key is currently held, **When** the key is released, **Then** the input system reflects the key as released this frame and not held on subsequent frames
3. **Given** the application loses window focus, **When** focus returns, **Then** no keys remain stuck in a pressed or held state

---

### User Story 2 — Mouse State Query (Priority: P2)

A developer queries mouse button state, client-area cursor position, and wheel delta. The input foundation captures `WM_LBUTTONDOWN`, `WM_RBUTTONDOWN`, `WM_MOUSEMOVE`, `WM_MOUSEWHEEL`, and related messages, and exposes a stable per-frame query interface covering position, button state, and wheel delta.

**Why this priority**: Mouse state is required for UI, camera look, and scene interaction. It is secondary only because keyboard state is verified first.

**Independent Test**: Launch `LXShell.exe`. Add temporary diagnostic logging that queries mouse position, button state, and wheel delta each frame. Move the mouse, click buttons, and scroll the wheel. Verify that corresponding state changes appear in the log.

**Acceptance Scenarios**:

1. **Given** the application is running, **When** the mouse moves within the client area, **Then** the input system reflects the updated client-relative cursor position
2. **Given** the application is running, **When** a mouse button is pressed and released, **Then** the input system reflects the button as pressed this frame, held on subsequent frames, and released when the button is lifted
3. **Given** the application is running, **When** the mouse wheel is scrolled, **Then** the input system reflects a non-zero wheel delta this frame and resets to zero on the next frame

---

### Edge Cases

- What happens when a key is held while the application loses focus (alt-tab)?
- What happens when the mouse cursor leaves the client area while a mouse button is held?
- What happens when multiple keys are pressed or released in the same message pump cycle?
- What happens when `WM_MOUSEWHEEL` is received but no scroll accumulation is expected?
- What happens if `WM_SIZE` with zero dimensions is received while input is active?

## Requirements

### Functional Requirements

- **FR-001**: The engine SHALL provide a centralized input foundation owned by engine/runtime code (`LXEngine`) rather than `LXShell`
- **FR-002**: The input foundation SHALL integrate with the existing Win32 message path in `Application::WindowProc`
- **FR-003**: The input foundation SHALL track keyboard key down and up state for keys received through `WM_KEYDOWN` and `WM_KEYUP`
- **FR-004**: The input foundation SHALL track mouse button down and up state for the left, right, and middle mouse buttons received through `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, `WM_RBUTTONDOWN`, `WM_RBUTTONUP`, `WM_MBUTTONDOWN`, and `WM_MBUTTONUP`
- **FR-005**: The input foundation SHALL track mouse cursor position relative to the window client area from `WM_MOUSEMOVE`
- **FR-006**: The input foundation SHALL accumulate mouse wheel delta across all `WM_MOUSEWHEEL` messages received within a single frame, then reset the accumulated delta to zero at the per-frame update boundary
- **FR-007**: The runtime SHALL call a per-frame input update operation at a deterministic point in the main loop to advance frame-boundary state transitions (Pressed → Down, Released → not-held, wheel delta reset to zero)
- **FR-008**: The input foundation SHALL expose a `Down` query returning whether a key or button is currently held this frame
- **FR-009**: The input foundation SHALL expose a `Pressed` query returning true only on the frame the key or button transitions from up to down, and a `Released` query returning true only on the frame the key or button transitions from down to up
- **FR-010**: Input state SHALL be cleared or normalized appropriately when window focus is lost (`WM_KILLFOCUS` or `WM_ACTIVATEAPP`), to prevent stuck pressed-state behavior
- **FR-011**: Input handling SHALL remain decoupled from gameplay logic, rendering logic, and UI logic
- **FR-012**: The input foundation SHALL integrate into the existing `Application` lifecycle (Initialize / Run / Shutdown) rather than bypassing it

### Non-Functional Requirements

- **NFR-001**: The input foundation SHALL have a maintainable structure with clear separation between message integration, per-frame state management, and query interface
- **NFR-002**: Ownership of all input state SHALL be explicit — no global input state accessible without going through the input system's defined interface
- **NFR-003**: Per-frame input state transitions SHALL be deterministic — given the same sequence of Win32 messages, the same frame-by-frame state transitions SHALL always result
- **NFR-004**: The input foundation SHALL be debuggable — diagnostic logging SHALL be available for initialization and focus-loss events at minimum
- **NFR-005**: Code changes SHALL be reviewable as incremental additions to the existing Spec 002 codebase without requiring modification of `LXShell` or the renderer
- **NFR-006**: The input foundation design SHALL be compatible with later input binding and gameplay systems without requiring fundamental redesign of the capture or query layer
- **NFR-007**: No speculative high-level input abstraction (action maps, input contexts, binding tables, event buses) SHALL be introduced beyond the current bootstrap need

### Key Entities

- **Input System**: The centralized engine object responsible for receiving Win32 input messages, managing per-frame state transitions, and exposing query operations
- **Key State**: Per-key boolean tracking covering current-frame pressed, held, and released transitions
- **Mouse Button State**: Per-button boolean tracking covering current-frame pressed, held, and released transitions
- **Mouse Position**: Integer client-area coordinates updated from `WM_MOUSEMOVE`
- **Wheel Delta**: Signed integer delta accumulated across all `WM_MOUSEWHEEL` messages within a frame and reset to zero at the per-frame update boundary

## Constraints

- Windows-only platform
- Raw Win32 message integration — no abstraction layer between Win32 messages and the input system
- Existing shell and bootstrap architecture (Spec 001, Spec 002) must remain intact and unmodified in observable behavior
- Dependency direction: `LXShell → LXEngine → LXCore`
- `Application` remains the central lifecycle owner
- Single-threaded runtime for this stage — input capture and query are on the same thread
- Input foundation must not assume gameplay systems, asset systems, or scene systems exist
- Input must remain an engine/runtime concern — `LXShell/Src/main.cpp` must not grow beyond its current `CreateApplication()` function

## Deliverables

- Centralized engine input foundation (`InputSystem` or equivalent) owned by `LXEngine`
- Win32-integrated keyboard state capture and per-frame transition management
- Win32-integrated mouse button state capture and per-frame transition management
- Client-space cursor position tracking
- Mouse wheel delta capture and per-frame reset
- Deterministic per-frame input update boundary integrated into `Application::Run`
- Focus-loss state reset handling integrated into `Application::WindowProc`
- Query interface suitable for later systems to read key state, mouse state, cursor position, and wheel delta

## Success Criteria

### Measurable Outcomes

- **SC-001**: The application continues to launch successfully with the input foundation integrated — no regression in window creation, renderer initialization, or clean shutdown
- **SC-002**: Keyboard key presses and releases produce correct per-frame state transitions observable through the input query interface
- **SC-003**: Mouse button presses and releases produce correct per-frame state transitions observable through the input query interface
- **SC-004**: Mouse cursor position updates are available through the input query interface after each mouse movement
- **SC-005**: Mouse wheel scrolling produces an observable non-zero delta through the input query interface on the frame it occurs, and a zero delta on subsequent frames without scrolling
- **SC-006**: Losing window focus does not leave any key or mouse button in a stuck active state when focus is later restored
- **SC-007**: Input lifecycle ownership remains centered in engine/runtime code — `LXShell` is not modified
- **SC-008**: Later systems can query key and mouse state through the input interface without directly reading Win32 messages or accessing `WindowProc`

## Acceptance Criteria

- **AC-001**: `LXShell.exe` builds without errors in Debug, Release, and Dist configurations with the input foundation integrated
- **AC-002**: Pressing and releasing a keyboard key results in corresponding pressed/held/released state transitions observable through the input query surface (verified via diagnostic log output)
- **AC-003**: Pressing and releasing mouse buttons results in corresponding pressed/held/released state transitions observable through the input query surface (verified via diagnostic log output)
- **AC-004**: Moving the mouse within the client area updates client-relative cursor position values exposed by the input system (verified via diagnostic log output)
- **AC-005**: Scrolling the mouse wheel produces a non-zero wheel delta observable through the input system this frame, which resets to zero on the next frame without scrolling (verified via diagnostic log output)
- **AC-006**: Alt-tabbing away from and returning to the application does not leave any previously pressed key or mouse button stuck in an active state
- **AC-007**: `LXShell/Src/main.cpp` remains unchanged from Spec 002 — still only `CreateApplication()`
- **AC-008**: The input foundation integrates into the existing `Application` lifecycle without restructuring `Application`, `Win32Window`, or the renderer

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Win32 input message handling scattered across `Application`, `Win32Window`, or `LXShell` instead of a centralized input system | Medium | High | Route all input messages through a single `InputSystem::OnMessage` dispatch point; do not handle input ad hoc in WindowProc |
| Input state coupled to gameplay or renderer code | Low | High | Keep the input system a self-contained engine object with no dependencies on renderer or gameplay types |
| Unclear frame-boundary behavior for pressed/released transitions causing state desync | High | Medium | Define and document the exact per-frame update contract: messages update raw state; `Update()` advances transitions; query reflects post-Update state |
| Focus-loss edge cases leaving stale key or button state | Medium | High | Handle `WM_KILLFOCUS` and `WM_ACTIVATEAPP` to clear all active state unconditionally |
| Overengineering the input layer with action maps, event buses, or abstract interfaces before real gameplay needs exist | Medium | Medium | Restrict this spec to concrete state-array implementation with direct query functions — no abstraction beyond what is needed |

## Assumptions

- Spec 001 shell bootstrap is complete and stable — window creation, message pump, lifecycle management, and clean shutdown are verified
- Spec 002 DX11 render bootstrap is complete and stable — clear/present frame path and resize handling are verified
- Later specs will build gameplay, camera, and UI behavior on top of this input foundation
- The current goal is input capture and query foundation only — no player-control or binding behavior is part of this spec
- The application runs on a system with a standard keyboard and mouse — exotic input devices are not considered

## Resolved Questions

- **OQ-001**: Mouse buttons in scope for this bootstrap are left, right, and middle only. Side buttons (X1, X2) are deferred to a later spec. _Resolved: User confirmed._
- **OQ-002**: Wheel delta SHALL be accumulated across all `WM_MOUSEWHEEL` messages within a frame and reset to zero at the per-frame update boundary. Last-delta-only is explicitly rejected. _Resolved: User confirmed._
- **OQ-003**: Key and mouse button state queries SHALL use a strongly typed `Key` enum and `MouseButton` enum defined in `LXEngine`. Raw Win32 virtual key codes are not exposed through the public query interface. _Resolved: User confirmed (clean Down/Pressed/Released API implies typed surface)._

## Boundary Note

This specification intentionally establishes only the minimal Win32 input capture and query foundation. Input binding tables, action mapping, gameplay control schemes, UI interaction semantics, controller support, and all higher-level input abstractions are expected to be addressed by later separate specifications. This spec is deliberately narrow to ensure a stable, reviewable input capture layer exists before adding complexity.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
