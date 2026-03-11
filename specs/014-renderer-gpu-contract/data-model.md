# Data Model: Spec 014 - Renderer API and Backend Architecture

## Entity: FrameLifecycleState

### Purpose
Represents legal renderer lifecycle progression and enforces API-order validity.

### Fields
- `Phase`: `NotStarted | InFrame | InPass | FrameEnded`
- `ActivePass`: `None | Scene | Sprite | DebugUI | External`
- `FrameIndex`: monotonic frame counter
- `HasPendingPresent`: boolean

### Validation Rules
- `BeginFrame` valid only from `NotStarted` or `FrameEnded`.
- `BeginPass` valid only from `InFrame` with no active pass.
- Draw submission valid only during `InPass`.
- `EndPass` valid only during `InPass`.
- `EndFrame` valid only during `InFrame` with no active pass.
- `Present` valid only from `FrameEnded`.

### State Transitions
- `NotStarted -> InFrame` on `BeginFrame`
- `InFrame -> InPass` on `BeginPass`
- `InPass -> InFrame` on `EndPass`
- `InFrame -> FrameEnded` on `EndFrame`
- `FrameEnded -> NotStarted` after successful present completion

## Entity: RenderPassType

### Purpose
Defines renderer-managed pass categories and pass sequencing intent.

### Values
- `Scene`
- `Sprite`
- `DebugUI`
- `External`

### Validation Rules
- Exactly one pass active at a time.
- Default pass order: `Scene -> Sprite -> DebugUI`.
- External pass executes only via renderer-owned bridge.

## Entity: GpuBaselineStateProfile

### Purpose
Defines mandatory baseline GPU state guaranteed after `BeginFrame`.

### Fields
- `RenderTargetBound`: boolean
- `DepthStencilBound`: boolean
- `ViewportBound`: boolean
- `RasterizerStateBound`: boolean
- `BlendStateBound`: boolean
- `DepthStateBound`: boolean

### Validation Rules
- All fields must be true before any pass draw submission.
- Failure to establish baseline state enters recovery handling path.

## Entity: RenderSurfaceState

### Purpose
Tracks active render surface geometry/resources and resize-deferred updates.

### Fields
- `Width`: integer
- `Height`: integer
- `Viewport`: renderer-owned viewport descriptor
- `RenderTargetStatus`: `Valid | Invalid | Recreating`
- `DepthTargetStatus`: `Valid | Invalid | Recreating`
- `PendingResize`: boolean
- `PendingWidth`: integer
- `PendingHeight`: integer

### Validation Rules
- Zero-size dimensions do not trigger invalid resource creation.
- Mid-frame resize updates pending fields only.
- Pending resize applies at next safe frame boundary.

## Entity: RendererHandleTypes

### Purpose
Represents backend-agnostic renderer-facing handles and values used by engine systems.

### Fields
- `TextureHandle`
- `BufferHandle`
- `ShaderHandle`
- `RenderTargetHandle`
- `DepthTargetHandle`
- `Viewport`
- `Color`

### Validation Rules
- No backend-native payload or types in public handle/value definitions.
- Handle validity is owned by renderer implementation.

## Entity: ContractViolationEvent

### Purpose
Captures invalid renderer API usage for diagnostics and policy handling.

### Fields
- `Operation`
- `ExpectedState`
- `ActualState`
- `Severity`
- `BuildBehavior`: `AssertAndLog | LogAndSkip`
- `Timestamp`

### Validation Rules
- Every rejected operation emits a violation event.

## Entity: ExternalPassBridge

### Purpose
Renderer-owned integration model for external rendering tools (for example DebugUI).

### Fields
- `BridgeId`
- `PassLabel`
- `AllowedStage`
- `CallbackRegistrationState`

### Validation Rules
- External pass executes only within renderer-managed lifecycle.
- External bridge does not expose backend-native handles.

## Entity: RendererRecoveryState

### Purpose
Tracks recovery behavior for present/device-loss/swapchain failure events.

### Fields
- `Mode`: `Normal | RecoveryPending | SafeNoRender | Reinitializing`
- `FailureType`: `PresentFailure | DeviceLoss | SwapchainFailure`
- `LastFailureTimestamp`
- `ReinitAttemptCount`
- `LastReinitResult`: `Success | Failure | NotAttempted`

### Validation Rules
- Runtime failure transitions to safe recovery flow.
- Recovery attempts are bounded and logged.

## Relationships
- `FrameLifecycleState` governs legal usage of `RenderPassType`.
- `GpuBaselineStateProfile` must be satisfied on transition into `InFrame`.
- `RenderSurfaceState` drives viewport and target rebinding.
- `RendererHandleTypes` are consumed by engine systems through renderer API only.
- `ExternalPassBridge` operates within frame lifecycle/pass constraints.
- `ContractViolationEvent` may be emitted from any invalid lifecycle/boundary interaction.
- `RendererRecoveryState` can override normal frame progression until recovery succeeds.

## Implementation Alignment (2026-03-11)
- `RendererTextureHandle`, `RendererBufferHandle`, and `RendererShaderHandle` are represented as opaque `std::shared_ptr<void>` handles.
- Backend-native payload is created/released only in DX11 backend implementation code.
- Engine-facing code consumes these handles without including DirectX headers/types.
