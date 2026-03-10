# Implementation Plan: Engine Core Refactor and Engine Service Access

**Branch**: `008-engine-core-refactor` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/008-engine-core-refactor/spec.md`

## Summary

Introduce a central **Engine** runtime object that owns and coordinates all engine subsystems (Renderer, InputSystem, VirtualFileSystem, TextureManager), removing subsystem ownership from Application and eliminating global variables through controlled service access via getter methods.

**Technical approach**: Pure architectural refactor - no new features. Move subsystem ownership from Application to Engine, provide Engine getter methods for service access, maintain single-threaded initialization and runtime.

## Technical Context

**Language/Version**: C++23 (MSVC v145) - per Article III
**Primary Dependencies**: DirectX 11, Win32 API, Spdlog (via LXCore)
**Storage**: N/A (in-memory object ownership)
**Testing**: Manual verification only (per Assumption #11 in spec)
**Target Platform**: Windows-only (per Article III)
**Project Type**: Desktop application with static library architecture
**Performance Goals**: <100ms initialization (per SC-004), zero performance regression (per Assumption #12)
**Constraints**: Single-threaded runtime (Article IX), static libraries only (Article III), no DLLs in Phase 1
**Scale/Scope**: 4 subsystems (Renderer, InputSystem, VFS, TextureManager), Application reduced from 6 to 2 members (per SC-001)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### ✅ Compliance Verification

**Article III: Non-Negotiable Technical Baseline**
- ✅ Platform: Windows-only - maintained
- ✅ Windowing: Raw Win32 API - maintained
- ✅ Renderer: DirectX 11 - maintained
- ✅ Build System: Premake5 generating Visual Studio 2026 projects - maintained
- ✅ Toolchain: MSVC v145, C++23 standard - maintained
- ✅ Build approach: Static libraries only - maintained
- ✅ Module organization: LXCore/LXEngine/LXShell - maintained
- ✅ Dependency direction: LXShell → LXEngine → LXCore - maintained
- ✅ Phase 1 runtime: Single-threaded execution - maintained

**Article IV: Architectural Laws**
- ✅ Entrypoint Discipline: Application remains thin - delegates to Engine
- ✅ Module Boundaries: No circular dependencies introduced
- ✅ State and Coupling: Eliminates global state (spec FR-010), no god modules
- ✅ Ownership and Authority: Engine owns runtime systems, not rendering/gameplay truth
- ✅ Abstraction Discipline: No premature abstraction - concrete refactor for proven need

**Article IX: Threading and Runtime Discipline**
- ✅ Phase 1 Runtime Policy: Single-threaded by rule (spec FR-014)
- ✅ MT-Aware Architecture: Reference passing (Engine&) remains compatible with future threading

**Article XII: Phase 1 Constitutional Guardrails**
- ✅ Phase 1 Scope: Native client foundation - Engine refactor is infrastructure work
- ✅ Phase 1 Exclusions: No gameplay, multiplayer, or advanced features

### ⚠️ Design Considerations

No violations. This refactor **strengthens** constitutional compliance by:
- Eliminating global state (Article IV - State and Coupling)
- Maintaining thin Application entrypoint (Article IV - Entrypoint Discipline)
- Preserving module boundaries and dependency direction (Article III - Module Structure)

**Gate Status**: ✅ PASSED - Proceed to Phase 0

## Project Structure

### Documentation (this feature)

```text
specs/008-engine-core-refactor/
├── spec.md              # Feature specification (complete)
├── plan.md              # This file
├── research.md          # Phase 0: Technical research (minimal - well-defined tech stack)
├── data-model.md        # Phase 1: Engine class and subsystem relationships
├── quickstart.md        # Phase 1: How to use Engine after refactor
├── contracts/           # Phase 1: Engine public API contract
│   └── engine-api.md    # Engine interface specification
└── tasks.md             # Phase 2: Implementation tasks (NOT created by this workflow)
```

### Source Code (repository root)

```text
LongXi/
├── LXCore/                     # Low-level foundation (unchanged)
│   └── Src/
│       ├── External/           # Spdlog (unchanged)
│       └── Core/
│           └── Logging/        # Logging infrastructure (unchanged)
│
├── LXEngine/                   # Engine/runtime systems (MODIFIED)
│   └── Src/
│       ├── Application/        # Application.h/cpp (MODIFIED - removes subsystem members)
│       ├── Window/             # Win32Window (unchanged)
│       ├── Renderer/           # DX11Renderer (unchanged)
│       ├── Input/              # InputSystem (unchanged)
│       ├── Texture/            # TextureManager (unchanged)
│       ├── Core/               # VirtualFileSystem (unchanged)
│       └── Engine/             # ⭐ NEW - Engine.h/cpp (central coordinator)
│
└── LXShell/                    # Shell executable (MODIFIED)
    └── Src/
        └── main.cpp            # TestApplication (MODIFIED - uses Engine)
```

**Structure Decision**: Option 1 (Single project structure with modular static libraries). The refactor does not change the physical module structure - only moves subsystem ownership from Application to Engine within the existing LXEngine module. No new modules or projects are created.

**Key Changes**:
- **NEW**: `LongXi/LXEngine/Src/Engine/Engine.h` and `Engine.cpp`
- **MODIFIED**: `LongXi/LXEngine/Src/Application/Application.h` and `Application.cpp` (removes subsystem members, adds Engine member)
- **MODIFIED**: `LongXi/LXEngine/LXEngine.h` (adds Engine.h include)

## Complexity Tracking

> **Not applicable** - No constitutional violations requiring justification.
