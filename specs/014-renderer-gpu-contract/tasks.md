# Tasks: Spec 014 - Renderer API and Backend Architecture

**Input**: Design documents from `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/`

**Tests**: No dedicated test-code authoring requested; validation tasks use runtime/build/audit scenarios from `quickstart.md`.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish public renderer surface and backend folder/build wiring.

- [X] T001 Create or update backend-agnostic renderer public entrypoint in `LongXi/LXEngine/Src/Renderer/Renderer.h`
- [X] T002 Create or update backend-agnostic renderer shared types in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T003 Create/verify DX11 backend structure in `LongXi/LXEngine/Src/Renderer/Backend/DX11/`
- [X] T004 [P] Register renderer API/backend files in `LongXi/LXEngine/premake5.lua`
- [X] T005 [P] Create/verify boundary audit script in `Scripts/Audit-RendererBoundaries.ps1`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement lifecycle/state/recovery foundations required before user stories.

**⚠️ CRITICAL**: No user story work begins until this phase is complete.

- [X] T006 Define lifecycle/pass/recovery state models in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T007 Add renderer lifecycle/pass/recovery members and API surface in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T008 Implement frame lifecycle enforcement (`BeginFrame`, `EndFrame`, `Present`) in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T009 Implement baseline GPU state rebind at frame start in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T010 Implement pass sequencing and state application in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T011 Implement queued resize and safe apply behavior in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T012 Implement present/device/swapchain recovery policy in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T013 Update engine render orchestration to renderer lifecycle contract in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T014 Update engine renderer-facing declarations in `LongXi/LXEngine/Src/Engine/Engine.h`

**Checkpoint**: Foundation ready; user stories can proceed.

---

## Phase 3: User Story 1 - Deterministic Frame Rendering (Priority: P1) 🎯 MVP

**Goal**: Ensure deterministic rendering with explicit renderer lifecycle and no implicit state reliance.

**Independent Test**: Run Scene + Sprite + DebugUI for long-run frames and verify no lifecycle-order violations or blank-frame regressions.

- [X] T015 [US1] Refactor scene renderer dependency to public renderer API in `LongXi/LXEngine/Src/Scene/Scene.h`
- [X] T016 [US1] Apply scene render lifecycle contract usage in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T017 [P] [US1] Refactor scene-node submit signatures to renderer API in `LongXi/LXEngine/Src/Scene/SceneNode.h`
- [X] T018 [US1] Apply scene-node traversal submit behavior to renderer API in `LongXi/LXEngine/Src/Scene/SceneNode.cpp`
- [X] T019 [US1] Refactor sprite renderer public dependency to renderer API in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`
- [X] T020 [US1] Define DX11 sprite pipeline backend surface in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.h`
- [X] T021 [US1] Implement DX11 sprite pipeline backend operations in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.cpp`
- [X] T022 [US1] Route sprite rendering through backend module and lifecycle-safe calls in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp`
- [X] T023 [US1] Integrate renderer-owned external pass bridge in engine loop in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T024 [US1] Use external-pass bridge for shell debug rendering in `LongXi/LXShell/Src/ImGui/ImGuiLayer.cpp`
- [X] T025 [US1] Document deterministic frame validation scenario/results in `specs/014-renderer-gpu-contract/quickstart.md`

**Checkpoint**: User Story 1 is independently functional and testable.

---

## Phase 4: User Story 2 - Reliable Resize and Viewport State (Priority: P2)

**Goal**: Guarantee resize-safe render-target/depth/viewport behavior with deterministic recovery.

**Independent Test**: Run repeated resize/minimize/restore scenarios and verify stable recovery.

