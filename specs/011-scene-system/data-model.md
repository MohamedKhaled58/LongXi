# Data Model: Scene System

**Feature**: 011-scene-system
**Date**: 2026-03-11

---

## Entity 1: Vector3 (new shared math type)

**Location**: `LongXi/LXEngine/Src/Math/Math.h` (added alongside Vector2 and Color)
**Namespace**: `LongXi`
**Kind**: POD struct

| Field | Type | Description |
|-------|------|-------------|
| x | float | X component |
| y | float | Y component |
| z | float | Z component |

**Default instances**:
- Zero vector: `{0.0f, 0.0f, 0.0f}` — used for position and rotation defaults
- Unit scale: `{1.0f, 1.0f, 1.0f}` — used for scale default

---

## Entity 2: SceneNode

**Location**: `LongXi/LXEngine/Src/Scene/SceneNode.h/.cpp`
**Namespace**: `LongXi`
**Kind**: Polymorphic base class (virtual destructor)

### Public Interface

| Method | Signature | Description |
|--------|-----------|-------------|
| AddChild | `void AddChild(std::unique_ptr<SceneNode>)` | Takes ownership, sets child parent pointer, marks child dirty |
| RemoveChild | `void RemoveChild(SceneNode*)` | Destroys matching child, logs removal |
| SetPosition | `void SetPosition(Vector3)` | Sets local position, marks dirty |
| SetRotation | `void SetRotation(Vector3)` | Sets local rotation (degrees), marks dirty |
| SetScale | `void SetScale(Vector3)` | Sets local scale, marks dirty |
| GetPosition | `const Vector3& GetPosition() const` | Returns local position |
| GetRotation | `const Vector3& GetRotation() const` | Returns local rotation (degrees) |
| GetScale | `const Vector3& GetScale() const` | Returns local scale |
| GetWorldPosition | `const Vector3& GetWorldPosition() const` | Returns computed world position |
| GetWorldRotation | `const Vector3& GetWorldRotation() const` | Returns computed world rotation (degrees) |
| GetWorldScale | `const Vector3& GetWorldScale() const` | Returns computed world scale |
| Update | `virtual void Update(float deltaTime)` | Per-frame game logic (no-op base) |
| Submit | `virtual void Submit(DX11Renderer& renderer)` | Per-frame render submission (no-op base) |

### Private Members

| Member | Type | Default | Description |
|--------|------|---------|-------------|
| m_Position | Vector3 | {0,0,0} | Local position |
| m_Rotation | Vector3 | {0,0,0} | Local rotation in degrees |
| m_Scale | Vector3 | {1,1,1} | Local scale |
| m_WorldPosition | Vector3 | {0,0,0} | Computed world position |
| m_WorldRotation | Vector3 | {0,0,0} | Computed world rotation in degrees |
| m_WorldScale | Vector3 | {1,1,1} | Computed world scale |
| m_TransformDirty | bool | true | True when world transform needs recomputation |
| m_Parent | SceneNode* | nullptr | Non-owning pointer to parent (null for root-level) |
| m_Children | `vector<unique_ptr<SceneNode>>` | empty | Owned child nodes |

### Private / Internal Methods

| Method | Description |
|--------|-------------|
| `ComputeWorldTransform()` | Computes world transform from m_Parent world transform + local transform |
| `TraverseUpdate(float, bool)` | Recursive pre-order update traversal with dirty propagation |
| `TraverseRender(DX11Renderer&)` | Recursive pre-order render traversal |

### Invariants

- `m_Parent` is set once by `AddChild()` and never changed (immutable after creation)
- `m_TransformDirty = true` at construction — world transform computed on first traversal
- `m_Scale` default is `{1,1,1}` (multiplicative identity for scale propagation)
- Calling any `Set*()` method sets `m_TransformDirty = true`

### World Transform Computation

```
World Position = Parent.WorldPosition + Local.Position
World Rotation = Parent.WorldRotation + Local.Rotation  (degrees, additive)
World Scale    = Parent.WorldScale    × Local.Scale     (component-wise multiply)

Root node (m_Parent == nullptr):
World Position = Local.Position
World Rotation = Local.Rotation
World Scale    = Local.Scale
```

---

## Entity 3: Scene

**Location**: `LongXi/LXEngine/Src/Scene/Scene.h/.cpp`
**Namespace**: `LongXi`
**Kind**: Concrete class (Engine-owned subsystem)

### Public Interface

| Method | Signature | Description |
|--------|-----------|-------------|
| Initialize | `bool Initialize()` | Creates root node, logs init; returns false on failure |
| Shutdown | `void Shutdown()` | Destroys root node and all children |
| AddNode | `void AddNode(std::unique_ptr<SceneNode>)` | Attaches node to scene root |
| RemoveNode | `void RemoveNode(SceneNode*)` | Removes and destroys node from scene root |
| Update | `void Update(float deltaTime)` | Triggers dirty-flag traversal + Update on all nodes |
| Render | `void Render(DX11Renderer& renderer)` | Triggers Submit traversal on all nodes |
| OnResize | `void OnResize(int width, int height)` | Guards against ≤0 dims, forwards to resize-aware nodes |
| IsInitialized | `bool IsInitialized() const` | Returns true after successful Initialize() |

### Private Members

