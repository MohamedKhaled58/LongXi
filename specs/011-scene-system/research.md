# Research: Scene System

**Feature**: 011-scene-system
**Date**: 2026-03-11
**Phase**: 0 — Outline & Research

## Overview

Research findings for implementing the Scene subsystem. All decisions are grounded in the existing LXEngine codebase (C++23, MSVC v145, DirectX 11, single-threaded) and the clarifications recorded during `/speckit.clarify`.

---

## Research Area 1: deltaTime Measurement

**Question**: Which clock should measure frame elapsed time, and how should it integrate with Engine::Update()?

**Codebase finding**: Engine::Update() currently calls `UpdateScene()` (a no-op stub). No clock exists in the Engine. Engine.h has no chrono include.

**Decision**: Use `std::chrono::steady_clock` with a persisted `m_LastFrameTime` member in Engine.

**Rationale**:
- `std::chrono::steady_clock` is guaranteed monotonic (never goes backward) — correct for game loops
- `std::chrono::high_resolution_clock` may alias to `system_clock` on some implementations and is not guaranteed monotonic
- `steady_clock` is the standard recommendation for elapsed-time measurement in C++ game loops
- First frame uses `deltaTime = 0.0f` to avoid a spuriously large initial tick

**Implementation**:
```cpp
// Engine.h (private)
std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime;
bool m_FirstFrame = true;

// Engine::Initialize() — record start time
m_LastFrameTime = std::chrono::steady_clock::now();
m_FirstFrame = true;

// Engine::Update()
auto now = std::chrono::steady_clock::now();
float deltaTime = m_FirstFrame ? 0.0f
    : std::chrono::duration<float>(now - m_LastFrameTime).count();
m_LastFrameTime = now;
m_FirstFrame = false;
m_Scene->Update(deltaTime);
```

**Alternatives considered**:
- QueryPerformanceCounter (Win32): More portable-to-Windows but no benefit over std::chrono on MSVC
- Fixed deltaTime (1/60s): Breaks for non-60 Hz displays; not suitable for a real engine

---

## Research Area 2: SceneNode Traversal — Recursive vs Iterative

**Question**: Should traversal be implemented recursively (simple) or iteratively (stack-based, avoids stack overflow)?

**Decision**: Recursive depth-first traversal for Phase 1.

**Rationale**:
- Phase 1 scenes have shallow hierarchies (< 10 levels deep) — stack overflow risk is negligible
- Recursive traversal is simpler, more readable, and matches the hierarchical data structure naturally
- Iterative traversal (explicit stack) is an optimization for deep/complex hierarchies — deferred
- Pre-order DFS (parent before children) is trivially implemented with a single recursive call

**Stack depth estimate**:
- Each recursive frame uses ~100–200 bytes of stack
- Default stack size on Windows is 1MB = ~5,000–10,000 levels before overflow
- Phase 1 scenes will have at most 3–5 levels — no risk

**Alternatives considered**:
- BFS (breadth-first): Wrong order for parent-before-children transform propagation
- Iterative DFS with explicit std::stack: Correct but 3× more code for no Phase 1 benefit

---

## Research Area 3: SceneNode Memory and Traversal Safety

**Question**: How is the child list protected during traversal when nodes may be added/removed inside Update()?

**Decision**: Phase 1 declares structural mutation during traversal as undefined behavior (FR-035). No safety mechanism is implemented. The traversal operates directly on `m_Children`.

**Rationale**:
- Adding a safe mutation mechanism (deferred queue, copy-on-iterate) adds significant complexity
- Phase 1 use cases (map nodes, character nodes) do not mutate the hierarchy during traversal
- The boundary is documented explicitly in the spec (FR-035 and edge cases section)
- When needed in a future spec, a deferred-mutation queue can be added without changing the traversal API

---

## Research Area 4: Engine Stub Replacement Strategy

**Question**: How do `UpdateScene()` and `RenderScene()` private stubs in Engine get replaced by real Scene calls?

**Codebase finding**:
```cpp
// Engine.h private section:
void UpdateScene();
void RenderScene();

// Engine.cpp:
void Engine::UpdateScene() {}
void Engine::RenderScene() {}
```

These stubs were added during the Spec 009 merge to hold the scene call site.

**Decision**: Remove both private stub methods and inline Scene calls directly in `Engine::Update()` and `Engine::Render()`.

