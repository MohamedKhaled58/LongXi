# Implementation Plan: Scene System

**Branch**: `011-scene-system` | **Date**: 2026-03-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/011-scene-system/spec.md`

## Summary

Introduce `Scene` as Engine subsystem #6 with a `SceneNode` base class hierarchy. Scene owns a root node that serves as the parent of all top-level world objects. Two traversals run each frame: a dirty-flag update traversal (world transforms recomputed only for dirty nodes or children of dirty parents) and a render traversal (Submit() called on every node). Engine::Update() now measures deltaTime with `std::chrono::steady_clock`. The existing `UpdateScene()`/`RenderScene()` private stubs are removed and replaced with direct Scene calls. All source goes into `LXEngine/Src/Scene/`. `Vector3` is added to the existing `Math/Math.h`.

## Technical Context

**Language/Version**: C++23, MSVC v145
**Primary Dependencies**: Existing LXEngine subsystems (DX11Renderer, Math/Math.h); `std::chrono::steady_clock` for deltaTime; `std::unique_ptr` + `std::vector` for node ownership
**Storage**: N/A (in-memory hierarchy only; no serialization in Phase 1)
**Testing**: Manual via LXShell TestSceneSystem() — log-driven verification
**Target Platform**: Windows 10+, x64
**Project Type**: Static library extension (LXEngine)
**Performance Goals**: No per-frame heap allocation during traversal; dirty-flag skips unnecessary world transform recomputation
**Constraints**: Single-threaded (Phase 1); no structural mutation during traversal (undefined behavior); parent pointer immutable after AddChild()
**Scale/Scope**: ~350 lines of new C++; 4 new source files; additions to 4 existing files; no new Premake projects

## Constitution Check

| Article | Requirement | Status | Notes |
|---------|-------------|--------|-------|
| III — Platform | DirectX 11, Windows, MSVC | ✅ PASS | No new platform dependencies |
| III — Module Structure | Static libs, LXEngine layer | ✅ PASS | All new code in LXEngine/Src/Scene/ |
| III — Dependency Direction | LXShell → LXEngine → LXCore | ✅ PASS | No reverse dependencies |
| III — Runtime | Single-threaded Phase 1 | ✅ PASS | All traversal on main thread |
| IV — Entrypoint Discipline | Engine::Initialize() thin | ✅ PASS | Scene init is 3-line delegation |
| IV — Module Boundaries | No cross-layer coupling | ✅ PASS | Scene only touches DX11Renderer (via Submit pass-through) |
| IV — Abstraction Discipline | No premature abstraction | ✅ PASS | SceneNode earned immediately; dirty-flag is concrete need |
| IV — Ownership | Rendering MUST NOT own gameplay truth | ✅ PASS | Scene orchestrates; nodes render; Scene has no draw code |
| V — Multiplayer Readiness | Presentation separable from state | ✅ PASS | Node transforms are local data; render submission is separate pass |
| IX — Threading | No premature threading | ✅ PASS | No background threads |
| XII — Phase 1 Scope | Foundation phase | ✅ PASS | Scene is explicitly in-scope (map/character/particle foundation) |

**Complexity Tracking**: No violations — no justification table needed.

## Project Structure

### Documentation (this feature)

```text
specs/011-scene-system/
├── plan.md              # This file
├── research.md          # Phase 0 — all decisions resolved
├── data-model.md        # Phase 1 — entities, traversal algorithms, modified types
├── quickstart.md        # Phase 1 — step-by-step implementation guide
├── contracts/
│   └── scene-api.md     # SceneNode and Scene public API contracts
└── tasks.md             # Phase 2 output (/speckit.tasks — not yet created)
```

### Source Code (new files)

```text
LongXi/LXEngine/Src/
├── Math/
│   └── Math.h                  # MODIFIED — add Vector3 struct
└── Scene/
    ├── SceneNode.h              # NEW — base class declaration
    ├── SceneNode.cpp            # NEW — traversal, transform, hierarchy
    ├── Scene.h                  # NEW — subsystem declaration
    └── Scene.cpp                # NEW — init, traversal entry points, accessors
```

### Modified Existing Files

```text
LongXi/LXEngine/Src/Engine/Engine.h    # Remove UpdateScene/RenderScene stubs;
                                       # add Scene forward decl, GetScene(),
                                       # m_Scene, m_LastFrameTime, m_FirstFrame
LongXi/LXEngine/Src/Engine/Engine.cpp  # Remove stub impls; add Scene init/
                                       # shutdown/update/render/resize/accessor;
                                       # add chrono + steady_clock deltaTime
LongXi/LXEngine/LXEngine.h             # Add Scene.h + SceneNode.h includes
LongXi/LXShell/Src/main.cpp            # Add TestSceneSystem()
```

**Structure Decision**: Single project extension. No new Premake projects. New files in `LXEngine/Src/Scene/` are picked up by the existing `Src/**.h` / `Src/**.cpp` wildcard glob in LXEngine/premake5.lua.

---

## Phase 0: Research Findings Summary

All decisions resolved. See [research.md](./research.md) for full rationale.

| Decision | Choice |
|----------|--------|
| deltaTime clock | `std::chrono::steady_clock`, persisted `m_LastFrameTime` in Engine |
| First frame deltaTime | `0.0f` |
| Traversal algorithm | Recursive pre-order DFS |
| Dirty-flag propagation | `parentWasRecomputed` boolean threaded through recursive call |
| New node dirty state | `m_TransformDirty = true` at construction |
| Stub replacement | Remove UpdateScene()/RenderScene() private stubs from Engine |
| Vector3 location | Existing `Math/Math.h` — no new file |
| File placement | `LXEngine/Src/Scene/` — 4 new files |

---

## Phase 1: Design Decisions

### Initialization Order

```
Step 1: DX11Renderer       (fatal if fails)
Step 2: InputSystem
Step 3: CVirtualFileSystem
Step 4: TextureManager
Step 5: SpriteRenderer     (non-fatal)
Step 6: Scene              (non-fatal) ← NEW
```

### Shutdown Order (reverse)

```
Step 0: Scene              ← NEW (first)
Step 1: SpriteRenderer
Step 2: TextureManager
Step 3: CVirtualFileSystem
Step 4: InputSystem
Step 5: DX11Renderer       (last)
```

### Frame Execution Order

```
Engine::Update():
    InputSystem::Update()
    [measure deltaTime with steady_clock]
    Scene::Update(deltaTime)         ← replaces UpdateScene() stub

Engine::Render():
    DX11Renderer::BeginFrame()
    Scene::Render(*m_Renderer)       ← replaces RenderScene() stub
    SpriteRenderer::Begin()
    SpriteRenderer::End()
    DX11Renderer::EndFrame()

Engine::OnResize(w, h):
    DX11Renderer::OnResize()
    SpriteRenderer::OnResize()
    Scene::OnResize()                ← NEW
```

### Memory Ownership Hierarchy

```
Engine
  └── unique_ptr<Scene>
        └── SceneNode m_Root         (value — always valid while Scene exists)
              ├── unique_ptr<SceneNode> topLevelNode1
              │     ├── unique_ptr<SceneNode> child1
              │     └── unique_ptr<SceneNode> child2
              └── unique_ptr<SceneNode> topLevelNode2
```

### Key Implementation Note: Scene Traversal vs Root

Scene::Update() and Scene::Render() skip the root node itself (it is an invisible container) and iterate `m_Root.m_Children` directly. This requires `friend class Scene` in SceneNode to access the private `m_Children` vector and the private traversal methods.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
