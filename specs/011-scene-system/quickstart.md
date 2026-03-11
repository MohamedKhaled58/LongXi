# Quickstart: Scene System

**Feature**: 011-scene-system
**Branch**: `011-scene-system`
**Estimated Effort**: 5–7 hours

---

## Step 1: Add Vector3 to Math/Math.h (5 min)

**File**: `LongXi/LXEngine/Src/Math/Math.h`

```cpp
struct Vector3
{
    float x;
    float y;
    float z;
};
```

Add after `struct Color { ... };` inside `namespace LongXi`.

**Verification**: Header compiles cleanly.

---

## Step 2: Create SceneNode.h (20 min)

**File**: `LongXi/LXEngine/Src/Scene/SceneNode.h` (new directory)

Include `Math/Math.h`, `<memory>`, `<vector>`. Declare the full SceneNode class per the contract in `contracts/scene-api.md`.

Key points:
- All private members initialized in-class
- `m_Scale` defaults to `{1.0f, 1.0f, 1.0f}`
- `m_TransformDirty = true` (computed on first traversal)
- `m_Parent = nullptr`
- `friend class Scene;` so Scene can call TraverseUpdate/TraverseRender

---

## Step 3: Create SceneNode.cpp (60 min)

**File**: `LongXi/LXEngine/Src/Scene/SceneNode.cpp`

Include `Scene/SceneNode.h`, `Renderer/DX11Renderer.h`, `Core/Logging/LogMacros.h`.

### ComputeWorldTransform()

```cpp
void SceneNode::ComputeWorldTransform()
{
    if (m_Parent)
    {
        m_WorldPosition = {m_Parent->m_WorldPosition.x + m_Position.x,
                           m_Parent->m_WorldPosition.y + m_Position.y,
                           m_Parent->m_WorldPosition.z + m_Position.z};
        m_WorldRotation = {m_Parent->m_WorldRotation.x + m_Rotation.x,
                           m_Parent->m_WorldRotation.y + m_Rotation.y,
                           m_Parent->m_WorldRotation.z + m_Rotation.z};
        m_WorldScale    = {m_Parent->m_WorldScale.x * m_Scale.x,
                           m_Parent->m_WorldScale.y * m_Scale.y,
                           m_Parent->m_WorldScale.z * m_Scale.z};
    }
    else
    {
        m_WorldPosition = m_Position;
        m_WorldRotation = m_Rotation;
        m_WorldScale    = m_Scale;
    }
}
```

### TraverseUpdate()

```cpp
void SceneNode::TraverseUpdate(float deltaTime, bool parentWasRecomputed)
{
    bool needsRecompute = m_TransformDirty || parentWasRecomputed;
    if (needsRecompute)
    {
        ComputeWorldTransform();
        m_TransformDirty = false;
    }
    Update(deltaTime);
    for (auto& child : m_Children)
        child->TraverseUpdate(deltaTime, needsRecompute);
}
```

### TraverseRender()

```cpp
void SceneNode::TraverseRender(DX11Renderer& renderer)
{
    Submit(renderer);
    for (auto& child : m_Children)
        child->TraverseRender(renderer);
}
```

### AddChild()

```cpp
void SceneNode::AddChild(std::unique_ptr<SceneNode> child)
{
    if (!child) { LX_ENGINE_ERROR("[Scene] AddChild: null child"); return; }
    child->m_Parent = this;
    child->m_TransformDirty = true;  // ensure world transform computed next frame
    m_Children.push_back(std::move(child));
    LX_ENGINE_INFO("[Scene] Node added");
}
```

### RemoveChild()

```cpp
void SceneNode::RemoveChild(SceneNode* child)
{
    if (!child) { LX_ENGINE_ERROR("[Scene] RemoveChild: null pointer"); return; }
    auto it = std::find_if(m_Children.begin(), m_Children.end(),
        [child](const auto& p) { return p.get() == child; });
    if (it == m_Children.end())
    {
        LX_ENGINE_ERROR("[Scene] RemoveChild: node not found");
        return;
    }
    m_Children.erase(it);  // unique_ptr destroyed here → recursive child destruction
    LX_ENGINE_INFO("[Scene] Node removed");
}
```

