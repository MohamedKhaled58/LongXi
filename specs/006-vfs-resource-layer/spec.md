# Feature Specification: Virtual File System (VFS) & Resource Access Layer

**Feature Branch**: `006-vfs-resource-layer`
**Created**: 2026-03-10
**Status**: Draft

---

## Overview

The Virtual File System (VFS) is a resource access layer that decouples all engine systems from the details of how and where assets are stored on disk. Engine systems (renderer, UI, audio, gameplay) request resources by a virtual path string. The VFS resolves that path across all mounted sources — WDF archives and loose directories — and returns the resource data without the caller needing to know the underlying storage.

The original Conquer Online client stores resources in WDF archives. During development, loose files on disk override packaged content. The VFS manages both sources transparently behind a single interface.

**This spec extends the existing `ResourceSystem` + `WdfArchive` foundation established in Specs 004 and 005.** The existing `ResourceSystem` provides whole-file reads and loose-file root management. Spec 006 adds the `IFileStream` abstraction, the `Open()` stream API, and formalizes the mount-point model.

### Scope (In)
- Unified path-based resource access via a single VFS class
- WDF archive mounting and lookup (via existing `WdfArchive`, Spec 005)
- Loose directory mounting and lookup
- Deterministic priority-ordered search across all mounted sources
- Abstract `IFileStream` interface for seekable, readable resource streams
- Error-safe operation — no crashes on missing or corrupt resources
- Logging of all mount and failed-lookup events

### Scope (Out)
- Resource caching or hot reload
- Async or streaming reads
- Writing or repacking resources (read-only)
- Network resource sources
- Archive creation

---

## Architecture Diagram

```
+--------------------------------------------------+
|               Engine Systems                     |
|       (Renderer, UI, Audio, Gameplay)            |
+---------------------+----------------------------+
                      |
                      | virtual path: "texture/terrain/grass.dds"
                      v
+---------------------+----------------------------+
|            CVirtualFileSystem                    |
|                                                  |
|   m_MountPoints: [ MP[0], MP[1], MP[2], ... ]   |
|   Searched in index order. First match wins.     |
+--------+-----------+------------+----------------+
         |           |            |
         v           v            v
  +------------+ +----------+ +----------+
  | CDirectory | | CDirectory| | CWdf     |
  | MountPoint | | MountPoint| | MountPoint|
  | (patch dir)| |(loose dir)| |(archive) |
  +------------+ +----------+ +----------+
         |           |            |
         v           v            v
    Filesystem   Filesystem   WdfArchive
    (disk)       (disk)       (Spec 005)
                                  |
                         +--------+--------+
                         |   IFileStream   |
                         +-----------------+
                         | CFileDiskStream |
                         | CFileWdfStream  |
                         +-----------------+
```

**Mount priority** is determined entirely by mount order. First mounted = highest priority. The caller is responsible for mounting in the intended order (patch first, then loose dirs, then archives).

---

## User Scenarios & Testing

### Scenario 1 — Unified Resource Loading (P1)

Engine systems load any resource by virtual path without knowing whether it lives in a WDF archive or on disk.

**Acceptance**:
1. **Given** a WDF archive containing `texture/terrain/grass.dds` is mounted, **When** `Open("texture/terrain/grass.dds")` is called, **Then** a valid `IFileStream` is returned with the correct data.
2. **Given** a loose directory containing `texture/terrain/grass.dds` is mounted, **When** `Open("texture/terrain/grass.dds")` is called, **Then** a valid `IFileStream` reading from disk is returned.
3. **Given** no source contains `texture/missing.dds`, **When** `Open("texture/missing.dds")` is called, **Then** `nullptr` is returned without crashing.

---

### Scenario 2 — Development Override and Patch Support (P2)

Loose files on disk override packaged resources when a directory is mounted before an archive.

**Acceptance**:
1. **Given** a directory mounted before a WDF archive, and both contain the same virtual path, **When** the resource is requested, **Then** the directory version is returned.
2. **Given** a directory does not contain a resource but a WDF archive does, **When** the resource is requested, **Then** the archive version is returned.

---

