# Implementation Tasks: Scene System

**Feature**: 011-scene-system
**Branch**: `011-scene-system`
**Generated**: 2026-03-11
**Estimate**: 5–7 hours

## Overview

Task breakdown for implementing the Scene subsystem. Organized by user story for independent delivery. No automated tests — all verification is manual via logs and visual output.

**User Stories** (from spec.md):
- **US1** (P1): Engine Has a Scene Subsystem — GetScene(), init, shutdown, log
- **US2** (P1): Add and Remove World Objects — AddNode/RemoveNode, node lifecycle
- **US3** (P1): Scene Nodes Update Each Frame — deltaTime traversal, dirty-flag world transforms
- **US4** (P1): Renderable Nodes Submit Draw Calls — render traversal, Scene has zero draw code
- **US5** (P2): Hierarchical Transforms Propagate — world = parent + local, dirty-flag recompute
- **US6** (P2): Scene Handles Window Resize Events — OnResize hook in Engine chain

---

## Phase 1: Setup

**Goal**: Verify branch and baseline before any changes

### Setup Tasks

- [X] T001 Verify current branch is `011-scene-system` and master is up to date via `git status`

---

## Phase 2: Foundational — Math.h Extension and Engine.h Declarations

**Goal**: Add Vector3 to Math.h; update Engine.h with Scene declarations and remove stub methods. All subsequent phases depend on these headers.

**⚠️ CRITICAL**: No user story work can begin until Engine.h is correct.

**Independent Test**: All modified headers compile without errors when included

### Foundational Tasks

- [X] T002 [P] Add `struct Vector3 { float x; float y; float z; };` to `LongXi/LXEngine/Src/Math/Math.h` after `struct Color` inside `namespace LongXi`
- [X] T003 [P] Add forward declaration `class Scene;` to `LongXi/LXEngine/Src/Engine/Engine.h` alongside existing forward declarations (after `class SpriteRenderer;`)
- [X] T004 [P] Add `Scene& GetScene();` to the Subsystem Accessors public section of `LongXi/LXEngine/Src/Engine/Engine.h`
- [X] T005 [P] Add `std::unique_ptr<Scene> m_Scene;` to the private subsystem ownership section of `LongXi/LXEngine/Src/Engine/Engine.h` (after `m_SpriteRenderer`)
- [X] T006 [P] Add `std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime;` and `bool m_FirstFrame = true;` to the private State section of `LongXi/LXEngine/Src/Engine/Engine.h`
- [X] T007 [P] Remove `void UpdateScene();` and `void RenderScene();` private method declarations from `LongXi/LXEngine/Src/Engine/Engine.h` (these stubs will be replaced by real Scene calls)
- [X] T008 Create `LongXi/LXEngine/Src/Scene/SceneNode.h` — full class declaration per `contracts/scene-api.md`: includes (`Math/Math.h`, `<memory>`, `<vector>`), forward declare `DX11Renderer`, declare `SceneNode` class with all public methods (AddChild, RemoveChild, Set/Get Position/Rotation/Scale, Update, Submit), private traversal methods (TraverseUpdate, TraverseRender, ComputeWorldTransform), `friend class Scene;`, and all private members (m_Position, m_Rotation, m_Scale defaulted; m_WorldPosition, m_WorldRotation, m_WorldScale; m_TransformDirty=true; m_Parent=nullptr; m_Children)

**Checkpoint**: Foundational headers ready

---

## Phase 3: User Story 1 — Engine Has a Scene Subsystem (Priority: P1) 🎯 MVP

**Goal**: Scene initializes as Engine step 6, `Engine::GetScene()` returns valid reference, `[Scene] Initialized` in log

**Independent Test**: Build succeeds; log shows `[Scene] Initialized` after `[Engine] Initializing scene`; no shutdown errors

### US1 Tasks

