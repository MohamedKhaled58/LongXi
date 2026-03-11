# Implementation Plan: Virtual File System (VFS) & Resource Access Layer

**Branch**: `006-vfs-resource-layer` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/006-vfs-resource-layer/spec.md`

---

## Summary

Implement `CVirtualFileSystem` — a priority-ordered, mount-point-based resource access layer that replaces `ResourceSystem` as the engine's primary resource interface. The implementation adds `IFileStream` (with disk and WDF-backed implementations), `IMountPoint` (with directory and WDF implementations), and `CVirtualFileSystem` itself — all to `LXCore/Src/Core/FileSystem/`. `Application` is migrated to own and expose `CVirtualFileSystem`. This also fixes a known gap: `ResourceSystem::Exists()` does not search archives; `CVirtualFileSystem::Exists()` does.

---

## Technical Context

**Language/Version**: C++23 (MSVC v145) — from constitution Article III
**Primary Dependencies**: LXCore (WdfArchive, LX_CORE_* macros), Win32 (MultiByteToWideChar), std::ifstream, std::filesystem
**Storage**: WDF archives (binary, indexed) + Win32 filesystem (loose files)
**Testing**: Manual build + integration test via Application startup + targeted resource reads
**Target Platform**: Windows (Win32) — from constitution Article III
**Project Type**: Static library (LXCore) — no premake changes required (glob auto-includes)
**Performance Goals**: O(log n) archive index lookup (binary search in WdfArchive); no re-scan after mount; O(1) buffer reads (CFileWdfStream)
**Constraints**: Phase 1 single-threaded runtime (constitution Article IX); design is concurrent-read-ready for Phase 2
**Scale/Scope**: 5–20 mount points; thousands of resource entries per archive; no resource caching at VFS layer

---

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Article | Gate | Status | Notes |
|---|---|---|---|
| III — Platform/Tooling | C++23, MSVC, Win32, static lib | ✅ PASS | All VFS files go into LXCore (static lib) |
| III — Module Structure | LXCore = low-level foundation | ✅ PASS | WdfArchive + ResourceSystem already in LXCore; VFS follows the same placement |
| III — Dependency Direction | LXShell → LXEngine → LXCore | ✅ PASS | No new cross-layer dependencies introduced |
| IV — Entrypoint Discipline | Entrypoints stay thin | ✅ PASS | Application::CreateVirtualFileSystem() is a thin delegator |
| IV — Module Boundaries | No cross-layer coupling | ✅ PASS | CVirtualFileSystem is self-contained within LXCore |
| IV — Abstraction Discipline | Abstractions earned by concrete need | ✅ PASS | IFileStream needed because disk and WDF streams have different backing; IMountPoint needed for two concrete source types |
| IV — State and Coupling | No god objects | ✅ PASS | CVirtualFileSystem is a focused subsystem |
| VIII — Change Scope | Changes focused on stated problem | ✅ PASS | Scope: VFS layer + Application migration only |
| IX — Threading | Phase 1 single-threaded | ✅ PASS | Design is MT-aware (read-after-mount is concurrent-safe) but imposes no threading complexity in Phase 1 |
| XII — Phase 1 Scope | Resource/filesystem foundation in scope | ✅ PASS | Constitution Article XII explicitly includes "Resource/filesystem foundation" in Phase 1 |

**All gates pass. No violations.**

---

## Complexity Tracking

*No constitution violations to justify.*

---

## Project Structure

### Documentation (this feature)

```text
specs/006-vfs-resource-layer/
├── plan.md              ← this file
├── spec.md              ← feature specification
├── research.md          ← Phase 0 findings
├── data-model.md        ← entity definitions and relationships
├── quickstart.md        ← implementation guide
├── contracts/
│   └── vfs-public-api.md ← public API contract
└── checklists/
    └── requirements.md  ← spec quality checklist