### Scenario 3 — Multiple Archive Support (P3)

Multiple WDF archives are mounted simultaneously; resources are found across all archives in mount order.

**Acceptance**:
1. **Given** two archives mounted in order, and a resource exists only in the second, **When** requested, **Then** the second archive's version is returned.
2. **Given** a resource exists in both archives, **When** requested, **Then** the first-mounted archive's version is returned.

---

### Edge Cases

- Empty or whitespace-only path → returns `nullptr`/`false` immediately; no mount point queried.
- Path with leading slash (`/texture/grass.dds`) → normalized by stripping the leading slash.
- Path with backslashes → normalized to forward slashes.
- Path with uppercase characters → normalized to lowercase.
- WDF archive path not found at mount time → `MountWdf` returns `false`; no mount point registered.
- WDF archive valid at mount but corrupt at extract time → VFS logs error, returns `nullptr` for that lookup; archive is not unmounted.
- `Read` past end-of-stream → returns `0` bytes; no crash.
- `Seek` beyond stream size → returns `false`; stream position unchanged.

---

## Functional Requirements

- **FR-001**: The VFS MUST expose a single unified interface (`CVirtualFileSystem`) for all resource reads regardless of storage source.
- **FR-002**: The VFS MUST support mounting WDF archives as resource sources via `MountWdf(path)`.
- **FR-003**: The VFS MUST support mounting directories as resource sources via `MountDirectory(path)`.
- **FR-004**: The VFS MUST support multiple simultaneously mounted sources.
- **FR-005**: The VFS MUST resolve resources using deterministic priority based on mount order (first-mounted = highest priority; first match wins).
- **FR-006**: The VFS MUST support querying resource existence via `Exists(path) → bool`.
- **FR-007**: The VFS MUST support opening a resource as a seekable stream via `Open(path) → unique_ptr<IFileStream>`.
- **FR-008**: The VFS MUST support reading all bytes of a resource via `ReadAll(path) → vector<uint8_t>`.
- **FR-009**: The VFS MUST normalize all caller-provided paths to lowercase, forward-slash delimited, no leading slash before any lookup.
- **FR-010**: All returned streams MUST implement `IFileStream` with `Read`, `Seek`, `Tell`, and `Size`.
- **FR-011**: The VFS MUST cache WDF index data at mount time; it MUST NOT re-read the index on subsequent lookups.
- **FR-012**: The VFS MUST log a message for every successful `MountWdf` call.
- **FR-013**: The VFS MUST log a message for every successful `MountDirectory` call.
- **FR-014**: The VFS MUST log a warning for every `Open` or `ReadAll` that finds no match.
- **FR-015**: The VFS MUST log an error when WDF archive corruption is detected during extraction.
- **FR-016**: The VFS MUST NOT crash, throw, or invoke undefined behavior on missing resources, malformed paths, or corrupt archives; all failures return safe null/false values.
- **FR-017**: `Exists`, `Open`, and `ReadAll` MUST be safe for concurrent calls once all mounts are complete. Mount operations are single-threaded only.
- **FR-018**: The VFS MUST use the `WdfArchive` class from Spec 005 for all WDF access.

---

## Class Definitions

### `CVirtualFileSystem`

The top-level resource manager. Owns an ordered list of mount points. All engine code interacts exclusively with this class for resource access.

```
class CVirtualFileSystem
{
public:
    bool MountWdf(const std::string& path);
    bool MountDirectory(const std::string& path);

    bool                              Exists(const std::string& path) const;
    std::unique_ptr<IFileStream>      Open(const std::string& path) const;
    std::vector<uint8_t>              ReadAll(const std::string& path) const;

private:
    std::string Normalize(const std::string& path) const;

    std::vector<std::unique_ptr<IMountPoint>> m_MountPoints;
};
```

---

### `IMountPoint`

Abstract interface for a single resource source. `CVirtualFileSystem` iterates its mount point list and calls these two methods on each until a match is found.

```
class IMountPoint
{
public:
    virtual ~IMountPoint() = default;
    virtual bool                         Exists(const std::string& normalizedPath) const = 0;
    virtual std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const   = 0;
};
```

---

