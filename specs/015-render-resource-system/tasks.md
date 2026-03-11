# Tasks: Spec 015 - Render Resource System

**Input**: Design documents from `/specs/015-render-resource-system/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: Validation tasks are included because `spec.md` defines explicit acceptance scenarios and independent test criteria for each user story.

## Format: `[ID] [P?] [Story] Description`

- [X] T001 Update project file lists for new backend resource modules in `premake5.lua`
- [X] T002 Create renderer resource table declarations in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.h`
- [X] T003 [P] Create texture resource module declarations in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Textures.h`
- [X] T004 [P] Create buffer resource module declarations in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.h`
- [X] T005 [P] Create shader resource module declarations in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Shaders.h`
- [X] T006 Implement resource table source scaffolding in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.cpp`
- [X] T007 [P] Implement texture resource source scaffolding in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Textures.cpp`
- [X] T008 [P] Implement buffer resource source scaffolding in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.cpp`
- [X] T009 [P] Implement shader resource source scaffolding in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Shaders.cpp`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add build-visible module skeletons and file layout needed for the resource system.

- [X] T010 Register backend resource module includes/usings in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T011 Add backend module ownership members to renderer implementation in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T012 [P] Add resource-system logging category conventions in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish backend-agnostic resource types/contracts and architecture enforcement before any story implementation.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T013 Replace pointer-like public handles with opaque id/generation handles in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T014 Define descriptor structs (`TextureDesc`, `BufferDesc`, `ShaderDesc`) in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T015 Define usage/access/bind enums and validation request/result structs in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
- [X] T016 Extend renderer public resource API surface in `LongXi/LXEngine/Src/Renderer/Renderer.h`
- [X] T017 Update DX11 renderer class overrides for full resource API in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [X] T018 Implement core handle validation helpers and resource table metadata in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.h`
- [X] T019 Implement core handle lookup and slot reuse/generation logic in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.cpp`
- [X] T020 Enforce no-DirectX-leakage checks for resource system files in `Scripts/Audit-RendererBoundaries.ps1`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Descriptor-Based GPU Resource Creation (Priority: P1) 🎯 MVP

**Goal**: Resource creation for textures/buffers/shaders is descriptor-driven and returns opaque handles only.

**Independent Test**: Create resources through renderer API from engine paths (including `TextureManager`) with zero DirectX type/header exposure.

### Tests for User Story 1

- [X] T021 [P] [US1] Add descriptor-creation validation scenario to `specs/015-render-resource-system/quickstart.md`
- [X] T022 [P] [US1] Add descriptor-creation smoke validation script in `Scripts/Validate-Spec015-ResourceCreation.ps1`

### Implementation for User Story 1

- [X] T023 [US1] Implement texture descriptor validation and creation path in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Textures.cpp`
- [X] T024 [P] [US1] Implement buffer descriptor validation and create functions in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.cpp`
- [X] T025 [P] [US1] Implement shader descriptor validation and create functions in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Shaders.cpp`
- [X] T026 [US1] Implement renderer create/destroy API dispatch into backend modules in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T027 [US1] Register created resources in renderer pools and allocate opaque handles in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.cpp`
- [X] T028 [US1] Refactor texture GPU allocation path to descriptor-based renderer API in `LongXi/LXEngine/Src/Texture/TextureManager.cpp`
- [X] T029 [US1] Align texture wrapper storage with finalized opaque handle contract in `LongXi/LXEngine/Src/Texture/Texture.h`
- [X] T030 [US1] Update texture implementation to match descriptor-backed metadata/handle behavior in `LongXi/LXEngine/Src/Texture/Texture.cpp`

**Checkpoint**: User Story 1 is independently functional and validates the backend-agnostic creation boundary.

---

## Phase 4: User Story 2 - Controlled Resource Update and Binding (Priority: P2)

**Goal**: Update/map/unmap and binding behavior is explicit, policy-validated, and deterministic.

**Independent Test**: Dynamic updates and binding operations succeed only for valid usage/stage combinations; invalid operations are rejected with diagnostics.

### Tests for User Story 2

- [X] T031 [P] [US2] Add update/binding validation scenario to `specs/015-render-resource-system/quickstart.md`
- [X] T032 [P] [US2] Add update/binding validation script in `Scripts/Validate-Spec015-UpdatesAndBinding.ps1`

### Implementation for User Story 2

- [X] T033 [US2] Implement `UpdateBuffer` and `Map/Unmap` backend operations with usage-policy checks in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.cpp`
- [X] T034 [US2] Implement renderer binding APIs for buffers/shaders/textures with stage checks in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T035 [US2] Route sprite rendering data uploads through renderer resource update APIs in `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp`
- [X] T036 [US2] Refactor DX11 sprite pipeline to consume renderer-managed resource handles in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.cpp`
- [X] T037 [US2] Add explicit update/bind policy violation logging paths in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T038 [US2] Finalize bind-stage contract mapping documentation in `specs/015-render-resource-system/contracts/renderer-resource-api-contract.md`

**Checkpoint**: User Stories 1 and 2 both work independently with deterministic update and binding rules.

---

## Phase 5: User Story 3 - Resource Lifetime Ownership and Safe Shutdown (Priority: P3)

**Goal**: Renderer owns lifetime tracking, stale-handle rejection, and complete shutdown cleanup.

**Independent Test**: Stress create/destroy cycles reject stale handles and shutdown releases all live resources with summary logs.

### Tests for User Story 3

- [X] T039 [P] [US3] Add lifetime/shutdown validation scenario to `specs/015-render-resource-system/quickstart.md`
- [X] T040 [P] [US3] Add stale-handle/lifetime stress validation script in `Scripts/Validate-Spec015-Lifetime.ps1`

### Implementation for User Story 3

- [X] T041 [US3] Implement stale-handle rejection via generation mismatch checks in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.cpp`
- [X] T042 [US3] Implement destroy-state transition flow (`Allocated/Bound/Mapped` to `Destroyed`) in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T043 [US3] Implement safe unbind-on-destroy behavior for bound resources in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T044 [US3] Implement renderer shutdown sweep for all pools with forced cleanup in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [X] T045 [US3] Track per-pool metrics (created/destroyed/force-released) in `LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.h`
- [X] T046 [US3] Finalize lifetime state/diagnostic contract details in `specs/015-render-resource-system/contracts/renderer-resource-lifetime-contract.md`

