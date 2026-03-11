# Contract: Scene System Public API

**Feature**: 011-scene-system
**Namespace**: `LongXi`

---

## Math Extension Contract

**Header**: `LongXi/LXEngine/Src/Math/Math.h` (addition to existing file)

```cpp
namespace LongXi
{

struct Vector3
{
    float x;
    float y;
    float z;
};

} // namespace LongXi
```

---

## SceneNode Contract

**Header**: `LongXi/LXEngine/Src/Scene/SceneNode.h`

```cpp
namespace LongXi
{

class DX11Renderer;

class SceneNode
{
  public:
    SceneNode();
    virtual ~SceneNode();

    // Non-copyable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // =========================================================================
    // Hierarchy
    // =========================================================================

    // Take ownership of child, set child's parent pointer (immutable after this).
    void AddChild(std::unique_ptr<SceneNode> child);

    // Find and destroy child by pointer. No-op with error log if not found.
    void RemoveChild(SceneNode* child);

    // =========================================================================
    // Local Transform (degrees for rotation)
    // =========================================================================

    void SetPosition(Vector3 position);
    void SetRotation(Vector3 rotationDegrees);
    void SetScale(Vector3 scale);

    const Vector3& GetPosition() const;
    const Vector3& GetRotation() const;  // degrees
    const Vector3& GetScale() const;

    // =========================================================================
    // World Transform (computed; read-only)
    // =========================================================================

    const Vector3& GetWorldPosition() const;
    const Vector3& GetWorldRotation() const;  // degrees
    const Vector3& GetWorldScale() const;

    // =========================================================================
    // Overridable behaviour (subclasses implement game logic / draw calls)
    // =========================================================================

    // Called once per frame during Scene::Update() traversal.
    // deltaTime is elapsed seconds since last frame.
    virtual void Update(float deltaTime);

    // Called once per frame during Scene::Render() traversal.
    // Subclasses issue draw calls here. Base implementation is a no-op.
    // Scene itself NEVER calls any renderer methods directly.
    virtual void Submit(DX11Renderer& renderer);

  private:
    // Internal traversal — called by Scene only
    void TraverseUpdate(float deltaTime, bool parentWasRecomputed);
    void TraverseRender(DX11Renderer& renderer);
    void ComputeWorldTransform();

    friend class Scene;

  private:
    Vector3 m_Position   = {0.0f, 0.0f, 0.0f};
    Vector3 m_Rotation   = {0.0f, 0.0f, 0.0f};  // degrees
    Vector3 m_Scale      = {1.0f, 1.0f, 1.0f};

    Vector3 m_WorldPosition = {0.0f, 0.0f, 0.0f};
    Vector3 m_WorldRotation = {0.0f, 0.0f, 0.0f};  // degrees
    Vector3 m_WorldScale    = {1.0f, 1.0f, 1.0f};

    bool m_TransformDirty = true;  // true at construction — computed on first traversal

    SceneNode* m_Parent = nullptr;  // non-owning; set by AddChild(), immutable after
    std::vector<std::unique_ptr<SceneNode>> m_Children;
};

} // namespace LongXi
```

---

## Scene Contract

**Header**: `LongXi/LXEngine/Src/Scene/Scene.h`

```cpp
namespace LongXi
{

class DX11Renderer;

class Scene
{
  public:
    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Lifecycle
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    // Top-level node management (attaches to scene root)
    void AddNode(std::unique_ptr<SceneNode> node);
    void RemoveNode(SceneNode* node);

    // Per-frame (called by Engine)
    void Update(float deltaTime);
    void Render(DX11Renderer& renderer);
    void OnResize(int width, int height);  // ignored if width <= 0 || height <= 0

  private:
    SceneNode m_Root;
    bool m_Initialized = false;
};

} // namespace LongXi
```

---

## Engine Extension Contract

New additions to `Engine` (Engine.h / Engine.cpp):

```cpp
// Engine.h — new public accessor
Scene& GetScene();

// Engine.h — new private members
std::unique_ptr<Scene> m_Scene;
std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime;
bool m_FirstFrame = true;
```

---

## Subclassing Contract (for game code)

To create a renderable world object:

```cpp
class MySpriteNode : public SceneNode
{
  public:
    explicit MySpriteNode(Texture* texture) : m_Texture(texture) {}

    void Update(float deltaTime) override
    {
        // move, animate, apply game logic
    }

    void Submit(DX11Renderer& renderer) override
    {
        // issue draw call using renderer.GetDevice() / renderer.GetContext()
        // use GetWorldPosition(), GetWorldRotation(), GetWorldScale() for transform
    }

  private:
    Texture* m_Texture;  // non-owning
};
```

**Rules for subclasses**:
- `Submit()` is the ONLY place draw calls are issued for this node
- `Update()` MUST NOT issue draw calls
- `Submit()` MUST NOT mutate the scene hierarchy (undefined behavior)
- Use `GetWorldPosition/Rotation/Scale()` for rendering, not local transforms

---

## Traversal Contract

| Call | Order | Transform recomputed? |
|------|-------|----------------------|
| `Scene::Update()` | Pre-order DFS | Yes — dirty nodes or children of dirty nodes |
| `Scene::Render()` | Pre-order DFS | No — world transforms already fresh from Update |

**deltaTime contract**:
- First frame: `deltaTime = 0.0f`
- Subsequent frames: elapsed seconds since last `Engine::Update()` call
- Measured with `std::chrono::steady_clock`

---

## Error Handling Contract

| Condition | Behavior |
|-----------|----------|
| `Scene::AddNode(nullptr)` | Ignored, log `[Scene] AddNode: null node` |
| `Scene::RemoveNode(nullptr)` | Ignored, log `[Scene] RemoveNode: null pointer` |
| `Scene::RemoveNode(notFound)` | Ignored, log `[Scene] RemoveNode: node not found` |
| `Scene::OnResize(≤0, ...)` | Ignored silently |
| `Scene::Initialize()` fails | Returns false; Engine continues (non-fatal) |
| `SceneNode::RemoveChild(nullptr)` | Ignored, log error |
| `SceneNode::RemoveChild(notFound)` | Ignored, log error |

---

## Logging Contract

| Event | Log Message |
|-------|-------------|
| Initialize success | `[Scene] Initialized` |
| Initialize failure | `[Scene] Initialization failed` |
| Shutdown | `[Scene] Shutdown` |
| Node added | `[Scene] Node added` |
| Node removed | `[Scene] Node removed` |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
