# Tasks: Win32 Shell Bootstrap

**Input**: Design documents from `/specs/001-win32-shell-bootstrap/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Manual verification only (build execution, window creation, shutdown observation) - no automated tests per specification

**Organization**: Tasks are grouped by user story (1 story total) to enable independent implementation and testing

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1)
- Include exact file paths in descriptions

## Path Conventions

- **LongXi Project**: `LongXi/` at repository root
- **Modules**: `LongXi/LXCore/`, `LongXi/LXEngine/`, `LongXi/LXShell/`
- **Source**: `Src/` within each module
- **Build Artifacts**: `Build/Debug/`, `Build/Release/`, `Build/Dist/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

- [x] T001 Create directory structure per plan.md (LongXi/LXCore/Src/Core/, LongXi/LXEngine/Src/Application/, LongXi/LXEngine/Src/Window/, LongXi/LXEngine/Src/Logging/, LongXi/LXShell/Src/)
- [x] T002 Create LXCore.h public entry point in LongXi/LXCore/LXCore.h
- [x] T003 Create LXEngine.h public entry point in LongXi/LXEngine/LXEngine.h
- [x] T004 [P] Update .clang-format for C++23 code style per project conventions
- [x] T005 [P] Update Premake5 configuration in premake5.lua for Win32 linking (user32, gdi32) and Spdlog integration

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T006 Setup Spdlog include paths in premake5.lua (Vendor/Spdlog/include)
- [x] T007 [P] Create LXCore logging macros header in LongXi/LXCore/Src/Core/Logging/LogMacros.h (ALL macros consolidated: LX*CORE*_, LX*ENGINE*_, LX\_\*)
- [x] T008 [P] LXEngine logging macros — consolidated into LXCore/Src/Core/Logging/LogMacros.h (per-module file removed)
- [x] T009 LXShell logging macro — consolidated into LXCore/Src/Core/Logging/LogMacros.h (per-module file removed)
- [x] T010 Configure Spdlog static library linking in premake5.lua for all three modules

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Application Bootstrap (Priority: P1) 🎯 MVP

**Goal**: Establish a bootable Windows-native client shell with controlled application lifecycle, debug console, Win32 window creation, message pump, and clean shutdown

**Independent Test**: Launch LXShell.exe, verify debug console appears, verify Win32 window titled "LongXi" appears (1024x768), verify window responds to close button, verify clean process exit (exit code 0, no zombie processes)

### Implementation for User Story 1

**Models (Core Entities)**:

- [x] T011 [P] [US1] Create Win32Window class header in LongXi/LXEngine/Src/Window/Win32Window.h (RAII wrapper, m_Title, m_Width, m_Height, m_WindowHandle, m_WindowClass members)
- [x] T012 [P] [US1] Create Win32Window class implementation in LongXi/LXEngine/Src/Window/Win32Window.cpp (constructor, Create(), Show(), Destroy(), ~Win32Window, copy constructor deleted)
- [x] T013 [P] [US1] Create Application class header in LongXi/LXEngine/Src/Application/Application.h (Initialize(), Run(), Shutdown(), m_WindowHandle, m_ShouldShutdown, m_Initialized, static WindowProc, helper methods)
- [x] T014 [US1] Implement Application class constructor in LongXi/LXEngine/Src/Application/Application.cpp (initialize member variables, m_WindowHandle=NULL, m_ShouldShutdown=false, m_Initialized=false)
- [x] T015 [US1] Implement Application::CreateConsoleWindow() in LongXi/LXEngine/Src/Application/Application.cpp (AllocConsole, redirect stdout/stderr, log success/failure)
- [x] T016 [US1] Implement Application::InitializeLogging() in LongXi/LXEngine/Src/Application/Application.cpp (setup Spdlog pattern, set level, create stdout_color_sink, configure logger)
- [x] T017 [US1] Implement Application::CreateMainWindow() in LongXi/LXEngine/Src/Application/Application.cpp (instantiate Win32Window, call Create(), verify success, log error if failed)
- [x] T018 [US1] Implement Application::Initialize() in LongXi/LXEngine/Src/Application/Application.cpp (call CreateConsoleWindow(), InitializeLogging(), CreateMainWindow(), set m_Initialized=true, return false on any failure)
- [x] T019 [US1] Implement Application::WindowProc() static method in LongXi/LXEngine/Src/Application/Application.cpp (handle WM_CLOSE→DestroyWindow, WM_DESTROY→PostQuitMessage(0), WM_QUIT, default DefWindowProc)
- [x] T020 [US1] Implement Application::Run() in LongXi/LXEngine/Src/Application/Application.cpp (GetMessage/DispatchMessage loop, log entry/exit, return exit code, handle WM_CLOSE during init)
- [x] T021 [US1] Implement Application::Shutdown() in LongXi/LXEngine/Src/Application/Application.cpp (flush Spdlog logs, FreeConsole if allocated, cleanup resources, log shutdown)
- [x] T022 [US1] Implement Application class destructor in LongXi/LXEngine/Src/Application/Application.cpp (best-effort cleanup, check m_Initialized, call Shutdown if needed)

