# Feature Specification: Camera System

**Feature Branch**: `012-camera-system`
**Created**: 2026-03-11
**Status**: Draft
**Input**: User description: "Spec 012 — Camera System: Introduce a Camera owned by Scene providing view and perspective projection matrices, handling window resize, with default position at (0, 0, -10)."

## Clarifications

### Session 2026-03-11

- Q: Matrix recalculation policy for camera updates? → A: Use dirty flags; camera setters mark matrices dirty, and matrices are recomputed once just before rendering.
- Q: Should camera getters auto-recompute matrices when dirty? → A: No. Getters return cached matrices only; recomputation happens during explicit pre-render update.
- Q: Should renderer expose camera matrices to nodes? → A: No. Scene/camera own camera state; renderer only consumes matrices for drawing.
- Q: How should invalid camera parameters be handled? → A: Clamp to safe range and log a warning.
- Q: How is initial aspect ratio determined before first resize? → A: Derive from renderer/backbuffer size during Scene initialization.

## User Scenarios & Testing

### User Story 1 — Scene Provides an Active Camera with View and Projection Matrices (Priority: P1)

As an engine developer, I want the Scene to own a Camera and expose it via `Scene::GetActiveCamera()` so that rendering code always has a single, consistent source for the view and projection matrices.

**Why this priority**: The Camera is the foundational prerequisite for all 3D rendering. Without valid view and projection matrices, no scene geometry can be positioned correctly in screen space. This is the enabling story for all other camera behavior.

**Independent Test**: Can be tested by verifying that `scene.GetActiveCamera()` returns a valid `Camera&` after `Scene::Initialize()` and that the returned camera provides non-zero view and projection matrices.

**Acceptance Scenarios**:

1. **Given** the Scene initializes successfully, **When** `Scene::GetActiveCamera()` is called, **Then** a valid `Camera` reference is returned
2. **Given** the Scene initializes, **When** `camera.GetViewMatrix()` is called, **Then** a non-identity 4×4 matrix is returned (camera is at default position, not origin)
3. **Given** the Scene initializes, **When** `camera.GetProjectionMatrix()` is called, **Then** a non-identity perspective projection matrix is returned
4. **Given** the Scene initializes, **When** the default camera position is queried, **Then** it is `(0, 0, -10)` and rotation is `(0, 0, 0)` degrees

---

### User Story 2 — Camera View Matrix Reflects Position and Orientation (Priority: P1)

As an engine developer, I want the camera view matrix to be recomputed whenever the camera position or rotation changes so that the rendered world correctly reflects the current viewpoint.

**Why this priority**: The view matrix directly controls what the player sees. An incorrect or stale view matrix means the world renders from the wrong position or direction. This must be correct before any world geometry is meaningful.

**Independent Test**: Can be tested by placing the camera at position (0, 0, -10) with zero rotation, calling `UpdateViewMatrix()`, and verifying that the resulting view matrix positions the world 10 units forward along the Z-axis.

**Acceptance Scenarios**:

1. **Given** camera is at position (0, 0, -10) with zero rotation, **When** `UpdateViewMatrix()` is called, **Then** the view matrix translates world space by +10 along Z (camera looks toward +Z)
2. **Given** camera position changes via `SetPosition()`, **When** `UpdateViewMatrix()` is called, **Then** `GetViewMatrix()` returns a matrix reflecting the new position
3. **Given** camera rotation changes via `SetRotation()`, **When** `UpdateViewMatrix()` is called, **Then** `GetViewMatrix()` returns a matrix that rotates the view accordingly
4. **Given** `SetPosition()` is called with any valid position, **When** `UpdateViewMatrix()` is called, **Then** the view matrix is recomputed before the next Scene::Render() call

---

### User Story 3 — Camera Projection Matrix Provides Perspective Frustum (Priority: P1)

As an engine developer, I want the camera to provide a perspective projection matrix so that the scene is rendered with correct depth perception and field of view.

**Why this priority**: The projection matrix defines the shape of the viewing frustum — without it, there is no perspective. All 3D rendering depends on a valid projection matrix being supplied to the renderer.

