# Tasks: Virtual File System (VFS) & Resource Access Layer

**Input**: Design documents from `/specs/006-vfs-resource-layer/`
**Prerequisites**: plan.md ✅ spec.md ✅ research.md ✅ data-model.md ✅ contracts/ ✅ quickstart.md ✅

**Tests**: Not requested — no test tasks generated.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story?] Description — file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[US1/US2/US3]**: Which user story this task belongs to

---

## Phase 1: Setup

**Purpose**: Confirm build infrastructure before writing any new files.

- [x] T001 Confirm LongXi/LXCore/premake5.lua uses `Src/**.h` / `Src/**.cpp` glob — new files under Src/Core/FileSystem/ require no premake changes

**Checkpoint**: Build system verified — new source files will be auto-included.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define all C++ interfaces and class declarations. No implementations. Must complete before any user story work begins.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [x] T002 [P] Create LongXi/LXCore/Src/Core/FileStream.h — declare `IFileStream` (pure virtual: Read, Seek, Tell, Size), `CFileDiskStream`, and `CFileWdfStream` class declarations; `#pragma once`; `namespace LongXi`
- [x] T003 [P] Create LongXi/LXCore/Src/Core/FileSystem/MountPoint.h — declare `IMountPoint` (pure virtual: Exists, Open returning `unique_ptr<IFileStream>`), `CDirectoryMountPoint`, and `CWdfMountPoint` class declarations; `#include "FileStream.h"`; `namespace LongXi`
- [x] T004 Create LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.h — declare `CVirtualFileSystem` class with `MountWdf`, `MountDirectory`, `Exists`, `Open`, `ReadAll` public methods and `m_MountPoints: vector<unique_ptr<IMountPoint>>` private member; `#include "MountPoint.h"`; `namespace LongXi`

**Checkpoint**: All interface headers compile. No implementations yet.

---

## Phase 3: User Story 1 — Unified Resource Loading (Priority: P1) 🎯 MVP

**Goal**: Engine systems can load any resource using a virtual path string, from either a WDF archive or a loose disk file, without knowing the storage source.

**Independent Test**: Application starts; VFS log messages appear confirming directory + archive mounts; calling `GetVirtualFileSystem().Exists("C3.wdf")` or a known resource returns correct result; `GetVirtualFileSystem().Open("known-resource")` returns a valid non-null stream.

### Implementation for User Story 1