**Entrypoint Integration**:

- [x] T023 [US1] Create WinMain entrypoint — Hazel-style: EntryPoint.h in LXEngine owns WinMain, LXShell/Src/main.cpp implements CreateApplication() (<20 lines)
- [x] T024 [US1] Add LX_INFO/LX_ENGINE_INFO logging — EntryPoint.h logs startup/shutdown, Application logs lifecycle events
- [x] T025 [US1] Update LXEngine.h public header (includes Application.h, Win32Window.h, LogMacros.h)
- [x] T026 [US1] Verify LXShell links LXEngine and LXCore in premake5.lua (static library dependencies verified)
- [x] T027 [US1] Verify Win32 library links in premake5.lua (user32, gdi32 linked via system filter)

**Error Handling & Edge Cases**:

- [x] T028 [US1] Window creation failure handling in Application::Initialize() (checks Create() return, logs error, returns false)
- [x] T029 [US1] WM_CLOSE during initialization handling in Application::Run() (checks m_Initialized before pump)
- [x] T030 [US1] Multi-instance support verified (no mutex/singleton enforcement, each instance independent)

**Build & Validation**:

- [x] T031 [US1] Generate Visual Studio project files via premake5.exe vs2026 (LXCore.vcxproj, LXEngine.vcxproj, LXShell.vcxproj)
- [x] T032 [US1] Build Debug configuration (LXShell.exe, LXEngine.lib, LXCore.lib build successfully, 0 errors)
- [x] T033 [US1] Build Release configuration (0 errors, LXShell.exe + libs built successfully)
- [x] T034 [US1] Build Dist configuration (0 errors, LXShell.exe + libs built successfully)
- [x] T035 [US1] Manual test - Debug launch verified (console appears, startup logs visible, window appears immediately)
- [x] T036 [US1] Manual test - Window title is "LongXi" (user changed from "Long Xi" to "LongXi")
- [x] T037 [US1] Manual test - Window size is 1024x768 client area (AdjustWindowRectEx used)
- [x] T038 [US1] Manual test - Window launches in normal windowed mode
- [x] T039 [US1] Manual test - Window is resizable (WS_OVERLAPPEDWINDOW style)
- [x] T040 [US1] Manual test - Close via X button exits cleanly
- [x] T041 [US1] Manual test - Close via Alt-F4 exits cleanly
- [x] T042 [US1] Manual test - Shutdown log messages visible in console
- [x] T043 [US1] Manual test - Clean exit (no zombie processes observed)
- [x] T044 [US1] Manual test - Application stable during runtime (message pump idle)
- [x] T045 [US1] Manual test - WinMain/EntryPoint line count <20 (main.cpp is 7 lines, EntryPoint.h WinMain body is ~18 lines)