- [X] T009 [US1] Create `LongXi/LXEngine/Src/Scene/Scene.h` — include `Scene/SceneNode.h`, forward declare `DX11Renderer`; declare `Scene` class: public Initialize()/Shutdown()/IsInitialized(), AddNode()/RemoveNode(), Update(float)/Render(DX11Renderer&)/OnResize(int,int); private `SceneNode m_Root` and `bool m_Initialized = false`
- [X] T010 [US1] Create `LongXi/LXEngine/Src/Scene/Scene.cpp` with includes (`Scene/Scene.h`, `Renderer/DX11Renderer.h`, `Core/Logging/LogMacros.h`) and `namespace LongXi` opening
- [X] T011 [US1] Implement `Scene::Initialize()` in `Scene.cpp` — set `m_Initialized = true`, log `[Scene] Initialized`, return true
- [X] T012 [US1] Implement `Scene::Shutdown()` in `Scene.cpp` — guard `!m_Initialized`, log `[Scene] Shutdown`, set `m_Initialized = false` (m_Root destructor recursively destroys all children via unique_ptr)
- [X] T013 [US1] Implement `Scene::IsInitialized() const` in `Scene.cpp` — return `m_Initialized`
- [X] T014 [P] [US1] Add `#include "Scene/Scene.h"` to `LongXi/LXEngine/Src/Engine/Engine.cpp` alongside existing includes
- [X] T015 [P] [US1] Add `#include <chrono>` to `LongXi/LXEngine/Src/Engine/Engine.cpp` alongside existing includes
- [X] T016 [US1] Add Scene initialization as step 6 to `Engine::Initialize()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — after SpriteRenderer init: `LX_ENGINE_INFO("[Engine] Initializing scene"); m_LastFrameTime = std::chrono::steady_clock::now(); m_FirstFrame = true; m_Scene = std::make_unique<Scene>(); if (!m_Scene->Initialize()) { LX_ENGINE_WARN("[Engine] Scene initialization failed"); }`
- [X] T017 [US1] Add Scene shutdown as new first step to `Engine::Shutdown()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — before existing SpriteRenderer shutdown: `if (m_Scene) { m_Scene->Shutdown(); m_Scene.reset(); }`
- [X] T018 [US1] Implement `Engine::GetScene()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — `return *m_Scene;` (add to Subsystem Accessors section)
- [X] T019 [P] [US1] Add `#include "Scene/Scene.h"` and `#include "Scene/SceneNode.h"` to `LongXi/LXEngine/LXEngine.h` (after existing Renderer and Math includes)
- [X] T020 [US1] Build solution to verify US1 compiles without errors

**Checkpoint**: `Engine::GetScene()` returns valid Scene — US1 complete

---

## Phase 4: User Story 2 — Add and Remove World Objects as Scene Nodes (Priority: P1)

**Goal**: SceneNode lifetime managed by Scene; AddNode/RemoveNode work; `[Scene] Node added` and `[Scene] Node removed` appear in log

**Independent Test**: Create SceneNode, call AddNode(), verify log; call RemoveNode(), verify log; no memory errors

### US2 Tasks