- [x] T00X [US1] Create LongXi/LXCore/Src/Core/FileSystem/FileStream.cpp — implement `CFileDiskStream`: static `ToWide(string)` helper using `MultiByteToWideChar`; constructor opens `std::ifstream` in binary+ate mode, caches `m_Size` from `tellg()`, seeks back to 0; `Read` calls `m_File.read` returns `gcount()`; `Seek` calls `seekg(offset)` returns bool; `Tell` returns `m_Position`; `Size` returns `m_Size`; `#include <Windows.h>` for `MultiByteToWideChar`
- [x] T00X [US1] Add `CFileWdfStream` to LongXi/LXCore/Src/Core/FileSystem/FileStream.cpp — constructor takes `vector<uint8_t>` by move into `m_Data`; `Read` does `memcpy` from `m_Data[m_Position]`, advances `m_Position`; `Seek` bounds-checks then assigns `m_Position`, returns false if `offset > m_Data.size()`; `Tell` returns `m_Position`; `Size` returns `m_Data.size()`
- [x] T00X [US1] Create LongXi/LXCore/Src/Core/FileSystem/MountPoint.cpp — implement `CDirectoryMountPoint`: constructor stores root path stripping trailing slash; `Exists` calls `std::filesystem::is_regular_file(ToWide(m_Root + "/" + normalizedPath))`; `Open` returns `make_unique<CFileDiskStream>(absPath)` or `nullptr` if not found
- [x] T00X [US1] Add `CWdfMountPoint` to LongXi/LXCore/Src/Core/FileSystem/MountPoint.cpp — constructor takes `unique_ptr<WdfArchive>` by move into `m_Archive`; `Exists` returns `m_Archive->HasEntry(normalizedPath)`; `Open` calls `m_Archive->ReadEntry(normalizedPath)`, returns `make_unique<CFileWdfStream>(move(*result))` if `has_value()` else `nullptr`
- [x] T00X [US1] Create LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.cpp — implement `CVirtualFileSystem::Normalize(path)` applying all 8 normalization steps from data-model.md: replace `\` → `/`; lowercase; collapse `//`; strip leading `/`; strip trailing `/`; reject `..` segments (log WARN, return empty); collapse `.` segments; return empty if result is empty after all steps
- [x] T010 [US1] Add `CVirtualFileSystem::MountDirectory()` and `MountWdf()` to LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.cpp — `MountDirectory`: check `filesystem::is_directory`, log ERROR + return false if missing, otherwise `push_back(make_unique<CDirectoryMountPoint>(path))` + log `LX_CORE_INFO("[VFS] Mounted directory: {}", path)` + return true; `MountWdf`: create `make_unique<WdfArchive>()`, call `Open(path)`, on failure log `LX_CORE_ERROR("[VFS] Failed to mount WDF: {}", path)` + return false, on success `push_back(make_unique<CWdfMountPoint>(move(archive)))` + log `LX_CORE_INFO("[VFS] Mounted WDF archive: {}", path)` + return true
- [x] T011 [US1] Add `CVirtualFileSystem::Exists()`, `Open()`, and `ReadAll()` to LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.cpp — `Exists`: normalize path, return false if empty, iterate `m_MountPoints` front-to-back returning true on first `mp->Exists(normalized)`; `Open`: normalize, if empty log `LX_CORE_WARN("[VFS] Open called with empty path")` + return nullptr, iterate mount points, on first `mp->Exists` call `mp->Open`, if stream is valid return it, after all misses log `LX_CORE_WARN("[VFS] Resource not found: {}", normalized)` + return nullptr; `ReadAll`: call `Open`, if nullptr return `{}`, resize buffer to `stream->Size()`, call `stream->Read`, return buffer
- [x] T012 [US1] [P] Update LongXi/LXCore/LXCore.h — add `#include "Core/FileSystem/VirtualFileSystem.h"` after the existing logging includes to expose `CVirtualFileSystem` and `IFileStream` as part of the LXCore public API
- [x] T013 [US1] [P] Update LongXi/LXEngine/Src/Application/Application.h — replace `class ResourceSystem;` forward declaration with `class CVirtualFileSystem;`; rename private member `m_ResourceSystem` to `m_VirtualFileSystem` changing type to `unique_ptr<CVirtualFileSystem>`; rename `GetResourceSystem()` to `GetVirtualFileSystem()` returning `const CVirtualFileSystem&`; rename `CreateResourceSystem()` to `CreateVirtualFileSystem()`
- [x] T014 [US1] Update LongXi/LXEngine/Src/Application/Application.cpp — replace `#include "Core/FileSystem/ResourceSystem.h"` with `#include "Core/FileSystem/VirtualFileSystem.h"`; rename `CreateResourceSystem()` to `CreateVirtualFileSystem()`; replace `ResourceSystem` construction with `CVirtualFileSystem`; mount directories then archives using the exe directory pattern from research.md; update all member references from `m_ResourceSystem` to `m_VirtualFileSystem`; preserve `ResourceSystem::GetExecutableDirectory()` call by keeping the old include alongside (or moving the helper call)

**Checkpoint**: Application compiles and starts. Log shows VFS mount messages. `GetVirtualFileSystem().Open()` returns valid streams for known resources.

---

## Phase 4: User Story 2 — Development Override and Patch Support (Priority: P2)

**Goal**: Loose files on disk override packaged WDF resources when a directory is mounted before an archive. Enables patch workflows without archive rebuilds.

**Independent Test**: Application mounts a directory before a WDF archive. Log confirms directory is index 0. Requesting a resource that exists in both sources returns the directory version (confirmed by checking log output or file bytes).

### Implementation for User Story 2

- [x] T015 [US2] Verify mount call order in `Application::CreateVirtualFileSystem()` in LongXi/LXEngine/Src/Application/Application.cpp — all `MountDirectory()` calls MUST appear before any `MountWdf()` call; if not already in this order, reorder them; add an inline comment documenting the priority rationale
- [x] T016 [US2] Confirm `CVirtualFileSystem::Open()` in LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.cpp returns the FIRST matching mount point's stream without continuing iteration — verify the `return stream` exits the loop immediately on success (correct first-match-wins behavior validates directory-before-archive priority)

**Checkpoint**: US1 and US2 both independently testable. Directory sources take priority over archives when mounted first.

---

## Phase 5: User Story 3 — Multiple Archive Support (Priority: P3)

**Goal**: Multiple WDF archives mounted simultaneously are all searched in mount order. Resources unique to later-mounted archives are still found.

**Independent Test**: Application mounts both C3.wdf and data.wdf. Log confirms two `[VFS] Mounted WDF archive:` entries. Requesting a resource only present in the second archive returns it successfully.

### Implementation for User Story 3

- [x] T017 [US3] Confirm `Application::CreateVirtualFileSystem()` in LongXi/LXEngine/Src/Application/Application.cpp calls `MountWdf()` for both C3.wdf and data.wdf using correct absolute paths built from `exeDir`; if paths differ from current `CreateResourceSystem()` logic, update them to match the expected archive locations
- [x] T018 [US3] Confirm `CVirtualFileSystem::Open()` in LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.cpp continues iterating all remaining mount points after a `CWdfMountPoint` returns nullptr — verify the loop does NOT exit early on a `nullptr` return from `mp->Open()` and only terminates the loop on a non-null stream