### `CDirectoryMountPoint` (implements `IMountPoint`)

Resolves virtual paths to absolute disk paths by appending to its root directory. Does not cache directory listings.

```
class CDirectoryMountPoint : public IMountPoint
{
public:
    explicit CDirectoryMountPoint(const std::string& rootDirectory);
    bool                         Exists(const std::string& normalizedPath) const override;
    std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const override;

private:
    std::string m_Root;
};
```

---

### `CWdfMountPoint` (implements `IMountPoint`)

Resolves virtual paths through a `WdfArchive` (Spec 005). Owns the archive instance.

```
class CWdfMountPoint : public IMountPoint
{
public:
    explicit CWdfMountPoint(std::unique_ptr<WdfArchive> archive);
    bool                         Exists(const std::string& normalizedPath) const override;
    std::unique_ptr<IFileStream> Open(const std::string& normalizedPath) const override;

private:
    std::unique_ptr<WdfArchive> m_Archive;
};
```

---

### `IFileStream`

Abstract interface for readable, seekable resource data. All engine systems use only this interface; they never depend on the concrete stream type.

```
class IFileStream
{
public:
    virtual ~IFileStream() = default;

    // Read up to `size` bytes into `buffer`. Returns bytes actually read.
    // Returns 0 at end of stream.
    virtual size_t Read(void* buffer, size_t size) = 0;

    // Seek to absolute byte offset from stream start. Returns false if offset > Size().
    virtual bool Seek(size_t offset) = 0;

    // Returns current byte position.
    virtual size_t Tell() const = 0;

    // Returns total stream size in bytes.
    virtual size_t Size() const = 0;
};
```

---

### `CFileDiskStream` (implements `IFileStream`)

Wraps an open file handle on disk. `Read` and `Seek` map directly to the platform file API.

```
class CFileDiskStream : public IFileStream
{
public:
    // Caller ensures absolutePath exists and is readable.
    explicit CFileDiskStream(const std::string& absolutePath);
    ~CFileDiskStream() override; // closes file handle

    size_t Read(void* buffer, size_t size) override;
    bool   Seek(size_t offset) override;
    size_t Tell() const override;
    size_t Size() const override;

private:
    // platform file handle / std::ifstream
    size_t m_Size;
    size_t m_Position;
};
```

---

### `CFileWdfStream` (implements `IFileStream`)

Wraps a `std::vector<uint8_t>` holding fully extracted WDF entry bytes. Supports `Read`/`Seek`/`Tell`/`Size` against the in-memory buffer. The WDF archive file handle is NOT retained after extraction.

```
class CFileWdfStream : public IFileStream
{
public:
    explicit CFileWdfStream(std::vector<uint8_t> data);

    size_t Read(void* buffer, size_t size) override;
    bool   Seek(size_t offset) override;
    size_t Tell() const override;
    size_t Size() const override;

private:
    std::vector<uint8_t> m_Data;
    size_t               m_Position;
};
```

---

## Key Entities

| Entity | Role |
|---|---|
| `CVirtualFileSystem` | Main access point. Owns mount points. Only VFS type referenced by engine systems. |
| `IMountPoint` | Abstract source interface. Exists + Open per mount type. |
| `CDirectoryMountPoint` | Loose-file source. Stateless filesystem lookups. |
| `CWdfMountPoint` | WDF archive source. Owns `WdfArchive` (Spec 005). |
| `IFileStream` | Abstract seekable resource stream. Only type callers receive. |
| `CFileDiskStream` | Disk-backed stream. |
| `CFileWdfStream` | In-memory buffer stream from WDF extraction. |
| Virtual path | Normalized: lowercase, forward-slash, no leading slash. Example: `texture/terrain/grass.dds`. |

---

## Mount System

### Mount Order and Priority

`m_MountPoints` is a `std::vector` iterated from index 0 (highest priority) to the end. The first mount point that returns a match wins. The caller defines priority through mount call order:

