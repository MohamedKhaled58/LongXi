# Contract: External Pass Bridge

## Purpose
Define how external tooling renderers (e.g., Debug UI) integrate with renderer lifecycle without backend leakage.

## Participants
- **Renderer**: Owns lifecycle, pass sequencing, and backend state.
- **External Client**: Provides draw callback/content through bridge contract only.

## Contract Rules
- External client never receives backend-native device/context/swapchain handles.
- External pass executes only within renderer-owned frame lifecycle.
- External pass cannot bypass `BeginFrame/BeginPass/EndPass/EndFrame` sequencing.

## Bridge Operations

### `RegisterExternalPass(passDescriptor)`
- Registers external pass metadata (name, ordering slot, enablement).
- Returns renderer-owned bridge identifier.

### `ExecuteExternalPass(bridgeId, renderCallback)`
- **Preconditions**: Active frame; renderer authorizes pass slot.
- **Execution**: Renderer begins managed external pass, invokes callback, then ends pass.
- **Postconditions**: Renderer regains ownership of lifecycle and state progression.

### `UnregisterExternalPass(bridgeId)`
- Removes pass registration safely outside active execution.

## Failure and Violation Behavior
- Callback invoked outside valid lifecycle -> rejected and logged.
- Callback attempts backend-native access -> treated as contract violation by architecture audit.
- Bridge execution failure -> renderer logs and follows recoverable policy if rendering cannot continue.

## Observability Requirements
- Log pass registration/unregistration.
- Log each external pass begin/end in debug diagnostics mode.
- Emit contract violations for invalid external pass usage.

## Latest Audit Result
- Audit script: `Scripts/Audit-RendererBoundaries.ps1 -Json`
- Date: 2026-03-11
- Result: PASS (`ok=true`, no leakage violations outside `LXEngine/Src/Renderer`).