- [X] T021 [US2] Create `LongXi/LXEngine/Src/Scene/SceneNode.cpp` with includes (`Scene/SceneNode.h`, `Renderer/DX11Renderer.h`, `Core/Logging/LogMacros.h`, `<algorithm>`) and `namespace LongXi` opening
- [X] T022 [US2] Implement `SceneNode::SceneNode()` constructor in `SceneNode.cpp` — default-initialize all members (m_Position={0,0,0}, m_Rotation={0,0,0}, m_Scale={1,1,1}, m_WorldPosition={0,0,0}, m_WorldRotation={0,0,0}, m_WorldScale={1,1,1}, m_TransformDirty=true, m_Parent=nullptr); implement `SceneNode::~SceneNode()` as default (unique_ptr children clean up recursively)
- [X] T023 [US2] Implement `SceneNode::SetPosition(Vector3)`, `SetRotation(Vector3)`, `SetScale(Vector3)` in `SceneNode.cpp` — each stores the value and sets `m_TransformDirty = true`; implement all six `Get*()` const accessors returning const references
- [X] T024 [US2] Implement `SceneNode::AddChild(std::unique_ptr<SceneNode> child)` in `SceneNode.cpp` — guard null child (log `[Scene] AddChild: null child`, return); set `child->m_Parent = this`; set `child->m_TransformDirty = true`; push to `m_Children`; log `[Scene] Node added`
- [X] T025 [US2] Implement `SceneNode::RemoveChild(SceneNode* child)` in `SceneNode.cpp` — guard null pointer (log error, return); find in m_Children by raw pointer comparison with `std::find_if`; if not found log `[Scene] RemoveChild: node not found` and return; erase from m_Children (unique_ptr destroyed, recursive child destruction); log `[Scene] Node removed`
- [X] T026 [US2] Implement `Scene::AddNode(std::unique_ptr<SceneNode> node)` in `Scene.cpp` — guard null (log `[Scene] AddNode: null node`, return); call `m_Root.AddChild(std::move(node))`
- [X] T027 [US2] Implement `Scene::RemoveNode(SceneNode* node)` in `Scene.cpp` — guard null (log `[Scene] RemoveNode: null pointer`, return); call `m_Root.RemoveChild(node)`
- [X] T028 [US2] Add `void TestSceneSystem()` private method to `TestApplication` in `LongXi/LXShell/Src/main.cpp` — log test header; access `engine.GetScene()`; create a `SceneNode`, call `AddNode()`, log that node was added; call `RemoveNode()`, log that node was removed; log test footer
- [X] T029 [US2] Call `TestSceneSystem()` from `TestApplication::Initialize()` in `LongXi/LXShell/Src/main.cpp` after existing test calls
- [X] T030 [US2] Build and run to verify `[Scene] Node added` and `[Scene] Node removed` appear in log

**Checkpoint**: Node add/remove works with correct log output — US2 complete

---

## Phase 5: User Story 3 — Scene Nodes Update Each Frame (Priority: P1)

**Goal**: Every node's `Update(deltaTime)` called per frame; world transforms recomputed via dirty-flag during traversal; deltaTime measured from steady_clock

**Independent Test**: Scene initializes; engine runs; `SceneNode::Update()` virtual dispatch confirmed (custom subclass increments counter); dirty nodes recomputed once, clean nodes skipped

### US3 Tasks

- [X] T031 [US3] Implement `SceneNode::ComputeWorldTransform()` in `SceneNode.cpp` — if `m_Parent != nullptr`: component-wise world position = parent world position + local position; world rotation = parent world rotation + local rotation; world scale = parent world scale × local scale; else world = local (root node or detached)
- [X] T032 [US3] Implement `SceneNode::TraverseUpdate(float deltaTime, bool parentWasRecomputed)` in `SceneNode.cpp` — compute `bool needsRecompute = m_TransformDirty || parentWasRecomputed`; if needsRecompute: call `ComputeWorldTransform()`, set `m_TransformDirty = false`; call `Update(deltaTime)`; range-for over `m_Children`: call `child->TraverseUpdate(deltaTime, needsRecompute)`
- [X] T033 [US3] Implement `SceneNode::Update(float deltaTime)` base implementation in `SceneNode.cpp` — empty no-op body (virtual, subclasses override)
- [X] T034 [US3] Implement `Scene::Update(float deltaTime)` in `Scene.cpp` — range-for over `m_Root.m_Children`: call `child->TraverseUpdate(deltaTime, false)` (root node itself is never updated — it is an invisible container)
- [X] T035 [US3] Replace `UpdateScene()` call in `Engine::Update()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` with deltaTime measurement and Scene call: `auto now = std::chrono::steady_clock::now(); float deltaTime = m_FirstFrame ? 0.0f : std::chrono::duration<float>(now - m_LastFrameTime).count(); m_LastFrameTime = now; m_FirstFrame = false; if (m_Scene && m_Scene->IsInitialized()) { m_Scene->Update(deltaTime); }`
- [X] T036 [US3] Remove empty `void Engine::UpdateScene() {}` implementation from `LongXi/LXEngine/Src/Engine/Engine.cpp`

