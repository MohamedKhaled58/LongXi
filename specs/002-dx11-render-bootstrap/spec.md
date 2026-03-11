# Spec 002 — DX11 Render Bootstrap

**Feature Branch**: `002-dx11-render-bootstrap`
**Created**: 2026-03-10
**Status**: Draft
**Input**: User description: "DX11 Render Bootstrap"

## Objective

Establish a minimal, stable DirectX 11 render bootstrap that initializes against the existing Win32 window, supports clear and present each frame, and shuts down cleanly.

## Problem Statement

The shell bootstrap (Spec 001) is complete: `LXShell.exe` launches a Win32 window with lifecycle management, structured logging, and clean shutdown. However, no rendering backend exists. The window client area is painted by the default `BLACK_BRUSH` background — there is no GPU-driven rendering.

Later visual and system integration cannot proceed safely until the renderer bootstrap exists. The project requires a stable device/swap chain/render target foundation before any higher rendering work (shaders, meshes, textures, scene rendering) can begin.

This spec addresses only the minimal DirectX 11 bootstrap integration into the existing application lifecycle.

## Scope

This specification includes only:

- DirectX 11 device creation
- Device context creation
- Swap chain creation bound to the existing Win32 window
- Back buffer acquisition
- Render target view creation
- Per-frame clear operation (single solid color)
- Per-frame present operation
- Resize handling sufficient to recreate render-target-dependent state safely
- Controlled DX11 shutdown and resource cleanup
- Logging and diagnostic hooks sufficient for renderer bootstrap visibility

## Out of Scope

This specification explicitly excludes:

- Texture loading
- Shader system development beyond strict bootstrap needs
- Pipeline abstraction layers beyond what bootstrap needs
- Mesh or model rendering
- Camera systems
- Scene rendering
- Depth/stencil buffer (deferred to a later spec unless strictly required for bootstrap)
- ImGui or editor overlays
- Resource manager integration beyond what bootstrap needs
- Animation or visual effects
- Gameplay rendering
- Multithreaded rendering
- DirectX 12, Vulkan, or OpenGL support

## User Scenarios & Testing

### User Story 1 — Renderer Bootstrap (Priority: P1)

A developer launches `LXShell.exe`. The application initializes the DirectX 11 device and swap chain against the existing Win32 window. Each frame, the back buffer is cleared to a visible color (e.g., cornflower blue) and presented. The developer observes GPU-driven rendering replacing the default window background. Closing the window triggers orderly renderer shutdown, releasing all DX11 resources before process exit.

**Why this priority**: This is the sole user story. Without a functioning render bootstrap, no subsequent rendering work can proceed.

**Independent Test**: Launch `LXShell.exe`, verify the window client area displays a solid clear color driven by DirectX 11 (not the default Win32 `BLACK_BRUSH`), verify the application exits cleanly after closing the window.

**Acceptance Scenarios**:

1. **Given** the application is launched, **When** renderer initialization succeeds, **Then** the window client area displays a solid clear color each frame
2. **Given** the application is running with an active renderer, **When** the user resizes the window, **Then** the clear color continues rendering correctly at the new size without corruption or crash
3. **Given** the application is running with an active renderer, **When** the user closes the window, **Then** all DX11 resources are released and the process exits cleanly
4. **Given** the system does not support DirectX 11, **When** the application attempts renderer initialization, **Then** initialization fails with observable diagnostic output and the process exits without crashing

### Edge Cases

- What happens if `D3D11CreateDeviceAndSwapChain` fails (unsupported hardware, missing runtime)?
- What happens if back buffer acquisition fails after swap chain creation?
- What happens if the window is resized to zero-area (minimized)?
- What happens if present fails mid-session?
- What happens if the window is closed during renderer initialization?

## Requirements

### Functional Requirements

