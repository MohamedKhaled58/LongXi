# Contract: Renderer Public API (Backend-Agnostic Surface)

## Purpose
Define the renderer contract consumed by engine systems and shell integrations, including lifecycle ordering, pass boundaries, and state invariants.

## Public Headers
- `LongXi/LXEngine/Src/Renderer/Renderer.h`
- `LongXi/LXEngine/Src/Renderer/RendererTypes.h`

## Contracted Operation Set

| Operation | Required State | Guaranteed Result |
|-----------|----------------|-------------------|
| `Initialize(HWND, int, int)` | Not initialized, valid window, positive size | Device/swapchain/surface resources created, phase set to `NotStarted` |
| `BeginFrame()` | `NotStarted` or `FrameEnded`, no active pass | Baseline GPU state rebound, queued resize applied if valid, phase set to `InFrame` |
| `BeginPass(RenderPassType)` | `InFrame`, no active pass | Pass state applied, phase set to `InPass` |
| `EndPass()` | `InPass` | Pass closed, baseline depth state restored, phase set to `InFrame` |
| `ExecuteExternalPass(ExternalPassCallback)` | `InFrame`, callback non-null | Renderer wraps callback in renderer-owned external pass boundary |
| `EndFrame()` | `InFrame`, no active pass | Frame closed, phase set to `FrameEnded` |
| `Present()` | `FrameEnded` | Swapchain present attempted, frame phase reset for next frame |
| `OnResize(int, int)` | Initialized | Mid-frame resize queued; safe-frame resize recreates RT/depth/viewport |
| `SetViewProjection(const Matrix4&, const Matrix4&)` | Initialized | Active camera matrices stored for current frame use |

## Lifecycle and Pass Rules
- Only one active pass is allowed at any point.
- Direct draw submission must happen inside an active pass.
- External tool rendering must run through `ExecuteExternalPass`, not direct pass ownership from callers.
- Contract violations are rejected and logged.

## Baseline GPU State Invariants
Immediately after `BeginFrame`, renderer guarantees:
- Render target bound
- Depth-stencil target bound
- Viewport applied
- Rasterizer state applied
- Blend state applied
- Depth-stencil state applied

No caller may assume previous-frame state survives.

## Resize Contract
- `OnResize(width, height)` with zero/negative dimensions is deferred safely.
- If called during `InPass`, resize is queued and applied at next safe `BeginFrame`.
- Applying resize recreates swapchain-dependent render resources and updates viewport atomically.

## Recovery Contract
On present/device/swapchain failure:
- Renderer logs failure context.
- Renderer transitions to safe recovery mode (`RecoveryPending`/`SafeNoRender`).
- Rendering is skipped until recovery path re-establishes valid state.

## Boundary Rules
- Engine systems may call only the renderer API surface.
- DirectX headers/types remain internal to renderer backend implementation and shell-side ImGui integration.
- Include leakage is validated by `Scripts/Audit-RendererBoundaries.ps1`.

## Current Validation Status
- Boundary audit: PASS (`ok=true`, no leakage outside renderer module).
- Build validation: PASS (Debug x64 build completed successfully on 2026-03-11).
- Runtime stress scenarios: tracked in `quickstart.md`.
