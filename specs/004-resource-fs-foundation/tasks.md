# Tasks: Resource and Filesystem Foundation

**Input**: Design documents from `/specs/004-resource-fs-foundation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: No automated tests requested. Verification is manual (build, launch, diagnostic log inspection, path normalization checks, shutdown).

**Organization**: Single user story (US1 — File Resolution and Read, P1). Phase 2 foundational tasks (ResourceSystem.h + Application.h) block US1 implementation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1)
- Exact file paths included in descriptions

---

## Phase 1: Setup (Build System)

**Purpose**: Confirm no build system changes are required before adding source files

- [x] T001 Verify `LongXi/LXCore/premake5.lua` — confirm `Src/**.h` and `Src/**.cpp` globs already cover the new `Core/FileSystem/` subdirectory; no edits needed

**Checkpoint**: Build system confirmed. New files under `LongXi/LXCore/Src/Core/FileSystem/` will be auto-included on next project regeneration.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish the complete `ResourceSystem` header and update `Application.h` — both are required before any `.cpp` work begins

**⚠️ CRITICAL**: US1 implementation cannot begin until both T002 and T003 are complete

- [x] T002 [P] Create `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.h` — declare `ResourceSystem` class inside `namespace LongXi` with: private `std::vector<std::string> m_Roots`; public lifecycle methods `Initialize(const std::vector<std::string>& roots)` and `Shutdown()`; public query methods `bool Exists(const std::string& relativePath) const` and `std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const`; private helpers `std::string Normalize(const std::string& path) const` and `std::string Resolve(const std::string& normalizedPath) const`; required includes `<string>`, `<vector>`, `<optional>`, `<cstdint>`
- [x] T003 [P] Modify `LongXi/LXEngine/Src/Application/Application.h` — add `class ResourceSystem;` forward declaration, add `std::unique_ptr<ResourceSystem> m_ResourceSystem` private member, add `const ResourceSystem& GetResourceSystem() const` public accessor, add `bool CreateResourceSystem()` private method declaration

**Checkpoint**: Headers compile cleanly. US1 implementation can now proceed.

---

## Phase 3: User Story 1 — File Resolution and Read (Priority: P1) 🎯 MVP

**Goal**: Implement the full `ResourceSystem` in `LXCore` and integrate it into the `Application` lifecycle — path normalization, root resolution, existence queries, whole-file reads, failure logging.

**Independent Test**: Launch `LXShell.exe`. Verify root registration log messages appear. A `test.dat` file placed under `Data/` is read and its byte count logged. Requesting `does_not_exist.dat` produces an ERROR log entry with no crash. Alt-tab and close — clean exit.

### Implementation for User Story 1 — ResourceSystem.cpp

- [x] T004 [US1] Create `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` with file-scope static helpers inside `namespace LongXi`: `ToWide(const std::string& utf8)` using two-call `MultiByteToWideChar(CP_UTF8)` pattern returning `std::wstring`; `GetExecutableDirectory()` using `GetModuleFileNameW(nullptr, buffer, MAX_PATH)` (guard return != 0 and != MAX_PATH) then `std::filesystem::path(buffer).parent_path().wstring()`; required includes `<Windows.h>`, `<filesystem>`, `<fstream>`, `"Core/FileSystem/ResourceSystem.h"`, `"Core/Logging/LogMacros.h"`
- [x] T005 [US1] Implement `ResourceSystem::Normalize(const std::string& path) const` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — apply in order: (1) trim leading/trailing ASCII whitespace, (2) replace all `\` with `/`, (3) collapse consecutive `//` into single `/`, (4) strip a single leading `/`, (5) split on `/` and reject (return `""` + `LX_CORE_WARN`) if any segment equals `..`, (6) collapse `.` segments, (7) strip trailing `/`; return empty string for empty input
- [x] T006 [US1] Implement `ResourceSystem::Resolve(const std::string& normalizedPath) const` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — iterate `m_Roots` in order; for each root, form `candidate = root + "/" + normalizedPath`; if `std::filesystem::is_regular_file(std::filesystem::path(ToWide(candidate)))` returns true, return `candidate`; return `""` if no root matches
- [x] T007 [US1] Implement `ResourceSystem::Initialize(const std::vector<std::string>& roots)` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — store roots after normalizing each as an absolute path; for each root call `std::filesystem::exists(ToWide(root))` and log `LX_CORE_INFO("ResourceSystem: registered root '{}'")` if exists or `LX_CORE_WARN("ResourceSystem: root '{}' does not exist...")` if not; log `LX_CORE_INFO("Resource system initialized ({} root(s))", m_Roots.size())`
- [x] T008 [US1] Implement `ResourceSystem::Shutdown()` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — clear `m_Roots`; log `LX_CORE_INFO("Resource system shutdown")`
- [x] T009 [US1] Implement `ResourceSystem::Exists(const std::string& relativePath) const` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — call `Normalize(relativePath)`; if empty return false; call `Resolve(normalized)`; return `!resolved.empty()`
- [x] T010 [US1] Implement `ResourceSystem::ReadFile(const std::string& relativePath) const` in `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp` — call `Normalize(relativePath)`; if empty log `LX_CORE_WARN` and return `std::nullopt`; call `Resolve(normalized)`; if empty log `LX_CORE_ERROR("ResourceSystem: file not found: '{}'", relativePath)` and return `std::nullopt`; open `std::ifstream(ToWide(resolved), std::ios::binary | std::ios::ate)`; if not open log `LX_CORE_ERROR("ResourceSystem: failed to open: '{}'", resolved)` and return `std::nullopt`; read `tellg()` size (handle zero-byte case — return empty vector, not nullopt); `seekg(0)`, read into `std::vector<uint8_t>`, return engaged `std::optional`

### Implementation for User Story 1 — Application integration

- [x] T011 [US1] Add `#include "Core/FileSystem/ResourceSystem.h"` and implement `Application::CreateResourceSystem()` in `LongXi/LXEngine/Src/Application/Application.cpp` — call `GetExecutableDirectory()` (declared static in ResourceSystem.cpp; expose via a free function or inline call with `GetModuleFileNameW`), compose roots: `{ exeDir + "/Data", exeDir }` (register Data subfolder first as primary, exe dir as fallback); instantiate `m_ResourceSystem = std::make_unique<ResourceSystem>()`; call `m_ResourceSystem->Initialize(roots)`; return true
- [x] T012 [US1] Implement `Application::GetResourceSystem() const` accessor in `LongXi/LXEngine/Src/Application/Application.cpp` — return `*m_ResourceSystem`
- [x] T013 [US1] Modify `Application::Initialize()` in `LongXi/LXEngine/Src/Application/Application.cpp` — call `CreateResourceSystem()` after `CreateInputSystem()`; on failure log error, call `m_InputSystem->Shutdown()`, `m_Renderer->Shutdown()`, `DestroyMainWindow()`, return false
- [x] T014 [US1] Modify `Application::Shutdown()` in `LongXi/LXEngine/Src/Application/Application.cpp` — call `m_ResourceSystem->Shutdown()` and `m_ResourceSystem.reset()` before `m_InputSystem->Shutdown()`
- [x] T015 [US1] Add temporary diagnostic log in `Application::Run()` in `LongXi/LXEngine/Src/Application/Application.cpp` — after input Update(), call `m_ResourceSystem->ReadFile("test.dat")` and log byte count on success; call `m_ResourceSystem->ReadFile("does_not_exist.dat")` to trigger and verify ERROR log; guard these calls to fire once on the first frame only (use a `bool m_ResourceVerified` flag); comment clearly as temporary

**Checkpoint**: Resource system fully integrated. Application initializes, roots logged, test file readable, missing file failure observable, shutdown clean.

---

## Phase 4: Verification & Polish

**Purpose**: Build all configurations, manual verification, cleanup, documentation

- [x] T016 Create test verification file — create a `Data/` folder alongside the `LXShell.exe` Debug build output (e.g., `LongXi/Build/Debug/Data/test.dat`) containing known ASCII bytes (e.g., the string `"LongXiTest"`) for AC-002 verification
- [x] T017 Regenerate Visual Studio project files via `Win-Generate Project.bat` to add `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.h` and `ResourceSystem.cpp` to `LongXi/LXCore/LXCore.vcxproj`
- [x] T018 Build Debug|x64 — verify 0 errors, 0 warnings across LXCore, LXEngine, LXShell
- [x] T019 Build Release|x64 — verify 0 errors
- [x] T020 Build Dist|x64 — verify 0 errors
- [x] T021 Manual test: launch LXShell.exe (Debug) — verify console shows `ResourceSystem: registered root '...'` lines and `Resource system initialized (2 root(s))`
- [x] T022 Manual test: verify `"Read N bytes from test.dat"` appears in console (AC-002 — file found and read)
- [x] T023 Manual test: verify `"ResourceSystem: file not found: 'does_not_exist.dat'"` ERROR line appears in console with no crash (AC-003)
- [x] T024 Manual test: temporarily test path with backslash (`"test\\dat"` equivalent path) — verify it resolves identically to forward-slash path (AC-004 — normalization confirmed)
- [x] T025 Manual test: temporarily test path `"sub/../test.dat"` — verify WARN log `"rejected path with traversal"` appears and ReadFile returns nullopt (traversal prevention confirmed)
- [x] T026 Manual test: close window — verify clean exit with no errors or warnings
- [x] T027 Verify `LXShell/Src/main.cpp` remains unchanged from Spec 003 — still only `CreateApplication()`
- [x] T028 Remove temporary diagnostic log lines and `m_ResourceVerified` flag added in T015 from `LongXi/LXEngine/Src/Application/Application.cpp`; remove `m_ResourceVerified` declaration from `LongXi/LXEngine/Src/Application/Application.h` if added
- [x] T029 Run `Win-Format Code.bat` to format all C++ source files
- [x] T030 Update `specs/004-resource-fs-foundation/spec.md` status from `Draft` to `Accepted`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — headers block all implementation
- **User Story 1 (Phase 3)**: Depends on Phase 2 — all implementation requires headers
- **Verification (Phase 4)**: Depends on Phase 3 — all code must be written before build/test

### Within User Story 1

```text
T002 (ResourceSystem.h) ──────┐
T003 (Application.h) ─────────┤
                               ├── T004 (file-scope helpers in .cpp)
                               ├── T005 (Normalize)
                               ├── T006 (Resolve)        ← depends on T004 (ToWide)
                               ├── T007 (Initialize)     ← depends on T004 (ToWide)
                               ├── T008 (Shutdown)
                               ├── T009 (Exists)         ← depends on T005, T006
                               ├── T010 (ReadFile)       ← depends on T004, T005, T006
                               ├── T011 (CreateResourceSystem in App.cpp)
                               ├── T012 (GetResourceSystem accessor)
                               ├── T013 (Initialize integration)
                               ├── T014 (Shutdown integration)
                               └── T015 (diagnostic logging)
```

### Parallel Opportunities

- T002 (ResourceSystem.h) and T003 (Application.h) can run in parallel — different files
- T004–T010 (ResourceSystem.cpp) are sequential — same file
- T011–T015 (Application.cpp) are sequential — same file
- T004–T010 and T011–T015 can overlap if two people work simultaneously (different files)
- T017 (project regen) must come after all new files are created
- T018–T020 (three build configs) are sequential
- T021–T027 (manual tests) can run as a single session

---

## Implementation Strategy

### MVP First (User Story 1 — all of Phase 3)

1. Complete Phase 1: Setup (T001)
2. Complete Phase 2: Foundational (T002–T003)
3. Complete Phase 3: User Story 1 (T004–T015)
4. **STOP and VALIDATE**: Regen project, build Debug, run manual tests (T016–T027)
5. Polish: remove diagnostic log, format, update spec status (T028–T030)

### Single Developer Execution

All phases are sequential for a single developer. Total: 30 tasks — T001–T015 implementation, T016–T030 verification and polish.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- No automated test tasks — spec does not request automated tests
- `GetExecutableDirectory()` is a file-scope static in `ResourceSystem.cpp`; if `Application.cpp` needs it, expose it as a free function in `ResourceSystem.h` or duplicate the `GetModuleFileNameW` call in `Application::CreateResourceSystem()` — choose one approach during T011 and be consistent
- `std::filesystem` headers (`<filesystem>`) included only in `ResourceSystem.cpp`, not in `ResourceSystem.h`
- `ToWide()` is file-scope static in `ResourceSystem.cpp` — does not appear in the public header
- Diagnostic log lines (T015) are temporary — must be removed in T028 before committing
- Commit after each logical group (Phase 2, Phase 3, Phase 4)
- All Spec 001–003 behavior (window, renderer, input) must remain unaffected

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