**Checkpoint**: All three user stories independently functional. Multiple archives searched in order.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Build verification, formatting, and cleanup.

- [ ] T019 Build full solution using `Win-Build Project.bat` — resolve any compile errors; verify zero errors and zero warnings related to new VFS files
- [ ] T020 [P] Run `Win-Format Code.bat` to auto-format all new .h/.cpp files under LongXi/LXCore/Src/Core/FileSystem/ using the repository clang-format configuration
- [ ] T021 Start application and inspect startup log — confirm all expected `[VFS]` INFO messages appear (directory mounts + archive mounts); confirm no unexpected ERROR or WARN messages on clean startup
- [x] T022 [P] Review LongXi/LXEngine/Src/Application/Application.cpp and LongXi/LXShell/Src/main.cpp — confirm no remaining references to `ResourceSystem` or `GetResourceSystem()`; if any remain, update them to `CVirtualFileSystem` / `GetVirtualFileSystem()`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — **BLOCKS all user stories**
- **User Story Phases (3–5)**: All depend on Phase 2 completion
  - US1 (Phase 3) must complete before US2 and US3 (US2/US3 verify properties of US1 code)
  - US2 (Phase 4) and US3 (Phase 5) depend on US1 completion — can run in parallel with each other
- **Polish (Phase 6)**: Depends on all user story phases

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational (Phase 2). Largest phase — builds entire VFS layer.
- **US2 (P2)**: Depends on US1 — verifies mount priority ordering in Application + VFS loop
- **US3 (P3)**: Depends on US1 — verifies multi-archive iteration in Application + VFS loop

### Within Each Phase

- T002 and T003 are independent (different files) — run in parallel if multiple agents available
- T012 and T013 are independent (different files) — run in parallel once T011 completes
- T005→T006 are sequential (same file: FileStream.cpp)
- T007→T008 are sequential (same file: MountPoint.cpp)
- T009→T010→T011 are sequential (same file: VirtualFileSystem.cpp, logical order)
- T015 and T017 are both in Application.cpp — sequential
- T019 must complete before T021

---

## Parallel Execution Examples

### Phase 2: Foundational (parallel header creation)

```
Parallel: T002 — Create FileStream.h
Parallel: T003 — Create MountPoint.h
Sequential: T004 — Create VirtualFileSystem.h (after T002, T003)
```

### Phase 3: US1 (parallel opportunities within the phase)

```
Sequential group A: T005 → T006 (FileStream.cpp)
Sequential group B: T007 → T008 (MountPoint.cpp)
Sequential group C: T009 → T010 → T011 (VirtualFileSystem.cpp)

Groups A, B, C can run in parallel (different files).

After T011:
Parallel: T012 (LXCore.h)
Parallel: T013 (Application.h)
Sequential: T014 (Application.cpp — after T012, T013)
```

### Polish (parallel cleanup)

```
Parallel: T020 — Format new files
Parallel: T022 — Review remaining ResourceSystem references
Sequential: T019 — Build (after all source changes)
Sequential: T021 — Startup log review (after T019)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete **Phase 1**: Setup (T001)
2. Complete **Phase 2**: Foundational (T002–T004) — interface headers only
3. Complete **Phase 3**: User Story 1 (T005–T014) — entire VFS implementation
4. **STOP and VALIDATE**: Application starts, VFS mount messages in log, `Open()` returns valid streams
5. Polish Phase 6 (T019–T022) to confirm build quality

### Incremental Delivery

1. Phase 1 + 2 → Headers defined; project compiles (no implementation yet)
2. Phase 3 (US1) → Full VFS working; Application uses CVirtualFileSystem
3. Phase 4 (US2) → Mount priority confirmed correct
4. Phase 5 (US3) → Multi-archive confirmed correct
5. Phase 6 → Build clean, formatted, no leftover ResourceSystem refs

---

## Notes

- `[P]` = different files, no blocking dependencies — parallelizable
- `[US1/US2/US3]` maps to user stories in spec.md
- US2 and US3 add no new classes — they verify behavioral properties of the US1 implementation
- ResourceSystem remains on disk after this implementation; it is superseded but not deleted
- `ResourceSystem::GetExecutableDirectory()` is still needed by Application.cpp during migration — keep the old `#include` or copy the helper
- Run `Win-Format Code.bat` before any commit — mandatory per CLAUDE.md
- All new C++ code must be inside `namespace LongXi { }` per CLAUDE.md Section 22
