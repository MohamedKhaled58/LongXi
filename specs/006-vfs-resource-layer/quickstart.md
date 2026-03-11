# Quickstart: VFS Implementation Guide

**Branch**: `006-vfs-resource-layer`
**Date**: 2026-03-10
**Audience**: Implementer working on Spec 006

---

## What Gets Built

Five new files in `LongXi/LXCore/Src/Core/FileSystem/`:

```
FileStream.h / FileStream.cpp       → IFileStream + CFileDiskStream + CFileWdfStream
MountPoint.h / MountPoint.cpp       → IMountPoint + CDirectoryMountPoint + CWdfMountPoint
VirtualFileSystem.h / VirtualFileSystem.cpp  → CVirtualFileSystem
```

Two existing files updated:
- `LongXi/LXCore/LXCore.h` — expose `VirtualFileSystem.h`
- `LongXi/LXEngine/Src/Application/Application.h/.cpp` — migrate to CVirtualFileSystem

---

## Build Notes

No premake5.lua changes needed. `LXCore/premake5.lua` uses `Src/**.h` and `Src/**.cpp` glob — all new files are auto-included.

After adding files, run `Win-Generate Project.bat` to regenerate the solution if needed by VS. A rebuild (`Win-Build Project.bat`) is sufficient to pick up new sources.

---

## Implementation Order

Build in this order to avoid compile-time dependencies on incomplete types:

### Step 1 — IFileStream + Stream Implementations (`FileStream.h/.cpp`)

Implement `IFileStream` (pure virtual), `CFileDiskStream`, and `CFileWdfStream`. No VFS or mount-point dependencies. Can be compiled and tested independently.

**`CFileDiskStream`** — model on existing `ResourceSystem::ReadFile()` pattern:
- UTF-8 → Wide conversion via `ToWide()` static helper (can be a private static or a shared header utility)
- `std::ifstream` opened in `std::ios::binary | std::ios::ate`
- Cache `m_Size` from `tellg()` at open; seek back to 0
- `Read`: call `m_File.read`; return `gcount()`
- `Seek`: call `m_File.seekg(offset)`; return success
- `Tell`: return `m_Position` (tracked manually)
- `Size`: return `m_Size`

**`CFileWdfStream`** — simple buffer wrapper:
- Constructor takes `std::vector<uint8_t>` by move
- `Read`: `memcpy` from `m_Data[m_Position]`; advance `m_Position`
- `Seek`: bounds-check then assign `m_Position`
- `Tell`: return `m_Position`
- `Size`: return `m_Data.size()`

---

### Step 2 — IMountPoint + Mount Implementations (`MountPoint.h/.cpp`)

Implement `IMountPoint` (pure virtual), `CDirectoryMountPoint`, `CWdfMountPoint`. Depends on `FileStream.h` and `WdfArchive.h`.

**`CDirectoryMountPoint`**:
- Constructor: store root path (strip trailing slash)
- `Exists`: `std::filesystem::is_regular_file(ToWide(m_Root + "/" + normalizedPath))`
- `Open`: same check; if found, return `make_unique<CFileDiskStream>(absPath)`

**`CWdfMountPoint`**:
- Constructor: takes `unique_ptr<WdfArchive>` by move
- `Exists`: `return m_Archive->HasEntry(normalizedPath)`
- `Open`: call `m_Archive->ReadEntry(normalizedPath)`; check `has_value()`; if yes, `make_unique<CFileWdfStream>(move(*result))`

---

### Step 3 — CVirtualFileSystem (`VirtualFileSystem.h/.cpp`)

Main class. Depends on `MountPoint.h`.

**`MountDirectory(path)`**:
1. Normalize the directory path (strip trailing slash, forward slashes)
2. Check existence: `std::filesystem::is_directory(ToWide(path))`
3. If not found: `LX_CORE_ERROR(...)`, return `false`
4. `m_MountPoints.push_back(make_unique<CDirectoryMountPoint>(path))`
5. `LX_CORE_INFO(...)`, return `true`

**`MountWdf(path)`**:
1. Create `auto archive = make_unique<WdfArchive>()`
2. `archive->Open(path)` — WdfArchive logs its own errors on failure
3. If fails: `LX_CORE_ERROR("[VFS] Failed to mount WDF: {}", path)`, return `false`
4. `m_MountPoints.push_back(make_unique<CWdfMountPoint>(move(archive)))`
5. `LX_CORE_INFO("[VFS] Mounted WDF archive: {}", path)`, return `true`

