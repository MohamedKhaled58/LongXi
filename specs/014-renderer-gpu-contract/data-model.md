# Data Model: Spec 014 - Renderer API and GPU State Contract

## Entity: FrameLifecycleState

### Purpose
Tracks legal renderer lifecycle transitions and validates API call ordering.

### Fields
- `Phase`: NotStarted | InFrame | InPass | FrameEnded
- `ActivePass`: None | Scene | Sprite | DebugUI | External
- `FrameIndex`: Monotonic frame counter
- `HasPendingPresent`: Boolean

### Validation Rules
- `BeginFrame` valid only when `Phase` is `NotStarted` or `FrameEnded`.
- `BeginPass` valid only when `Phase` is `InFrame` and `ActivePass` is `None`.
- Draw operations valid only when `Phase` is `InPass`.
- `EndPass` valid only when `Phase` is `InPass`.
- `EndFrame` valid only when `Phase` is `InFrame` and no active pass.
- `Present` valid only after `EndFrame`.

### State Transitions
- `NotStarted -> InFrame` on `BeginFrame`
- `InFrame -> InPass` on `BeginPass`
- `InPass -> InFrame` on `EndPass`
- `InFrame -> FrameEnded` on `EndFrame`
- `FrameEnded -> NotStarted` after successful `Present`/frame completion

## Entity: RenderPassType

### Purpose
Defines pass categories and state configuration intent.

### Values
- `Scene`
- `Sprite`
- `DebugUI`
- `External` (bridge-mediated tooling pass)

### Validation Rules
- Pass order in this spec: Scene -> Sprite -> DebugUI (External pass usage is controlled by renderer integration rules).
- Only one active pass at a time.

## Entity: GpuBaselineStateProfile

### Purpose
Represents mandatory GPU state that must be rebound at frame start.

### Fields
- `RenderTargetBound`: Boolean
- `DepthStencilBound`: Boolean
- `ViewportBound`: Boolean
- `RasterizerStateBound`: Boolean
- `BlendStateBound`: Boolean
- `DepthStateBound`: Boolean

### Validation Rules
- All fields must be true immediately after `BeginFrame` succeeds.
- If any field cannot be set, renderer enters recoverable failure path.

## Entity: RenderSurfaceState

### Purpose
Represents active render-surface geometry and dependent resources.

### Fields
- `Width`: Positive integer (0 allowed only for minimized/deferred state)
- `Height`: Positive integer (0 allowed only for minimized/deferred state)
- `Viewport`: Renderer-owned viewport descriptor
- `RenderTargetStatus`: Valid | Invalid | Recreating
- `DepthTargetStatus`: Valid | Invalid | Recreating
- `PendingResize`: Boolean
- `PendingWidth`: Integer
- `PendingHeight`: Integer

### Validation Rules
- `Width` and `Height` > 0 required before creating size-dependent resources.
- Mid-frame resize sets pending values only; apply at next `BeginFrame`.

## Entity: ContractViolationEvent

### Purpose
Captures invalid renderer API usage for diagnostics and policy handling.

### Fields
- `Operation`: API call identifier
- `ExpectedPhase`: Lifecycle phase required
- `ActualPhase`: Lifecycle phase observed
- `Severity`: Warning | Error | FatalCandidate
- `BuildBehavior`: AssertAndLog | LogAndSkip
- `Timestamp`: Event time

### Validation Rules
- Every rejected operation must emit one violation event.

## Entity: RendererHandleTypes

### Purpose
Defines backend-agnostic value/handle types consumed by engine modules.

### Fields
- `TextureHandle`
- `BufferHandle`
- `ViewportDesc`
- `RenderTargetHandle`
- `DepthTargetHandle`

### Validation Rules
- No backend-native object types may appear in these definitions.

## Entity: ExternalPassBridge

### Purpose
Renderer-owned bridge contract enabling tool rendering without backend leakage.

### Fields
- `BridgeId`
- `PassLabel`
- `LifecycleBinding`: Begin/Render/End hooks
- `AllowedStage`: Pass stage constraints

### Validation Rules
- Bridge can execute only inside active frame and within renderer-controlled pass boundaries.
- Bridge cannot expose backend-native handles.

## Entity: RendererRecoveryState

### Purpose
Tracks recoverable rendering failures and controlled reinitialization flow.

### Fields
- `Mode`: Normal | RecoveryPending | SafeNoRender | Reinitializing
- `FailureType`: PresentFailure | DeviceLoss | SwapchainFailure
- `LastFailureTimestamp`
- `ReinitAttemptCount`
- `LastReinitResult`: Success | Failure | NotAttempted

### Validation Rules
- On failure event, state transitions to `RecoveryPending` then `SafeNoRender` until recovery trigger.
- Reinitialization attempts must be logged and bounded by policy.

## Relationships
- `FrameLifecycleState` governs legal usage of `RenderPassType`.
- `GpuBaselineStateProfile` must be satisfied for each `FrameLifecycleState` transition into `InFrame`.
- `RenderSurfaceState` drives viewport and resource rebinding in baseline state.
- `ExternalPassBridge` executes within `FrameLifecycleState` and `RenderPassType` constraints.
- `ContractViolationEvent` may be emitted from any invalid lifecycle or boundary interaction.
- `RendererRecoveryState` can override normal frame flow and force safe no-render mode.


## Implementation Alignment (Current)
- `FrameLifecycleState` is implemented with explicit phase checks at `BeginFrame`, `BeginPass`, `EndPass`, `EndFrame`, and `Present`.
- `RenderSurfaceState` applies a queued-resize model: active-frame resize requests are deferred to next safe frame boundary.
- `RendererRecoveryState` enters `SafeNoRender` on present/device/swapchain failure and retries through controlled recovery path.
- `ExternalPassBridge` is implemented as renderer-owned callback execution under `ExecuteExternalPass`.

- `RendererHandleTypes` are consumed by engine systems without direct `DX11Renderer` dependency in `SceneNode`, `Scene`, and `SpriteRenderer` public interfaces.
- `ExternalPassBridge` integration in shell uses engine bridge call (`ExecuteExternalRenderPass`) and avoids direct renderer backend class coupling.


