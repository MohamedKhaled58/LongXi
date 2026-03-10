# Implementation Plan: Input Foundation

**Branch**: `003-input-foundation` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/003-input-foundation/spec.md`

## Summary

Add a centralized `InputSystem` class to `LXEngine` that integrates with the existing `Application::WindowProc` message path to capture keyboard and mouse state, exposes `Down` / `Pressed` / `Released` transition queries via a typed `Key` and `MouseButton` enum surface, and advances frame-boundary state deterministically via `Application::Run`. No external dependencies are required — Win32 API message integration is the only input source.

## Technical Context

**Language/Version**: C++23, MSVC v145
**Primary Dependencies**: Win32 API (WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_MOUSEMOVE, WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN/UP, WM_MBUTTONDOWN/UP, WM_MOUSEWHEEL, WM_KILLFOCUS, WM_ACTIVATEAPP) — no external libraries
**Storage**: In-memory only — two bool arrays (256 entries each for keyboard, current + previous frame) plus three mouse button pairs and two int fields (position, wheel delta)
**Testing**: Manual verification via diagnostic log output — no automated test framework established
**Target Platform**: Windows x64 only
**Project Type**: Static library (`LXEngine`) integrated into a desktop game engine executable (`LXShell`)
**Performance Goals**: Input processing per frame is O(1) per query; full state copy at frame boundary is O(256) — negligible
**Constraints**: Single-threaded runtime (Phase 1 by constitutional rule); no raw input; no controller support; no text input; `LXShell` must remain unchanged
**Scale/Scope**: Single window, single player, keyboard + 3-button mouse

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Article | Requirement | Status |
|---------|-------------|--------|
| III — Platform | Windows-only, raw Win32 API | PASS — spec explicitly Win32 only |
| III — Module Structure | `LXCore` / `LXEngine` / `LXShell` static libs | PASS — InputSystem in `LXEngine` |
| III — Dependency Direction | `LXShell → LXEngine → LXCore` | PASS — no reverse dependencies introduced |
| III — Runtime Model | Central `Application` lifecycle owner | PASS — integrated into Initialize/Run/Shutdown |
| IV — Entrypoint Discipline | Entrypoints remain thin | PASS — `LXShell` unchanged; `Application` delegates |
| IV — Module Boundaries | No cross-layer coupling | PASS — InputSystem is self-contained, no renderer or gameplay deps |
| IV — No God Modules | No hidden global state | PASS — InputSystem is an owned subsystem, not a global singleton |
| IV — Abstraction Discipline | No speculative abstraction | PASS — concrete state-array implementation only |
| V — Client Architecture | Compatible with future server-authoritative multiplayer | PASS — input capture layer is decoupled from gameplay truth; client sends input to server, not the other way |
| IX — Single-Threaded | Phase 1 is single-threaded by rule | PASS — InputSystem is single-threaded, no locking introduced |
| IX — MT-Aware | Architecture ready for future threading | PASS — InputSystem is a contained object with no hidden global mutable state |
| XII — Phase 1 Scope | Input foundation is Phase 1 constitutional scope | PASS — "Main loop and time/input foundation" is explicitly listed |

**No violations. No complexity tracking required.**

## Project Structure

### Documentation (this feature)

```text
specs/003-input-foundation/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
LongXi/LXEngine/Src/
├── Input/
│   ├── InputSystem.h       # Key enum, MouseButton enum, InputSystem class declaration
│   └── InputSystem.cpp     # InputSystem implementation
├── Application/
│   ├── Application.h       # Add: m_InputSystem member, GetInput() accessor
│   └── Application.cpp     # Add: message routing, InputSystem::Update() in Run()
└── [all other existing files unchanged]

LongXi/LXEngine/
└── premake5.lua            # No change needed (Src/** glob already covers new files)
```

**Structure Decision**: New `Input/` subdirectory under `LXEngine/Src/` following the established pattern of `Application/`, `Window/`, `Renderer/`. Single class per responsibility. No contracts directory — `InputSystem` is an internal engine subsystem with no external interface surface (no RPC, no network protocol, no CLI).
