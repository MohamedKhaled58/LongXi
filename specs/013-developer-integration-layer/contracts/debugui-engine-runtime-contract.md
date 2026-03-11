# Contract: DebugUI / Application / Engine Runtime Integration

**Feature**: `013-developer-integration-layer`
**Version**: 1.0
**Date**: 2026-03-11

## Purpose

Define lifecycle, render-order, input-routing, resize, and shutdown contracts for integrating LXShell DebugUI with Engine runtime while preserving module boundaries.

## 1) Ownership and Dependency Contract

- DebugUI is owned by LXShell runtime composition.
- Engine does not include DebugUI or Dear ImGui headers.
- Allowed direction: `LXShell(DebugUI) -> Engine`.
- Forbidden direction: `Engine -> DebugUI` and `Engine -> ImGui`.

## 2) DebugUI Public Runtime Contract

Required subsystem behaviors:

```text
Initialize(windowHandle, rendererDevice, rendererContext) -> bool
OnFrameBegin()
RenderPanelsAndSubmit()
OnResize(width, height)
HandleWin32Event(...) -> consumed(bool)
Shutdown()
IsInitialized() -> bool
```

Behavioral guarantees:
- `Initialize` may run only after successful Engine init.
- `RenderPanelsAndSubmit` is called at most once per frame when initialized.
- `Shutdown` is idempotent and safe when partially initialized.

## 3) Application and Frame Lifecycle Contract

Required startup ordering:

```text
Application.Initialize()
 -> Engine.Initialize()
 -> DebugUI.Initialize(...)
```

Required per-frame ordering:

```text
Engine.Render()
  -> Renderer.BeginFrame()
  -> Scene.Render()
  -> SpriteRenderer flush/submit
  -> DebugUI.RenderPanelsAndSubmit()
  -> Renderer.EndFrame()
```

Rules:
- DebugUI draw data submission occurs before `EndFrame`.
- DebugUI render path must not call present directly.

## 4) Input Routing Contract

Per-event routing:

```text
consumed = DebugUI.HandleWin32Event(event)
if consumed:
    do not forward event to InputSystem
else:
    forward event to InputSystem
```

Guarantees:
- Consumed events never mutate InputSystem state.
- Non-consumed events are processed by InputSystem unchanged.

## 5) Resize Contract

Required flow:

```text
Win32Window -> Application -> Engine.OnResize -> Renderer.OnResize -> DebugUI.OnResize
```

Rules:
- DebugUI uses resized viewport on subsequent frame.
- Invalid dimensions follow existing runtime guard behavior.

## 6) Panel Contract

Required panel set:
- EnginePanel
- SceneInspector
- TextureViewer
- CameraPanel
- InputMonitor

Panel behaviors:
- EnginePanel: read-only metrics (FPS, frame time, renderer stats, GPU info).
- SceneInspector: hierarchy display + editable transform for selected node.
- TextureViewer: loaded texture list + resolution + memory usage.
- CameraPanel: editable active camera fields.
- InputMonitor: runtime mouse and keyboard state display.

## 7) Development Build Restriction Contract

- DebugUI compiles only for development/debug build targets.
- Production targets exclude or disable DebugUI execution path.
- Build gating must be explicit and reviewable in project configuration.

## 8) Failure and Logging Contract

Failure policy:
- DebugUI init failure enters degraded mode (UI disabled), runtime continues, warning logged.

Required logs:
- `[DebugUI] Initialized`
- `[DebugUI] Scene inspector opened` (or equivalent panel-open trace)
- `[DebugUI] Camera parameters updated`

## 9) Shutdown Contract

Required teardown ordering:

```text
DebugUI.Shutdown()
 -> ImGui backend shutdown sequence
 -> ImGui context destroy
 -> Engine/renderer teardown continues
```

Rules:
- DebugUI teardown must complete before renderer destruction.
- Repeated startup/shutdown cycles must not leak DebugUI resources.
