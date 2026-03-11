# Implementation Plan: Camera System

**Branch**: `012-camera-system` | **Date**: 2026-03-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/012-camera-system/spec.md`

## Summary

Add a Scene-owned single active camera that provides view and perspective projection matrices to the renderer each frame. The implementation will:

1. Introduce a `Camera` class under `LXEngine/Src/Scene/`
2. Add `Matrix4` to shared math types
3. Integrate camera lifecycle and resize flow into `Scene`
4. Add consume-only `DX11Renderer::SetViewProjection(...)` API
5. Apply dirty-flag matrix recomputation policy (sync once before render submit)
6. Enforce clamped projection parameter validation with warning logs

This plan preserves the current runtime architecture and constitutional constraints (Windows-only, DX11, static libs, single-threaded Phase 1).

## Technical Context

**Language/Version**: C++23 (MSVC v145, Premake5 VS2026 generator)  
**Primary Dependencies**: Win32 API, DirectX 11, C++ STL, existing `Math.h` POD types  
**Storage**: N/A (runtime memory only; no persistence introduced)  
**Testing**: Manual runtime verification + log assertions via LXShell (no automated harness in Phase 1)  
**Target Platform**: Windows x64 desktop  
**Project Type**: Native desktop engine/runtime (static libraries: `LXCore`, `LXEngine`, `LXShell`)  
**Performance Goals**: Maintain frame pacing at 60 FPS; camera matrix sync adds no per-frame heap allocations; matrix sync cost negligible vs frame budget  
**Constraints**: Single-threaded runtime; renderer consume-only camera contract; no Scene direct draw calls; module dependency direction unchanged (`LXShell -> LXEngine -> LXCore`)  
**Scale/Scope**: Single active camera, one scene integration path, one renderer matrix ingress path

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Phase 0 Gate

✅ **Article III: Non-Negotiable Technical Baseline** - COMPLIANT  
- Windows-only + Win32 + DX11 preserved  
- Static library module topology preserved  
- C++23 + current toolchain preserved

✅ **Article IV: Architectural Laws** - COMPLIANT  
- Scene remains orchestration-only (no direct draw calls)  
- Renderer consumes matrices; gameplay/world truth stays outside renderer  
- No new cross-layer dependency violations

✅ **Article V: Multiplayer and State Ownership Principles** - COMPLIANT  
- Camera state remains presentation/view concern  
- No client-authoritative gameplay truth introduced  
- Scene-owned camera keeps future server-authoritative integration feasible

✅ **Article IX: Threading and Runtime Discipline** - COMPLIANT  
- Main-thread updates only  
- Dirty-flag model is MT-aware but no premature threading added

✅ **Article XII: Phase 1 Constitutional Guardrails** - COMPLIANT  
- Feature is part of native client foundation and rendering pipeline maturation  
- No gameplay/combat/full multiplayer scope creep

**Gate Result (Pre-Phase 0)**: ✅ PASS

### Post-Phase 1 Re-Check

✅ **Article VII: Specification-First Delivery Principles** - COMPLIANT  
- `research.md`, `data-model.md`, `contracts/`, and `quickstart.md` all trace directly to FR/SC requirements in `spec.md`

✅ **Article VIII: Change Control Principles** - COMPLIANT  
- Planned edits constrained to camera/math/scene/renderer integration files only  
- No unrelated subsystem rewrites

✅ **Article X & XI: Documentation + Verification Honesty** - COMPLIANT  
- Decisions documented with alternatives  
- Manual validation scope explicitly stated (no false automated test claims)

**Gate Result (Post-Phase 1)**: ✅ PASS

## Project Structure

### Documentation (this feature)

```text
specs/012-camera-system/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── camera-scene-renderer.md
└── tasks.md
```

### Source Code (repository root)

```text
LongXi/
├── LXEngine/
│   ├── LXEngine.h                                # Public include surface (modified)
│   └── Src/
│       ├── Math/
│       │   └── Math.h                            # Matrix4 added (modified)
│       ├── Scene/
│       │   ├── Camera.h                          # New
│       │   ├── Camera.cpp                        # New
│       │   ├── Scene.h                           # Camera ownership + API (modified)
│       │   └── Scene.cpp                         # init/render/resize camera flow (modified)
│       └── Renderer/
│           ├── DX11Renderer.h                    # SetViewProjection + viewport accessors (modified)
│           └── DX11Renderer.cpp                  # matrix consume path + resize state (modified)
└── LXShell/
    └── Src/
        └── main.cpp                              # Optional manual validation scaffolding only
```

**Structure Decision**: Existing native engine structure is retained. Implementation is localized to `LXEngine` scene/renderer/math boundaries with no module topology changes.

## Phase Outputs

### Phase 0 - Research

- Completed: [research.md](./research.md)
- All planning uncertainties resolved without open `NEEDS CLARIFICATION` items.

### Phase 1 - Design & Contracts

- Completed: [data-model.md](./data-model.md)
- Completed: [contracts/camera-scene-renderer.md](./contracts/camera-scene-renderer.md)
- Completed: [quickstart.md](./quickstart.md)
- Agent context refresh executed via `.specify/scripts/powershell/update-agent-context.ps1 -AgentType codex`

### Phase 2 - Planning Handoff

- `tasks.md` is intentionally not generated by this command and is the next step (`/speckit.tasks`).

## Complexity Tracking

No constitutional violations requiring justification.
