# Implementation Plan: Developer Integration Layer (LXShell + Dear ImGui)

**Branch**: `013-developer-integration-layer` | **Date**: 2026-03-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/013-developer-integration-layer/spec.md`

## Summary

Introduce a development-only DebugUI integration layer in LXShell using Dear ImGui to validate engine runtime behavior without creating any Engine dependency on UI frameworks. The implementation plan will:

1. Add a dedicated `DebugUI` subsystem under `LXShell/Src/DebugUI/`.
2. Integrate Dear ImGui as a Git submodule at `Vendor/imgui` and compile required ImGui sources in LXShell only.
3. Wire DebugUI lifecycle to Application runtime flow (initialize, frame render, resize, input routing, shutdown).
4. Implement five developer panels: EnginePanel, SceneInspector, TextureViewer, CameraPanel, InputMonitor.
5. Add a development test rendering scene path in LXShell for cross-subsystem validation.
6. Enforce build gating so DebugUI compiles only in development/debug builds.

This approach preserves architectural boundaries: `LXShell -> LXEngine -> LXCore`, with no reverse dependency from Engine to DebugUI/ImGui.

## Technical Context

**Language/Version**: C++23 (MSVC v145, Premake5 VS2026 generator)
**Primary Dependencies**: Win32 API, DirectX 11, Dear ImGui (core + Win32/DX11 backends), C++ STL
**Storage**: N/A (runtime-only debug state; no persistence in this spec)
**Testing**: Manual runtime verification + log assertions + development build/production build comparison
**Target Platform**: Windows x64 desktop
**Project Type**: Native desktop client/runtime (`LXCore` static lib, `LXEngine` static lib, `LXShell` windowed app)
**Performance Goals**: Maintain interactive runtime target (60 FPS baseline) with DebugUI enabled; keep per-frame DebugUI overhead stable and bounded
**Constraints**: Engine must remain ImGui-agnostic; ImGui code limited to LXShell; input consumed by DebugUI must not reach InputSystem; DebugUI must render before frame present; single-threaded Phase 1 runtime discipline
**Scale/Scope**: One DebugUI subsystem, one active panel set, five initial panels, one validation scene path

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Phase 0 Gate

PASS **Article III: Non-Negotiable Technical Baseline**
- Windows-only + Win32 + DX11 baseline preserved.
- Premake + static library architecture preserved.
- C++23 + current toolchain preserved.

PASS **Article IV: Architectural Laws**
- Engine remains presentation-framework agnostic.
- Dependency direction remains `LXShell -> LXEngine -> LXCore`.
- DebugUI concerns isolated in LXShell.

PASS **Article V: Multiplayer and State Ownership Principles**
- DebugUI is observational/diagnostic only and does not own gameplay truth.
- No client-authoritative gameplay state is introduced.

PASS **Article IX: Threading and Runtime Discipline**
- DebugUI updates/rendering remain on main thread.
- No premature threading complexity is introduced.

PASS **Article XII: Phase 1 Constitutional Guardrails**
- Feature strengthens native client foundation and debugging infrastructure.
- No gameplay/combat/full multiplayer scope creep.

**Gate Result (Pre-Phase 0)**: PASS

### Post-Phase 1 Re-Check

PASS **Article VII: Specification-First Delivery Principles**
- `research.md`, `data-model.md`, `contracts/`, and `quickstart.md` trace directly to FR/SC requirements in `spec.md`.

PASS **Article VIII: Change Control Principles**
- Planned implementation scope remains focused on LXShell integration + minimal safe integration touchpoints.
- No unrelated subsystem rewrites are required.

PASS **Article X and XI: Documentation and Verification Honesty**
- Design decisions and alternatives documented.
- Validation approach explicitly states manual/runtime checks and build-scope checks.

**Gate Result (Post-Phase 1)**: PASS

## Project Structure

### Documentation (this feature)

```text
specs/013-developer-integration-layer/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- debugui-engine-runtime-contract.md
`-- tasks.md
```

### Source Code (repository root)

```text
LongXi/
|-- LXShell/
|   |-- premake5.lua                                 # ImGui sources compiled in LXShell only (modified)
|   `-- Src/
|       |-- main.cpp                                 # DebugUI lifecycle wiring + validation scene bootstrap (modified)
|       `-- DebugUI/
|           |-- DebugUI.h                            # New
|           |-- DebugUI.cpp                          # New
|           `-- Panels/
|               |-- EnginePanel.h/.cpp              # New
|               |-- SceneInspector.h/.cpp           # New
|               |-- TextureViewer.h/.cpp            # New
|               |-- CameraPanel.h/.cpp              # New
|               `-- InputMonitor.h/.cpp             # New
|-- LXEngine/
|   `-- Src/                                         # Consumed via interfaces only; no ImGui includes
`-- Vendor/
    `-- imgui/                                       # Git submodule
        |-- imgui.cpp
        |-- imgui_draw.cpp
        |-- imgui_tables.cpp
        |-- imgui_widgets.cpp
        |-- imgui_demo.cpp
        `-- backends/
            |-- imgui_impl_win32.cpp
            `-- imgui_impl_dx11.cpp
```

**Structure Decision**: Keep implementation localized to LXShell and vendor dependency wiring. Engine remains framework-agnostic and accessed through existing/public interfaces.

## Phase Outputs

### Phase 0 - Research

- Completed: [research.md](./research.md)
- All planning uncertainties resolved with no open `NEEDS CLARIFICATION` items.

### Phase 1 - Design and Contracts

- Completed: [data-model.md](./data-model.md)
- Completed: [contracts/debugui-engine-runtime-contract.md](./contracts/debugui-engine-runtime-contract.md)
- Completed: [quickstart.md](./quickstart.md)
- Agent context update executed via `.specify/scripts/powershell/update-agent-context.ps1 -AgentType codex`

### Phase 2 - Planning Handoff

- `tasks.md` is intentionally not generated by this command and is the next step (`/speckit.tasks`).

## Complexity Tracking

No constitutional violations requiring justification.
