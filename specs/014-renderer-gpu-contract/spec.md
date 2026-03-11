# Feature Specification: Spec 014 - Renderer API and Backend Architecture

**Feature Branch**: `014-renderer-gpu-contract`  
**Created**: 2026-03-11  
**Status**: Ready for planning  
**Input**: User description: "Spec 014 - Renderer API and Backend Architecture"

## Clarifications

### Session 2026-03-11

- Q: How should the renderer handle resize events that arrive during an active frame? -> A: Queue the resize request and apply it at the next `BeginFrame`.
- Q: How should contract violations be handled in development vs non-development builds? -> A: Development builds assert and log; non-development builds log and skip the invalid operation while continuing execution.
- Q: Should the public renderer contract be backend-agnostic or DX11-specific? -> A: Public renderer API is backend-agnostic; DX11 remains internal to backend implementation modules.
- Q: How should external systems (for example Debug UI) render without breaking backend boundaries? -> A: Renderer provides a renderer-owned external-pass bridge and keeps backend API calls internal.
- Q: How should renderer handle present/device-loss or swapchain failure events? -> A: Use a recoverable policy: log failure, enter safe non-rendering mode, and attempt controlled reinitialization.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Deterministic Frame Rendering (Priority: P1)

As an engine developer, I need a strict renderer lifecycle and GPU-state contract so engine rendering remains stable regardless of subsystem or external pass behavior.

**Why this priority**: This addresses the highest-risk failure class: engine geometry failing while external systems still render.

**Independent Test**: Run Scene + Sprite + Debug UI rendering for 10,000 frames and verify no blank-frame regressions or lifecycle-order violations.

**Acceptance Scenarios**:

1. **Given** a new frame starts, **When** `BeginFrame` executes, **Then** required baseline GPU state is explicitly set before any draw submission.
2. **Given** external rendering executes through the renderer bridge, **When** control returns to engine rendering, **Then** engine rendering remains correct without relying on previous pass state.
3. **Given** a subsystem submits draw calls outside valid lifecycle order, **When** renderer receives the call, **Then** renderer rejects it and emits a contract-violation diagnostic.

---

### User Story 2 - Reliable Resize and Viewport State (Priority: P2)

As an engine developer, I need renderer-owned resize and viewport management so rendering remains correct after resize/minimize/restore sequences.

**Why this priority**: Resize-related state invalidation is frequent and directly affects runtime correctness.

**Independent Test**: Execute 100 resize/minimize/restore iterations and verify rendering recovers correctly with valid viewport/surface state.

**Acceptance Scenarios**:

1. **Given** a resize event, **When** renderer handles `OnResize`, **Then** swapchain, render targets, depth resources, and viewport are recreated/updated in a valid order.
2. **Given** a zero-size minimized state, **When** resize handling runs, **Then** invalid resource creation is skipped and renderer recovers once dimensions are valid.
3. **Given** a resize arrives during active frame processing, **When** renderer is in-frame, **Then** resize is queued and applied at next `BeginFrame`.

---

### User Story 3 - Enforced Backend-Agnostic Engine Boundary (Priority: P3)

As a technical lead, I need strict renderer abstraction boundaries so engine systems cannot depend on DirectX headers, types, or backend internals.

**Why this priority**: This prevents architectural drift and enables future multi-backend support.

**Independent Test**: Run boundary audit and interface review to verify no DirectX leakage in engine modules and public engine headers.

**Acceptance Scenarios**:

1. **Given** engine subsystems integrate rendering, **When** headers are reviewed, **Then** only `Renderer.h` and `RendererTypes.h` are used for renderer interaction.
2. **Given** public engine headers are reviewed, **When** type declarations are checked, **Then** no forbidden DirectX types appear.
3. **Given** backend implementation files are reviewed, **When** GPU API usage is checked, **Then** DirectX calls are confined to backend implementation.

---

### Edge Cases

- Frame operations called before renderer initialization.
- Multiple resize events in one frame; latest valid dimensions must win.
- External render pass modifies pipeline state unexpectedly.
- Subsystem attempts to rely on prior-frame viewport/render-target state.
- Present/device-loss/swapchain failure occurs mid-runtime.
- Zero-size window state persists for extended periods.

## Requirements *(mandatory)*

### Architectural Constraints

