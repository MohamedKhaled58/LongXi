# Implementation Tasks: Engine Event Routing and Application Boundary Cleanup

**Feature**: 009-engine-routing-refactor
**Branch**: `009-event-routing-refactor`
**Generated**: 2025-03-10
**Estimate**: 4-6 hours

## Overview

This task breakdown implements architectural refactoring to separate platform concerns, move VFS configuration to Application, and introduce callback-based event routing. Tasks are organized by user story for independent implementation and testing.

**User Stories** (from spec.md):
- **US1** (P1): Clean Engine Architecture - Remove platform-specific code from Engine
- **US2** (P1): Application-Managed VFS Configuration - Move VFS mounting to Application
- **US3** (P2): Centralized Win32 Message Handling - Move WindowProc to Win32Window
- **US4** (P2): Event Routing to Engine Systems - Wire callbacks to route events
- **US5** (P3): Platform-Independent Engine Interface - Clean up public interface

---

## Phase 1: Setup

**Goal**: Prepare workspace and verify build environment

**Independent Test**: Build succeeds without errors

### Setup Tasks

- [X] T001 Verify current branch is 009-event-routing-refactor
- [X] T002 Open solution in Visual Studio 2022
- [X] T003 Confirm build configuration (Debug x64)
- [X] T004 Run Win-Build Debug to verify clean build state

---

## Phase 2: Foundational - Win32Window Callback Infrastructure

**Goal**: Add callback infrastructure to Win32Window (enables US3 and US4)

**Independent Test**: Win32Window compiles with new callback members

### Foundation Tasks

- [X] T005 [P] Add std::function callback members to Win32Window class in LongXi/LXEngine/Src/Window/Win32Window.h
- [X] T006 Verify Win32Window compilation succeeds with new callback members

---

## Phase 3: User Story 1 - Clean Engine Architecture

**Goal**: Remove platform-specific code and application configuration from Engine

**Independent Test**:
- Engine.h has no Win32 types in public interface (except HWND in Initialize parameter)
- Engine.cpp has no hardcoded VFS mount paths
- Engine compiles and links successfully

### US1 Tasks

- [X] T007 [US1] Remove VFS mount configuration code from Engine::Initialize() in LongXi/LXEngine/Src/Engine/Engine.cpp
- [X] T008 [US1] Search Engine.cpp for "Data/" and ".wdf" to verify all mount paths removed
- [X] T009 [US1] Verify Engine::Initialize() creates empty VFS without attempting mounts
- [X] T010 [US1] Build and link Engine library to verify no Win32 header dependencies in public interface
- [X] T011 [US1] Run application and verify Engine initializes successfully without VFS mounts

---

## Phase 4: User Story 2 - Application-Managed VFS Configuration

**Goal**: Move VFS mount configuration from Engine to Application

**Independent Test**:
- Application::ConfigureVirtualFileSystem() method exists and is called
- VFS mounts succeed with correct directory/archive order (Patch, Data, root, WDF)
- Engine.cpp requires no changes when mount paths are modified

### US2 Tasks

- [X] T012 [P] [US2] Add ConfigureVirtualFileSystem() declaration to Application class in LongXi/LXEngine/Src/Application/Application.h
- [X] T013 [P] [US2] Implement ConfigureVirtualFileSystem() method in LongXi/LXEngine/Src/Application/Application.cpp
- [X] T014 [US2] Add GetExecutableDirectory() call to retrieve executable directory
- [X] T015 [US2] Add VFS mount calls in correct order: Patch → Data → root → C3.wdf → data.wdf
- [X] T016 [US2] Add logging for VFS configuration steps (LX_ENGINE_INFO)
- [X] T017 [US2] Call ConfigureVirtualFileSystem() in Application::CreateEngine() after Engine::Initialize() succeeds
- [X] T018 [US2] Build and run application to verify VFS mounts succeed
- [X] T019 [US2] Verify log shows "[Application] VFS configuration complete"

---

## Phase 5: User Story 3 - Centralized Win32 Message Handling

**Goal**: Move WindowProc from Application to Win32Window

**Independent Test**:
- Win32Window::WindowProc exists as static member
- Application has no WindowProc function
- Win32 messages invoke callbacks correctly

### US3 Tasks