**Checkpoint**: All user stories are independently testable with safe resource ownership and shutdown behavior.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, docs sync, and quality checks across all user stories.

- [X] T047 [P] Update execution/validation instructions for all scenarios in `specs/015-render-resource-system/quickstart.md`
- [X] T048 [P] Update final architecture and verification notes in `specs/015-render-resource-system/plan.md`
- [X] T049 Run boundary audit verification using `Scripts/Audit-RendererBoundaries.ps1`
- [ ] T050 Run full build validation using `Win-Build Project.bat`
- [X] T051 Run repository code formatting using `Win-Format Code.bat`

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup (Phase 1) depends on initial scaffolding tasks T001-T009.
- Foundational (Phase 2) depends on Setup completion and blocks all user stories.
- User Stories (Phase 3-5) depend on Foundational completion.
- Polish (Phase 6) depends on completion of all selected user stories.

### User Story Dependencies

- US1 (P1): starts after Foundational and provides base resource creation contracts.
- US2 (P2): depends on US1 creation handles/descriptors but remains independently testable once US1 is complete.
- US3 (P3): depends on US1/US2 resource operation paths for full lifetime and shutdown validation.

### Dependency Graph

- `US1 -> US2 -> US3`

### Parallel Opportunities

- Setup parallel tasks: T003, T004, T005 and T007, T008, T009.
- Foundational work is mostly sequential due to shared API files; T020 can run in parallel with T018-T019 once T016 exists.
- Story test-script/documentation tasks (`T021/T022`, `T031/T032`, `T039/T040`) can run in parallel.

---

## Parallel Example: User Story 1

```bash
Task T021: Update specs/015-render-resource-system/quickstart.md
Task T022: Create Scripts/Validate-Spec015-ResourceCreation.ps1
Task T024: Implement LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.cpp
Task T025: Implement LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Shaders.cpp
```

## Parallel Example: User Story 2

```bash
Task T031: Update specs/015-render-resource-system/quickstart.md
Task T032: Create Scripts/Validate-Spec015-UpdatesAndBinding.ps1
Task T035: Implement LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp
Task T036: Implement LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.cpp
```

## Parallel Example: User Story 3

```bash
Task T039: Update specs/015-render-resource-system/quickstart.md
Task T040: Create Scripts/Validate-Spec015-Lifetime.ps1
Task T045: Implement LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.h
Task T046: Update specs/015-render-resource-system/contracts/renderer-resource-lifetime-contract.md
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete scaffolding + Setup + Foundational.
2. Complete US1 tasks T021-T030.
3. Validate descriptor creation boundary with `Scripts/Validate-Spec015-ResourceCreation.ps1`.
4. Demo engine resource creation path before moving to US2.

### Incremental Delivery

1. Deliver US1 (descriptor creation and opaque handles).
2. Deliver US2 (controlled update/bind policy enforcement).
3. Deliver US3 (lifetime ownership and shutdown safety).
4. Run final cross-cutting validation and formatting in Phase 6.

### Parallel Team Strategy

1. One developer owns public API and handle model (`RendererTypes.h`, `Renderer.h`).
2. One developer owns DX11 backend modules (`DX11Textures/Buffers/Shaders/ResourceTables`).
3. One developer owns integration and validation assets (`TextureManager`, `SpriteRenderer`, scripts/docs).

---

## Notes

- All tasks follow required checklist format: `- [ ] T### [P] [US#] Description with file path`.
- `[US#]` labels are applied only in user story phases.
- Parallel markers are only used for tasks on separate files with no incomplete dependencies.
- Reference inspection remains constrained to `D:\Yamen Development\Old-Reference\cqClient\Conquer`; implementation must follow LongXi architecture.