**Independent Test**: Can be tested by verifying the projection matrix encodes the configured field of view (45°), aspect ratio, near plane (1.0), and far plane (10000.0), and that it changes when `UpdateProjectionMatrix()` is called with new viewport dimensions.

**Acceptance Scenarios**:

1. **Given** the default camera is initialized, **When** `GetProjectionMatrix()` is called, **Then** the matrix encodes FOV=45°, near=1.0, far=10000.0 with the correct aspect ratio for the viewport
2. **Given** the viewport is 1024×768, **When** the projection matrix is computed, **Then** the aspect ratio is `1024.0 / 768.0 ≈ 1.333`
3. **Given** `SetFOV()` is called with a new field of view value, **When** `UpdateProjectionMatrix()` is called, **Then** `GetProjectionMatrix()` returns a matrix with the updated FOV encoded
4. **Given** near plane is 1.0 and far plane is 10000.0, **When** projection is computed, **Then** geometry outside this range is clipped

---

### User Story 4 — Renderer Receives Camera Matrices Before Scene Nodes Draw (Priority: P1)

As an engine developer, I want the renderer to receive the view and projection matrices from the active camera before any scene nodes submit draw calls so that every renderable node in the scene uses the correct camera-space transform.

**Why this priority**: Without passing camera matrices to the renderer before drawing, every renderable node would draw in undefined or world-origin space. This is the integration point that makes the camera system actually functional for rendering.

**Independent Test**: Can be tested by verifying that `DX11Renderer::SetViewProjection()` is called with the active camera's matrices during every `Scene::Render()` invocation, before any `SceneNode::Submit()` calls occur.

**Acceptance Scenarios**:

1. **Given** the scene renders a frame, **When** `Scene::Render()` is called, **Then** `DX11Renderer::SetViewProjection(viewMatrix, projMatrix)` is called with the active camera's matrices before any `SceneNode::Submit()` call
2. **Given** the camera position changes, **When** `Scene::Render()` runs the next frame, **Then** renderable nodes draw from the updated camera perspective
3. **Given** camera state is Scene-owned, **When** renderer integration is reviewed, **Then** no renderer camera-matrix getter API is required or exposed to scene nodes

---

### User Story 5 — Projection Matrix Updates on Window Resize (Priority: P2)

As an engine developer, I want the camera projection matrix to be recomputed when the window is resized so that the aspect ratio remains correct and the rendered image does not appear stretched.

**Why this priority**: On window resize, the viewport aspect ratio changes. Without updating the projection matrix, the rendered image will appear stretched or compressed. This is required for any real-world usage.

**Independent Test**: Can be tested by triggering `Scene::OnResize(1920, 1080)` and verifying the camera's projection matrix aspect ratio changes from `1024/768 ≈ 1.333` to `1920/1080 ≈ 1.778`.

**Acceptance Scenarios**:

1. **Given** the window is resized, **When** `Engine::OnResize()` is called, **Then** `Camera::UpdateProjectionMatrix(newWidth, newHeight)` is called through the resize chain
2. **Given** the window resizes from 1024×768 to 1920×1080, **When** the projection matrix is updated, **Then** the encoded aspect ratio changes from `1.333` to `1.778`
3. **Given** `Camera::UpdateProjectionMatrix()` is called with zero or negative dimensions, **When** the call is processed, **Then** the call is ignored and the previous projection matrix is retained

---

### User Story 6 — Camera Parameters Can Be Modified at Runtime (Priority: P2)

As an engine developer, I want to change camera position, rotation, and projection parameters at runtime so that I can implement camera controllers and test different viewpoints without restarting the application.

**Why this priority**: The camera is useless as a static object. Future controllers (third-person, free camera) will modify camera parameters every frame. The setter API must exist and work before those controllers can be built.

**Independent Test**: Can be tested by calling `SetPosition({5, 0, -10})`, then `UpdateViewMatrix()`, and verifying `GetViewMatrix()` differs from the view matrix at the default position.

**Acceptance Scenarios**:

