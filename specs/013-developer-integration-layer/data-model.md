# Data Model: Developer Integration Layer (LXShell + Dear ImGui)

**Feature**: `013-developer-integration-layer`
**Date**: 2026-03-11
**Phase**: 1 - Design and Contracts

## Overview

This feature introduces runtime developer-observability models in LXShell only. No persistent storage is added. Models below define ownership, data flow, validation, and lifecycle states.

## Entities

### 1) DebugUI

**Purpose**: Own Dear ImGui lifecycle and panel rendering for developer inspection.

**Owned by**: LXShell runtime (application-level composition)

**Core fields**:
- `bool Initialized`
- `bool EnabledForBuild`
- `void* WindowHandle` (non-owning)
- `void* DeviceHandle` (non-owning)
- `void* DeviceContextHandle` (non-owning)
- `PanelSet Panels`

**Validation rules**:
- Can initialize only when Engine + renderer context are valid.
- Can render only when initialized and enabled for build.
- Must shutdown before renderer/device teardown.

**State transitions**:
- `Created -> Initialized -> FrameActive (per frame) -> Shutdown -> Destroyed`
- `Created -> InitFailed -> DisabledDegradedMode`

### 2) PanelSet

**Purpose**: Container and dispatcher for all debug panels.

**Owned by**: `DebugUI`

**Members**:
- `EnginePanel`
- `SceneInspector`
- `TextureViewer`
- `CameraPanel`
- `InputMonitor`

**Rules**:
- Panel draw order is deterministic each frame.
- Each panel may be visible/hidden independently.
- Panels read from Engine-facing adapters/view models only.

### 3) EngineMetricsSnapshot

**Purpose**: Per-frame metrics shown in Engine Panel.

**Fields**:
- `float FramesPerSecond`
- `float FrameTimeMs`
- `int DrawCallCount`
- `string GpuDeviceName`

**Rules**:
- Snapshot refreshed once per frame.
- Missing metrics are displayed as unavailable states, not guessed values.

### 4) SceneNodeViewModel

**Purpose**: Scene hierarchy item and selection payload for Scene Inspector.

**Fields**:
- `string NodeName`
- `string NodePath`
- `Vector3 Position`
- `Vector3 Rotation`
- `Vector3 Scale`
- `bool Selected`

**Relationships**:
- Hierarchical parent/children representation under scene root.

**Validation rules**:
- Selection references one active node at a time.
- Transform edits apply only to selected node.

### 5) TextureInfoViewModel

**Purpose**: Texture list row for Texture Viewer.

**Fields**:
- `string TextureName`
- `int Width`
- `int Height`
- `size_t MemoryBytes`

**Rules**:
- One row per loaded texture.
- Empty texture inventory renders an explicit empty-state message.

### 6) CameraStateViewModel

**Purpose**: Editable active-camera values for Camera Panel.

**Fields**:
- `Vector3 Position`
- `Vector3 RotationDegrees`
- `float FieldOfView`
- `float NearPlane`
- `float FarPlane`

**Validation rules**:
- Uses camera's existing clamping/normalization behavior for invalid ranges.
- Updates must mark camera matrices dirty and apply by next render sync.

### 7) InputStateViewModel

**Purpose**: Current input state shown in Input Monitor.

**Fields**:
- `Vector2 MousePosition`
- `set MouseButtonsDown`
- `set KeysDown`
- `bool ConsumedByDebugUI` (last event route indicator)

**Rules**:
- Reflects post-routing state.
- Consumed events do not mutate InputSystem state.

### 8) InputRoutingDecision

**Purpose**: Per-event routing outcome between DebugUI and InputSystem.

**Fields**:
- `EventType`
- `bool Consumed`
- `string RoutedTo` (`DebugUIOnly` or `DebugUIAndInputSystem`)

**State transitions**:
- `Received -> EvaluatedByDebugUI -> Consumed|Forwarded`

### 9) TestSceneConfig

**Purpose**: Development-only validation scene bootstrap configuration.

**Fields**:
- `bool Enabled`
- `string TexturePath` (`Data/texture/test.dds`)
- `string RootNodeName`
- `string TestSpriteNodeName`

**Rules**:
- Only active in development/debug builds.
- Missing texture path triggers warning and fallback behavior, not silent failure.

## Relationships Summary

```text
DebugUI
 |-- owns PanelSet
 |-- reads EngineMetricsSnapshot
 |-- reads/edits SceneNodeViewModel
 |-- reads TextureInfoViewModel
 |-- reads/edits CameraStateViewModel
 |-- reads InputStateViewModel

Win32 Event
 -> InputRoutingDecision
    -> DebugUI consumes OR InputSystem receives
```

## Invariants

- Engine has no dependency on DebugUI or Dear ImGui headers.
- DebugUI exists only in LXShell runtime scope.
- DebugUI frame render occurs before present and after scene/sprite work.
- DebugUI lifecycle is disabled/excluded in production build targets.
- DebugUI shutdown precedes renderer teardown.

## Edge-Case Handling Model

- DebugUI init failure: disable DebugUI path and continue runtime with warning logs.
- Zero scene nodes / zero textures: show explicit empty panel states.
- Invalid camera edits: rely on camera validation/clamping rules with warnings.
- Rapid focus changes: maintain deterministic per-event routing based on consumption result.
- Resize during interaction: update viewport-dependent UI state on next frame without crashing.