**Rationale**:
- The stubs served as placeholder call sites — their purpose is fulfilled by real Scene integration
- No other code calls UpdateScene() or RenderScene()
- Inlining the calls matches the existing pattern for SpriteRenderer (no private wrapper method)
- Removing private stubs reduces dead code and cognitive overhead

**Changes required**:
- Remove `void UpdateScene()` and `void RenderScene()` from Engine.h private section
- Remove their empty implementations from Engine.cpp
- Replace `UpdateScene()` call in Engine::Update() with `m_Scene->Update(deltaTime)`
- Replace `RenderScene()` call in Engine::Render() with `m_Scene->Render(*m_Renderer)`

---

## Research Area 5: Dirty-Flag Traversal Implementation

**Question**: How does the dirty-flag transform recomputation propagate correctly in a pre-order traversal?

**Decision**: Each node receives a `parentWasRecomputed` boolean from its parent during traversal. Recompute if `m_TransformDirty || parentWasRecomputed`.

**Algorithm**:
```cpp
// SceneNode::TraverseUpdate(float deltaTime, bool parentWasRecomputed)
bool needsRecompute = m_TransformDirty || parentWasRecomputed;
if (needsRecompute)
{
    ComputeWorldTransform();  // reads m_Parent->m_World* (already fresh, pre-order)
    m_TransformDirty = false;
}
Update(deltaTime);
for (auto& child : m_Children)
    child->TraverseUpdate(deltaTime, needsRecompute);
```

**Key invariant**: Pre-order traversal guarantees a parent's world transform is always fresh when its child recomputes — no secondary pass needed.

**New nodes**: Created with `m_TransformDirty = true` to ensure first-frame world transform is computed.

---

## Research Area 6: Scene file placement and LXEngine.h update

**Question**: Where do Scene and SceneNode source files live, and what headers must be exposed publicly?

**Decision**:
- Files: `LongXi/LXEngine/Src/Scene/Scene.h`, `Scene.cpp`, `SceneNode.h`, `SceneNode.cpp`
- LXEngine.h additions: `#include "Scene/Scene.h"` and `#include "Scene/SceneNode.h"`
- Picked up automatically by LXEngine premake5.lua wildcard `Src/**.h` / `Src/**.cpp`

**Rationale**:
- SceneNode.h must be publicly exposed so LXShell (and future game code) can subclass it
- Scene.h must be publicly exposed so callers can access `Engine::GetScene()`
- Placing in `Src/Scene/` follows the established per-subsystem directory convention (Src/Renderer/, Src/Input/, etc.)

---

## Research Area 7: Vector3 Addition to Math.h

**Question**: Should Vector3 be in the same Math.h as Vector2/Color, or in a separate file?

**Decision**: Add `struct Vector3 { float x; float y; float z; };` to the existing `LongXi/LXEngine/Src/Math/Math.h`.

**Rationale**:
- Math.h was established in Spec 010/011 as the shared LXEngine math types header
- Adding Vector3 to the same file keeps all math types co-located
- No new files needed; callers who already include Math.h get Vector3 automatically
- Consistent with how Vector2 and Color coexist in the same header

---

## Summary of All Decisions

| Area | Decision | Rationale |
|------|----------|-----------|
| deltaTime clock | `std::chrono::steady_clock`, persisted `m_LastFrameTime` in Engine | Guaranteed monotonic, standard C++ game loop pattern |
| First frame deltaTime | 0.0f | Avoids spuriously large first tick |
| Traversal | Recursive pre-order DFS | Simple, correct, no stack risk for Phase 1 depths |
| Mutation during traversal | Undefined behavior (documented, not guarded) | Complexity not justified for Phase 1 use cases |
| Stub replacement | Remove UpdateScene()/RenderScene() stubs, inline Scene calls | Dead code removal, consistent with SpriteRenderer pattern |
| Dirty-flag propagation | `parentWasRecomputed` boolean threaded through recursive traversal | Correct in pre-order, zero extra passes |
| New node dirty flag | `m_TransformDirty = true` at construction | First-frame world transform computed automatically |
| File placement | `LXEngine/Src/Scene/` — 4 files (Scene.h/.cpp, SceneNode.h/.cpp) | Per-subsystem directory convention |
| Vector3 location | Existing `Math/Math.h` | Co-located with Vector2/Color, no new file needed |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