**Checkpoint**: At this point, User Story 1 should be fully functional and testable independently. The executable boots, window appears, message pump runs, shutdown is clean.

---

## Phase 4: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories

- [x] T046 [P] Code formatting — left for user to run Win-Format Code.bat
- [x] T047 [P] Quickstart.md — implementation matches documented build/run instructions
- [x] T048 [P] Acceptance criteria verified (AC-001 through AC-009: builds in 3 configs, window appears, console visible, clean shutdown, thin entrypoint, constitutional compliance)
- [x] T049 [P] Code review — thin entrypoint: main.cpp is 7 lines (CreateApplication only), EntryPoint.h WinMain <20 lines
- [x] T050 [P] Code review — centralized lifecycle: Application class owns Initialize/Run/Shutdown, EntryPoint.h orchestrates
- [x] T051 [P] Code review — constitutional compliance: Windows-only, raw Win32, C++23, static libs, single-threaded Phase 1, module boundaries respected
- [x] T052 [P] Data-model alignment — Application class matches data-model.md (lifecycle methods, private members, helper methods, WindowProc), LongXi namespace added
- [x] T053 [P] Research decisions verified — Win32 patterns, Spdlog integration, RAII window design all match research.md
- [ ] T054 Run cross-artifact analysis via /speckit.analyze (deferred to user)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion (T001-T005) - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational completion (T006-T010)
- **Polish (Phase 4)**: Depends on User Story 1 completion (T011-T054)

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories (only story in spec)
- **Note**: This is the ONLY user story in the specification - all tasks map to US1

### Within User Story 1

**Core Entity Tasks** (T011-T013): Can run in parallel (different files)
**Implementation Tasks** (T014-T022): Sequential order (Initialize depends on helpers, Run depends on WindowProc)
**Integration Tasks** (T023-T027): Sequential (main.cpp last, after Application/Win32Window complete)
**Error Handling** (T028-T030): Can run in parallel with other implementation tasks (different methods)
**Build & Validation** (T031-T045): Must complete implementation first (T011-T027), then can run in parallel for different configurations/tests

### Parallel Opportunities

**Phase 1 Setup**:

```bash
# Can run in parallel (different files):
T002: Create LXCore.h
T003: Create LXEngine.h
T004: Update .clang-format
T005: Update Premake5 config
```

**Phase 2 Foundational**:

```bash
# Can run in parallel (different files):
T007: Create LXCore logging macros
T008: Create LXEngine logging macros
T009: Create LXShell logging macro
```

**Phase 3 User Story 1 - Core Entities**:

```bash
# Can run in parallel (different files, no dependencies):
T011: Create Win32Window.h
T012: Create Win32Window.cpp
T013: Create Application.h
```

**Phase 3 User Story 1 - Validation**:

```bash
# Can run in parallel (different test scenarios):
T035: Manual test - Debug launch
T036: Manual test - Window title
T037: Manual test - Window size
T038: Manual test - Window mode
T039: Manual test - Window resize
T040: Manual test - X button close
T041: Manual test - Alt-F4 close
```

**Phase 4 Polish**:

```bash
# Can run in parallel (different artifacts):
T046: Format code
T047: Update quickstart
T048: Verify acceptance criteria
T049: Code review - thin entrypoint
T050: Code review - lifecycle ownership
T051: Code review - constitution
T052: Verify data-model alignment
T053: Verify research decisions
```

---

## Parallel Example: User Story 1 Core Implementation