- [X] T020 [P] [US3] Add WindowProc static member declaration to Win32Window class in LongXi/LXEngine/Src/Window/Win32Window.h
- [X] T021 [US3] Move Application::WindowProc implementation to Win32Window::WindowProc in LongXi/LXEngine/Src/Window/Win32Window.cpp
- [X] T022 [US3] Update WindowProc to retrieve Win32Window* from GWLP_USERDATA instead of Application*
- [X] T023 [US3] Add callback invocation logic for WM_SIZE in WindowProc with OnResize callback
- [X] T024 [P] [US3] Add callback invocation logic for WM_KEYDOWN/WM_SYSKEYDOWN in WindowProc with OnKeyDown callback
- [X] T025 [P] [US3] Add callback invocation logic for WM_KEYUP/WM_SYSKEYUP in WindowProc with OnKeyUp callback
- [X] T026 [P] [US3] Add callback invocation logic for WM_MOUSEMOVE in WindowProc with OnMouseMove callback
- [X] T027 [P] [US3] Add callback invocation logic for WM_LBUTTONDOWN/WM_LBUTTONUP in WindowProc with mouse button callbacks
- [X] T028 [P] [US3] Add callback invocation logic for WM_RBUTTONDOWN/WM_RBUTTONUP in WindowProc with mouse button callbacks
- [X] T029 [P] [US3] Add callback invocation logic for WM_MBUTTONDOWN/WM_MBUTTONUP in WindowProc with mouse button callbacks
- [X] T030 [P] [US3] Add callback invocation logic for WM_MOUSEWHEEL in WindowProc with OnMouseWheel callback
- [X] T031 [US3] Add null check for Win32Window* pointer before invoking callbacks
- [X] T032 [US3] Add LX_WINDOW_INFO logging for WM_SIZE events in WindowProc
- [X] T033 [US3] Add guard for zero-area window dimensions (width > 0 && height > 0)
- [X] T034 [US3] Remove WindowProc static function from Application.cpp
- [X] T035 [US3] Remove WindowProc declaration from Application.h
- [X] T036 [US3] Build and verify Application compiles without WindowProc
- [X] T037 [US3] Run application and verify WindowProc is called from Win32Window
- [X] T038 [US3] Test window resize and verify OnResize callback invoked
- [X] T039 [US3] Test keyboard input and verify OnKeyDown callback invoked
- [X] T040 [US3] Test mouse movement and verify OnMouseMove callback invoked

---

## Phase 6: User Story 4 - Event Routing to Engine Systems

**Goal**: Wire Win32Window callbacks to Engine subsystems

**Independent Test**:
- All 7 callbacks wired in Application
- Resize events route to Engine::OnResize → DX11Renderer::OnResize
- Input events route to Engine::GetInput() → InputSystem

### US4 Tasks

- [X] T041 [P] [US4] Add WireWindowCallbacks() declaration to Application class in LongXi/LXEngine/Src/Application/Application.h
- [X] T042 [P] [US4] Implement WireWindowCallbacks() method in LongXi/LXEngine/Src/Application/Application.cpp
- [X] T043 [P] [US4] Wire OnResize callback with null safety check (m_Engine && m_Engine->IsInitialized())
- [X] T044 [P] [US4] Wire OnKeyDown callback with null safety check and forward to m_Engine->GetInput().OnKeyDown()
- [X] T045 [P] [US4] Wire OnKeyUp callback with null safety check and forward to m_Engine->GetInput().OnKeyUp()
- [X] T046 [P] [US4] Wire OnMouseMove callback with null safety check and forward to m_Engine->GetInput().OnMouseMove()
- [X] T047 [P] [US4] Wire OnMouseButtonDown callback with null safety check and forward to m_Engine->GetInput().OnMouseButtonDown()
- [X] T048 [P] [US4] Wire OnMouseButtonUp callback with null safety check and forward to m_Engine->GetInput().OnMouseButtonUp()
- [X] T049 [P] [US4] Wire OnMouseWheel callback with null safety check and forward to m_Engine->GetInput().OnMouseWheel()
- [X] T050 [US4] Add LX_APPLICATION_INFO logging to each callback lambda
- [X] T051 [US4] Call WireWindowCallbacks() in Application::CreateEngine() after Engine::Initialize() succeeds
- [X] T052 [US4] Add Engine::OnResize() declaration to Engine class in LongXi/LXEngine/Src/Engine/Engine.h
- [X] T053 [US4] Implement Engine::OnResize() method in LongXi/LXEngine/Src/Engine/Engine.cpp
- [X] T054 [US4] Add IsInitialized() check at start of Engine::OnResize()
- [X] T055 [US4] Add guard for zero-area dimensions in Engine::OnResize()
- [X] T056 [US4] Add LX_ENGINE_INFO logging in Engine::OnResize()
- [X] T057 [US4] Forward resize event to m_Renderer->OnResize(width, height)
- [X] T058 [US4] Build and verify all callback wirings compile successfully
- [X] T059 [US4] Run application and test window resize (check logs for routing)
- [X] T060 [US4] Run application and test keyboard input (press keys, check console)
- [X] T061 [US4] Run application and test mouse input (move, click, scroll wheel)