```
// Recommended call order (highest priority first):
vfs.MountDirectory("Data/Patch");      // [0] patch overrides
vfs.MountDirectory("Data/Dev");        // [1] dev overrides
vfs.MountWdf("Data/texture.wdf");      // [2] baseline archives
vfs.MountWdf("Data/model.wdf");        // [3]
vfs.MountWdf("Data/ui.wdf");           // [4]
```

### `MountWdf` Behavior

1. Create a new `WdfArchive` instance.
2. Call `WdfArchive::Open(path)`.
3. If `Open` fails: log error, return `false`. Do not add to `m_MountPoints`.
4. If `Open` succeeds: wrap in `CWdfMountPoint`, append to `m_MountPoints`, return `true`.

### `MountDirectory` Behavior

1. Verify the directory exists on disk.
2. If not found: log error, return `false`.
3. If found: create `CDirectoryMountPoint(path)`, append to `m_MountPoints`, return `true`.

---

## Path Normalization Algorithm

Applied to every caller-provided path before any lookup:

```
1. Replace all '\' with '/'
2. Convert entire string to lowercase
3. Strip leading '/' if present
4. Strip trailing '/' if present
5. Collapse any '//' to '/'
6. If result is empty → reject (return nullptr/false)
```

**Examples:**

| Input | Normalized |
|---|---|
| `Texture\\Terrain\\Grass.DDS` | `texture/terrain/grass.dds` |
| `/texture/terrain/grass.dds` | `texture/terrain/grass.dds` |
| `MESH/Character/Warrior.mesh` | `mesh/character/warrior.mesh` |
| `INI//itemtype.dat` | `ini/itemtype.dat` |

---

## File Lookup Algorithm

```
Exists(callerPath):
    normalized = Normalize(callerPath)
    if normalized is empty: return false
    for each mp in m_MountPoints:
        if mp.Exists(normalized): return true
    return false

Open(callerPath):
    normalized = Normalize(callerPath)
    if normalized is empty:
        LOG_WARN "[VFS] Open called with empty path"
        return nullptr
    for each mp in m_MountPoints:
        if mp.Exists(normalized):
            stream = mp.Open(normalized)
            if stream != nullptr: return stream
    LOG_WARN "[VFS] Resource not found: " + normalized
    return nullptr

ReadAll(callerPath):
    stream = Open(callerPath)
    if stream == nullptr: return {}
    data.resize(stream.Size())
    stream.Read(data.data(), data.size())
    return data
```

### `CDirectoryMountPoint` Lookup

```
Exists(normalizedPath):
    absPath = m_Root + "/" + normalizedPath
    return FileExistsOnDisk(absPath)

Open(normalizedPath):
    absPath = m_Root + "/" + normalizedPath
    if not FileExistsOnDisk(absPath): return nullptr
    return make_unique<CFileDiskStream>(absPath)
```

### `CWdfMountPoint` Lookup

```
Exists(normalizedPath):
    return m_Archive.HasEntry(normalizedPath)

Open(normalizedPath):
    result = m_Archive.ReadEntry(normalizedPath)   // returns optional<vector<uint8_t>>
    if not result.has_value(): return nullptr
    return make_unique<CFileWdfStream>(std::move(*result))
```

`WdfArchive::HasEntry` and `ReadEntry` accept the full normalized path. UID computation is internal to `WdfArchive` (Spec 005). The VFS does not compute or store UIDs.

---

## WDF Integration (Spec 005)

The VFS uses the `WdfArchive` class from Spec 005 through `CWdfMountPoint`. The integration contract is:

```
class WdfArchive                                          // defined in Spec 005 / LXCore
{
public:
    bool                                   Open(const std::string& absolutePath);
    bool                                   HasEntry(const std::string& normalizedPath) const;
    std::optional<std::vector<uint8_t>>    ReadEntry(const std::string& normalizedPath) const;

    void               Close();
    bool               IsOpen() const;
    const std::string& GetPath() const;
};
```

**Key integration notes:**