1. **Given** the camera exists, **When** `SetPosition()`, `SetRotation()`, `SetFOV()`, `SetNearFar()` are called, **Then** the corresponding stored values are updated
2. **Given** camera parameters are updated, **When** `UpdateViewMatrix()` or `UpdateProjectionMatrix()` is called, **Then** the matrices reflect the new parameters
3. **Given** FOV is set to 60 degrees, **When** `UpdateProjectionMatrix()` is called, **Then** the projection matrix encodes a wider field of view than the default 45 degrees

---

### Edge Cases

- What happens when `UpdateViewMatrix()` is called before any position or rotation is set?
- What happens when `SetFOV()` is called with 0 degrees or negative FOV?
- What happens when `SetNearFar()` is called with near ≥ far?
- What happens when `UpdateProjectionMatrix()` is called with zero aspect ratio?
- What happens when `SetPosition()` is called during an active `Scene::Render()` traversal?
- What happens when `UpdateProjectionMatrix()` receives width=0 or height=0?
- What happens when the projection matrix is accessed before `Initialize()` is called?
- What happens when camera rotation exceeds 360 degrees?

---

## Requirements

### Functional Requirements

- **FR-001**: The system MUST introduce a `Camera` class that stores position (`Vector3`), rotation (`Vector3`, degrees), field of view (float, degrees), aspect ratio (float), near plane (float), and far plane (float)
- **FR-002**: The system MUST introduce a `Matrix4` type added to `LongXi/LXEngine/Src/Math/Math.h` as a 4×4 row-major float matrix — `struct Matrix4 { float m[16]; };`
- **FR-003**: The system MUST provide `Camera::UpdateViewMatrix()` that computes the view matrix from current position and rotation using an Euler-angle-based approach (yaw around Y-axis, pitch around X-axis, roll around Z-axis applied in YXZ order). Rotation values are stored in degrees; conversion to radians occurs internally during matrix computation.
- **FR-004**: The system MUST provide `Camera::UpdateProjectionMatrix(int viewportWidth, int viewportHeight)` that computes a left-handed perspective projection matrix using the current FOV, the aspect ratio derived from viewport dimensions, near plane, and far plane. The call MUST be ignored if `viewportWidth <= 0 || viewportHeight <= 0`.
- **FR-005**: The system MUST provide `Camera::GetViewMatrix() const` returning `const Matrix4&`
- **FR-006**: The system MUST provide `Camera::GetProjectionMatrix() const` returning `const Matrix4&`
- **FR-007**: The system MUST provide `Camera::SetPosition(Vector3)`, `Camera::GetPosition() const`
- **FR-008**: The system MUST provide `Camera::SetRotation(Vector3 degrees)`, `Camera::GetRotation() const`
- **FR-009**: The system MUST provide `Camera::SetFOV(float degrees)` and clamp invalid values to a safe range with a warning log
- **FR-010**: The system MUST provide `Camera::SetNearFar(float near, float far)` and clamp invalid near/far values to a safe valid range with a warning log
- **FR-011**: The system MUST initialize the Camera with default parameters: position `(0, 0, -10)`, rotation `(0, 0, 0)`, FOV=45°, near=1.0, far=10000.0
- **FR-012**: The system MUST give `Scene` ownership of a `Camera m_Camera` member (by value, not pointer)
- **FR-013**: The system MUST provide `Scene::GetActiveCamera()` returning `Camera&`
- **FR-014**: The system MUST call `Camera::UpdateViewMatrix()` and `Camera::UpdateProjectionMatrix()` during `Scene::Initialize()` after creating the default camera, so valid matrices are ready before the first render
- **FR-015**: The system MUST call `Scene::GetActiveCamera().UpdateProjectionMatrix(width, height)` from `Scene::OnResize()` when valid dimensions are provided
- **FR-016**: The system MUST call `DX11Renderer::SetViewProjection(const Matrix4& view, const Matrix4& proj)` from `Scene::Render()` with the active camera's matrices BEFORE calling `TraverseRender()` on any scene node
- **FR-017**: The system MUST introduce `DX11Renderer::SetViewProjection(const Matrix4& view, const Matrix4& proj)` as a consume-only renderer interface used by Scene before node submission
- **FR-018**: The system MUST NOT expose `DX11Renderer::GetViewMatrix()` or `DX11Renderer::GetProjectionMatrix()` camera-state accessors for scene nodes
- **FR-019**: The system MUST use a left-handed coordinate system for all matrix math, consistent with DirectX 11 conventions (confirmed by legacy client analysis: left-handed Y-up, `LookAtLH` / `PerspectiveFovLH` equivalents)
- **FR-020**: The system MUST place `Camera` source files in `LongXi/LXEngine/Src/Scene/` alongside Scene and SceneNode
- **FR-021**: The system MUST place `Camera` in the `LongXi` namespace
- **FR-022**: The system MUST add `Camera.h` to `LXEngine/LXEngine.h` for public access
- **FR-023**: The system MUST log `[Camera] Initialized` after the default camera is set up in `Scene::Initialize()`
- **FR-024**: The system MUST log `[Camera] View matrix updated` when `UpdateViewMatrix()` completes successfully
- **FR-025**: The system MUST log `[Camera] Projection updated` when `UpdateProjectionMatrix()` completes successfully
- **FR-026**: The system MUST mark the view matrix as dirty when camera position or rotation changes, and mark the projection matrix as dirty when FOV/near/far parameters change
- **FR-027**: The system MUST recompute only dirty camera matrices once per frame immediately before `DX11Renderer::SetViewProjection(...)` in `Scene::Render()`
- **FR-028**: `Camera::GetViewMatrix() const` and `Camera::GetProjectionMatrix() const` MUST NOT trigger recomputation and MUST return the last cached matrix values
- **FR-029**: Safe camera parameter ranges for clamping MUST be: FOV `[1.0, 179.0]`, NearPlane `>= 0.01`, and FarPlane `>= NearPlane + 0.01`
- **FR-030**: During `Scene::Initialize()`, camera aspect ratio MUST be derived from the current renderer/backbuffer dimensions before first projection update