---

## Phase 7: User Story 5 - Platform-Independent Engine Interface

**Goal**: Verify Engine public interface uses platform-independent types

**Independent Test**: Inspect Engine.h and verify no Win32 types (except HWND in Initialize parameter)

### US5 Tasks

- [X] T062 [US5] Search Engine.h for HWND, WPARAM, LPARAM types (only allowed in Initialize parameter)
- [X] T063 [US5] Search Engine.h for platform-specific includes (<Windows.h>, <windowsx.h>)
- [X] T064 [US5] Verify all public method signatures use simple types (int, bool, void, references)
- [X] T065 [US5] Document any remaining platform-specific types as architectural exceptions
- [X] T066 [US5] Update Engine.h documentation if HWND needs explanation for Windows-only platform constraint

---

## Phase 8: Polish & Cross-Cutting Concerns

**Goal**: Final validation, performance verification, and cleanup

### Polish Tasks

- [X] T067 Run full application test suite (all input modalities working)
- [X] T068 Verify VFS mounts succeed by checking asset loading works
- [X] T069 Verify no VFS mount paths remain in Engine.cpp (search for "MountDirectory", "MountWdf")
- [X] T070 Verify Application::WindowProc removed (search Application.cpp for "WindowProc")
- [X] T071 Verify all 7 callback members exist in Win32Window.h
- [X] T072 Verify ConfigureVirtualFileSystem() exists in Application
- [X] T073 Verify ConfigureVirtualFileSystem() called after Engine::Initialize()
- [X] T074 Verify Engine::OnResize() exists and forwards to Renderer
- [X] T075 Check event routing logs include component names (Window, Application, Engine)
- [ ] T076 Profile resize event routing (target: < 1ms per SC-008)
- [ ] T077 Profile input event routing (target: < 100μs per SC-009)
- [X] T078 Code review: Verify architectural boundaries respected (no cross-layer coupling)
- [X] T079 Verify Build/Debug Executables/LXShell.exe runs successfully
- [X] T080 Clean up any temporary or debug code added during implementation

---

## Dependencies

### User Story Dependencies

```
US1 (P1): Clean Engine Architecture
├─ No dependencies
└─ Enables: US2, US5

US2 (P1): Application-Managed VFS Configuration
├─ Depends on: US1 (Engine must not own VFS config)
└─ Enables: US4 (VFS ready for use)

US3 (P2): Centralized Win32 Message Handling
├─ No dependencies
├─ Enables: US4 (callbacks needed)
└─ Can run parallel to: US1, US2

US4 (P2): Event Routing to Engine Systems
├─ Depends on: US1 (Engine interface clean), US2 (VFS configured), US3 (callbacks exist)
└─ Enables: US5 (interface validation)

US5 (P3): Platform-Independent Engine Interface
├─ Depends on: US1, US2, US3, US4 (all refactoring complete)
└─ Final validation
```

### Execution Order

**Priority 1 (P1 - Foundation)**:
1. Phase 1: Setup
2. Phase 2: Foundational (Win32Window callbacks)
3. Phase 3: US1 - Clean Engine Architecture
4. Phase 4: US2 - Application-Managed VFS Configuration

**Priority 2 (P2 - Message Handling & Event Routing)**:
5. Phase 5: US3 - Centralized Win32 Message Handling (parallel to P1 after setup)
6. Phase 6: US4 - Event Routing to Engine Systems

**Priority 3 (P3 - Validation)**:
7. Phase 7: US5 - Platform-Independent Engine Interface
8. Phase 8: Polish & Cross-Cutting Concerns

---

## Parallel Execution Opportunities