- [X] T026 [US2] Add/verify queued resize state fields in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T027 [US2] Implement resize resource recreation + viewport apply flow in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T028 [US2] Ensure engine resize forwarding preserves renderer ownership in `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T029 [US2] Align scene/camera resize interaction with renderer-driven dimensions in `LongXi/LXEngine/Src/Scene/Scene.cpp`
- [X] T030 [US2] Update resize contract invariants in `specs/014-renderer-gpu-contract/contracts/renderer-api-contract.md`
- [X] T031 [US2] Document resize stress validation scenario/results in `specs/014-renderer-gpu-contract/quickstart.md`

**Checkpoint**: User Stories 1 and 2 are independently testable and stable.

---

## Phase 5: User Story 3 - Enforced Backend-Agnostic Engine Boundary (Priority: P3)

**Goal**: Enforce architectural boundary so engine modules cannot leak backend headers/types.

**Independent Test**: Run boundary audit and verify no DirectX header/type leakage outside backend implementation modules.

- [X] T032 [US3] Remove backend-native renderer includes from public scene header in `LongXi/LXEngine/Src/Scene/Scene.h`
- [X] T033 [US3] Remove backend-native renderer includes from sprite public header in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`
- [X] T034 [US3] Replace backend-specific texture handle exposure with renderer types in `LongXi/LXEngine/Src/Texture/Texture.h`
- [X] T035 [US3] Update texture manager renderer-handle usage to backend-agnostic API in `LongXi/LXEngine/Src/Texture/TextureManager.cpp`
- [X] T036 [US3] Keep shell ImGui integration backend usage shell-local and non-public in `LongXi/LXShell/Src/ImGui/ImGuiLayer.h`
- [X] T037 [US3] Harden DirectX include/type leakage checks in `Scripts/Audit-RendererBoundaries.ps1`
- [X] T038 [US3] Execute boundary audit and capture compliance result in `specs/014-renderer-gpu-contract/contracts/external-pass-bridge-contract.md`

**Checkpoint**: All user stories are independently functional with enforceable architecture boundaries.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency, validation sweep, and documentation lock.

- [X] T039 [P] Align research decisions with final implementation outcomes in `specs/014-renderer-gpu-contract/research.md`
- [X] T040 [P] Align state entities and transitions with final implementation in `specs/014-renderer-gpu-contract/data-model.md`
- [X] T041 [P] Update end-to-end validation steps and evidence in `specs/014-renderer-gpu-contract/quickstart.md`
- [ ] T042 Run full build + runtime + boundary validation sweep and record summary in `specs/014-renderer-gpu-contract/research.md`
- [X] T043 [P] Run formatting for touched C++ files via `Win-Format Code.bat`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: Starts immediately.
- **Phase 2 (Foundational)**: Depends on Phase 1 and blocks all user stories.
- **Phase 3 (US1)**: Depends on Phase 2; delivers MVP rendering contract behavior.
- **Phase 4 (US2)**: Depends on Phase 2; may proceed after US1 integration.
- **Phase 5 (US3)**: Depends on Phase 2; recommended after US1/US2 integration to avoid churn.
- **Phase 6 (Polish)**: Depends on completion of targeted user stories.

### User Story Dependency Graph

```text
US1 (P1) -> MVP
US2 (P2) -> depends on Foundation, integrates with lifecycle/resize flow
US3 (P3) -> depends on Foundation, enforces architectural boundaries

Recommended order: US1 -> US2 -> US3
```

### Parallel Opportunities

- Setup: `T004`, `T005`
- US1: `T017`
- Polish: `T039`, `T040`, `T041`, `T043`

---

## Parallel Example: User Story 1

```bash
Task: "T017 [US1] Refactor scene-node submit signatures to renderer API in LongXi/LXEngine/Src/Scene/SceneNode.h"
Task: "T020 [US1] Define DX11 sprite pipeline backend surface in LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.h"
```

## Parallel Example: User Story 2

```bash
Task: "T028 [US2] Ensure engine resize forwarding preserves renderer ownership in LongXi/LXEngine/Src/Engine/Engine.cpp"
Task: "T030 [US2] Update resize contract invariants in specs/014-renderer-gpu-contract/contracts/renderer-api-contract.md"
```

## Parallel Example: User Story 3

```bash
Task: "T034 [US3] Replace backend-specific texture handle exposure with renderer types in LongXi/LXEngine/Src/Texture/Texture.h"
Task: "T037 [US3] Harden DirectX include/type leakage checks in Scripts/Audit-RendererBoundaries.ps1"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Setup and Foundational phases.
2. Deliver User Story 1 deterministic lifecycle/state behavior.
3. Validate US1 independently with runtime scenario evidence.
4. Stop for MVP review.

### Incremental Delivery

1. Foundation complete (Phases 1-2).
2. Deliver US1 (deterministic lifecycle and state ownership).
3. Deliver US2 (resize and viewport robustness).
4. Deliver US3 (backend boundary enforcement).
5. Finalize with cross-cutting validation and documentation alignment.

### Suggested MVP Scope

- **MVP**: Phase 1 + Phase 2 + Phase 3 (US1)

---

## Notes

- All task entries follow required checklist format.
- `[P]` markers indicate file-independent parallelizable work.
- `[US#]` labels are applied only to user-story phases.
- Runtime validation is documented in `quickstart.md`; no separate test-code authoring tasks are required by current spec.
- Reference implementation inspection must follow the repository rule in spec/plan artifacts.