**Checkpoint**: Scene::Update() called each frame with measured deltaTime — US3 complete

---

## Phase 6: User Story 4 — Renderable Nodes Submit Draw Calls to Renderer (Priority: P1)

**Goal**: `SceneNode::Submit()` called per-frame for every node; Scene itself contains zero direct renderer calls

**Independent Test**: Scene::Render() traverses all nodes; base Submit() is no-op; code inspection confirms Scene.cpp has zero renderer method calls

### US4 Tasks

- [X] T037 [US4] Implement `SceneNode::Submit(DX11Renderer& renderer)` base implementation in `SceneNode.cpp` — empty no-op body (virtual, renderable subclasses override)
- [X] T038 [US4] Implement `SceneNode::TraverseRender(DX11Renderer& renderer)` in `SceneNode.cpp` — call `Submit(renderer)`; range-for over `m_Children`: call `child->TraverseRender(renderer)`
- [X] T039 [US4] Implement `Scene::Render(DX11Renderer& renderer)` in `Scene.cpp` — range-for over `m_Root.m_Children`: call `child->TraverseRender(renderer)` (root node itself is never submitted — it is an invisible container)
- [X] T040 [US4] Replace `RenderScene()` call in `Engine::Render()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` with `if (m_Scene && m_Scene->IsInitialized()) { m_Scene->Render(*m_Renderer); }` (positioned before the existing SpriteRenderer Begin/End block)
- [X] T041 [US4] Remove empty `void Engine::RenderScene() {}` implementation from `LongXi/LXEngine/Src/Engine/Engine.cpp`
- [X] T042 [US4] Verify `Scene.cpp` contains zero direct calls to any `DX11Renderer` or `ID3D11DeviceContext` methods — Scene must only call `child->TraverseRender()` (code inspection of Scene.cpp)

**Checkpoint**: Render traversal runs; Scene is draw-code-free — US4 complete

---

## Phase 7: User Story 5 — Hierarchical Transforms Propagate (Priority: P2)

**Goal**: Child world position = parent world position + child local position; dirty-flag skips unchanged nodes; new nodes recomputed on first frame

**Independent Test**: Add parent at (10,0,0) and child at local (5,0,0); after one Engine::Update(), child world position is (15,0,0)

### US5 Tasks

- [X] T043 [P] [US5] Verify `SceneNode::SetPosition()`, `SetRotation()`, `SetScale()` in `SceneNode.cpp` each set `m_TransformDirty = true` — inspect all three setter implementations
- [X] T044 [P] [US5] Verify `SceneNode` constructor sets `m_TransformDirty = true` in `SceneNode.cpp` — new nodes always recomputed on first traversal
- [X] T045 [US5] Verify `SceneNode::AddChild()` sets `child->m_TransformDirty = true` in `SceneNode.cpp` — attached nodes always get fresh world transform on next frame
- [X] T046 [US5] Add hierarchical transform test to `TestSceneSystem()` in `LongXi/LXShell/Src/main.cpp` — create parent node set to position (10,0,0); create child node set to local position (5,0,0); add child to parent via AddChild; add parent to scene via AddNode; log that after Update, child world position should be (15,0,0); verify by calling `engine.GetScene()` and running one update cycle

**Checkpoint**: World transform propagation verified — US5 complete

---

## Phase 8: User Story 6 — Scene Handles Window Resize Events (Priority: P2)

