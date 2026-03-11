# Implementation Plan: Resource and Filesystem Foundation

**Branch**: `004-resource-fs-foundation` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/004-resource-fs-foundation/spec.md`

## Summary

Add a `ResourceSystem` class to `LXCore` providing centralized path normalization, root-relative resolution, existence queries, and whole-file byte reads. `Application` in `LXEngine` owns and initializes `ResourceSystem` within the existing lifecycle. No external dependencies are required — only the C++ standard library and Win32 API at the OS call boundary. All internal paths use `std::string` with forward-slash normalization; wide-string conversion occurs only at Win32 call sites.

## Technical Context

**Language/Version**: C++23, MSVC v145
**Primary Dependencies**: C++ standard library (`<optional>`, `<vector>`, `<string>`, `<fstream>`); Win32 API (`GetModuleFileNameW`, `GetFileAttributesW`) at the OS call boundary only — no external libraries
**Storage**: In-memory only — file contents returned as `std::vector<uint8_t>` owned by the caller; no caching
**Testing**: Manual verification via diagnostic log output — no automated test framework
**Target Platform**: Windows x64 only
**Project Type**: Static library (`LXCore`) + `LXEngine` Application integration
**Performance Goals**: No performance requirements for the bootstrap phase; whole-file reads are synchronous and blocking
**Constraints**: Single-threaded runtime (Phase 1); read-only access only; no archive parsing; no caching; `LXShell` unchanged
**Scale/Scope**: Loose files only; resource files in the 1 KB–50 MB range are well within scope for synchronous whole-file reads

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Article | Requirement | Status |
|---------|-------------|--------|
| III — Platform | Windows-only, raw Win32 API | PASS — Win32 used only at OS call boundary; internal API is platform-agnostic string-based |
| III — Module Structure | `LXCore` / `LXEngine` / `LXShell` static libs | PASS — ResourceSystem in `LXCore`; Application integration in `LXEngine` |
| III — Dependency Direction | `LXShell → LXEngine → LXCore` | PASS — no reverse dependencies; LXEngine includes LXCore headers |
| III — Runtime Model | Central `Application` lifecycle owner | PASS — ResourceSystem initialized and shut down by Application |
| IV — Entrypoint Discipline | Entrypoints remain thin | PASS — `LXShell` unchanged; Application delegates |
| IV — Module Boundaries | No cross-layer coupling | PASS — ResourceSystem has no dependency on renderer, input, or gameplay |
| IV — No God Modules | No hidden global state | PASS — ResourceSystem is an owned subsystem instance, not a global singleton |
| IV — Abstraction Discipline | No speculative abstraction | PASS — concrete directory-backed implementation; extension boundary is a reserved comment, not a partial framework |
| IX — Single-Threaded | Phase 1 single-threaded by rule | PASS — all file I/O is synchronous, single-threaded |
| IX — MT-Aware | Architecture ready for future threading | PASS — ResourceSystem has no hidden shared mutable state; roots fixed at init |
| XII — Phase 1 Scope | Resource/filesystem foundation is Phase 1 constitutional scope | PASS — "Resource/filesystem foundation" explicitly listed in Article XII Phase 1 scope |

**No violations. No complexity tracking required.**

## Project Structure

### Documentation (this feature)

```text
specs/004-resource-fs-foundation/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks — NOT created here)
```

### Source Code (repository root)

```text
LongXi/LXCore/Src/
└── Core/
    └── FileSystem/                         # New subdirectory
        ├── ResourceSystem.h                # New — ResourceSystem class declaration
        └── ResourceSystem.cpp              # New — ResourceSystem implementation

LongXi/LXEngine/Src/
└── Application/
    ├── Application.h                       # Modified — forward decl, m_ResourceSystem, GetResourceSystem()
    └── Application.cpp                     # Modified — CreateResourceSystem(), lifecycle integration
```

**No premake changes required**: Both `LXCore/premake5.lua` and `LXEngine/premake5.lua` already use `Src/**.h` and `Src/**.cpp` globs. New files are auto-included on next project regeneration.

**No contracts directory**: `ResourceSystem` is an internal engine subsystem with no external interface surface. The caller-facing C++ API is documented in data-model.md.

**Structure Decision**: `ResourceSystem` placed in `LXCore/Src/Core/FileSystem/` following the established `Core/Logging/` pattern. Placing it in `LXCore` makes it accessible to all engine layers without upward dependencies.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