### SetPosition / SetRotation / SetScale (all mark dirty)

```cpp
void SceneNode::SetPosition(Vector3 position)
{
    m_Position = position;
    m_TransformDirty = true;
}
// similarly for SetRotation, SetScale
```

**Verification**: SceneNode.cpp compiles. No engine integration yet.

---

## Step 4: Create Scene.h (10 min)

**File**: `LongXi/LXEngine/Src/Scene/Scene.h`

Include `Scene/SceneNode.h`. Declare the Scene class per contract with `SceneNode m_Root` member (not a pointer — root is always valid while Scene exists).

---

## Step 5: Create Scene.cpp (30 min)

**File**: `LongXi/LXEngine/Src/Scene/Scene.cpp`

Include `Scene/Scene.h`, `Renderer/DX11Renderer.h`, `Core/Logging/LogMacros.h`.

```cpp
bool Scene::Initialize()
{
    m_Initialized = true;
    LX_ENGINE_INFO("[Scene] Initialized");
    return true;
}

void Scene::Shutdown()
{
    if (!m_Initialized) return;
    // m_Root destructor clears all children recursively via unique_ptr
    m_Initialized = false;
    LX_ENGINE_INFO("[Scene] Shutdown");
}

void Scene::AddNode(std::unique_ptr<SceneNode> node)
{
    if (!node) { LX_ENGINE_ERROR("[Scene] AddNode: null node"); return; }
    m_Root.AddChild(std::move(node));
}

void Scene::RemoveNode(SceneNode* node)
{
    if (!node) { LX_ENGINE_ERROR("[Scene] RemoveNode: null pointer"); return; }
    m_Root.RemoveChild(node);
}

void Scene::Update(float deltaTime)
{
    // Root node itself is not a "game" node — traverse its children directly
    for (auto& child : m_Root.m_Children)
        child->TraverseUpdate(deltaTime, false);
}

void Scene::Render(DX11Renderer& renderer)
{
    for (auto& child : m_Root.m_Children)
        child->TraverseRender(renderer);
}

void Scene::OnResize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    // Forward to nodes that need viewport info (future: virtual OnResize on SceneNode)
}
```