**Goal**: `Scene::OnResize()` is called when `Engine::OnResize()` fires; zero/negative dimensions are ignored

**Independent Test**: Trigger resize event; verify `Scene::OnResize()` is called (no crash, no assert); log confirms resize forwarded

### US6 Tasks

- [X] T047 [US6] Implement `Scene::OnResize(int width, int height)` in `Scene.cpp` — guard `width <= 0 || height <= 0` (return silently); body is a no-op for Phase 1 (no resize-aware nodes yet); log can be omitted unless nodes request it
- [X] T048 [US6] Add `Scene::OnResize()` call to `Engine::OnResize()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — after the existing `m_SpriteRenderer->OnResize()` call: `if (m_Scene && m_Scene->IsInitialized()) { m_Scene->OnResize(width, height); }`
- [X] T049 [US6] Manual resize test: run `Build/Debug/Executables/LXShell.exe`, drag window edge to resize, verify no crash and [Engine] log shows resize forwarded

---

## Phase 9: Polish & Cross-Cutting Concerns

**Goal**: Code formatting, final build validation, all success criteria verified

### Polish Tasks

- [X] T050 Run `Win-Format Code.bat` to apply clang-format to all modified files (`Math/Math.h`, `Scene/SceneNode.h`, `Scene/SceneNode.cpp`, `Scene/Scene.h`, `Scene/Scene.cpp`, `Engine/Engine.h`, `Engine/Engine.cpp`, `LXEngine.h`, `LXShell/Src/main.cpp`)
- [X] T051 Build Debug configuration and verify zero errors, zero warnings (SC-001 prerequisite)
- [X] T052 Run `Build/Debug/Executables/LXShell.exe` and verify `[Scene] Initialized` in log (SC-002)
- [X] T053 Verify D3D11 debug layer reports no live object warnings on shutdown (SC-009)
- [X] T054 Verify `Scene.cpp` contains zero calls to any renderer draw methods — code inspection confirms Scene never calls `IASet*`, `VSSet*`, `PSSet*`, `Draw*`, or `OMSet*` (SC-005)
- [X] T055 Verify Engine::Shutdown() log shows Scene shutdown before SpriteRenderer shutdown (correct reverse-init order, SC-009 related)
- [X] T056 Verify `Application.h` and `Application.cpp` have zero new lines added by this spec (Application must be unchanged)

---

## Dependencies

### User Story Dependencies

```
US1 (P1): Engine Has a Scene Subsystem
├── Depends on: Phase 2 Foundational (Engine.h, SceneNode.h)
└── Enables: US2, US3, US4, US5, US6 (all require Scene to exist)

US2 (P1): Add and Remove World Objects
├── Depends on: US1 (Scene accessible)
└── Enables: US3, US4, US5 (need nodes in scene)

US3 (P1): Scene Nodes Update Each Frame
├── Depends on: US1 (Scene.Update() entry), US2 (nodes exist to traverse)
└── Enables: US5 (dirty-flag traversal needed for world transform)

US4 (P1): Renderable Nodes Submit Draw Calls
├── Depends on: US1 (Scene.Render() entry), US2 (nodes exist to traverse)
└── Independent of US3 (render traversal is separate)

US5 (P2): Hierarchical Transforms
├── Depends on: US3 (ComputeWorldTransform() built as part of TraverseUpdate)
└── Primarily a verification phase