### Key Entities

- **Camera**: Stores position, rotation (degrees), projection parameters (FOV, near, far), and computed view and projection matrices. Provides UpdateViewMatrix(), UpdateProjectionMatrix(), and accessors. Owned by Scene by value.
- **Matrix4**: New shared math type (`struct Matrix4 { float m[16]; }`) added to `Math/Math.h`. Row-major 4×4 float matrix. Used for view and projection matrices, and by future renderable nodes for world transform matrices.
- **Scene** (modified): Adds `Camera m_Camera` member, `GetActiveCamera()` accessor, and calls to camera update in `Initialize()`, `OnResize()`, and `Render()`.
- **DX11Renderer** (modified): Adds consume-only `SetViewProjection()` used by Scene to provide current camera matrices before node submission.

## Success Criteria

### Measurable Outcomes

- **SC-001**: `Scene::GetActiveCamera()` returns a valid `Camera&` after `Scene::Initialize()` — verified by calling the accessor and checking it doesn't crash
- **SC-002**: Default camera position is `(0, 0, -10)` and rotation is `(0, 0, 0)` — verified by calling `GetPosition()` and `GetRotation()` after init
- **SC-003**: `Camera::GetViewMatrix()` returns a non-identity matrix after init (camera is not at origin) — verified by inspecting matrix element `m[14]` (Z-translation component ≈ +10 for default position)
- **SC-004**: `Camera::GetProjectionMatrix()` encodes FOV=45°, near=1.0, far=10000.0 — verified by inspecting matrix elements
- **SC-005**: `DX11Renderer::SetViewProjection()` is called during every `Scene::Render()` invocation — verified by log `[Camera] View matrix updated` appearing before any renderable node draw
- **SC-006**: After `Scene::OnResize(1920, 1080)`, the projection matrix aspect ratio changes from `1024/768` to `1920/1080` — verified by comparing matrix element before and after resize
- **SC-007**: `Camera::UpdateProjectionMatrix(0, 0)` does NOT update the matrix (zero dimensions are rejected) — verified by matrix value unchanged after the call
- **SC-008**: `[Camera] Initialized` appears in log during `Scene::Initialize()`
- **SC-009**: Engine initializes and runs without crashes with the default camera active
- **SC-010**: `Camera::SetFOV(0)` is clamped to `1.0` and a warning log is emitted
- **SC-011**: `Camera::SetNearFar(10, 5)` is normalized to a valid near/far pair and a warning log is emitted
- **SC-012**: Before the first rendered frame, camera projection aspect ratio matches the renderer/backbuffer aspect ratio