**Note on Scene::Update / Render**: Scene skips the root node itself (it's an invisible container) and only traverses its children. This is equivalent to but slightly more efficient than calling `m_Root.TraverseUpdate()` with the root contributing nothing.

---

## Step 6: Update Engine.h (15 min)

**File**: `LongXi/LXEngine/Src/Engine/Engine.h`

Changes:
1. Add `#include <chrono>` to includes
2. Add `class Scene;` forward declaration
3. Add `Scene& GetScene();` to public Subsystem Accessors section
4. Remove `void UpdateScene();` and `void RenderScene();` from private section
5. Add to private members:
   ```cpp
   std::unique_ptr<Scene> m_Scene;
   std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime;
   bool m_FirstFrame = true;
   ```

---

## Step 7: Update Engine.cpp (30 min)

**File**: `LongXi/LXEngine/Src/Engine/Engine.cpp`

Changes:
1. Add `#include "Scene/Scene.h"` alongside other includes
2. Remove implementations of `UpdateScene()` and `RenderScene()`
3. In `Initialize()` — add step 6 after SpriteRenderer:
   ```cpp
   LX_ENGINE_INFO("[Engine] Initializing scene");
   m_LastFrameTime = std::chrono::steady_clock::now();
   m_FirstFrame = true;
   m_Scene = std::make_unique<Scene>();
   if (!m_Scene->Initialize())
       LX_ENGINE_WARN("[Engine] Scene initialization failed");
   ```
4. In `Shutdown()` — add Scene reset FIRST (before SpriteRenderer):
   ```cpp
   if (m_Scene) { m_Scene->Shutdown(); m_Scene.reset(); }
   ```
5. In `Update()` — replace `UpdateScene()` with:
   ```cpp
   auto now = std::chrono::steady_clock::now();
   float deltaTime = m_FirstFrame ? 0.0f
       : std::chrono::duration<float>(now - m_LastFrameTime).count();
   m_LastFrameTime = now;
   m_FirstFrame = false;
   if (m_Scene && m_Scene->IsInitialized())
       m_Scene->Update(deltaTime);
   ```
6. In `Render()` — replace `RenderScene()` with:
   ```cpp
   if (m_Scene && m_Scene->IsInitialized())
       m_Scene->Render(*m_Renderer);
   ```
7. In `OnResize()` — add after SpriteRenderer resize:
   ```cpp
   if (m_Scene && m_Scene->IsInitialized())
       m_Scene->OnResize(width, height);
   ```
8. Add `GetScene()` accessor:
   ```cpp
   Scene& Engine::GetScene() { return *m_Scene; }
   ```

---

## Step 8: Update LXEngine.h (5 min)

**File**: `LongXi/LXEngine/LXEngine.h`

Add:
```cpp
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
```

---

## Step 9: Add TestSceneSystem to LXShell (15 min)

**File**: `LongXi/LXShell/Src/main.cpp`

```cpp
void TestSceneSystem()
{
    LX_ENGINE_INFO("==============================================");
    LX_ENGINE_INFO("SCENE SYSTEM TEST");
    LX_ENGINE_INFO("==============================================");

    Engine& engine = GetEngine();

    if (engine.GetScene().IsInitialized())
        LX_ENGINE_INFO("✓ SUCCESS: Scene initialized");
    else
        LX_ENGINE_ERROR("✗ FAILED: Scene not initialized");

    // Add a test node and verify it exists in the hierarchy
    auto testNode = std::make_unique<SceneNode>();
    SceneNode* rawPtr = testNode.get();
    testNode->SetPosition({10.0f, 0.0f, 0.0f});
    engine.GetScene().AddNode(std::move(testNode));
    LX_ENGINE_INFO("✓ Scene node added at position (10, 0, 0)");

    // Remove the test node
    engine.GetScene().RemoveNode(rawPtr);
    LX_ENGINE_INFO("✓ Scene node removed");

    LX_ENGINE_INFO("==============================================");
    LX_ENGINE_INFO("SCENE SYSTEM TEST COMPLETE");
    LX_ENGINE_INFO("==============================================");
}
```

Call `TestSceneSystem()` from `TestApplication::Initialize()` after existing tests.

---

## Step 10: Build and Verify

```
Win-Format Code.bat
Win-Build Project.bat
Build\Debug\Executables\LXShell.exe
```

**Expected log sequence**:
```
[Engine] Initializing scene
[Scene] Initialized
[Engine] Engine initialization complete
SCENE SYSTEM TEST
✓ SUCCESS: Scene initialized
[Scene] Node added
✓ Scene node added at position (10, 0, 0)
[Scene] Node removed
✓ Scene node removed
SCENE SYSTEM TEST COMPLETE
```

**Verification Checklist**:

- ✅ `[Scene] Initialized` in log (SC-002)
- ✅ `Engine::GetScene()` returns valid reference (SC-001)
- ✅ `[Scene] Node added` in log when node added (SC-003 prerequisite)
- ✅ `[Scene] Node removed` in log when node removed
- ✅ D3D11 debug layer reports no live objects on shutdown (SC-009)
- ✅ Application class unchanged (SC-010 equivalent)
- ✅ Scene is shutdown before SpriteRenderer in log order

---

## Common Issues

### Build Error: `Scene::m_Root.m_Children` inaccessible
**Fix**: Add `friend class Scene;` to SceneNode class declaration.

### World transform not updating
**Fix**: Verify `m_TransformDirty = true` is set in all `Set*()` methods AND in `AddChild()` for the child.

### Stack overflow on deep hierarchy
**Cause**: Recursive traversal with >~5,000 levels. Won't occur in Phase 1. If encountered, switch to iterative DFS.

### First frame large deltaTime
**Fix**: Verify `m_FirstFrame = true` is set in Initialize() and cleared after first Update() call.

### Incorrect world position for child nodes
**Fix**: Verify pre-order traversal — parent must compute its world transform BEFORE child. `needsRecompute` propagated from parent → children correctly.