US6 (P2): Resize Handling
├── Depends on: US1 (Scene exists)
└── Independent of US3/US4/US5
```

### Execution Order

**Priority 1 (P1 — Foundation + Core)**:
1. Phase 1: Setup
2. Phase 2: Foundational
3. Phase 3: US1 — Engine Has a Scene
4. Phase 4: US2 — Add/Remove Nodes
5. Phase 5: US3 — Update Traversal
6. Phase 6: US4 — Render Traversal

**Priority 2 (P2 — Extended)**:
7. Phase 7: US5 — Hierarchical Transforms (verification)
8. Phase 8: US6 — Resize Handling

**Priority 3 (Validation)**:
9. Phase 9: Polish

---

## Parallel Execution Opportunities

### Within Phase 2 (Foundational)
T002–T007 all target different files or different sections of Engine.h — can run in parallel:
- T002: Math.h (new struct)
- T003–T007: Engine.h (declarations and removals in different sections)

### Within Phase 3 (US1)
- T014, T015, T019: All different files, can run in parallel
- T009 and T010: SceneNode.h must precede Scene.h (T008 before T009)

### Within Phase 4 (US2)
- T021–T025: SceneNode.cpp implementations are independent methods

### Within Phase 7 (US5)
- T043, T044: Independent verification tasks

---

## MVP Scope (Minimum Viable Product)

**Definition**: US1 + US2 + US3 + US4 (all P1 stories)

**MVP Tasks**: T001–T042 (42 tasks)

**MVP Test Criteria**:
- ✅ `[Scene] Initialized` in log
- ✅ `Engine::GetScene()` returns valid reference
- ✅ Nodes added/removed with correct log output
- ✅ Scene::Update() traverses nodes per frame
- ✅ Scene::Render() traverses nodes per frame
- ✅ Scene.cpp has zero direct draw calls

**MVP Value**: Full scene lifecycle working — maps, characters, and any world object can be added as SceneNode subclasses

---

## Implementation Strategy

### Incremental Delivery

1. **Phase 1–2**: Environment prepared, headers declared
2. **Phase 3 (US1)**: Scene exists in Engine — `[Scene] Initialized` visible
3. **Phase 4 (US2)**: Nodes can be added and removed
4. **Phase 5 (US3)**: Update traversal running — world game objects tick
5. **Phase 6 (US4)**: Render traversal running — renderable nodes can draw
6. **MVP complete** — P1 stories done, engine world representation functional
7. **Phase 7 (US5)**: Transform hierarchy verified
8. **Phase 8 (US6)**: Resize forwarded to Scene
9. **Phase 9**: Final validation

### Risk Mitigation

- **Recursive traversal**: Safe for Phase 1 depths (< 10 levels). Stack overflow only at >5,000 levels.
- **Stub removal**: `UpdateScene()` and `RenderScene()` removed from Engine — ensure no other code calls them before removal (search first).
- **Dirty-flag correctness**: `m_TransformDirty = true` must be set in constructor AND in AddChild for the child — both cases covered.
- **Scene::Update vs root**: Scene iterates `m_Root.m_Children` directly (not root itself) — requires `friend class Scene` in SceneNode; verify compile.

---

## Total Task Count

**Total**: 56 tasks
- Phase 1 (Setup): 1 task
- Phase 2 (Foundational): 7 tasks
- Phase 3 (US1 — Engine Subsystem): 12 tasks
- Phase 4 (US2 — Add/Remove Nodes): 10 tasks
- Phase 5 (US3 — Update Traversal): 6 tasks
- Phase 6 (US4 — Render Traversal): 6 tasks
- Phase 7 (US5 — Hierarchical Transforms): 4 tasks
- Phase 8 (US6 — Resize): 3 tasks
- Phase 9 (Polish): 7 tasks

**Parallel Opportunities**: 16 tasks marked [P] (29% parallelizable)

**Estimated Timeline**: 5–7 hours (matches quickstart.md estimate)

---

## Notes

- No automated test harness — all validation is manual via logs and visual output
- `friend class Scene` in SceneNode is required so Scene can call private TraverseUpdate/TraverseRender and access m_Children
- Build after each phase checkpoint before proceeding
- `Win-Format Code.bat` must run before commit (Allman braces, 4-space indent, 200-char line limit)
- Rotation values are in degrees — not radians (clarified in session 2026-03-11)
- Parent pointer is immutable after AddChild() — re-parenting is out of scope for Phase 1