### Within US2 (Application-Managed VFS Configuration)
- T012, T013 can run sequentially (implementation then declaration)
- T014, T015, T016 can run in parallel once T013 is complete

### Within US3 (Centralized Win32 Message Handling)
- T020-T033 (all WindowProc callback invocations) can run in parallel
- Each callback type is independent (resize, keyboard, mouse, wheel)

### Within US4 (Event Routing to Engine Systems)
- T041-T050 (all callback wirings) can run in parallel
- T052-T057 (Engine::OnResize implementation) can run in parallel to callback wirings

### Maximum Parallelism
- **Phase 3**: T007-T011 (mostly sequential, T008 can run in parallel to T007)
- **Phase 4**: T012-T016 can run in parallel once T013 completes
- **Phase 5**: T020-T033 can run in parallel (all callback implementations)
- **Phase 6**: T041-T057 can run in parallel (all callback wirings and Engine::OnResize)

---

## MVP Scope (Minimum Viable Product)

**Definition**: Deliver US1 + US2 (P1 stories) only

**MVP Tasks**: T001-T019

**MVP Test Criteria**:
- ✅ Engine has no hardcoded VFS paths
- ✅ Application owns VFS configuration via ConfigureVirtualFileSystem()
- ✅ VFS mounts succeed with correct order
- ✅ Engine initializes and runs without application-specific configuration

**MVP Value**: Application can control asset mounting independently of Engine code

---

## Implementation Strategy

### Incremental Delivery

1. **MVP (Stories 1-2)**: Engine is clean, Application controls VFS
   - Delivers architectural separation (US1)
   - Enables application-specific asset configurations (US2)

2. **Complete (Stories 1-4)**: Full event routing
   - Win32Window owns messages (US3)
   - Callbacks route events to Engine (US4)

3. **Validated (Stories 1-5)**: Platform independence verified
   - Engine interface is platform-clean (US5)
   - Ready for future cross-platform ports

### Risk Mitigation

- **No breaking changes to existing interfaces**: All changes are internal refactors
- **Incremental compilation**: Each phase compiles and runs before proceeding
- **Rollback safe**: Each step can be independently tested and reverted
- **Performance validated**: Measure event routing latency against targets

---

## Task Format Validation

**ALL tasks follow checklist format**:
- ✅ Checkbox prefix `- [ ]`
- ✅ Sequential ID (T001-T080)
- ✅ [P] marker for parallelizable tasks
- ✅ [Story] label for user story phases (US1-US5)
- ✅ Clear description with exact file path

**Example**:
```
✅ CORRECT: - [ ] T024 [P] [US3] Add callback invocation logic for WM_KEYDOWN/WM_SYSKEYDOWN in WindowProc with OnKeyDown callback
```

---

## Success Criteria Tracking

**From spec.md SC-001 through SC-010**:

- [X] SC-001: Engine.h contains zero Win32-specific types in public interface
- [X] SC-002: Engine.cpp contains zero hardcoded VFS mount paths
- [X] SC-003: Application::WindowProc removed
- [X] SC-004: Win32Window exposes exactly 7 callback members
- [X] SC-005: All VFS mount configurations in Application::ConfigureVirtualFileSystem()
- [X] SC-006: Event routing ≤ 3 hops (verified by callback structure)
- [X] SC-007: Engine initializes with zero VFS mounts
- [ ] SC-008: Resize events propagate in < 1ms
- [ ] SC-009: Input events propagate in < 100μs
- [X] SC-010: Engine.h has zero platform-specific includes

---

## Total Task Count

**Total**: 80 tasks
- Setup: 4 tasks
- Foundational: 2 tasks
- US1 (P1): 11 tasks
- US2 (P1): 8 tasks
- US3 (P2): 21 tasks
- US4 (P2): 21 tasks
- US5 (P3): 5 tasks
- Polish: 14 tasks

**Parallel Opportunities**: 35 tasks marked [P] (44% of implementation work)

**Estimated Timeline**: 4-6 hours (matches quickstart.md estimate)

---

## Notes

- **No automated tests**: This is Phase 1 single-threaded runtime; all testing is manual via test application
- **Build frequently**: Each phase ends with build verification task
- **Log-driven verification**: Extensive use of logging macros for debugging event routing
- **Null safety emphasized**: All callback invocations include null checks to prevent crashes
- **Incremental compilation**: Build after each significant change to catch errors early