- `WdfArchive::Open` loads and retains the archive index in memory (Spec 005 guarantee). No re-scan occurs on subsequent `HasEntry`/`ReadEntry` calls.
- `WdfArchive::ReadEntry` returns an owned `optional<vector<uint8_t>>`. On failure (entry not found or I/O error), returns `nullopt`. `CWdfMountPoint` checks for `nullopt` and returns `nullptr` stream.
- `WdfArchive::HasEntry` / `ReadEntry` take the full **normalized path** (e.g., `texture/terrain/grass.dds`). UID derivation is encapsulated inside `WdfArchive`. The VFS does not hash paths or reference UIDs.
- `CWdfMountPoint` owns its `WdfArchive` via `unique_ptr`. Archive lifetime is tied to the mount point lifetime, which is tied to `CVirtualFileSystem` lifetime.

---

## Error Model

All VFS operations return safe values on failure. No exceptions propagate to callers for expected failure conditions.

| Failure | Return | Log |
|---|---|---|
| `MountWdf` — file not found | `false` | `[VFS] ERROR Failed to mount WDF: <path>` |
| `MountWdf` — archive invalid/corrupt | `false` | `[VFS] ERROR Failed to open WDF: <path>` |
| `MountDirectory` — directory not found | `false` | `[VFS] ERROR Failed to mount directory: <path>` |
| `Exists` — no match | `false` | (silent) |
| `Open` — empty path after normalization | `nullptr` | `[VFS] WARN Open called with empty path` |
| `Open` — no match across all sources | `nullptr` | `[VFS] WARN Resource not found: <path>` |
| `ReadEntry` returns nullopt | `nullptr` stream | `[VFS] ERROR Extraction failed: <path>` |
| WDF corruption detected by `WdfArchive` | `nullptr` stream | `[VFS] ERROR Archive corruption in: <archivePath>` |
| `Read` past end-of-stream | `0` bytes | (silent — normal EOF) |
| `Seek` beyond stream size | `false` | (silent — caller handles) |

**Rules:**
- `nullptr` from `Open` is the standard signal for "resource unavailable."
- Callers MUST check the `Open`/`ReadAll` return values before use.
- Archive corruption does not unmount the archive. Future lookups for other entries are still attempted.

---

## Logging Model

All log output uses `LX_CORE_INFO`, `LX_CORE_WARN`, `LX_CORE_ERROR` macros from LXCore (Spec 004). Every message is prefixed with `[VFS]`.

| Event | Level | Example |
|---|---|---|
| WDF archive mounted | INFO | `[VFS] Mounted WDF archive: Data/texture.wdf` |
| Directory mounted | INFO | `[VFS] Mounted directory: Data/Patch` |
| Resource not found | WARN | `[VFS] Resource not found: texture/sky.dds` |
| Open with empty path | WARN | `[VFS] Open called with empty path` |
| MountWdf failure | ERROR | `[VFS] Failed to mount WDF: Data/missing.wdf` |
| MountDirectory failure | ERROR | `[VFS] Failed to mount directory: Data/Patch` |
| Extraction failure | ERROR | `[VFS] Extraction failed: texture/sky.dds` |
| Archive corruption | ERROR | `[VFS] Archive corruption in: Data/texture.wdf` |

---

## Performance Considerations

**Archive index caching**: `WdfArchive::Open` loads the full index into memory (Spec 005). Subsequent `HasEntry`/`ReadEntry` calls use the cached index. No disk scan occurs per-lookup.

**Mount point traversal**: `m_MountPoints` is a `std::vector` iterated linearly. With 5–20 sources this is negligible. No filesystem scans or archive re-reads happen during traversal.

**No directory caching**: `CDirectoryMountPoint` performs a live filesystem stat per `Exists` call. This is intentional — loose directories are used primarily during development, not in production builds which rely entirely on WDF archives.

**`CFileWdfStream` memory model**: `CFileWdfStream` holds extracted bytes in a `std::vector<uint8_t>`. The WDF file handle is not retained after extraction. `Read`/`Seek`/`Tell` are O(1) against the buffer.

**No resource-level caching**: The VFS does not cache extracted data. Systems that load the same resource repeatedly are responsible for their own caching. This keeps the VFS simple and avoids ownership complexity.

---

## Thread Safety

**Initialization (single-threaded):** All `MountWdf` and `MountDirectory` calls MUST complete before any concurrent reads begin. Mount operations are not synchronized and must not overlap with reads.