**`Normalize(path)`** — implement the 8-step algorithm from `data-model.md`. Note: `ResourceSystem::Normalize()` already implements steps 1–7. Consider extracting it into a shared helper or duplicating the logic (acceptable given no unit test framework yet).

**`Exists(path)`**:
```
normalized = Normalize(path)
if empty: return false
for each mp in m_MountPoints:
    if mp->Exists(normalized): return true
return false
```

**`Open(path)`**:
```
normalized = Normalize(path)
if empty: LOG_WARN(...); return nullptr
for each mp in m_MountPoints:
    if mp->Exists(normalized):
        stream = mp->Open(normalized)
        if stream: return stream
LOG_WARN("[VFS] Resource not found: {}", normalized)
return nullptr
```

**`ReadAll(path)`**:
```
stream = Open(path)
if !stream: return {}
vector<uint8_t> buf(stream->Size())
stream->Read(buf.data(), buf.size())
return buf
```

---

### Step 4 — Update LXCore.h

Add include for `VirtualFileSystem.h`:
```cpp
// LXCore.h
#pragma once
#include "Core/Logging/Log.h"
#include "Core/Logging/LogMacros.h"
#include "Core/FileSystem/VirtualFileSystem.h"
```

---

### Step 5 — Migrate Application

In `Application.h`:
- Remove `class ResourceSystem;` forward declaration
- Add `class CVirtualFileSystem;`
- Change `m_ResourceSystem: unique_ptr<ResourceSystem>` → `m_VirtualFileSystem: unique_ptr<CVirtualFileSystem>`
- Rename `GetResourceSystem()` → `GetVirtualFileSystem()` returning `const CVirtualFileSystem&`

In `Application.cpp`:
- Replace `#include "Core/FileSystem/ResourceSystem.h"` with `#include "Core/FileSystem/VirtualFileSystem.h"`
- Rename `CreateResourceSystem()` → `CreateVirtualFileSystem()`
- Replace `ResourceSystem` creation logic with `CVirtualFileSystem`:

```cpp
bool Application::CreateVirtualFileSystem()
{
    std::string exeDir = ResourceSystem::GetExecutableDirectory();   // keep using this helper

    m_VirtualFileSystem = std::make_unique<CVirtualFileSystem>();

    // Mount patch/dev directories first (highest priority)
    if (!exeDir.empty())
    {
        m_VirtualFileSystem->MountDirectory(exeDir + "/Data/Patch");
        m_VirtualFileSystem->MountDirectory(exeDir + "/Data");
        m_VirtualFileSystem->MountDirectory(exeDir);
    }

    // Mount baseline archives (lower priority)
    if (!exeDir.empty())
    {
        m_VirtualFileSystem->MountWdf(exeDir + "/Data/C3.wdf");
        m_VirtualFileSystem->MountWdf(exeDir + "/Data/data.wdf");
    }

    return true;
}
```

Note: `ResourceSystem::GetExecutableDirectory()` is a useful static utility. Either keep `ResourceSystem.h` for this helper, or move `GetExecutableDirectory()` into a platform utilities header.

---

## Verification Checklist

After implementation, verify:

- [ ] Project builds without errors (`Win-Build Project.bat`)
- [ ] `CVirtualFileSystem::MountDirectory(validPath)` returns `true` and logs INFO
- [ ] `CVirtualFileSystem::MountDirectory(invalidPath)` returns `false` and logs ERROR
- [ ] `CVirtualFileSystem::Exists("known-resource")` returns `true` when the file is in a mounted dir
- [ ] `CVirtualFileSystem::Open("known-resource")` returns a non-null stream
- [ ] `IFileStream::Read` returns correct bytes
- [ ] `IFileStream::Seek` and `Tell` work correctly
- [ ] `CVirtualFileSystem::Open("nonexistent")` returns `nullptr` and logs WARN
- [ ] Directory mount overrides WDF archive when mounted first
- [ ] `CVirtualFileSystem::Exists` correctly finds resources in WDF archives (fixes ResourceSystem gap)
- [ ] Application starts and logs VFS mount messages correctly

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
