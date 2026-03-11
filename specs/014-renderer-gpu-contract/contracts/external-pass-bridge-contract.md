# Contract: External Pass Bridge

## Purpose
Define how external tooling renderers (for example DebugUI) integrate through renderer-owned lifecycle boundaries without backend leakage.

## Participants
- **Renderer**: Owns frame lifecycle, pass sequencing, and GPU state contract.
- **External Client**: Provides callback content only; no backend-native access.

## Contract Rules
- External client does not receive backend-native device/context/swapchain handles.
- External pass executes only inside active renderer frame lifecycle.
- External pass cannot bypass renderer pass ordering and state reset rules.

## Bridge Operations

### `RegisterExternalPass(passDescriptor)`
- Registers external pass metadata (label, order slot, enable state).
- Returns renderer-owned bridge identifier.

### `ExecuteExternalPass(bridgeId, callback)`
- **Preconditions**: Active frame; bridge registered; callback valid.
- **Execution**: Renderer enters managed external pass, invokes callback, exits pass, and restores lifecycle ownership.
- **Postconditions**: Renderer state remains valid for subsequent frame progression.

### `UnregisterExternalPass(bridgeId)`
- Removes bridge registration outside active execution.

## Failure and Violation Behavior
- Execution outside valid lifecycle -> reject and log.
- Unregistered bridge id -> reject and log.
- Callback failure -> log and route through renderer recovery/violation policy.

## Observability
- Log registration/unregistration.
- Log external pass begin/end in debug diagnostics mode.
- Emit contract violation events for invalid usage.

## Boundary Compliance
External pass bridge integration must preserve:
- Backend-agnostic engine-side interfaces.
- Renderer-exclusive ownership of backend API calls and GPU state.

## Compliance Evidence (2026-03-11)
- Boundary audit command: `powershell -ExecutionPolicy Bypass -File Scripts/Audit-RendererBoundaries.ps1 -Json`
- Result: `{"scanned":"LongXi/LXEngine/Src","violations":[],"ok":true}`
- Interpretation: no DirectX include/type leakage detected outside approved renderer backend implementation files.
