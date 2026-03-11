# Tasks: Camera System

**Input**: Design documents from `/specs/012-camera-system/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/, quickstart.md

**Tests**: No dedicated automated test tasks are included because the specification does not require TDD/automated tests for this feature; validation is manual/runtime as defined in spec and quickstart.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no blocking dependency)
- **[Story]**: User story label (`US1`..`US6`) for story-phase tasks
- Every task includes exact file path(s)

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish camera implementation scaffold and include surface.

- [X] T001 Create camera class scaffold files `LongXi/LXEngine/Src/Scene/Camera.h` and `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T002 [P] Export camera public header include in `LongXi/LXEngine/LXEngine.h`
- [X] T003 [P] Add camera include wiring in `LongXi/LXEngine/Src/Scene/Scene.h` and `LongXi/LXEngine/Src/Scene/Scene.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core primitives and API surfaces required by all user stories.

**⚠️ CRITICAL**: Complete this phase before starting user-story implementation.

- [X] T004 [P] Add `Matrix4` row-major type to `LongXi/LXEngine/Src/Math/Math.h`
- [X] T005 [P] Add camera consume API and storage declarations to `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T006 Implement `DX11Renderer::SetViewProjection(...)` matrix consume path in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T007 Implement viewport width/height tracking on renderer initialize/resize in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T008 [P] Add renderer viewport accessors needed by scene bootstrap in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h` and `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T009 Update scene initialization call contract in `LongXi/LXEngine/Src/Scene/Scene.h`, `LongXi/LXEngine/Src/Scene/Scene.cpp`, `LongXi/LXEngine/Src/Engine/Engine.h`, and `LongXi/LXEngine/Src/Engine/Engine.cpp`

**Checkpoint**: Foundation complete - user stories can be implemented.

---

## Phase 3: User Story 1 - Scene Provides Active Camera (Priority: P1) 🎯 MVP

**Goal**: Scene owns one active camera with valid default state/matrices immediately after initialization.

**Independent Test**: After engine+scene init, `Scene::GetActiveCamera()` is valid, default transform is `(0,0,-10)/(0,0,0)`, and cached matrices are ready before first render.

- [X] T010 [P] [US1] Add `Camera m_Camera` member and `Camera& GetActiveCamera()` API in `LongXi/LXEngine/Src/Scene/Scene.h`
- [X] T011 [US1] Initialize default camera parameters in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T012 [US1] Derive initial camera aspect ratio from renderer/backbuffer dimensions in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T013 [US1] Compute initial cached view/projection matrices during scene init in `LongXi/LXEngine/Src/Scene/Scene.cpp` and `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T014 [US1] Emit `[Camera] Initialized` initialization log in `LongXi/LXEngine/Src/Scene/Scene.cpp`

**Checkpoint**: US1 is independently functional and render bootstrap-ready.

---

## Phase 4: User Story 2 - View Matrix Updates from Transform (Priority: P1)

**Goal**: Camera view matrix reflects runtime position/rotation updates.

**Independent Test**: Changing `SetPosition`/`SetRotation` affects view matrix by next render, with expected LH/YXZ behavior.

- [X] T015 [P] [US2] Add transform field/getter API (`Position`, `RotationDegrees`) to `LongXi/LXEngine/Src/Scene/Camera.h`
- [X] T016 [US2] Implement `SetPosition`/`SetRotation` dirty-flag behavior in `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T017 [US2] Implement `Camera::UpdateViewMatrix()` using degree-to-radian YXZ left-handed math in `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T018 [P] [US2] Add pre-render view dirty sync call in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T019 [US2] Emit `[Camera] View matrix updated` on successful recompute in `LongXi/LXEngine/Src/Scene/Camera.cpp`

**Checkpoint**: US2 works independently with deterministic view updates.

---

## Phase 5: User Story 3 - Perspective Projection Matrix (Priority: P1)

**Goal**: Camera computes and serves perspective projection with validated parameters.

**Independent Test**: Projection matrix reflects FOV/aspect/near/far and updates correctly when projection inputs change.

- [X] T020 [P] [US3] Add projection parameter field/getter API (`FOV`, `Aspect`, `Near`, `Far`) to `LongXi/LXEngine/Src/Scene/Camera.h`
- [X] T021 [US3] Implement `SetFOV` and `SetNearFar` with clamping/warning policy in `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T022 [US3] Implement `Camera::UpdateProjectionMatrix(int viewportWidth, int viewportHeight)` in `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T023 [US3] Enforce cached-only matrix getters (`GetViewMatrix`, `GetProjectionMatrix`) in `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T024 [P] [US3] Add pre-render projection dirty sync call in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T025 [US3] Emit `[Camera] Projection updated` on valid projection recompute in `LongXi/LXEngine/Src/Scene/Camera.cpp`

**Checkpoint**: US3 projection behavior is independently correct and stable.

---

## Phase 6: User Story 4 - Renderer Receives Camera Matrices First (Priority: P1)

**Goal**: Scene pushes camera matrices to renderer before any node submit traversal.

**Independent Test**: Every `Scene::Render()` calls `DX11Renderer::SetViewProjection(...)` before `SceneNode::Submit()` traversal.

- [X] T026 [US4] Call `DX11Renderer::SetViewProjection(view, projection)` before render traversal in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T027 [US4] Enforce consume-only renderer contract (no camera getter API) in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h` and `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T028 [US4] Persist latest consumed camera matrices in renderer private state in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h` and `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T029 [US4] Add ordering guard comments/assert-ready checks around camera handoff path in `LongXi/LXEngine/Src/Scene/Scene.cpp`

