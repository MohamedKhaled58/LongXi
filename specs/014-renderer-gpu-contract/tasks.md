# Tasks: Spec 014 - Renderer API and GPU State Contract

**Input**: Design documents from `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/`

**Tests**: No test-code authoring was explicitly requested; tasks use runtime validation steps from `quickstart.md`.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish renderer API surface and file layout for backend isolation.

- [X] T001 Create backend-agnostic renderer public interface in `LongXi/LXEngine/Src/Renderer/Renderer.h`
- [X] T002 Create backend-agnostic renderer shared types in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T003 Create DX11 backend folder structure for renderer internals in `LongXi/LXEngine/Src/Renderer/Backend/DX11/`
- [X] T004 [P] Add renderer boundary audit script skeleton in `Scripts/Audit-RendererBoundaries.ps1`
- [X] T005 [P] Register new renderer interface files in build configuration at `premake5.lua`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement lifecycle/state foundations required by all user stories.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T006 Add frame lifecycle and pass state models to `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T007 Refactor renderer ownership contract in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h` to implement backend-agnostic API
- [X] T008 Implement lifecycle gating and contract-violation handling in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T009 Implement baseline GPU state rebind at frame start in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T010 Add renderer recovery state handling for present/device/swapchain failures in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T011 Update renderer ownership and API usage in `LongXi/LXEngine/Src/Engine/Engine.h`
- [X] T012 Update engine frame lifecycle orchestration to renderer API calls in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T013 Update renderer API contract document to match implemented lifecycle/state model in `specs/014-renderer-gpu-contract/contracts/renderer-api-contract.md`

**Checkpoint**: Foundation ready - user story implementation can now proceed.

---

## Phase 3: User Story 1 - Deterministic Frame Rendering (Priority: P1) 🎯 MVP

**Goal**: Deliver deterministic frame rendering regardless of external pass state changes.

**Independent Test**: Run Scene + Sprite + Debug UI for 10,000 frames and confirm no renderer-state-related blank frames or lifecycle-order errors.

- [X] T014 [US1] Update scene render entry to consume renderer pass contract in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T015 [US1] Refactor sprite renderer initialization to consume renderer API instead of direct context access in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`
- [X] T016 [US1] Route sprite draw submission through renderer API lifecycle in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp`
- [X] T017 [US1] Add renderer-owned external pass bridge interface usage in `LongXi/LXShell/Src/ImGui/ImGuiLayer.h`
- [X] T018 [US1] Implement Debug UI external pass execution through renderer bridge in `LongXi/LXShell/Src/ImGui/ImGuiLayer.cpp`
- [X] T019 [US1] Integrate Scene->Sprite->DebugUI pass sequencing in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T020 [US1] Add deterministic lifecycle diagnostics for runtime validation in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T021 [US1] Execute and document P1 runtime validation scenario in `specs/014-renderer-gpu-contract/quickstart.md`

**Checkpoint**: User Story 1 is independently functional and testable as MVP.

---

## Phase 4: User Story 2 - Reliable Resize and Viewport State (Priority: P2)

**Goal**: Make resize and viewport updates reliable, deterministic, and frame-safe.

**Independent Test**: Run 100 resize/minimize/restore iterations, including mid-frame resize events, and confirm rendering recovers on next valid frame.

- [X] T022 [US2] Add queued resize fields and invariants to renderer state in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T023 [US2] Implement mid-frame resize queueing and next-frame application in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T024 [US2] Recreate render targets/depth resources and apply viewport on resize in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T025 [US2] Enforce zero-size safe handling path for minimized window states in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T026 [US2] Ensure engine resize forwarding keeps renderer as sole owner in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T027 [US2] Align scene/camera resize interactions with renderer-driven aspect updates in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T028 [US2] Execute and document resize stress validation scenario in `specs/014-renderer-gpu-contract/quickstart.md`

**Checkpoint**: User Stories 1 and 2 are independently testable and stable.

---

## Phase 5: User Story 3 - Enforced Renderer Ownership Boundary (Priority: P3)

**Goal**: Enforce architecture boundary so non-renderer modules cannot use DirectX types/APIs directly.

**Independent Test**: Run boundary audit and verify no non-renderer module includes `d3d11.h`/`dxgi.h` or depends on backend-native renderer types.