- **FR-001**: The engine SHALL initialize a DirectX 11 device for the existing Win32 application window
- **FR-002**: The engine SHALL initialize an associated DirectX 11 device context
- **FR-003**: The engine SHALL create a DXGI swap chain bound to the application window handle
- **FR-004**: The engine SHALL acquire the swap chain back buffer
- **FR-005**: The engine SHALL create a render target view for the active back buffer
- **FR-006**: The runtime SHALL clear the active render target once per frame to a visible solid color
- **FR-007**: The runtime SHALL present the frame once per frame via swap chain present
- **FR-008**: The runtime SHALL handle window resize events sufficiently to recreate render-target-dependent state (back buffer, render target view) without process instability
- **FR-009**: DX11 initialization failure SHALL produce controlled failure with observable diagnostics (logged error messages identifying the failure point)
- **FR-010**: DX11 shutdown SHALL release all bootstrap-owned rendering resources (render target view, back buffer reference, swap chain, device context, device) in correct dependency order
- **FR-011**: Render bootstrap ownership SHALL remain within engine/runtime code (`LXEngine`) and SHALL NOT thicken `LXShell`
- **FR-012**: The renderer bootstrap SHALL integrate into the existing `Application` lifecycle (Initialize/Run/Shutdown) rather than bypassing it

### Non-Functional Requirements

- **NFR-001**: The renderer bootstrap SHALL have a maintainable structure with clear separation between DX11 initialization, per-frame operations, resize handling, and shutdown
- **NFR-002**: Ownership of all DX11 COM objects SHALL be explicit (no leaked references, no ambiguous shared ownership)
- **NFR-003**: The renderer bootstrap SHALL be debuggable — DX11 debug device/layer SHALL be enabled in Debug builds for diagnostic visibility
- **NFR-004**: Renderer startup behavior SHALL be deterministic — given the same system state, initialization either succeeds or fails consistently with the same diagnostics
- **NFR-005**: Renderer shutdown behavior SHALL be deterministic — all resources released in reverse-creation order with no crash or hang
- **NFR-006**: Code changes SHALL be reviewable as incremental additions to the existing Spec 001 codebase
- **NFR-007**: The renderer bootstrap SHALL be compatible with later renderer expansion (shader systems, pipeline state, resource management) without requiring fundamental redesign of the bootstrap path
- **NFR-008**: No speculative renderer abstraction SHALL be introduced beyond the current bootstrap need

### Key Entities

- **DX11 Device**: The ID3D11Device representing the GPU interface
- **Device Context**: The ID3D11DeviceContext for issuing rendering commands
- **Swap Chain**: The IDXGISwapChain presenting rendered frames to the window
- **Back Buffer**: The ID3D11Texture2D obtained from the swap chain
- **Render Target View**: The ID3D11RenderTargetView bound for frame clearing

## Constraints

- Windows-only platform
- Raw Win32 window integration (existing HWND from Spec 001)
- DirectX 11 only (no DX12, Vulkan, or OpenGL)
- Existing shell/bootstrap architecture (Spec 001) must remain intact
- Dependency direction: `LXShell → LXEngine → LXCore`
- `Application` class remains the central lifecycle owner
- Single-threaded runtime for this stage
- Render bootstrap must not assume gameplay or asset systems exist
- All DX11 types use `LongXi` namespace where applicable

## Deliverables

- DX11 renderer bootstrap integrated into the existing application lifecycle
- Device, device context, and swap chain initialization path
- Render target view setup from back buffer
- Per-frame clear and present path
- Resize-safe render target recreation path
- Controlled renderer shutdown and resource release path
- Renderer startup, failure, and shutdown logging visibility

## Success Criteria

### Measurable Outcomes

- **SC-001**: Solution builds successfully in all three configurations (Debug, Release, Dist) with DX11 bootstrap integrated
- **SC-002**: Application launches successfully with renderer initialization observable in log output
- **SC-003**: A visible clear color is rendered to the window client area each frame (not the default Win32 background)
- **SC-004**: Present occurs each frame without crashing or producing visual artifacts
- **SC-005**: Window resize does not corrupt the process, crash, or leave invalid render target state — clear color renders correctly at the new size
- **SC-006**: Renderer startup and shutdown events are observable in structured log output
- **SC-007**: Lifecycle ownership remains centered in `Application` and engine runtime code — `LXShell` main.cpp does not grow beyond its current `CreateApplication()` function
- **SC-008**: All DX11 COM objects are released during shutdown with no outstanding references reported by the debug layer
- **SC-009**: Renderer initialization failure produces observable error diagnostics and a controlled process exit (not a crash)

