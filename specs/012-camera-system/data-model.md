# Data Model: Camera System

**Feature**: `012-camera-system`  
**Date**: 2026-03-11  
**Phase**: 1 - Design & Contracts

## Overview

This feature is primarily a runtime state model (no persistent storage). The model below defines camera state, ownership relationships, validation rules, and lifecycle transitions across Scene and Renderer.

## Entities

### 1) Camera

**Purpose**: Own viewpoint and projection state used by the renderer each frame.

**Owned by**: `Scene` (single active camera, value member)

**Fields**:
- `Vector3 m_Position`
- `Vector3 m_RotationDegrees`
- `float m_FieldOfViewDegrees`
- `float m_AspectRatio`
- `float m_NearPlane`
- `float m_FarPlane`
- `Matrix4 m_ViewMatrix`
- `Matrix4 m_ProjectionMatrix`
- `bool m_ViewDirty`
- `bool m_ProjectionDirty`

**Validation / normalization rules**:
- `FOV` clamped to `[1.0, 179.0]`
- `NearPlane` clamped to `>= 0.01`
- `FarPlane` clamped to `>= NearPlane + 0.01`
- Aspect ratio recompute is skipped for invalid viewport dimensions (`<= 0`)

**Behavioral rules**:
- `SetPosition` / `SetRotation` set `m_ViewDirty = true`
- `SetFOV` / `SetNearFar` set `m_ProjectionDirty = true`
- Getters return cached matrices only (no implicit recompute)
- Recompute occurs during explicit pre-render sync

### 2) Scene (camera-facing subset)

**Purpose**: Camera owner and render orchestration boundary.

**Fields added/modified**:
- `Camera m_Camera` (single active camera)

**Relationships**:
- Owns `Camera` lifecycle
- Provides `Camera& GetActiveCamera()`
- On render, synchronizes dirty camera matrices before submitting to renderer

**Event flow responsibilities**:
- `Initialize`: configure default camera and initialize matrices using renderer/backbuffer dimensions
- `OnResize`: forward dimensions to camera projection update path
- `Render`: call renderer consume API before scene-node submit traversal

### 3) DX11Renderer (camera integration subset)

**Purpose**: Consume camera matrices for draw pipeline setup.

**Fields added/modified**:
- `Matrix4 m_CurrentViewMatrix` (cached consumed view)
- `Matrix4 m_CurrentProjectionMatrix` (cached consumed projection)
- `int m_ViewportWidth` / `int m_ViewportHeight` (tracked from init + resize for camera bootstrap)

**Public API**:
- `void SetViewProjection(const Matrix4& view, const Matrix4& projection)`
- Optional viewport dimension accessors for scene bootstrap (`GetViewportWidth()`, `GetViewportHeight()`)

**Rules**:
- Renderer consumes state from Scene; does not expose camera matrix getters for scene nodes.

### 4) Matrix4

**Purpose**: Shared 4x4 matrix type used for view/projection.

**Definition**:
- `struct Matrix4 { float m[16]; };`

**Storage convention**:
- Row-major float array.

**Usage**:
- Camera internal matrices
- Renderer consume interface payload
- Future renderables/world transforms

## Relationships Summary

```text
Scene
 └── owns Camera (1:1)

Scene::Render()
 ├── Camera sync dirty matrices
 └── DX11Renderer::SetViewProjection(view, projection)
```

## State Transitions

### Camera Matrix State

```text
Clean
  ├── SetPosition/SetRotation -> ViewDirty
  ├── SetFOV/SetNearFar -> ProjectionDirty
  └── OnResize(valid) -> ProjectionDirty

ViewDirty / ProjectionDirty
  └── PreRenderSync() -> Clean
```

### Camera Lifecycle

```text
Constructed
  -> Scene::Initialize (defaults applied + first matrix compute)
  -> Active Runtime (setter-driven dirty flags + pre-render sync)
  -> Scene::Shutdown (camera destroyed with Scene)
```

## Invariants

- Exactly one active camera in Scene for Spec 012.
- Camera exists and is valid after successful Scene initialization.
- Scene calls renderer consume API before any renderable submission each frame.
- Scene never issues draw commands directly.
- Camera and Scene updates run on main thread only.

## Edge-Case Handling Model

- Invalid parameter input: clamped + warning log.
- Invalid resize dimensions: retain previous projection matrix.
- Getter before first update: initialization guarantees matrices are valid before first render.
- Rotation overflow: accepted; matrix math uses normalized trigonometric periodicity.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