## Out of Scope

The following items are explicitly OUT of scope for this specification:

- **Camera controllers**: FreeCameraController, ThirdPersonCameraController, isometric camera controller — all deferred to future specs using InputSystem integration
- **Multiple cameras**: Camera switching, render-to-texture with secondary camera, split-screen (deferred to future spec)
- **Orthographic projection**: 2D/isometric orthographic camera mode (deferred to future spec)
- **Camera animations / keyframes**: Cinematic camera paths, keyframe interpolation (deferred to future spec)
- **Camera frustum culling**: Visibility testing against camera frustum (deferred to future spec)
- **Camera shake / effects**: Screen shake, depth of field (deferred to future spec)
- **Camera parenting**: Attaching camera to a SceneNode (deferred — requires scene node integration)
- **LookAt helper**: `Camera::LookAt(Vector3 target)` convenience method (deferred to future spec)
- **Quaternion-based rotation**: Camera rotation via quaternions (deferred — Phase 1 uses Euler angles in degrees)
- **Aspect ratio locking**: Letterboxing or pillarboxing for fixed aspect ratios (deferred)
- **Z-buffer configuration**: Reverse-Z or logarithmic depth (deferred)

## Assumptions

1. **Left-handed coordinate system**: The engine uses a left-handed coordinate system consistent with DirectX 11 and the legacy client (`c3_camera` used `D3DXMatrixLookAtLH` and `D3DXMatrixPerspectiveFovLH`). The new camera must produce compatible matrices.
2. **Y-up convention**: The Y-axis is the world up direction. Legacy client analysis confirms Y-up with the map in the XZ plane. The default camera at `(0, 0, -10)` looks toward `+Z` in this coordinate system.
3. **Default FOV**: 45 degrees (from legacy camera defaults: `D3DXToRadian(45)` used in `Camera_Clear()`). This is the standard perspective FOV for this game engine.
4. **Default near/far planes**: near=1.0, far=10000.0 (from legacy client actual usage values, overriding the initial defaults of 10/10000).
5. **Rotation order**: Euler angles applied as Yaw (Y-axis) → Pitch (X-axis) → Roll (Z-axis), consistent with the engine's existing SceneNode rotation convention.
6. **Matrix4 row-major**: The existing SpriteRenderer uses row-major matrices with `#pragma pack_matrix(row_major)` in HLSL. Camera matrices must also be row-major to maintain consistency.
7. **Dirty-flag matrix update policy**: Camera setters only mark matrices dirty. `Scene::Render()` performs one pre-submit synchronization pass that updates only matrices marked dirty before calling `SetViewProjection()`.
8. **Getter behavior**: Camera matrix getters return cached data only and never trigger hidden recomputation.
9. **Renderer consume-only contract**: `SetViewProjection()` is an input contract from Scene to renderer; camera state ownership remains in Scene/Camera and renderer does not expose camera matrix getters for nodes.
10. **Camera is not a SceneNode**: Camera is a standalone value-type owned by Scene, not a node in the hierarchy. Future camera-parenting to a SceneNode is deferred.
11. **File placement**: Camera source files go in `LXEngine/Src/Scene/` alongside Scene.h and SceneNode.h, consistent with the existing scene subsystem organization.
12. **No external math library**: Matrix computation uses hand-computed float arrays, consistent with the existing Math.h approach (Vector2, Vector3, Color are all hand-defined POD structs).
13. **Testing**: Manual log-driven testing — `[Camera] Initialized` confirms setup; visual inspection of rendered output confirms correctness; no automated test harness.
14. **Initial aspect ratio source**: Scene initialization reads active renderer/backbuffer dimensions and derives the camera aspect ratio before the first projection matrix computation.