**Reads (concurrent-safe):** Once all mounts are complete, `Exists`, `Open`, and `ReadAll` are safe for concurrent calls because:
- `m_MountPoints` is read-only after initialization.
- Each `Open` returns an independent `IFileStream` owned exclusively by the caller.
- `WdfArchive::HasEntry` and `ReadEntry` must be thread-safe for concurrent calls (Spec 005 requirement).
- `CDirectoryMountPoint` calls are stateless OS filesystem operations.

| Operation | Thread Safety |
|---|---|
| `MountWdf`, `MountDirectory` | Single-threaded only; no concurrent mounts or reads |
| `Exists`, `Open`, `ReadAll` | Concurrent-safe after all mounts complete |
| `IFileStream` instances | Not thread-safe; each instance owned by one caller |

**Phase 1 note (Article IX):** Phase 1 runtime is single-threaded by constitution. The concurrent-read design is future-ready for Phase 2 but imposes no cost in Phase 1.

---

## Module Placement

The VFS (`CVirtualFileSystem`, `IMountPoint`, `IFileStream`, and implementations) belongs in **LXCore**, consistent with the existing `ResourceSystem` and `WdfArchive` placement.

**Rationale:**
- `WdfArchive` is already in `LXCore/Src/Core/FileSystem/`.
- `ResourceSystem` is already in `LXCore/Src/Core/FileSystem/`.
- `LXEngine` depends on `LXCore`. VFS in `LXCore` means all layers (LXEngine, LXShell) can access it without creating reverse dependencies.
- Resource access is a low-level platform/foundation concern, consistent with `LXCore`'s role.

**Proposed file layout:**

```
LongXi/LXCore/Src/Core/FileSystem/
├── WdfArchive.h / .cpp          (Spec 005 — existing)
├── ResourceSystem.h / .cpp      (Spec 004 — existing foundation)
├── VirtualFileSystem.h / .cpp   (Spec 006 — new: CVirtualFileSystem)
├── MountPoint.h                 (Spec 006 — new: IMountPoint, CDirectoryMountPoint, CWdfMountPoint)
├── FileStream.h / .cpp          (Spec 006 — new: IFileStream, CFileDiskStream, CFileWdfStream)
```

`CVirtualFileSystem` MUST be exposed through `LXCore.h` (the module public entry header).

---

## Success Criteria

- **SC-001**: All engine systems load resources through `CVirtualFileSystem` only; no direct `WdfArchive` or filesystem calls appear in LXEngine or LXShell code.
- **SC-002**: A loose file on disk overrides the equivalent WDF resource when the directory is mounted first, verified in 100% of test cases.
- **SC-003**: Resource lookup does not trigger any archive re-scan or index re-read after initial mount.
- **SC-004**: A request for a missing resource returns `nullptr` in 100% of cases without crash or exception.
- **SC-005**: Every mount call and every failed lookup produces a log entry visible in the engine log.
- **SC-006**: Virtual paths with uppercase, backslashes, or leading slashes resolve to the same resource as their normalized equivalents.

---

## Assumptions

- `WdfArchive::HasEntry` / `ReadEntry` accept the full normalized path (e.g., `texture/terrain/grass.dds`). UID computation is internal to `WdfArchive`. The VFS does not hash paths.
- `WdfArchive::ReadEntry` is safe for concurrent calls — this is a requirement on Spec 005. If not, a mutex must be added to `CWdfMountPoint`.
- The VFS does not support runtime unmounting. All sources are mounted at startup and persist for the VFS lifetime.
- The VFS does not decompress data — decompression (if any) is handled inside `WdfArchive::ReadEntry`.
- `CFileDiskStream` uses the standard platform file API (`std::ifstream` or Win32) with no additional abstraction.

---

## Dependencies

- **Spec 004 (LXCore foundation)**: Provides `LX_CORE_INFO/WARN/ERROR` logging macros. Module structure and `LXCore.h` entry point.
- **Spec 005 (WDF Archive)**: Provides `WdfArchive` (`Open`, `HasEntry`, `ReadEntry`). UID hash is internal to `WdfArchive`.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
