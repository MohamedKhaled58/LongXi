# Research: Spec 015 - Render Resource System

## Decision 1: Opaque handle identity model
- **Decision**: Use backend-agnostic opaque handle structs with logical identity fields (resource class + slot/index + generation) and no backend-native pointer payload.
- **Rationale**: Enables stale-handle detection and backend table lookup without exposing DirectX resource types.
- **Alternatives considered**:
  - Raw pointers in handles (rejected: leaks backend details, unsafe lifetime semantics).
  - `std::shared_ptr<void>` as public handle payload (rejected for this spec scope: still pointer-like ownership crossing boundaries).

## Decision 2: Descriptor-first creation APIs
- **Decision**: All GPU resource creation uses descriptor structures (`TextureDesc`, `BufferDesc`, `ShaderDesc`) instead of long parameter lists.
- **Rationale**: Descriptor growth remains backward-compatible and keeps API contracts stable across future backend evolution.
- **Alternatives considered**:
  - Per-resource overloaded parameter lists (rejected: brittle and hard to version).
  - Backend-specific descriptor variants in public API (rejected: violates backend-agnostic boundary).

## Decision 3: Resource usage policy enforcement
- **Decision**: Enforce explicit usage classes (`Static`, `Dynamic`, `Staging`) with hard policy checks on map/update operations.
- **Rationale**: Prevents undefined CPU↔GPU access behavior and keeps update costs predictable.
- **Alternatives considered**:
  - Soft warnings only (rejected: allows silent misuse).
  - Single usage mode for all resources (rejected: too restrictive and inefficient).

## Decision 4: Renderer-owned resource tables
- **Decision**: Maintain backend-internal resource tables/pools (`TexturePool`, `BufferPool`, `ShaderPool`) as the sole source of resource truth.
- **Rationale**: Centralized ownership simplifies lifecycle tracking, stale-handle rejection, and shutdown cleanup.
- **Alternatives considered**:
  - Resource ownership split across subsystems (rejected: coupling and leak risk).
  - Global singleton resource registries outside renderer (rejected: hidden state and layering violations).

## Decision 5: Explicit binding contract
- **Decision**: Define dedicated binding operations per resource type and enforce valid stage/bind-point combinations.
- **Rationale**: Keeps pipeline behavior deterministic and avoids hidden implicit binds.
- **Alternatives considered**:
  - Implicit bind-on-draw behavior (rejected: non-deterministic state coupling).
  - Generic untyped bind entrypoint (rejected: weak validation and ambiguous usage).

## Decision 6: Controlled dynamic update workflow
- **Decision**: Keep both `UpdateBuffer` and `Map/Unmap` APIs under renderer ownership with validation gates.
- **Rationale**: Supports simple update paths and advanced streaming paths while preserving safety checks.
- **Alternatives considered**:
  - Map/Unmap only (rejected: unnecessary complexity for simple updates).
  - Update-only with no map (rejected: limits future high-frequency streaming workflows).

## Decision 7: Deterministic destroy and shutdown behavior
- **Decision**: Destroy APIs invalidate handle generations immediately; renderer shutdown force-releases all remaining tracked resources and logs summary.
- **Rationale**: Eliminates use-after-free ambiguity and guarantees cleanup at runtime termination.
- **Alternatives considered**:
  - Lazy deferred deletion with no generation invalidation (rejected: stale-handle hazards).
  - Best-effort shutdown without accounting (rejected: non-diagnosable leaks).

## Decision 8: Boundary compliance validation remains automated
- **Decision**: Continue enforcing DirectX boundary rules with `D:\Yamen Development\LongXi\Scripts\Audit-RendererBoundaries.ps1`, extended for resource-layer files.
- **Rationale**: Architectural guardrails must be continuously checkable, not review-only.
- **Alternatives considered**:
  - Manual review only (rejected: inconsistent enforcement).
  - Build-system-only checks with no custom audit (rejected: weaker boundary coverage).

## Open Questions Resolution
All `Technical Context` unknowns are resolved for planning scope; no remaining `NEEDS CLARIFICATION` items.

## Reference Implementation Rule
- The agent may inspect `D:\Yamen Development\Old-Reference\cqClient\Conquer` only to understand behavior/constraints.
- Final architecture remains aligned with LongXi constitution and module boundaries.
