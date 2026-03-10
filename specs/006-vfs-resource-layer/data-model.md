# Data Model: Virtual File System (VFS) & Resource Access Layer

**Branch**: `006-vfs-resource-layer`
**Date**: 2026-03-10

---

## Entity Map

```
CVirtualFileSystem
│
├── m_MountPoints: vector<unique_ptr<IMountPoint>>
│                  [ordered list; index 0 = highest priority]
│
IMountPoint (abstract)
├── CDirectoryMountPoint
│   └── m_Root: string          (absolute path, no trailing slash)
│
└── CWdfMountPoint
    └── m_Archive: unique_ptr<WdfArchive>   (owns the archive; Spec 005)

IFileStream (abstract)
├── CFileDiskStream
│   ├── m_File: std::ifstream   (open file handle)
│   ├── m_Size: size_t          (total bytes, cached at open)
│   └── m_Position: size_t      (current byte offset)
│
└── CFileWdfStream
    ├── m_Data: vector<uint8_t> (extracted bytes, owned)
    └── m_Position: size_t      (current byte offset)
```

---

## Entities

### CVirtualFileSystem

Primary resource access class. Owns the ordered list of mount points. Exposes the public API used by all engine systems.

| Field | Type | Notes |
|---|---|---|
| `m_MountPoints` | `vector<unique_ptr<IMountPoint>>` | Ordered; index 0 = first searched |

**Methods:**

| Method | Signature | Notes |
|---|---|---|
| `MountWdf` | `bool(const string& path)` | Creates CWdfMountPoint; appends to m_MountPoints |
| `MountDirectory` | `bool(const string& path)` | Creates CDirectoryMountPoint; appends to m_MountPoints |
| `Exists` | `bool(const string& path) const` | Normalizes; iterates mount points |
| `Open` | `unique_ptr<IFileStream>(const string& path) const` | Normalizes; returns first match |
| `ReadAll` | `vector<uint8_t>(const string& path) const` | Calls Open(); reads all bytes |
| `Normalize` (private) | `string(const string& path) const` | See normalization rules |

**Constraints:**
- `m_MountPoints` is append-only after construction; no removal or reordering
- All public methods are safe to call from multiple threads once mounts are complete
- Mount operations (`MountWdf`, `MountDirectory`) must not be called concurrently with any other operation

---

### IMountPoint

Abstract interface representing a single resource source. Two pure-virtual methods.

| Method | Signature | Notes |
|---|---|---|
| `Exists` | `bool(const string& normalizedPath) const` | Must not throw |
| `Open` | `unique_ptr<IFileStream>(const string& normalizedPath) const` | Returns nullptr on miss |

**Contract invariants:**
- If `Exists(path)` returns true, `Open(path)` MUST NOT return nullptr (barring I/O errors)
- If `Exists(path)` returns false, `Open(path)` MUST return nullptr
- Implementations may not modify shared state during `Exists` or `Open`

---

### CDirectoryMountPoint

Resolves virtual paths by appending to a root directory path.

| Field | Type | Notes |
|---|---|---|
| `m_Root` | `string` | Absolute directory path; no trailing slash |

**Lookup logic:**
```
Exists(normalizedPath) → FileExistsOnDisk(m_Root + "/" + normalizedPath)
Open(normalizedPath)   → CFileDiskStream(m_Root + "/" + normalizedPath) or nullptr
```

**No caching.** Each call performs a live filesystem stat. Intended for development/patch use.

---

### CWdfMountPoint

Resolves virtual paths through a `WdfArchive` instance (Spec 005).

| Field | Type | Notes |
|---|---|---|
| `m_Archive` | `unique_ptr<WdfArchive>` | Owned; lives as long as the mount point |

**Lookup logic:**
```
Exists(normalizedPath) → m_Archive.HasEntry(normalizedPath)
Open(normalizedPath)   → optional<vector<uint8_t>> = m_Archive.ReadEntry(normalizedPath)
                          → CFileWdfStream(move(*result)) or nullptr
```

**Index is cached** inside `WdfArchive` at `Open()` time; no re-read per lookup.

---

### IFileStream

Abstract sequential/seekable resource stream. Returned by `CVirtualFileSystem::Open()`. Callers own the instance.

| Method | Signature | Notes |
|---|---|---|
| `Read` | `size_t(void* buf, size_t size)` | Returns bytes actually read; 0 at EOF |
| `Seek` | `bool(size_t offset)` | Returns false if offset > Size() |
| `Tell` | `size_t() const` | Current byte position |
| `Size` | `size_t() const` | Total stream size in bytes |

**Invariants:**
- `Tell()` always returns a value in `[0, Size()]`
- `Read()` never reads beyond `Size()`; returns fewer bytes near EOF
- `Seek(0)` always succeeds
- After a failed `Seek`, position is unchanged

---

### CFileDiskStream

Disk-backed stream. Wraps `std::ifstream`. Platform path conversion handled via `ToWide()`.

| Field | Type | Notes |
|---|---|---|
| `m_File` | `std::ifstream` | Opened in binary mode at construction |
| `m_Size` | `size_t` | Cached from `tellg` at open |
| `m_Position` | `size_t` | Tracked manually; synchronized with `m_File` seekg |

**Lifetime**: File handle held open for the duration of the stream's lifetime. Closed in destructor.

---

### CFileWdfStream

In-memory stream. Holds extracted WDF entry bytes. No file handle retained after construction.

| Field | Type | Notes |
|---|---|---|
| `m_Data` | `vector<uint8_t>` | Owned buffer; received by move from WdfArchive::ReadEntry |
| `m_Position` | `size_t` | Current read position |

**Lifetime**: Buffer owned by this stream instance. No external dependencies after construction.

---

## State Transitions

### CVirtualFileSystem lifecycle

```
[Constructed]
    │
    ├─ MountWdf() / MountDirectory()  ──►  [Mount phase]
    │                                         │
    │         (repeat for each source)        │
    │◄─────────────────────────────────────── │
    │
    ├─ [Mount phase complete]
    │
    ├─ Exists() / Open() / ReadAll()  ──►  [Read phase]
    │         (concurrent-safe)
    │
    └─ [Destructor]  ──►  m_MountPoints cleared; archives closed
```

### IFileStream lifecycle

```
[Constructed by CVirtualFileSystem::Open()]
    │
    ├─ Read() / Seek() / Tell() / Size()   ──►  [In use]
    │
    └─ [Caller destroys unique_ptr]  ──►  [Destroyed]
                                            File handle / buffer released
```

---

## Normalization Rules (path → normalized form)

Normalization is applied in `CVirtualFileSystem::Normalize()` before any mount-point query.

| Step | Rule |
|---|---|
| 1 | Replace `\` with `/` |
| 2 | Lowercase entire string |
| 3 | Collapse `//` to `/` |
| 4 | Strip leading `/` |
| 5 | Strip trailing `/` |
| 6 | Reject segments containing `..` (return empty string) |
| 7 | Collapse `.` segments |
| 8 | Return empty string if result is empty → caller receives nullptr/false |

---

## Validation Rules

| Rule | Source |
|---|---|
| Mount path must not be empty | FR-002, FR-003 |
| Virtual path must not normalize to empty | FR-009, FR-016 |
| MountWdf path must resolve to an openable WDF file | FR-002 |
| MountDirectory path must exist on disk | FR-003 |
| ReadEntry failure (nullopt) → return nullptr stream | FR-016 |
| Seek offset > Size() → return false, leave position unchanged | IFileStream invariant |
| Read at EOF → return 0 bytes | IFileStream invariant |
