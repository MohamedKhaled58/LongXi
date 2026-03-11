# Quickstart: Camera System Implementation

**Feature**: `012-camera-system`  
**Branch**: `012-camera-system`  
**Goal**: Implement a Scene-owned camera that provides view/projection matrices to renderer with dirty-flag synchronization.

## Prerequisites

- Read [spec.md](./spec.md)
- Read [research.md](./research.md)
- Read [data-model.md](./data-model.md)
- Read [contracts/camera-scene-renderer.md](./contracts/camera-scene-renderer.md)

## Step 1: Add Matrix Type

**File**: `LongXi/LXEngine/Src/Math/Math.h`

- Add `Matrix4` POD type (`float m[16]`) with row-major convention comments.
- Keep existing `Vector2`, `Vector3`, and `Color` unchanged.

## Step 2: Implement Camera Class

**New files**:
- `LongXi/LXEngine/Src/Scene/Camera.h`
- `LongXi/LXEngine/Src/Scene/Camera.cpp`

Implement:
- Transform setters/getters (`Position`, `RotationDegrees`)
- Projection setters/getters (`FOV`, `Near/Far`)
- Matrix getters (cached-only)
- `UpdateViewMatrix()`
- `UpdateProjectionMatrix(width, height)`
- Dirty flags for view/projection
- Clamp + warning behavior for invalid projection inputs

## Step 3: Integrate Camera into Scene

**Files**:
- `LongXi/LXEngine/Src/Scene/Scene.h`
- `LongXi/LXEngine/Src/Scene/Scene.cpp`

Changes:
- Add `Camera m_Camera` field
- Add `Camera& GetActiveCamera()`
- `Initialize(DX11Renderer& renderer)`:
- apply default camera values
- derive aspect ratio from renderer/backbuffer size
- compute initial matrices
- log `[Camera] Initialized`
- `OnResize(width, height)` forwards valid dimensions to camera projection update path
- `Render(renderer)`:
- perform dirty-only camera sync via `SyncDirtyMatricesForRender(...)`
- call `renderer.SetViewProjection(view, projection)`
- then traverse scene-node submit path

## Step 4: Extend DX11Renderer Interface

**Files**:
- `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`

Add:
- `SetViewProjection(const Matrix4&, const Matrix4&)`
- Internal storage for consumed view/projection matrices
- Viewport/backbuffer width-height tracking during initialize/resize
- Optional viewport dimension getters used by Scene initialize

Constraints:
- Do not add camera matrix getters for scene nodes.
- Keep renderer as consume-only endpoint for camera state.

## Step 5: Expose Camera in Public Engine Include

**File**: `LongXi/LXEngine/LXEngine.h`

- Add/include `Scene/Camera.h` in the public engine include surface.

## Step 6: Build and Smoke Test

1. Generate/build project as usual.
2. Launch LXShell.
3. Verify logs and behavior:
- `[Scene] Initialized`
- `[Camera] Initialized`
- `[Camera] View matrix updated` when transform changes
- `[Camera] Projection updated` on valid projection update/resize
4. Resize window and confirm no stretch artifacts from stale aspect ratio.
5. Confirm invalid parameter inputs clamp and produce warning logs (not crashes).

## Validation Results (2026-03-11)

- Build command executed: `MSBuild.exe LongXi.slnx /t:Build /p:Configuration=Debug /m`
- Result: build failed due missing required toolset `v145` on this machine (`MSB8020`).
- Manual smoke run: blocked because the executable could not be built in the current environment.

## Manual Validation Checklist

- `Scene::GetActiveCamera()` returns valid reference after init.
- First rendered frame uses backbuffer-derived aspect ratio.
- Camera getters do not mutate state.
- Dirty-flag recompute happens once before render submit (not every getter call).
- Renderer receives camera matrices before any node submit path executes.
- Zero-size resize does not corrupt current projection matrix.

## Out-of-Scope Guardrails

Do not add in this implementation:
- Multiple active cameras
- Camera controllers (free/third-person)
- Orthographic projection
- SceneNode-attached camera parenting
- Multithreaded camera update paths
