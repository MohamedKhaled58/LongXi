# Feature Specification: Spec 014 - Renderer API and GPU State Contract

**Feature Branch**: `014-renderer-gpu-contract`  
**Created**: 2026-03-11  
**Status**: Draft  
**Input**: User description: "Spec 014 — Renderer API and GPU State Contract"

## Clarifications

### Session 2026-03-11

- Q: How should the renderer handle resize events that arrive during an active frame? → A: Queue the resize request and apply it at the next `BeginFrame`.
- Q: How should contract violations be handled in development vs non-development builds? → A: Development builds must assert and log; non-development builds must log and skip the invalid operation while continuing execution.
- Q: Should the public renderer contract be backend-agnostic or DX11-specific? → A: Public renderer API is backend-agnostic; DX11 remains internal to `DX11Renderer`, and engine modules must not see DirectX types.
- Q: How should external systems (for example Debug UI) render without breaking backend boundaries? → A: Renderer must provide a backend-agnostic external-pass bridge; backend API calls remain internal to renderer.
- Q: How should renderer handle present/device-loss or swapchain failure events? → A: Use a recoverable policy: log the failure, enter safe non-rendering mode, and attempt controlled renderer reinitialization.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Deterministic Frame Rendering (Priority: P1)

As an engine developer, I need a strict renderer frame contract so world rendering works consistently regardless of what other runtime systems draw before or after it.

**Why this priority**: This removes the current class of failures where engine rendering disappears while other systems still render, which blocks reliable development.

**Independent Test**: Can be fully tested by running a frame sequence with Scene, Sprite, and Debug UI draws and verifying the engine-rendered content remains visible and correct across frames.

**Acceptance Scenarios**:

1. **Given** the renderer is initialized, **When** a new frame begins, **Then** the renderer must establish all required baseline GPU state before any subsystem draw call.
2. **Given** an external subsystem renders through the renderer-owned external pass bridge, **When** its pass executes, **Then** engine boundaries remain backend-agnostic and the next engine frame renders correctly without depending on prior frame state.
3. **Given** a subsystem calls draw operations outside a valid frame lifecycle, **When** the call is received, **Then** the renderer must reject the operation and emit a contract violation diagnostic.

---

### User Story 2 - Reliable Resize and Viewport State (Priority: P2)

As an engine developer, I need resize handling to reliably rebuild render surface state so rendering remains correct after window size changes.

**Why this priority**: Resize and viewport bugs are high-frequency runtime issues that directly impact render correctness and validation workflows.

**Independent Test**: Can be tested by repeatedly resizing, minimizing, and restoring the window while validating scene and sprite output remains visible and correctly scaled.

**Acceptance Scenarios**:

1. **Given** a window resize event, **When** the renderer processes the resize, **Then** render targets, depth resources, and viewport must be recreated or updated to match the new surface size.
2. **Given** the window is minimized to zero render size, **When** resize is processed, **Then** the renderer must avoid invalid resource creation and recover cleanly when size becomes valid again.

---

### User Story 3 - Enforced Renderer Ownership Boundary (Priority: P3)

As a technical lead, I need an explicit renderer API boundary so engine modules cannot bypass renderer ownership of GPU pipeline state.

**Why this priority**: This prevents architectural drift and recurring hidden-state defects as more systems are added.

**Independent Test**: Can be tested by auditing module interfaces and build dependencies to confirm all GPU interactions flow through renderer interfaces only.

**Acceptance Scenarios**:

1. **Given** engine subsystems submit rendering work, **When** integration is reviewed, **Then** all GPU interactions must be routed through backend-agnostic renderer API calls with no backend-specific types exposed to engine modules.
2. **Given** direct graphics API usage appears outside the renderer module, **When** validation runs, **Then** it must be flagged as a specification violation.

---

### Edge Cases

