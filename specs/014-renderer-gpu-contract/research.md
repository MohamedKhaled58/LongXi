# Research: Spec 014 - Renderer API and Backend Architecture

## Decision 1: Public renderer API remains backend-agnostic
- **Decision**: Engine systems consume only backend-agnostic renderer contracts (`Renderer.h`, `RendererTypes.h`); backend-native types remain internal.
- **Rationale**: Prevents DirectX leakage, reduces coupling, and preserves future backend flexibility.
- **Alternatives considered**:
  - Expose `DX11Renderer` directly to engine modules (rejected: boundary violation and tight coupling).
  - Expose read-only DirectX device/context handles in public APIs (rejected: backend leakage risk).

## Decision 2: Renderer enforces explicit lifecycle state machine
- **Decision**: Enforce legal renderer call ordering via explicit lifecycle phases (`NotStarted`, `InFrame`, `InPass`, `FrameEnded`).
- **Rationale**: Eliminates implicit call-order assumptions and makes violations diagnosable.
- **Alternatives considered**:
  - Rely on caller discipline only (rejected: high regression risk).
  - Soft warnings without hard gating (rejected: insufficient protection).

## Decision 3: Baseline GPU state is rebound every frame
- **Decision**: `BeginFrame` must explicitly bind required baseline state (RT, DS, viewport, rasterizer, blend, depth).
- **Rationale**: Prevents state-leakage bugs from previous frame/pass/external renderer modifications.
- **Alternatives considered**:
  - Change-detection-only rebinding (rejected: external state mutation can bypass detection).
  - Rebind only after external passes (rejected: incomplete guarantee).

## Decision 4: Resize requests during active frame are queued
- **Decision**: If resize arrives while rendering, store latest dimensions and apply once at next safe frame start.
- **Rationale**: Avoids mid-frame resource invalidation and preserves deterministic frame boundaries.
- **Alternatives considered**:
  - Immediate mid-frame resource recreation (rejected: unstable and unsafe).
  - Drop active-frame resize events (rejected: stale surface state).

## Decision 5: External rendering uses renderer-owned pass bridge
- **Decision**: External/tooling rendering executes through renderer-managed external-pass callback boundaries.
- **Rationale**: Preserves renderer ownership of lifecycle and GPU state while allowing DebugUI integration.
- **Alternatives considered**:
  - Give external systems backend device/context access (rejected: backend leakage).
  - Separate independent present path for tooling (rejected: lifecycle fragmentation).

## Decision 6: Contract violation handling is environment-aware
- **Decision**: Development builds assert + log; non-development builds log + skip invalid operations safely.
- **Rationale**: Strong diagnostics in development without unnecessary hard failure in non-development runs.
- **Alternatives considered**:
  - Always terminate on violation (rejected: too disruptive).
  - Silent ignore (rejected: masks defects).

## Decision 7: Recovery policy for present/device/swapchain failures
- **Decision**: Log failure details, enter safe non-rendering mode, and attempt controlled reinitialization.
- **Rationale**: Avoids undefined rendering behavior and provides deterministic recovery flow.
- **Alternatives considered**:
  - Continue normal rendering after failure (rejected: undefined behavior risk).
  - Fatal termination on first failure (rejected: poor resilience).

## Decision 8: Boundary compliance is validated by automated audit
- **Decision**: Enforce include/type boundary rules using `Scripts/Audit-RendererBoundaries.ps1`.
- **Rationale**: Makes architectural guardrails continuously verifiable.
- **Alternatives considered**:
  - Manual review only (rejected: inconsistent enforcement).
  - Build-only checks without explicit boundary scan (rejected: weaker coverage).

## Open Questions Resolution
All open clarification items are resolved by the decisions above.

## Reference Implementation Rule
- The agent may inspect `D:\Yamen Development\Old-Reference\cqClient\Conquer` only to understand behavior/constraints.
- Final design remains aligned with LongXi architecture.


## Implementation Outcome Alignment (2026-03-11)
- Public renderer contract refactored to abstract `Renderer` + `CreateRenderer()` factory entrypoint.
- DX11 backend remains behind `DX11Renderer`; engine modules now consume `Renderer` interface.
- Texture handle exposure refactored to opaque renderer handles (`std::shared_ptr<void>`), removing backend handle types from `Texture.h`.
- Sprite renderer backend coupling moved behind private implementation + DX11 backend pipeline module.
- Boundary audit script hardened to detect DirectX symbol leakage and DX11 backend-header include leakage.

## Validation Sweep Summary (2026-03-11)
- Boundary audit: PASS (`ok=true`, zero violations).
- Runtime: developer-provided logs show validation scene + debug panels + texture load execution paths active.
- Build in this automation sandbox: `MSB4018` FileTracker failure blocked full compile verification; requires confirmation on developer machine toolchain session.
- Follow-up build check (LXEngine project-only) after API refactor produced no C++ diagnostics before the same MSBuild FileTracker (MSB4018) tooling failure.
