# Contract: Camera / Scene / Renderer Integration

**Feature**: `012-camera-system`  
**Version**: 1.0  
**Date**: 2026-03-11

## Purpose

Define the interface and call-order contract between `Camera`, `Scene`, and `DX11Renderer` for frame-consistent view/projection usage.

## 1) Camera Public Contract

### Required API

```cpp
class Camera
{
public:
    // Transform
    void SetPosition(Vector3 position);
    Vector3 GetPosition() const;
    void SetRotation(Vector3 rotationDegrees);
    Vector3 GetRotation() const;

    // Projection params
    void SetFOV(float degrees);
    float GetFOV() const;
    void SetNearFar(float nearPlane, float farPlane);
    float GetNearPlane() const;
    float GetFarPlane() const;

    // Matrix update + access
    void UpdateViewMatrix();
    void UpdateProjectionMatrix(int viewportWidth, int viewportHeight);
    void SyncDirtyMatricesForRender(int viewportWidth, int viewportHeight);
    const Matrix4& GetViewMatrix() const;
    const Matrix4& GetProjectionMatrix() const;
};
```

### Behavioral guarantees

- Setters mark matrix dirty state as needed.
- Invalid projection parameters are clamped to safe values and logged as warnings.
- Getters never recompute matrices implicitly.

## 2) Scene Public Contract

### Required API

```cpp
class Scene
{
public:
    bool Initialize(DX11Renderer& renderer);
    void Shutdown();
    bool IsInitialized() const;

    Camera& GetActiveCamera();

    void Update(float deltaTime);
    void Render(DX11Renderer& renderer);
    void OnResize(int width, int height);
};
```

### Behavioral guarantees

- `GetActiveCamera()` always returns the single active camera after successful initialize.
- Scene computes/synchronizes camera matrices before renderer camera-state consume call.
- Scene calls renderer consume API before any scene-node render submission.
- Scene remains draw-command agnostic.

## 3) Renderer Consume Contract

### Required API

```cpp
class DX11Renderer
{
public:
    void SetViewProjection(const Matrix4& view, const Matrix4& projection);
};
```

### Optional integration API (for camera bootstrap)

```cpp
int GetViewportWidth() const;
int GetViewportHeight() const;
```

### Behavioral guarantees

- Renderer accepts latest camera matrices each frame.
- Renderer does not expose camera matrix getters for scene nodes.

## 4) Call-Order Contract

Per-frame render sequence must be:

```text
Engine::Render()
  -> Renderer::BeginFrame()
  -> Scene::Render(renderer)
      -> Camera pre-render matrix sync (dirty-only)
      -> Renderer::SetViewProjection(view, projection)
      -> Scene node submit traversal
  -> Renderer::EndFrame()
```

Non-compliant sequence examples:
- Node submit before `SetViewProjection`
- Scene issuing draw calls directly
- Getter-triggered matrix recompute during render traversal

## 5) Resize Contract

Required flow:

```text
Win32Window -> Application -> Engine::OnResize -> Scene::OnResize -> Camera::UpdateProjectionMatrix
```

Rules:
- Width/height `<= 0` do not trigger projection recompute.
- Valid resize updates aspect ratio and marks/updates projection matrix.

## 6) Validation Contract

- FOV clamp: `[1.0, 179.0]`
- Near clamp: `>= 0.01`
- Far clamp: `>= Near + 0.01`
- Any clamped input emits warning log.

## 7) Threading Contract

- Camera mutation, scene render sync, and renderer consume call are all main-thread only in Spec 012.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
