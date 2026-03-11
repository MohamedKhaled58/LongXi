# Tasks: Developer Integration Layer (LXShell + Dear ImGui)

**Input**: Design documents from `/specs/013-developer-integration-layer/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/, quickstart.md

**Tests**: No dedicated automated test tasks are included because the feature specification requires manual/runtime validation and does not mandate a TDD workflow.

**Organization**: Tasks are grouped by user story for independent implementation and validation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parallelizable task (different files, no dependency on incomplete tasks)
- **[Story]**: User story label (`US1`..`US5`) for story-phase tasks only
- Every task includes explicit file path(s)

## Phase 1: Setup (Project Initialization)

**Purpose**: Prepare dependency source and shell-side scaffolding for DebugUI.

- [x] T001 Add Dear ImGui git submodule at `Vendor/imgui` and record submodule pointer in `.gitmodules`
- [x] T002 [P] Create DebugUI directory and base files `LongXi/LXShell/Src/DebugUI/DebugUI.h` and `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T003 [P] Create panel file stubs in `LongXi/LXShell/Src/DebugUI/Panels/EnginePanel.h`, `LongXi/LXShell/Src/DebugUI/Panels/EnginePanel.cpp`, `LongXi/LXShell/Src/DebugUI/Panels/SceneInspector.h`, `LongXi/LXShell/Src/DebugUI/Panels/SceneInspector.cpp`, `LongXi/LXShell/Src/DebugUI/Panels/TextureViewer.h`, `LongXi/LXShell/Src/DebugUI/Panels/TextureViewer.cpp`, `LongXi/LXShell/Src/DebugUI/Panels/CameraPanel.h`, `LongXi/LXShell/Src/DebugUI/Panels/CameraPanel.cpp`, `LongXi/LXShell/Src/DebugUI/Panels/InputMonitor.h`, and `LongXi/LXShell/Src/DebugUI/Panels/InputMonitor.cpp`
- [x] T004 Update LXShell build includes for ImGui in `LongXi/LXShell/premake5.lua`
- [x] T005 Add required ImGui core/backend compile units to LXShell in `LongXi/LXShell/premake5.lua`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish lifecycle contracts, build gating, and cross-cutting integration points required by all stories.

**CRITICAL**: Complete this phase before user-story implementation.

- [x] T006 Add development-build compile gating for DebugUI path in `LongXi/LXShell/premake5.lua`
- [x] T007 Implement DebugUI subsystem interface (initialize, frame, resize, input route, shutdown) in `LongXi/LXShell/Src/DebugUI/DebugUI.h` and `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T008 Implement DebugUI init failure degraded-mode policy and warnings in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T009 Wire DebugUI ownership into shell runtime startup/shutdown flow in `LongXi/LXShell/Src/main.cpp`
- [x] T010 [P] Define shared panel data view-model structs in `LongXi/LXShell/Src/DebugUI/DebugUI.h`
- [x] T011 [P] Add DebugUI initialization and lifecycle log events in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`

**Checkpoint**: Foundation complete; user stories can be delivered independently.

---

## Phase 3: User Story 1 - Initialize and Run Debug UI Lifecycle (Priority: P1) MVP

**Goal**: DebugUI initializes after Engine and renders each frame, then shuts down safely before renderer destruction.

**Independent Test**: Start LXShell (development build), confirm DebugUI initializes, renders once per frame, and shuts down cleanly before renderer teardown.

- [x] T012 [US1] Implement Dear ImGui context creation and backend initialization in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T013 [US1] Implement per-frame Dear ImGui begin/render/submit flow in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T014 [US1] Integrate DebugUI render call in frame pipeline before present in `LongXi/LXShell/Src/main.cpp`
- [x] T015 [US1] Implement DebugUI shutdown sequence and backend teardown in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T016 [US1] Ensure shutdown ordering enforces DebugUI before renderer destruction in `LongXi/LXShell/Src/main.cpp`

**Checkpoint**: US1 lifecycle works as a standalone MVP.

---

## Phase 4: User Story 2 - Route Input Correctly Between Debug UI and Engine Input (Priority: P1)

**Goal**: Input events are offered to DebugUI first, with InputSystem receiving only non-consumed events.

**Independent Test**: Interact with UI controls and verify consumed events do not update InputSystem state while non-consumed events do.

