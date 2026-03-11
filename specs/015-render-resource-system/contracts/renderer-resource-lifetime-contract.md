# Contract: Renderer Resource Lifetime and Ownership

## Purpose
Define renderer-owned lifetime tracking, stale-handle safety, and shutdown cleanup behavior for all GPU resource classes.

## Ownership Rules
- Renderer is the sole owner of GPU resource allocation and release.
- Engine systems may hold handles but do not own backend resources.
- Backend tables are internal and not directly exposed.

## Handle Validity Rules
- Handles include generation/version semantics.
- Reused pool slots must increment generation.
- Any generation mismatch is treated as stale-handle access and rejected.

## Lifetime States
- `Allocated`
- `Bound`
- `Mapped`
- `PendingDestroy`
- `Destroyed`

## Legal Transition Rules
- `Allocated -> Bound` on valid bind operation.
- `Allocated -> Mapped` only when usage policy allows map mode.
- `Mapped -> Allocated` on successful unmap.
- `Allocated|Bound -> PendingDestroy -> Destroyed` on destroy.
- `Destroyed` is terminal for operation access (diagnostics only).

## Destroy Semantics
- Destroy operations are explicit API calls (`DestroyTexture`, `DestroyBuffer`, `DestroyShader`).
- Destroy invalidates handle generation immediately.
- Destroying bound resources must transition through controlled unbind/release behavior in backend before resource-table invalidation.
- Destroying a stale handle MUST fail with `InvalidHandle` and must not mutate pool state.

## Shutdown Semantics
- Renderer shutdown walks all resource pools.
- Any live resource record is force-released in backend order-safe sequence.
- Shutdown logs include per-pool counts for created, destroyed, and force-released resources.
- Shutdown summary MUST report `live = 0` for texture, buffer, and shader pools when cleanup succeeds.

## Failure and Diagnostics Rules
- Unknown handle: reject + log (`invalid handle`).
- Stale handle: reject + log (`stale generation`).
- Illegal state transition: reject + log (`lifecycle policy violation`).
- Backend release failure: log with resource class and handle identity context.

## Compliance Checks
- Boundary audit must show no DirectX leakage outside backend modules.
- Runtime validation must show no stale-handle crash during stress create/destroy cycles.

## Reference Implementation Rule
- Legacy reference code at `D:\Yamen Development\Old-Reference\cqClient\Conquer` is informational only.
- LongXi renderer architecture remains the controlling design authority.