- [X] T029 [US3] Remove backend-native includes from renderer consumer header in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`
- [X] T030 [US3] Move DX11 sprite pipeline specifics into backend implementation in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.h`
- [X] T031 [US3] Implement DX11 sprite pipeline backend logic in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.cpp`
- [X] T032 [US3] Refactor sprite renderer to consume backend-agnostic handles/types in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp`
- [X] T033 [US3] Ensure Debug UI integration consumes renderer bridge without backend handles in `LongXi/LXShell/Src/ImGui/ImGuiLayer.cpp`
- [X] T034 [US3] Implement include boundary checks for DirectX leakage in `Scripts/Audit-RendererBoundaries.ps1`
- [X] T035 [US3] Execute boundary audit and record results in `specs/014-renderer-gpu-contract/contracts/external-pass-bridge-contract.md`

**Checkpoint**: All user stories are independently functional; renderer ownership boundary is enforceable.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final hardening, validation, and documentation consistency.

- [X] T036 [P] Update renderer API contract with final operation signatures and invariants in `specs/014-renderer-gpu-contract/contracts/renderer-api-contract.md`
- [X] T037 [P] Update data model with implementation-aligned state transitions in `specs/014-renderer-gpu-contract/data-model.md`
- [X] T038 [P] Update quickstart with final validation commands and expected logs in `specs/014-renderer-gpu-contract/quickstart.md`
- [X] T039 Run full feature validation sweep and summarize outcomes in `specs/014-renderer-gpu-contract/research.md`
- [X] T040 Run C++ formatting across modified source files via `Win-Format Code.bat`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: Starts immediately.
- **Phase 2 (Foundational)**: Depends on Phase 1; blocks all user stories.
- **Phase 3 (US1)**: Depends on Phase 2; defines MVP.
- **Phase 4 (US2)**: Depends on Phase 2; can run after US1 or in parallel with US3 if file conflicts are managed.
- **Phase 5 (US3)**: Depends on Phase 2; recommended after US1 integration to reduce merge/file conflicts.
- **Phase 6 (Polish)**: Depends on completion of targeted user stories.

### User Story Dependency Graph

```text
US1 (P1) -> MVP
US2 (P2) -> depends on Foundation, integrates with renderer lifecycle
US3 (P3) -> depends on Foundation, validates/enforces architecture boundaries

Recommended execution order: US1 -> US2 -> US3
Parallel-capable after Foundation: US2 || US3 (with coordination on shared renderer files)
```

### Parallel Opportunities

- Setup parallel tasks: `T004`, `T005`
- Polish parallel tasks: `T036`, `T037`, `T038`
- Cross-story parallelism after foundation: selected US2 and US3 tasks that do not touch the same files.

---

## Parallel Example: User Story 1

```bash
Task: "T017 [US1] Add renderer-owned external pass bridge interface usage in LongXi/LXShell/Src/ImGui/ImGuiLayer.h"
Task: "T020 [US1] Add deterministic lifecycle diagnostics for runtime validation in LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp"
```

## Parallel Example: User Story 2

```bash
Task: "T026 [US2] Ensure engine resize forwarding keeps renderer as sole owner in LongXi/LXEngine/Src/Engine/Engine.cpp"
Task: "T027 [US2] Align scene/camera resize interactions with renderer-driven aspect updates in LongXi/LXEngine/Src/Scene/Scene.cpp"
```

## Parallel Example: User Story 3

```bash
Task: "T030 [US3] Move DX11 sprite pipeline specifics into backend implementation in LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.h"
Task: "T034 [US3] Implement include boundary checks for DirectX leakage in Scripts/Audit-RendererBoundaries.ps1"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 and Phase 2.
2. Complete Phase 3 (US1).
3. Validate deterministic frame behavior and external-pass stability.
4. Stop for MVP review.

### Incremental Delivery

1. Foundation complete (Phases 1-2).
2. Deliver US1 (deterministic lifecycle and pass contract).
3. Deliver US2 (resize and viewport robustness).
4. Deliver US3 (architecture enforcement).
5. Final polish and validation.

### Suggested MVP Scope

- **MVP**: Phase 1 + Phase 2 + Phase 3 (US1 only)

---

## Notes

- All task lines follow strict checklist format.
- [P] markers are used only where tasks are file-independent.
- User-story labels are applied only to story-phase tasks.
- Boundary and validation tasks are included to satisfy architectural compliance outcomes.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.