- **ARC-001**: Engine systems MUST NOT include `d3d11.h` or `dxgi.h`.
- **ARC-002**: Engine systems MUST interact with renderer only through public renderer headers: `Renderer.h` and `RendererTypes.h`.
- **ARC-003**: DirectX types MUST NOT appear in public engine headers, including: `ID3D11Device`, `ID3D11DeviceContext`, `D3D11_VIEWPORT`, `DXGI_SWAP_CHAIN_DESC`, `DXGI_FORMAT`.
- **ARC-004**: All GPU interaction MUST occur inside renderer backend implementation files.
- **ARC-005**: Renderer API MUST be backend-agnostic and MUST NOT expose backend-native handles or backend header dependencies.
- **ARC-006**: Renderer MUST be sole owner of GPU pipeline state.
- **ARC-007**: Engine subsystems MUST assume no implicit GPU state persistence across passes or frames.
- **ARC-008**: Backend structure MUST isolate DirectX implementation under backend implementation modules.
- **ARC-009**: Architecture MUST preserve dependency flow: `Engine Systems -> Renderer API -> DX11 Backend -> DirectX 11`.

### Functional Requirements

- **FR-001**: System MUST provide backend-agnostic renderer operations including `BeginFrame`, `EndFrame`, `Clear`, `SetViewport`, `SetRenderTarget`, `DrawIndexed`, and `Present`.
- **FR-002**: System MUST enforce strict frame lifecycle ordering: `BeginFrame -> pass execution -> EndFrame -> Present`.
- **FR-003**: System MUST ensure `BeginFrame` establishes valid baseline GPU state: render target, depth stencil, viewport, rasterizer state, blend state, and depth state.
- **FR-004**: System MUST define renderer-managed pass categories for Scene, Sprite, and Debug UI.
- **FR-005**: System MUST reject draw submissions outside valid frame/pass context.
- **FR-006**: System MUST reset required GPU state every frame to prevent state-leakage bugs.
- **FR-007**: System MUST own render-target/depth-resource lifecycle (create, bind, recreate, dispose).
- **FR-008**: System MUST maintain and apply authoritative viewport state when render surface changes.
- **FR-009**: System MUST route resize handling through `Win32Window -> Application -> Engine.OnResize -> Renderer.OnResize`.
- **FR-010**: System MUST queue resize requests received during active frame and apply at next safe frame boundary.
- **FR-011**: System MUST handle zero-size surface safely and recover when valid size is restored.
- **FR-012**: System MUST keep DX11 backend encapsulated behind renderer API layer.
- **FR-013**: System MUST define backend-agnostic renderer handle/value types in `RendererTypes.h`.
- **FR-014**: System MUST keep backend-native data out of renderer public type definitions.
- **FR-015**: System MUST log renderer lifecycle events: device initialization, swapchain creation, viewport updates, and swapchain resize.
- **FR-016**: System MUST provide diagnosable contract-violation logs for invalid lifecycle/state usage.
- **FR-017**: System MUST support recoverable handling for present/device-loss/swapchain failure events.
- **FR-018**: System MUST allow external rendering integration only through renderer-owned boundaries without backend leakage.

### Key Entities *(include if feature involves data)*

- **RendererAPI**: Backend-agnostic operations consumed by engine systems.
- **RendererTypes**: Backend-agnostic handle/value types (`TextureHandle`, `BufferHandle`, `ShaderHandle`, `Viewport`, `Color`).
- **FrameLifecycleState**: Tracks renderer phase validity and allowed operations.
- **RenderPassType**: Enumerates pass categories and pass-state intent.
- **GpuBaselineStateProfile**: Defines required baseline GPU state after `BeginFrame`.
- **RenderSurfaceState**: Owns surface dimensions, render targets, depth targets, and viewport.
- **ContractViolationEvent**: Captures operation, expected state, actual state, and severity.
- **RendererRecoveryState**: Tracks safe-mode/recovery transitions after backend failures.

### Assumptions

- Single active render surface per frame for this spec.
- Initial backend implementation is DirectX 11, with future backend expansion expected.
- Main-thread renderer execution model for current scope.

### Dependencies

- Existing Engine main loop and resize event flow.
- Existing Scene/Sprite/Debug UI runtime integration points.
- Existing logging subsystem.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 10,000-frame runtime validation with Scene, Sprite, and Debug UI active, 100% of frames render without lifecycle-order failures.
- **SC-002**: In 100 resize/minimize/restore operations, rendering recovers correctly after valid-size restoration with zero persistent blank-frame outcomes.
- **SC-003**: 100% of invalid lifecycle API calls in negative tests are detected and logged.
- **SC-004**: Boundary audit reports zero DirectX header/type leakage outside backend implementation modules.
- **SC-005**: Renderer logs lifecycle/swapchain/viewport events on every relevant event occurrence.
- **SC-006**: Injected present/device-loss/swapchain failures always transition to defined safe handling path without undefined rendering behavior.

## Reference Implementation Rule

- The agent may inspect reference implementations located in `D:\Yamen Development\Old-Reference\cqClient\Conquer`.
- Relevant files may include renderer initialization, viewport handling, device creation, and swapchain configuration.
- Reference code must be used only to understand behavior and constraints.
- Final design must follow LongXi engine architecture.