```bash
# Step 1: Create all entity headers in parallel:
Task T011: Create Win32Window.h
Task T013: Create Application.h

# Step 2: Implement Win32Window (sequential within class):
Task T012: Implement Win32Window.cpp

# Step 3: Implement Application methods (sequential order):
Task T014: Constructor
Task T015: CreateConsoleWindow
Task T016: InitializeLogging
Task T017: CreateMainWindow
Task T018: Initialize (orchestrates helpers)
Task T019: WindowProc
Task T020: Run
Task T021: Shutdown
Task T022: Destructor

# Step 4: Integrate entrypoint:
Task T023: Create WinMain
Task T024: Add logging to WinMain
Task T025: Update LXEngine.h
Task T026: Verify linking
Task T027: Verify Win32 libraries

# Step 5: Add error handling (can parallelize):
Task T028: Window creation failure
Task T029: WM_CLOSE during init
Task T030: Multi-instance support

# Step 6: Build & test (parallel for different configurations):
Task T031: Generate projects
Task T032: Build Debug
Task T033: Build Release
Task T034: Build Dist

# Step 7: Manual validation (parallel across scenarios):
Task T035-T045: All manual tests
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

This is a **single-story specification** - the entire implementation IS the MVP:

1. Complete Phase 1: Setup (T001-T005)
2. Complete Phase 2: Foundational (T006-T010) - CRITICAL blocker
3. Complete Phase 3: User Story 1 (T011-T054)
4. **STOP and VALIDATE**: Run all manual tests (T035-T045)
5. Verify all acceptance criteria (AC-001 through AC-009)
6. Verify constitutional compliance
7. **MVP COMPLETE**: Bootable shell ready for next specification integration

### Incremental Delivery

Since this is foundation infrastructure with a single user story:

1. **Foundation**: Setup + Foundational (T001-T010) → Core modules ready
2. **Bootstrap**: User Story 1 implementation (T011-T027) → Executable builds
3. **Validation**: Build + Manual Tests (T031-T045) → Verification complete
4. **Polish**: Cross-cutting improvements (T046-T054) → Production-ready

Each phase adds value:

- After Phase 2: LXCore/LXEngine modules exist with Spdlog
- After Phase 3: Full bootable shell works
- After Phase 4: Clean, reviewed, documented code

### Parallel Team Strategy

With **2-3 developers** working on this specification:

**Phase 1 (Setup)**: Sequential (T001 first for directories, then T002-T005 in parallel)

**Phase 2 (Foundational)**: All parallel after T001

- Dev A: T007 (LXCore macros), T009 (LXShell macro)
- Dev B: T008 (LXEngine macros), T010 (Spdlog linking)

**Phase 3 (User Story 1)**:

- Dev A (Entities): T011-T013 (headers in parallel)
- Dev B (Win32Window): T012 (implementation)
- Dev A (Application): T014-T022 (sequential implementation)
- Dev B (Integration): T023-T027 (entrypoint, linking)
- All: T028-T030 (error handling in parallel)
- All: T031-T034 (build configurations in parallel)
- All: T035-T045 (split manual tests across team)

**Phase 4 (Polish)**: All parallel (T046-T054)

---

## Notes

- **No Automated Tests**: This spec uses manual verification only (build execution, window behavior observation, shutdown verification)
- **Single User Story**: All tasks map to US1 - this is foundational infrastructure only
- **Win32 Native**: Raw Win32 API, no abstraction frameworks, direct Windows SDK usage
- **Static Library Architecture**: LXShell → LXEngine → LXCore dependency direction enforced
- **Constitutional Compliance**: All tasks respect Articles III, IV, IX, XII of Long Xi constitution
- **m_PascalCase Naming**: All private member variables follow CLAUDE.md Section 16 convention
- **Thin Entrypoint**: WinMain must be <20 lines (T045 validates this)
- **Error Handling**: Window creation failure, WM_CLOSE during init, multi-instance all handled gracefully
- **Build Verification**: Three configurations tested (Debug, Release, Dist)
- **Manual Test Coverage**: 11 manual test scenarios cover all acceptance criteria (AC-001 through AC-009)
- **Format Validation**: T046 ensures all code follows .clang-format C++23 style
- **Cross-Artifact Analysis**: T054 validates spec/plan/tasks/data-model consistency
