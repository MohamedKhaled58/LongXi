# Contract: CVirtualFileSystem Public API

**Module**: LXCore
**Header**: `LongXi/LXCore/Src/Core/FileSystem/VirtualFileSystem.h`
**Public entry**: `LongXi/LXCore/LXCore.h`
**Date**: 2026-03-10

---

## CVirtualFileSystem

The sole public entry point for resource access in the engine. Owned by `Application` and accessed via `Application::GetVirtualFileSystem()`.

### Mounting

```cpp
bool MountWdf(const std::string& path);
```
- `path`: Absolute or root-relative path to a `.wdf` file.
- Returns `true` if the archive was opened and indexed successfully.
- Returns `false` and logs `[VFS] ERROR` if the file is missing or the archive is invalid.
- Must be called before any `Exists`/`Open`/`ReadAll` operations for the archive to be searched.
- Must be called from the main thread only.

```cpp
bool MountDirectory(const std::string& path);
```
- `path`: Absolute path to a directory on disk.
- Returns `true` if the directory exists.
- Returns `false` and logs `[VFS] ERROR` if the directory is not found.
- Must be called from the main thread only.

**Mount order** defines lookup priority. First mounted = highest priority.

---

### Querying

```cpp
bool Exists(const std::string& path) const;
```
- Returns `true` if any mounted source contains a resource at the given virtual path.
- Normalizes `path` before lookup.
- Returns `false` silently for missing resources (no log).
- Safe to call from multiple threads after all mounts are complete.

---

### Reading

```cpp
std::unique_ptr<IFileStream> Open(const std::string& path) const;
```
- Returns a valid `IFileStream` if a match is found in any mounted source.
- Returns `nullptr` if no source contains the resource; logs `[VFS] WARN`.
- Returns `nullptr` for empty or invalid paths; logs `[VFS] WARN`.
- Caller owns the returned stream.
- Safe to call from multiple threads after all mounts are complete.

```cpp
std::vector<uint8_t> ReadAll(const std::string& path) const;
```
- Convenience wrapper: opens the resource and reads all bytes into a vector.
- Returns an empty vector if the resource is not found (consistent with `Open` returning nullptr).
- Safe to call from multiple threads after all mounts are complete.

---

## IFileStream

Abstract read/seek interface. Returned exclusively by `CVirtualFileSystem::Open()`. Not constructible directly by callers.

```cpp
virtual size_t Read(void* buffer, size_t size) = 0;
```
- Reads up to `size` bytes into `buffer` starting at the current position.
- Advances the position by the number of bytes read.
- Returns bytes actually read. Returns `0` at end-of-stream (not an error).
- Behavior is undefined if `buffer` is null or `buffer` is smaller than `size`.

```cpp
virtual bool Seek(size_t offset) = 0;
```
- Sets the current read position to `offset` bytes from the start of the stream.
- Returns `true` on success.
- Returns `false` if `offset > Size()`. Position is unchanged on failure.
- `Seek(0)` always succeeds.

```cpp
virtual size_t Tell() const = 0;
```
- Returns the current byte position. Always in range `[0, Size()]`.

```cpp
virtual size_t Size() const = 0;
```
- Returns the total size of the resource in bytes. Does not change after construction.

---

## Application Integration

`Application` exposes the VFS via:

```cpp
const CVirtualFileSystem& Application::GetVirtualFileSystem() const;
```

Replaces the former `GetResourceSystem()`.

---

## Error Behavior Summary

| Caller action | Result when resource missing | Log |
|---|---|---|
| `Exists("path")` | `false` | None |
| `Open("path")` | `nullptr` | `[VFS] WARN Resource not found: path` |
| `ReadAll("path")` | `{}` | (from Open) |
| `MountWdf("bad.wdf")` | `false` | `[VFS] ERROR Failed to mount WDF: bad.wdf` |
| `MountDirectory("bad/")` | `false` | `[VFS] ERROR Failed to mount directory: bad/` |

---

## Usage Example

```cpp
// Initialization (Application startup)
CVirtualFileSystem vfs;
vfs.MountDirectory("Data/Patch");      // highest priority — patch overrides
vfs.MountDirectory("Data/Dev");        // dev overrides (development builds)
vfs.MountWdf("Data/C3.wdf");          // baseline game archive
vfs.MountWdf("Data/data.wdf");        // secondary archive

// Resource access (engine systems)
if (vfs.Exists("texture/terrain/grass.dds"))
{
    auto stream = vfs.Open("texture/terrain/grass.dds");
    // stream is unique_ptr<IFileStream>; use Read/Seek/Tell/Size
}

// Whole-file read
std::vector<uint8_t> bytes = vfs.ReadAll("ini/itemtype.dat");
if (!bytes.empty())
{
    // process bytes
}
```