```

### Source Code Changes

```text
LongXi/LXCore/Src/Core/FileSystem/
├── WdfArchive.h/.cpp           (Spec 005 — existing, unchanged)
├── ResourceSystem.h/.cpp       (Spec 004 — existing, superseded, kept for GetExecutableDirectory)
├── FileStream.h/.cpp           ← NEW: IFileStream, CFileDiskStream, CFileWdfStream
├── MountPoint.h/.cpp           ← NEW: IMountPoint, CDirectoryMountPoint, CWdfMountPoint
└── VirtualFileSystem.h/.cpp    ← NEW: CVirtualFileSystem

LongXi/LXCore/
└── LXCore.h                    ← UPDATED: add VirtualFileSystem.h include

LongXi/LXEngine/Src/Application/
├── Application.h               ← UPDATED: CVirtualFileSystem replaces ResourceSystem
└── Application.cpp             ← UPDATED: CreateVirtualFileSystem() replaces CreateResourceSystem()
```

**Structure Decision**: All new VFS types live in `LXCore/Src/Core/FileSystem/` — consistent with the existing `WdfArchive` and `ResourceSystem` placement. Premake5 glob (`Src/**.h`, `Src/**.cpp`) auto-includes new files; no premake changes required.

---

## Phase 0: Research Summary

All NEEDS CLARIFICATION items resolved. See [research.md](research.md) for full details.

| Question | Finding |
|---|---|
| WdfArchive API? | `HasEntry(fullPath)` / `ReadEntry(fullPath)` — UID internal to WdfArchive |
| Module placement? | LXCore — matches existing WdfArchive + ResourceSystem location |
| Premake changes needed? | None — glob auto-picks up new `.h/.cpp` files |
| LXCore.h changes? | Add `#include "Core/FileSystem/VirtualFileSystem.h"` |
| CFileDiskStream file access? | `std::ifstream` with `ToWide()` — matches existing codebase pattern |
| ResourceSystem relationship? | CVirtualFileSystem supersedes it; Application migrates |
| Exists() gap in ResourceSystem? | Fixed by CVirtualFileSystem::Exists() checking all mount types |

---

## Phase 1: Design Artifacts

All design artifacts complete. See linked documents.

- **Data model**: [data-model.md](data-model.md) — entity definitions, fields, relationships, validation rules
- **API contract**: [contracts/vfs-public-api.md](contracts/vfs-public-api.md) — public API signatures, behavior, error model
- **Quickstart**: [quickstart.md](quickstart.md) — step-by-step implementation guide with code notes

---

## Implementation Plan

### Task 1 — FileStream Layer

**File**: `LXCore/Src/Core/FileSystem/FileStream.h` + `FileStream.cpp`

Implement:
- `IFileStream` — pure virtual base; `Read`, `Seek`, `Tell`, `Size`
- `CFileDiskStream` — wraps `std::ifstream`; `ToWide()` for Win32 path conversion; caches `m_Size` at open
- `CFileWdfStream` — wraps `std::vector<uint8_t>` (move-constructed); in-memory Read/Seek

Dependencies: `WdfArchive.h` (for include in MountPoint), `LX_CORE_*` macros
No dependencies on VirtualFileSystem or MountPoint.

---

### Task 2 — MountPoint Layer

**File**: `LXCore/Src/Core/FileSystem/MountPoint.h` + `MountPoint.cpp`

Implement:
- `IMountPoint` — pure virtual; `Exists(normalizedPath)`, `Open(normalizedPath)`
- `CDirectoryMountPoint` — `m_Root` string; filesystem stat for `Exists`; `CFileDiskStream` for `Open`
- `CWdfMountPoint` — owns `unique_ptr<WdfArchive>`; delegates to `HasEntry`/`ReadEntry`; wraps result in `CFileWdfStream`

Dependencies: `FileStream.h`, `WdfArchive.h`
No dependencies on VirtualFileSystem.

---

### Task 3 — CVirtualFileSystem

**File**: `LXCore/Src/Core/FileSystem/VirtualFileSystem.h` + `VirtualFileSystem.cpp`

Implement:
- `CVirtualFileSystem` — `m_MountPoints: vector<unique_ptr<IMountPoint>>`
- `Normalize(path)` — 8-step algorithm (see data-model.md); reuse logic from ResourceSystem::Normalize or duplicate
- `MountDirectory(path)` — validate, create CDirectoryMountPoint, log INFO, append
- `MountWdf(path)` — create WdfArchive, open, on success create CWdfMountPoint, log INFO, append
- `Exists(path)` — normalize; iterate mount points; return first true
- `Open(path)` — normalize; iterate mount points; return first valid stream; log WARN on miss
- `ReadAll(path)` — call Open; bulk-read into vector

Dependencies: `MountPoint.h`, `WdfArchive.h`, LX_CORE_* macros

---

### Task 4 — LXCore.h Update

**File**: `LongXi/LXCore/LXCore.h`

Add:
```cpp
#include "Core/FileSystem/VirtualFileSystem.h"
```

This exposes `CVirtualFileSystem`, `IFileStream` to all LXCore consumers.

---

### Task 5 — Application Migration

**Files**: `LXEngine/Src/Application/Application.h` + `Application.cpp`

Changes:
- `Application.h`: remove `class ResourceSystem` forward decl; add `class CVirtualFileSystem`; rename member and accessor
- `Application.cpp`: replace `#include ResourceSystem.h` with `#include VirtualFileSystem.h`; rewrite `CreateVirtualFileSystem()` to mount directories then archives; rename all usages

`ResourceSystem::GetExecutableDirectory()` is still needed. Either:
  - Keep `#include "Core/FileSystem/ResourceSystem.h"` in Application.cpp alongside the new include, OR
  - Move `GetExecutableDirectory()` to a platform utility — preferred long-term but deferred to avoid scope creep

Simplest safe choice: include both headers in Application.cpp during transition.

---

### Task 6 — Verification

Build and validate:

1. `Win-Build Project.bat` → zero errors, zero warnings
2. Application starts → VFS mount messages appear in log
3. `CVirtualFileSystem::Exists("C3.wdf")` or a known resource → returns true (confirms archives searched)
4. `CVirtualFileSystem::Open("known-file")` → returns valid stream, `ReadAll` returns correct bytes
5. Missing resource → `nullptr` returned, WARN logged
6. Directory mount before archive → directory version returned (priority test)

---

## Constitution Check (Post-Design)

Re-evaluated after Phase 1 design. All gates still pass:

- No new external dependencies introduced
- No premature abstractions: `IFileStream` and `IMountPoint` have exactly two concrete implementations each — earned by concrete need
- `CVirtualFileSystem` is a focused class; no god-object scope creep
- Application migration is a direct 1:1 rename — no scope expansion
- `ResourceSystem` retained (not deleted) to minimize blast radius during transition
- Phase 1 threading policy respected: VFS design is thread-safe by design but adds no threading infrastructure

---

## Risks and Notes

| Risk | Mitigation |
|---|---|
| `WdfArchive::ReadEntry` is `const` but `m_File` (seekg) is `mutable` — not thread-safe for concurrent reads | Acceptable in Phase 1 (single-threaded). Document as a Phase 2 threading concern. |
| `ToWide()` utility is duplicated in ResourceSystem.cpp and WdfArchive.cpp | For now, add a private static helper in FileStream.cpp and MountPoint.cpp. Consolidating to a shared utility is a follow-up, not Spec 006 scope. |
| `ResourceSystem::Exists()` bug (doesn't check archives) | Fixed implicitly by CVirtualFileSystem replacing ResourceSystem. ResourceSystem left as-is. |
| Application accessor renamed from `GetResourceSystem` to `GetVirtualFileSystem` | Verify no callers in LXEngine or LXShell depend on the old name. Currently none found. |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
