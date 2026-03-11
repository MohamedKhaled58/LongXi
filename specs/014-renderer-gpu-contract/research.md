# Research: Spec 014 - Renderer API and GPU State Contract

## Decision 1: Public renderer contract is backend-agnostic
- **Decision**: Expose renderer operations and renderer-owned value/handle types only; keep DirectX 11 objects and headers internal to DX11 backend implementation.
- **Rationale**: Prevents backend leakage, reduces coupling, and keeps engine-side contracts stable.
- **Alternatives considered**:
  - Expose read-only DX11 handles to engine modules (rejected: leaks backend and breaks boundary rule).
  - Keep fully DX11-specific public API (rejected: locks architecture and increases churn risk).

## Decision 2: Enforce renderer lifecycle through explicit state machine
- **Decision**: Implement `FrameLifecycleState` as legal-operation gate (`NotStarted -> InFrame -> InPass -> InFrame -> Ended`).
- **Rationale**: Makes invalid operation order detectable and testable instead of implicit.
- **Alternatives considered**:
  - Trust call-site ordering only (rejected: high regression risk).
  - Soft advisory ordering without enforcement (rejected: insufficient for contract correctness).

## Decision 3: Rebind baseline GPU state at every `BeginFrame`
- **Decision**: `BeginFrame` always rebinds render target, depth-stencil target, viewport, rasterizer state, blend state, and depth-stencil state.
- **Rationale**: Eliminates reliance on previous frame/pass state and neutralizes external state mutation side effects.
- **Alternatives considered**:
  - Rebind only on detected changes (rejected: can miss external mutations).
  - Rebind only after external passes (rejected: incomplete and fragile).

## Decision 4: Queue resize requests during active frame
- **Decision**: If resize arrives mid-frame, store latest dimensions and apply once at next `BeginFrame`.
- **Rationale**: Avoids resource invalidation while draw work is in progress and preserves deterministic frame boundaries.
- **Alternatives considered**:
  - Resize immediately mid-frame (rejected: invalidates active state/resources).
  - Drop resize events during active frame (rejected: stale surface behavior).

## Decision 5: Provide renderer-owned external pass bridge
- **Decision**: External renderers (e.g., Debug UI) submit through a renderer-controlled bridge without backend-native handle access.
- **Rationale**: Preserves boundary discipline while enabling tool rendering in frame lifecycle.
- **Alternatives considered**:
  - Give external systems DX11 device/context (rejected: boundary violation).
  - Let external systems own independent present path (rejected: lifecycle fragmentation).

## Decision 6: Contract violation behavior is environment-specific
- **Decision**: Development builds assert + log; non-development builds log + skip invalid operation and continue safely.
- **Rationale**: Maximizes diagnosis in development and avoids hard crashes in non-development validation runs.
- **Alternatives considered**:
  - Always terminate on violation (rejected: overly disruptive).
  - Silent ignore (rejected: hides defects).

## Decision 7: Use recoverable failure policy for present/device-loss/swapchain errors
- **Decision**: On failure, log details, enter safe non-rendering mode, and attempt controlled reinitialization on valid recovery trigger.
- **Rationale**: Prevents undefined rendering behavior and provides deterministic recovery path.
- **Alternatives considered**:
  - Fatal terminate on first error (rejected: poor runtime resilience).
  - Continue normal rendering after failure (rejected: undefined behavior risk).

## Decision 8: Standardize renderer observability events
- **Decision**: Log initialization, lifecycle transitions, pass boundaries, resize operations, recovery transitions, and contract violations.
- **Rationale**: Supports reproducibility and root-cause analysis for GPU-state regressions.
- **Alternatives considered**:
  - Minimal success/failure logging only (rejected: low diagnostic value).
  - Verbose per-draw logging in all builds (rejected: noisy and costly).

## Validation Sweep Summary (2026-03-11)
- Build validation: PASS (Debug x64 build succeeded for `LXCore`, `LXEngine`, `LXShell`).
- Runtime initialization/lifecycle smoke: PASS (renderer, scene, camera, sprite renderer, and DebugUI all active with no lifecycle-order errors in logs).
- Resize validation: PASS (manual resize testing confirmed stable behavior and rendering recovery).
- Boundary audit: PASS (`Scripts/Audit-RendererBoundaries.ps1 -Json` returned `ok=true` with no violations).
- Logging quality fix applied: per-frame sprite flush info spam removed from `SpriteRenderer::FlushBatch`.

## Notes
- Reference implementation inspection is constrained to behavior/constraints only.
- Final design remains aligned with LongXi architecture and repository rules.