## Acceptance Criteria

- **AC-001**: `LXShell.exe` builds without errors in Debug, Release, and Dist configurations with DX11 bootstrap code included
- **AC-002**: Launching `LXShell.exe` produces a visible window with a solid GPU-cleared color (not the default Win32 black brush background)
- **AC-003**: Debug console shows renderer initialization log messages (device created, swap chain created, render target bound)
- **AC-004**: Closing the application after renderer initialization exits cleanly without resource-lifecycle instability (no crash, no hang, no debug-layer warnings about leaked objects)
- **AC-005**: Resizing the window preserves a valid clear/present path — the clear color renders correctly at the new dimensions after render target recreation
- **AC-006**: Minimizing the window does not crash or corrupt renderer state — rendering resumes correctly when the window is restored
- **AC-007**: If DX11 initialization fails (e.g., missing DX11 runtime), the application produces observable error diagnostics and exits without crashing
- **AC-008**: `LXShell/Src/main.cpp` remains unchanged from Spec 001 (still only `CreateApplication()`)
- **AC-009**: DX11 debug layer is active in Debug builds and reports no warnings or errors during normal clear/present operation

## Risks

| Risk                                                                      | Likelihood | Impact | Mitigation                                                                                               |
| ------------------------------------------------------------------------- | ---------- | ------ | -------------------------------------------------------------------------------------------------------- |
| Device/swap chain init coupled too tightly to window bootstrap code       | Medium     | High   | Renderer bootstrap in a separate class within `LXEngine`, not embedded in `Application` or `Win32Window` |
| Renderer details leaking into `LXShell`                                   | Low        | High   | Enforce that `LXShell` only implements `CreateApplication()` — renderer is engine-internal               |
| Unclear ownership of COM object lifetime                                  | Medium     | High   | Explicit `ComPtr` or manual `Release()` in reverse-creation order with clear ownership comments          |
| Incorrect resize handling leaving invalid back-buffer/render-target state | High       | High   | Release render target view and back buffer reference before `ResizeBuffers()`, re-acquire after          |
| Premature abstraction around the renderer bootstrap                       | Medium     | Medium | Keep the bootstrap class concrete and DX11-specific — no renderer interface or backend abstraction       |
| DX11 debug layer masking release-build behavior                           | Low        | Low    | Test in both Debug and Release configurations                                                            |

## Assumptions

- Spec 001 shell bootstrap is complete and stable (window creation, message pump, lifecycle management, clean shutdown all verified)
- The existing `Win32Window` provides a valid `HWND` accessible from `Application` during initialization
- Later specs will add shader, resource, and rendering capabilities incrementally on top of this bootstrap
- The current goal is renderer foundation only, not content rendering
- The system running the application has a DirectX 11–capable GPU and runtime installed (failure is handled but not the expected path)
- `d3d11.lib` and `dxgi.lib` are available through the Windows SDK already configured in the build environment

## Resolved Questions

- **OQ-001**: DX11 debug device/layer (`D3D11_CREATE_DEVICE_DEBUG`) SHALL be enabled in development builds only (Debug configuration). Release and Dist builds use standard device creation without the debug flag. _Resolved: User confirmed._
- **OQ-002**: Initial present mode SHALL use VSync (`SyncInterval = 1`) as the simple default present path. _Resolved: User confirmed._
- **OQ-003**: Depth/stencil buffer creation is explicitly deferred to a later spec. This bootstrap requires only clear/present on a color render target. _Resolved: User confirmed._

## Boundary Note

This specification intentionally establishes only the minimal DirectX 11 render bootstrap boundary. Content rendering, shader systems, depth/stencil buffers, resource-backed drawing, and all higher-level rendering capabilities are expected to be handled by later separate specifications. This spec is deliberately narrow to ensure a stable, reviewable device/swap chain/render target foundation before adding complexity.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