- Frame submission requested before renderer initialization completes.
- Resize request arrives during active frame processing and is queued until the next frame start.
- Multiple resize events arrive in rapid sequence with changing dimensions.
- Render pass begins without a valid active frame.
- Subsystem attempts to continue rendering after renderer signals a contract violation; renderer asserts in development builds and skips the invalid operation in non-development builds.
- Debug UI or other external pass leaves pipeline in undefined state before next engine frame.
- Device-loss, present failure, or swapchain failure occurs during runtime rendering.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST define a formal renderer API that is the only allowed interface for GPU interaction by engine subsystems.
- **FR-002**: System MUST enforce a strict frame lifecycle with ordered stages: frame begin, pass execution, frame end, and presentation.
- **FR-003**: System MUST establish required baseline GPU state at each frame start, including active render target, depth target, viewport, and baseline pipeline state.
- **FR-004**: System MUST require each render pass to explicitly configure pass-required state and MUST forbid dependence on previous pass or previous frame state.
- **FR-005**: System MUST provide renderer-managed pass types for Scene, Sprite, and Debug UI rendering.
- **FR-006**: System MUST reject draw submissions that occur outside a valid frame and pass context.
- **FR-007**: System MUST own render target and depth resource lifecycle, including creation, binding, recreation, and disposal.
- **FR-008**: System MUST maintain an authoritative viewport definition and apply it whenever render surface dimensions or active render target changes.
- **FR-009**: System MUST process resize events through renderer resize handling that updates surface resources and viewport before the next valid frame.
- **FR-010**: System MUST handle zero-size render surfaces safely by deferring invalid operations until a valid render size is restored.
- **FR-011**: System MUST emit structured logs for initialization, frame lifecycle transitions, resize handling, viewport updates, and contract violations.
- **FR-012**: System MUST prevent direct graphics API calls from non-renderer engine modules as an architectural compliance requirement.
- **FR-013**: System MUST queue resize requests received during an active frame and apply the latest queued resize once at the next `BeginFrame` before any render pass executes.
- **FR-014**: System MUST handle contract violations with environment-specific behavior: development builds assert and log; non-development builds log and skip the invalid operation without terminating the application.
- **FR-015**: System MUST expose a backend-agnostic renderer public API for engine subsystems, while backend-specific implementations remain internal to renderer backend modules.
- **FR-016**: System MUST ensure renderer public API surface and shared renderer-facing types do not expose backend-specific objects or backend header dependencies.
- **FR-017**: System MUST enforce module boundaries so non-renderer modules in scope (including Scene, SpriteRenderer, TextureManager, and other engine systems) do not include DirectX headers or call DirectX interfaces directly.
- **FR-018**: System MUST provide a backend-agnostic renderer-owned external pass integration bridge so tools such as Debug UI can render through renderer-controlled lifecycle and state boundaries without accessing backend-native handles.
- **FR-019**: System MUST implement a recoverable failure policy for present/device-loss/swapchain failures: log failure details, transition to a safe non-rendering state, and attempt controlled renderer reinitialization using defined recovery triggers.

### Key Entities *(include if feature involves data)*

- **FrameLifecycleState**: Tracks renderer frame status (not started, in frame, in pass, frame ended) and validates legal operation order.
- **RenderPassType**: Enumerates supported pass categories (Scene, Sprite, Debug UI) with required state expectations.
- **GpuBaselineStateProfile**: Defines the mandatory baseline state the renderer guarantees at frame start.
- **RenderSurfaceState**: Represents active render surface dimensions and bindings, including render target, depth target, and viewport.
- **ContractViolationEvent**: Captures invalid API usage with operation, lifecycle context, and severity for diagnostics.
- **RendererHandleTypes**: Backend-agnostic renderer-facing handle/value types (for example texture, buffer, and viewport descriptors) used by engine subsystems without exposing backend-native objects.
- **ExternalPassBridge**: Renderer-owned interface used by external rendering tools to execute pass submission without direct access to backend-native device/context handles.
- **RendererRecoveryState**: Tracks recoverable failure mode, recovery eligibility, and reinitialization attempt outcomes after present/device-loss/swapchain failures.

### Assumptions

- This specification targets a single active render surface per frame.
- Pass order for Scene, Sprite, and Debug UI is fixed for this specification and may be extended by later specs.
- Renderer remains backend-specific internally, while external subsystem integration remains backend-agnostic through renderer interfaces.
- All runtime rendering for this feature executes on the main thread.

### Dependencies

- Existing Engine update/render loop integration points.
- Existing Scene, SpriteRenderer, and DebugUI subsystems consuming renderer services.
- Existing application resize event flow from window to engine.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a 10,000-frame validation run that includes Scene, Sprite, and Debug UI rendering, 100% of frames begin with a valid baseline renderer state and complete without contract-order failures.
- **SC-002**: During a stress sequence of 100 resize/minimize/restore operations, rendering resumes correctly with no persistent blank or mis-scaled output after valid size restoration.
- **SC-003**: 100% of invalid lifecycle API calls are detected and logged as contract violations during negative test scenarios.
- **SC-004**: Architecture compliance review finds zero direct GPU API calls outside the renderer module for subsystems in scope.
- **SC-005**: Renderer diagnostic logs include lifecycle, resize, and viewport events in every run where those events occur.
- **SC-006**: In injected failure tests for present/device-loss/swapchain errors, 100% of events are logged and handled via safe non-rendering transition without undefined rendering behavior.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