**Checkpoint**: US4 matrix handoff contract is independently verified.

---

## Phase 7: User Story 5 - Projection Updates on Resize (Priority: P2)

**Goal**: Resize chain updates camera projection/aspect without invalid matrix states.

**Independent Test**: Valid resize updates aspect/projection; zero or negative resize keeps prior projection unchanged.

- [X] T030 [US5] Forward valid resize dimensions from scene to active camera in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T031 [P] [US5] Preserve engine resize forwarding order (`Renderer` -> `SpriteRenderer` -> `Scene`) in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T032 [US5] Handle invalid resize dimensions by retaining prior projection in `LongXi/LXEngine/Src/Scene/Scene.cpp` and `LongXi/LXEngine/Src/Scene/Camera.cpp`
- [X] T033 [US5] Validate resize-driven aspect ratio source from renderer viewport in `LongXi/LXEngine/Src/Scene/Scene.cpp` and `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`

**Checkpoint**: US5 resize behavior is independently reliable.

---

## Phase 8: User Story 6 - Runtime Camera Parameter Updates (Priority: P2)

**Goal**: Runtime camera parameter changes propagate safely and predictably to next frame.

**Independent Test**: Runtime updates to position/rotation/FOV/near/far are reflected at next render sync, with clamping warnings where needed.

- [X] T034 [US6] Ensure runtime setter changes are consumed by next pre-render sync in `LongXi/LXEngine/Src/Scene/Camera.cpp` and `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T035 [P] [US6] Add developer runtime camera adjustment hook for manual verification in `LongXi/LXShell/Src/main.cpp`
- [X] T036 [US6] Normalize large rotation inputs through periodic degree handling in `LongXi/LXEngine/Src/Scene/Camera.cpp`

**Checkpoint**: US6 runtime mutability is independently validated.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency, validation, and documentation alignment.

- [X] T037 [P] Align implementation notes with final camera API in `specs/012-camera-system/quickstart.md`
- [X] T038 [P] Reconcile final signatures/ordering with contract doc in `specs/012-camera-system/contracts/camera-scene-renderer.md`
- [X] T039 Run build + manual smoke validation and record outcomes in `specs/012-camera-system/quickstart.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: no prerequisites
- **Phase 2 (Foundational)**: depends on Phase 1; blocks all user stories
- **Phases 3-8 (User Stories)**: depend on Phase 2 completion
- **Phase 9 (Polish)**: depends on completion of selected user stories

### User Story Dependencies

- **US1 (P1)**: starts immediately after Foundational
- **US2 (P1)**: depends on US1 camera ownership/bootstrap
- **US3 (P1)**: depends on US1 camera baseline; can overlap late US2 work
- **US4 (P1)**: depends on US1 + US2 + US3 matrix availability
- **US5 (P2)**: depends on US3 projection pipeline and US4 renderer handoff path
- **US6 (P2)**: depends on US2/US3 setter+dirty model

### Suggested Story Completion Order

1. US1 (MVP bootstrap)
2. US2 + US3
3. US4
4. US5 + US6

---

## Parallel Execution Examples

### US1

```bash
# Parallelizable after camera scaffold exists:
T010 [US1] Scene API/member changes (Scene.h)
T012 [US1] Initial aspect-ratio bootstrap logic (Scene.cpp)
```

### US2

```bash
# Parallelizable chunks:
T015 [US2] Camera transform API declarations (Camera.h)
T018 [US2] Scene pre-render view sync hook (Scene.cpp)
```

### US3

```bash
# Parallelizable chunks:
T020 [US3] Projection API declarations (Camera.h)
T024 [US3] Scene pre-render projection sync hook (Scene.cpp)
```

### US4

```bash
# Parallelizable by file split:
Implement Scene render call-order in Scene.cpp
Implement renderer consume API internals in DX11Renderer.h/.cpp
```

### US5

```bash
# Parallelizable chunks:
T031 [US5] Engine resize forwarding checks (Engine.cpp)
T030 [US5] Scene resize forwarding to camera (Scene.cpp)
```

### US6

```bash
# Parallelizable chunks:
T035 [US6] Manual runtime hook in LXShell main.cpp
T036 [US6] Rotation normalization in Camera.cpp
```

---

## Implementation Strategy

### MVP First (Recommended)

1. Complete Phase 1 and Phase 2.
2. Complete **US1 only** (Phase 3).
3. Validate first-render camera bootstrap behavior.
4. Demo/merge MVP foundation.

### Incremental Delivery

1. Add US2 and US3 for full matrix computation.
2. Add US4 for renderer handoff contract.
3. Add US5 and US6 for resize/runtime robustness.
4. Finish with Phase 9 polish.

### Team Parallel Strategy

- One engineer: execute sequentially in story order above.
- Multiple engineers: split by file ownership after Foundational:
- Engineer A: `Scene/*`
- Engineer B: `Renderer/*`
- Engineer C: `Math/*` + `LXShell/*` manual verification path

---

## Notes

- Every user story phase is independently testable with its stated criterion.
- All checklist tasks follow required format: checkbox + TaskID + optional `[P]` + optional story label + exact file path.
- Keep changes scoped to camera/math/scene/renderer integration to maintain constitutional compliance.


## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