| Member | Type | Description |
|--------|------|-------------|
| m_Root | SceneNode | Root scene node; owns all top-level nodes as children |
| m_Initialized | bool | True after Initialize() |

### Traversal Algorithm (Update)

```
Scene::Update(deltaTime):
    m_Root.TraverseUpdate(deltaTime, parentWasRecomputed=false)

SceneNode::TraverseUpdate(deltaTime, parentWasRecomputed):
    needsRecompute = m_TransformDirty || parentWasRecomputed
    if needsRecompute:
        ComputeWorldTransform()    // reads m_Parent world transform (already fresh)
        m_TransformDirty = false
    Update(deltaTime)              // virtual — subclass game logic
    for each child:
        child.TraverseUpdate(deltaTime, needsRecompute)
```

### Traversal Algorithm (Render)

```
Scene::Render(renderer):
    m_Root.TraverseRender(renderer)

SceneNode::TraverseRender(renderer):
    Submit(renderer)               // virtual — subclass issues draw calls (no-op base)
    for each child:
        child.TraverseRender(renderer)
```

---

## Entity 4: Engine (modified)

### New Members

| Member | Type | Description |
|--------|------|-------------|
| m_Scene | `std::unique_ptr<Scene>` | Owned Scene subsystem |
| m_LastFrameTime | `time_point<steady_clock>` | Last frame timestamp for deltaTime computation |
| m_FirstFrame | bool | True until first Update() call; forces deltaTime=0 on first frame |

### New Public Method

| Method | Description |
|--------|-------------|
| `Scene& GetScene()` | Returns reference to Scene subsystem |

### Modified Methods

| Method | Change |
|--------|--------|
| `Initialize()` | Add Scene init as step 6 (non-fatal if fails) |
| `Shutdown()` | Destroy Scene before SpriteRenderer (new first step) |
| `Update()` | Measure deltaTime with steady_clock; call `m_Scene->Update(deltaTime)` |
| `Render()` | Replace `RenderScene()` stub with `m_Scene->Render(*m_Renderer)` |
| `OnResize()` | Add `m_Scene->OnResize(width, height)` call |

### Removed Members / Methods

| Removed | Reason |
|---------|--------|
| `void UpdateScene()` (private) | Replaced by direct `m_Scene->Update()` call |
| `void RenderScene()` (private) | Replaced by direct `m_Scene->Render()` call |

---

## State Machine: SceneNode Lifecycle

```
[Created]  ←─── constructor sets m_TransformDirty=true
    │
    │ AddChild() called on a parent
    ▼
[Attached to parent] ── m_Parent set (immutable from here)
    │
    │ TraverseUpdate called (first frame)
    ▼
[World transform computed] ─ m_TransformDirty=false
    │
    │ SetPosition/SetRotation/SetScale called
    ▼
[Transform dirty] ─ m_TransformDirty=true
    │
    │ TraverseUpdate called (next frame)
    ▼
[World transform recomputed] ─ m_TransformDirty=false
    │
    │ RemoveChild() called on parent
    ▼
[Destroyed] ─ unique_ptr releases, children recursively destroyed
```

---

## Initialization Order in Engine

```
Step 1: DX11Renderer       (fatal if fails)
Step 2: InputSystem        (always succeeds)
Step 3: CVirtualFileSystem (always succeeds)
Step 4: TextureManager     (always succeeds)
Step 5: SpriteRenderer     (non-fatal if fails)
Step 6: Scene              (non-fatal if fails — NEW)
```

## Shutdown Order in Engine (reverse)

```
Step 0: Scene          ← NEW (first, before SpriteRenderer)
Step 1: SpriteRenderer
Step 2: TextureManager
Step 3: CVirtualFileSystem
Step 4: InputSystem
Step 5: DX11Renderer   ← last
```

## Render Order in Engine::Render()

```
DX11Renderer::BeginFrame()
  → Scene::Render()       ← NEW (3D world — before 2D overlay)
  → SpriteRenderer::Begin()
      → (DrawSprite calls from game code)
  → SpriteRenderer::End()
DX11Renderer::EndFrame()
```

---

## Files Added / Modified

### New Files

| File | Purpose |
|------|---------|
| `LXEngine/Src/Scene/SceneNode.h` | SceneNode base class declaration |
| `LXEngine/Src/Scene/SceneNode.cpp` | SceneNode implementation |
| `LXEngine/Src/Scene/Scene.h` | Scene class declaration |
| `LXEngine/Src/Scene/Scene.cpp` | Scene implementation |

### Modified Files

| File | Change |
|------|--------|
| `LXEngine/Src/Math/Math.h` | Add `struct Vector3 { float x; float y; float z; };` |
| `LXEngine/Src/Engine/Engine.h` | Add Scene forward decl, GetScene(), m_Scene, m_LastFrameTime, m_FirstFrame; remove UpdateScene()/RenderScene() |
| `LXEngine/Src/Engine/Engine.cpp` | Add Scene include + chrono; impl init/shutdown/update/render/resize/accessor; remove stubs |
| `LXEngine/LXEngine.h` | Add `#include "Scene/Scene.h"` and `#include "Scene/SceneNode.h"` |
| `LXShell/Src/main.cpp` | Add TestSceneSystem() to verify initialization and node traversal |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
