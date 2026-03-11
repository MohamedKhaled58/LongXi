# Implementation Plan: Engine Event Routing and Application Boundary Cleanup

**Branch**: `009-event-routing-refactor` | **Date**: 2025-03-10 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/009-event-routing-refactor/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Refactor the engine runtime architecture to establish clean separation between platform-specific code (Win32), application coordination, and core engine systems. The primary goal is to move VFS mounting configuration from Engine to Application, relocate Win32 message handling from Application to Win32Window, and introduce a callback-based event routing model that keeps Engine platform-agnostic.

Technical approach:
1. Extract VFS mount configuration from Engine::Initialize() into Application::ConfigureVirtualFileSystem()
2. Move Application::WindowProc static function into Win32Window as owned message handler
3. Add std::function callback members to Win32Window for each event type (resize, keyboard, mouse)
4. Wire callbacks in Application to forward events to Engine subsystems
5. Ensure Engine public interface uses platform-independent types

## Technical Context

**Language/Version**: C++23 (MSVC v145)
**Primary Dependencies**: None introduced (uses existing: Win32 API, DirectX 11, std::function)
**Storage**: Virtual File System (VFS) with directory and WDF archive mounting
**Testing**: Manual verification via test application; no automated test framework yet (Phase 1 scope)
**Target Platform**: Windows-only (Win32 API)
**Project Type**: Desktop engine/runtime library (static libraries: LXCore, LXEngine, LXShell)
**Performance Goals**: Event routing < 1ms for resize, < 100μs for input (SC-008, SC-009)
**Constraints**: Single-threaded runtime (Phase 1), no DLL architecture, static libraries only
**Scale/Scope**: 5 architectural boundary changes across 3 modules (Application, Win32Window, Engine)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Implementation Gate

✅ **Article III: Non-Negotiable Technical Baseline** - COMPLIANT
- Platform: Windows-only ✅
- Windowing: Raw Win32 API ✅
- Renderer: DirectX 11 ✅
- Build: Static libraries only (no DLLs) ✅
- Module structure: LXShell → LXEngine → LXCore ✅
- Single-threaded Phase 1 runtime ✅

✅ **Article IV: Architectural Laws** - COMPLIANT
- Thin entrypoints: Application::CreateEngine() delegates, doesn't implement ✅
- Module boundaries: No cross-layer coupling introduced ✅
- No god objects: Engine coordinates but doesn't own platform code ✅
- State ownership: Platform state in Win32Window, engine state in Engine ✅

✅ **Article IX: Threading and Runtime Discipline** - COMPLIANT
- Single-threaded Phase 1 runtime ✅
- MT-aware architecture: Callback model enables future threading ✅
- No premature threading ✅

✅ **Article XII: Phase 1 Constitutional Guardrails** - COMPLIANT
- Within Phase 1 scope: "Native Client Foundation" includes windowing, input, resource foundation ✅
- No excluded features (gameplay, multiplayer, etc.) ✅
- Architectural groundwork for future multiplayer: Event routing enables network-driven input ✅

### Post-Design Gate (to be re-checked after Phase 1)

⏳ **Article VII: Specification-First Delivery Principles** - PENDING
- Plan must trace back to spec requirements ✅ (FR-001 through FR-017)
- Implementation must maintain traceability ⏳ (verified during task execution)

⏳ **Article VIII: Change Control Principles** - PENDING
- Changes must remain focused on stated problem ⏳ (no scope creep)
- No unrelated edits or "cleanup" ⏳ (verified during task execution)

**Gate Result**: ✅ **PASSED** - No constitutional violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/009-event-routing-refactor/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
# Existing structure (this refactor modifies, doesn't create new structure)
LongXi/
├── LXCore/                     # Core static library (unchanged)
│   └── Src/
│       ├── Core/
│       │   └── FileSystem/     # VFS, ResourceSystem
│       └── Platform/           # Platform abstractions
├── LXEngine/                   # Engine static library (MODIFIED)
│   └── Src/
│       ├── Application/        # Application.cpp, Application.h (MODIFIED)
│       ├── Window/             # Win32Window.cpp, Win32Window.h (MODIFIED)
│       └── Engine/             # Engine.cpp, Engine.h (MODIFIED)
└── LXShell/                    # Shell executable (unchanged)
    └── Src/
        └── main.cpp
```

**Structure Decision**: Existing 3-module static library structure (LXCore, LXEngine, LXShell) is maintained. This refactor only modifies internal boundaries within LXEngine, moving VFS configuration and Win32 message handling to appropriate modules per architectural laws.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitutional violations. This section is not applicable.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