- [x] T017 [US2] Implement Win32 event consumption bridge in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T018 [US2] Route window callbacks through DebugUI-first decision logic in `LongXi/LXShell/Src/main.cpp`
- [x] T019 [US2] Enforce non-forwarding of consumed events to engine input in `LongXi/LXShell/Src/main.cpp`
- [x] T020 [P] [US2] Track last routing decision state for monitor display in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`

**Checkpoint**: US2 input routing is deterministic and independently verifiable.

---

## Phase 5: User Story 3 - Inspect Runtime State Through Debug Panels (Priority: P1)

**Goal**: Implement five panels for live engine/scene/texture/camera/input inspection and editing.

**Independent Test**: Open all panels and verify each displays live data; scene and camera edits apply immediately.

- [x] T021 [P] [US3] Implement EnginePanel data rendering in `LongXi/LXShell/Src/DebugUI/Panels/EnginePanel.cpp`
- [x] T022 [P] [US3] Implement SceneInspector hierarchy + selection UI in `LongXi/LXShell/Src/DebugUI/Panels/SceneInspector.cpp`
- [x] T023 [US3] Implement SceneInspector transform edit application path in `LongXi/LXShell/Src/DebugUI/Panels/SceneInspector.cpp`
- [x] T024 [P] [US3] Implement TextureViewer list rendering in `LongXi/LXShell/Src/DebugUI/Panels/TextureViewer.cpp`
- [x] T025 [P] [US3] Implement CameraPanel edit controls and apply path in `LongXi/LXShell/Src/DebugUI/Panels/CameraPanel.cpp`
- [x] T026 [P] [US3] Implement InputMonitor display in `LongXi/LXShell/Src/DebugUI/Panels/InputMonitor.cpp`
- [x] T027 [US3] Integrate panel draw dispatch order in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`
- [x] T028 [US3] Add panel-open and camera-update logs in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp` and `LongXi/LXShell/Src/DebugUI/Panels/CameraPanel.cpp`

**Checkpoint**: US3 provides complete runtime inspection toolkit.

---

## Phase 6: User Story 4 - Validate End-to-End Rendering Integration (Priority: P2)

**Goal**: Boot a development test scene that validates VFS, texture, sprite, scene, camera, and debug overlay in one run.

**Independent Test**: Launch with validation scene and verify test node renders with overlay while panel data reflects active systems.

- [x] T029 [US4] Add development-only test scene bootstrap path in `LongXi/LXShell/Src/main.cpp`
- [x] T030 [US4] Create root and test sprite node initialization flow in `LongXi/LXShell/Src/main.cpp`
- [x] T031 [US4] Wire test texture load path `Data/texture/test.dds` via engine interfaces in `LongXi/LXShell/Src/main.cpp`
- [x] T032 [US4] Add failure logs and fallback behavior for missing test texture in `LongXi/LXShell/Src/main.cpp`

**Checkpoint**: US4 validates cross-subsystem rendering integration path.

---

## Phase 7: User Story 5 - Restrict DebugUI to Development Builds (Priority: P2)

**Goal**: Ensure DebugUI is present only in development/debug targets and absent from production runtime path.

**Independent Test**: Compare debug and release builds; DebugUI is available only in development/debug configuration.

- [x] T033 [US5] Wrap DebugUI runtime path behind development-build guards in `LongXi/LXShell/Src/main.cpp`
- [x] T034 [US5] Ensure release build excludes ImGui compile units or disables DebugUI code path in `LongXi/LXShell/premake5.lua`
- [x] T035 [US5] Add explicit production-disable log or no-op guard behavior in `LongXi/LXShell/Src/DebugUI/DebugUI.cpp`

**Checkpoint**: US5 enforces production-safe exclusion policy.

---

## Phase 8: Polish and Cross-Cutting Concerns

**Purpose**: Final consistency, contract alignment, and manual validation artifacts.

- [x] T036 [P] Reconcile implementation with lifecycle/input contracts in `specs/013-developer-integration-layer/contracts/debugui-engine-runtime-contract.md`
- [x] T037 [P] Update runbook with actual validation notes in `specs/013-developer-integration-layer/quickstart.md`
- [ ] T038 Execute debug-build and release-build manual smoke checks and record outcomes in `specs/013-developer-integration-layer/quickstart.md`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: no prerequisites
- **Phase 2 (Foundational)**: depends on Phase 1 and blocks all stories
- **Phase 3-7 (User Stories)**: depend on Phase 2 completion
- **Phase 8 (Polish)**: depends on implemented story scope

### User Story Dependencies

- **US1 (P1)**: starts immediately after Foundational
- **US2 (P1)**: depends on US1 lifecycle/runtime ownership
- **US3 (P1)**: depends on US1 lifecycle and US2 input routing plumbing
- **US4 (P2)**: depends on US1 lifecycle and core panel visibility from US3
- **US5 (P2)**: depends on setup/foundational build wiring and US1 runtime path

### Suggested Story Completion Order

1. US1 (MVP)
2. US2
3. US3
4. US4
5. US5

---

## Parallel Execution Examples

### US1

```text
T012 [US1] ImGui init/shutdown core in DebugUI.cpp
T014 [US1] Render pipeline hook in main.cpp
```

### US2

```text
T017 [US2] DebugUI event consume bridge in DebugUI.cpp
T020 [US2] Routing decision tracking state in DebugUI.cpp
```

### US3

```text
T021 [US3] EnginePanel implementation
T024 [US3] TextureViewer implementation
T026 [US3] InputMonitor implementation
```

### US4

```text
T030 [US4] Test node bootstrap
T032 [US4] Missing texture fallback logging
```

### US5

```text
T033 [US5] Runtime guard in main.cpp
T034 [US5] Build exclusion in premake5.lua
```

---

## Implementation Strategy

### MVP First (Recommended)

1. Complete Phase 1 and Phase 2.
2. Deliver US1 only.
3. Validate lifecycle, frame rendering, and shutdown order.

### Incremental Delivery

1. Add US2 input routing correctness.
2. Add US3 panel suite for full runtime inspection.
3. Add US4 integration validation scene.
4. Add US5 production build restrictions.
5. Finish with Phase 8 polish and runbook updates.

### Team Parallel Strategy

- Engineer A: DebugUI core lifecycle (`DebugUI.cpp`, `main.cpp` integration)
- Engineer B: Panel implementations (`Panels/*.cpp`)
- Engineer C: Build/config + validation scene wiring (`premake5.lua`, `main.cpp`, runbook/contracts)

---

## Notes

- Tasks are immediately executable and file-specific.
- All story phases are independently testable using declared criteria.
- All tasks follow required checklist format with ID, optional `[P]`, optional `[US#]`, and path.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
