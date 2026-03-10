# Spec 004 — Resource and Filesystem Foundation

**Feature Branch**: `004-resource-fs-foundation`
**Created**: 2026-03-10
**Status**: Accepted
**Input**: User description: "Resource and Filesystem Foundation"

## Objective

Establish a minimal, stable, centralized resource/filesystem foundation that allows the engine to resolve, normalize, open, and read files through a controlled interface suitable for later asset and archive integration.

## Problem Statement

The shell, renderer, and input foundation are complete, but no formal resource or filesystem layer exists. All file access is currently ad hoc. Later asset parsers and reverse-engineered format readers — for `.c3`, `.ani`, `.ini`, `.dds`, `.scene`, `.dmap`, `.pul`, and `.wdf` — cannot safely depend on scattered direct OS file access spread across the codebase. A centralized, controlled file access boundary must exist before any format-specific loader work can begin.

## Scope

This specification includes only:

- A centralized engine-owned resource/filesystem interface
- Path normalization rules sufficient to prevent inconsistent file lookup behavior
- Path resolution relative to one or more configured root locations or mount points
- Opening files for read access through the centralized interface
- Reading file contents as raw bytes or stream-oriented access suitable for later parsers
- File existence and query checks
- Diagnostics and logging for missing or failed file access
- A defined extension boundary for future non-directory-backed sources (e.g., archives) without implementing archive parsing in this spec
- Integration into the existing application/engine runtime without adding to `LXShell`

## Out of Scope

This specification explicitly excludes:

- Parsing `.wdf` archives
- Parsing any asset or data format (`.c3`, `.ani`, `.ini`, `.dds`, `.scene`, `.dmap`, `.pul`)
- Asset caching and pooling systems
- Asynchronous or background loading
- Hot reload
- Asset dependency tracking
- Resource reference counting beyond minimal ownership required for this spec
- Editor and browser tooling
- Compression and decompression systems
- Virtual filesystem complexity beyond what a minimal bootstrap foundation requires

## User Scenarios & Testing

### User Story 1 — File Resolution and Read (Priority: P1)

A developer adds a file under the configured resource root. The resource/filesystem foundation resolves the path, opens the file, and provides raw byte access. The developer can read the file contents without directly invoking OS file APIs or embedding path logic in caller code.

**Why this priority**: This is the sole user story. Without a working file resolution and read path, no subsequent asset loader or format parser can be built on a stable foundation.

**Independent Test**: Place a known test file (e.g., `test.dat` containing known bytes) under the configured resource root. Use the centralized interface to open and read it. Verify the returned bytes match the file contents. Then request a non-existent path and verify the failure is reported with a diagnostic log entry.

**Acceptance Scenarios**:

1. **Given** a file exists under the configured resource root, **When** the centralized interface is asked to open it by relative path, **Then** the file is opened and byte-level read access is available
2. **Given** two equivalent paths that differ by separator style or casing normalization, **When** both are resolved through the centralized interface, **Then** both resolve consistently to the same location
3. **Given** a path that does not exist under any configured root, **When** the centralized interface is asked to open it, **Then** the result is a controlled failure and a diagnostic log entry is produced — no undefined behavior or crash occurs
4. **Given** the application is running with the resource foundation initialized, **When** a later parser component calls the centralized interface, **Then** it can read file bytes without depending on direct OS file access

---

### Edge Cases

- What happens when a path contains mixed forward and backward slashes?
- What happens when a path contains leading or trailing whitespace?
- What happens when a path traversal sequence (`..`) is present?
- What happens when the configured root itself does not exist at startup?
- What happens when a file exists but cannot be read (permission denied)?
- What happens when the file size is zero?

## Requirements

### Functional Requirements

- **FR-001**: The engine SHALL provide a centralized resource/filesystem foundation owned by engine/runtime code (`LXCore` or `LXEngine`) rather than `LXShell`
- **FR-002**: The foundation SHALL normalize file paths — replacing backslashes with forward slashes, collapsing redundant separators, and trimming leading/trailing whitespace — before any resolution or lookup
- **FR-003**: The foundation SHALL support resolving normalized relative paths against one or more configured root locations registered at startup
- **FR-004**: The foundation SHALL support existence queries — determining whether a given relative path resolves to a readable file under any configured root — without opening the file
- **FR-005**: The foundation SHALL support opening files for read-only access through the centralized interface
- **FR-006**: The foundation SHALL support reading file contents as a contiguous block of raw bytes (`std::vector<uint8_t>`), returning the result to the caller for interpretation; a stream-oriented extension point is reserved in the design but not implemented in this spec
- **FR-007**: File access failure — missing file, inaccessible path, unresolvable root — SHALL produce a controlled failure result and a structured log message identifying the failing path; no undefined behavior, no silent empty result
- **FR-008**: The foundation's internal design SHALL include a defined extension boundary that allows future archive-backed or non-directory-backed sources to be added without restructuring the caller-facing interface; no archive parsing is implemented in this spec
- **FR-009**: The resource/filesystem foundation SHALL remain decoupled from rendering, input, gameplay, and asset-format-specific logic
- **FR-010**: The resource/filesystem foundation SHALL integrate into the existing `Application`/engine lifecycle (Initialize / Shutdown) rather than bypassing it
- **FR-011**: Later format loaders and parsers SHALL be able to consume resource data through the centralized interface without directly invoking OS file APIs
- **FR-012**: `LXShell` SHALL NOT require knowledge of filesystem internals to use the engine; the foundation is an engine-internal concern

### Key Entities

- **ResourceSystem**: The centralized engine object responsible for managing root registrations, path normalization, path resolution, and file open/read operations
- **ResourceRoot**: A registered base directory path from which relative resource paths are resolved; multiple roots may be registered in priority order
- **ResourcePath**: A normalized relative path string used to identify a resource within the registered root set
- **FileData**: A contiguous block of raw bytes returned from a successful file read operation, owned by the caller
- **MountPoint** *(extension boundary only)*: A named registration slot reserved for future non-directory-backed sources; not implemented in this spec, but the interface must not preclude it

## Success Criteria

### Measurable Outcomes

- **SC-001**: The application continues to launch successfully with the resource/filesystem foundation initialized — no regression in window, renderer, or input startup
- **SC-002**: A file placed under the configured resource root can be opened and its raw byte contents read through the centralized interface, with the returned bytes matching the file's actual contents
- **SC-003**: A request for a non-existent file produces a controlled failure observable through both the return value and the structured log output — no crash, hang, or silent empty result
- **SC-004**: Two paths that are equivalent after normalization resolve to the same file consistently — path normalization produces deterministic results for any valid input
- **SC-005**: A later parser component can be written to consume file bytes through the centralized interface without importing or calling Windows file access functions directly
- **SC-006**: Resource/filesystem lifecycle ownership remains in engine/runtime code — `LXShell/Src/main.cpp` is not modified
- **SC-007**: The interface design accommodates a future archive-backed source addition without requiring changes to the caller-facing API

## Acceptance Criteria

- **AC-001**: `LXShell.exe` builds without errors in Debug, Release, and Dist configurations with the resource/filesystem foundation integrated
- **AC-002**: A known test file placed under the configured resource root can be opened and read through the centralized interface; the returned byte count and content match the file on disk (verified via diagnostic log output during the bootstrap verification pass)
- **AC-003**: Requesting a path that does not exist produces a controlled failure: the return value reflects failure and a log entry naming the failed path is visible in the console
- **AC-004**: A path submitted with backslash separators resolves identically to the same path submitted with forward slash separators
- **AC-005**: The resource/filesystem foundation initializes and shuts down within the existing `Application::Initialize` and `Application::Shutdown` call sequence without disrupting the window, renderer, or input systems
- **AC-006**: `LXShell/Src/main.cpp` remains unchanged from Spec 003 — still only `CreateApplication()`
- **AC-007**: No asset-format-specific parsing code is present in or required by this spec's deliverables

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Direct OS file access spreading outside the centralized layer in later parser code | High | High | Establish the centralized interface first; enforce through code review that parsers only call the resource interface |
| Overdesigning a complex virtual filesystem abstraction before real needs are known | Medium | Medium | Restrict this spec to a concrete directory-backed implementation with only a reserved extension boundary; no abstract virtual FS |
| Tying the resource layer too tightly to Windows-specific path or handle details | Medium | Medium | Normalize all paths to a forward-slash internal representation; contain Windows-specific file APIs to the implementation, not the interface |
| Confusing or inconsistent path normalization policy causing lookup failures for real assets | Medium | High | Define normalization rules explicitly in this spec; document and test them with edge case paths during verification |
| Leaking archive-specific assumptions (e.g., WDF file offset logic) into the pre-archive foundation | Low | High | Keep the extension boundary minimal — a reserved slot, not a partial implementation |
| Mixing parser or asset-interpretation logic into filesystem access code | Low | High | Enforce that the resource layer returns raw bytes only; all interpretation is the caller's responsibility |

## Assumptions

- Spec 001 shell bootstrap is complete and stable
- Spec 002 DX11 render bootstrap is complete and stable
- Spec 003 input foundation is complete and stable
- Later specs will add `.wdf` archive support and individual format parsers (`ResourceSystem` will eventually support archive-backed roots)
- The reference client (Long Xi 6609) stores most assets in `.wdf` archive files, but the bootstrap foundation must first handle loose files before archive integration is layered on top
- The current goal is file access foundation only — no content interpretation or format knowledge

## Resolved Questions

- **OQ-001**: Internal resource/file paths SHALL use `std::string` as the engine-facing representation. Paths SHALL be treated as UTF-8/ASCII-compatible project strings. Internal normalized paths SHALL use forward slashes (`/`). Conversion to Win32 wide-string form SHALL occur only at the OS call boundary where required. The goal is to keep the internal API clean and minimize Windows-specific type leakage into the interface. _Resolved: User confirmed._
- **OQ-002**: Resource roots SHALL be registered during `ResourceSystem::Initialize()`. The initial root set is fixed for the runtime session in this spec. Dynamic root registration is explicitly out of scope for Spec 004. This keeps startup deterministic and the initial design narrow. _Resolved: User confirmed._
- **OQ-003**: Spec 004 requires whole-file read support only. The primary read operation SHALL return the full file contents as raw bytes (`std::vector<uint8_t>` or equivalent). Stream-oriented access is preserved as a future extension point but is not required by this spec. This keeps the API minimal and sufficient for early parser and reverse-engineering work. _Resolved: User confirmed._

## Boundary Note

This specification intentionally establishes only the minimal resource and filesystem access boundary. Archive integration (`.wdf` and similar formats), format-specific parsers (`.c3`, `.ani`, `.dds`, `.ini`, `.scene`, `.dmap`, `.pul`), asset caching, asynchronous loading, hot reload, and all higher-level resource systems are expected to be addressed by later separate specifications. This spec is deliberately narrow to ensure a stable, reviewable file access foundation exists before adding complexity.
