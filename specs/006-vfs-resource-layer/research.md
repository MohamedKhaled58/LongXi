# Research: Virtual File System (VFS) & Resource Access Layer

**Branch**: `006-vfs-resource-layer`
**Date**: 2026-03-10
**Status**: Complete — no NEEDS CLARIFICATION items remain

---

## Finding 1 — Existing Foundation in LXCore

**Decision**: CVirtualFileSystem will be implemented in `LXCore/Src/Core/FileSystem/`, alongside existing `WdfArchive` and `ResourceSystem`.

**Rationale**: Both `WdfArchive` and `ResourceSystem` already live in LXCore. The `LXEngine → LXCore` dependency direction means all engine layers can use LXCore types. Resource access is a low-level foundation concern consistent with LXCore's role.

**Evidence from codebase**:
- `LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.h/.cpp` — whole-file resource reader
- `LongXi/LXCore/Src/Core/FileSystem/WdfArchive.h/.cpp` — WDF archive reader

**Alternatives considered**: Placing VFS in LXEngine. Rejected because LXCore already hosts both the archive reader and the resource system, and placing VFS in LXCore avoids a dependency inversion.

---

## Finding 2 — WdfArchive API (Confirmed from Source)

**Decision**: VFS calls `WdfArchive::HasEntry(normalizedPath)` and `WdfArchive::ReadEntry(normalizedPath)`. The VFS passes the **full normalized path** (e.g., `texture/terrain/grass.dds`). UID computation is internal to WdfArchive.

**Evidence**:
```cpp
bool WdfArchive::HasEntry(const std::string& normalizedPath) const;
std::optional<std::vector<uint8_t>> WdfArchive::ReadEntry(const std::string& normalizedPath) const;
```

Both methods compute the UID internally by calling `ComputeUid(normalizedPath)` which runs `NormalizeAndHash()` on the full path string. The VFS never handles UIDs directly.

**Important**: `WdfArchive` uses **binary search** (`std::lower_bound`) on the sorted in-memory index for O(log n) lookup. Index is loaded once at `Open()` time and cached in `m_Index`.

**UID Hash**: Custom 32-bit hash (`stringtoid`) reimplemented from x86 assembly in the reference client. Status: `[Adopted — verify]` — must be tested against real WDF archives.

---

## Finding 3 — ResourceSystem Relationship and Migration

**Decision**: `CVirtualFileSystem` supersedes `ResourceSystem`. `ResourceSystem` will be retained as a deprecated class until it can be safely removed. `Application` will be updated to own `CVirtualFileSystem` instead of `ResourceSystem`.

**Evidence**: `Application.cpp` creates `ResourceSystem` in `CreateResourceSystem()`, owns it as `m_ResourceSystem`, and exposes it via `GetResourceSystem()`. All callers go through `Application`.

**Migration path**:
- `Application::CreateResourceSystem()` → `Application::CreateVirtualFileSystem()`
- `Application::m_ResourceSystem` → `Application::m_VirtualFileSystem`
- `Application::GetResourceSystem()` → `Application::GetVirtualFileSystem()`
- `ResourceSystem` stays on disk but is no longer called from Application

**Alternatives considered**: Adding `Open()` and `IFileStream` directly to `ResourceSystem`. Rejected because the spec defines `CVirtualFileSystem` as the target class and the formal mount-point model is a cleaner redesign.

---

## Finding 4 — ResourceSystem::Exists() Gap

**Decision**: `CVirtualFileSystem::Exists()` MUST check both directory mount points AND WDF mount points. This fixes a known gap in the existing `ResourceSystem::Exists()`.

**Evidence**: `ResourceSystem::Exists()` calls `Resolve()` which only iterates `m_Roots` (loose dirs), never `m_Archives`. Archive entries are invisible to `Exists()`. `CVirtualFileSystem` fixes this by having each `IMountPoint::Exists()` cover its own source.

---

## Finding 5 — Premake5 Auto-Glob

**Decision**: No premake5.lua changes required for new VFS source files.

**Evidence**: `LXCore/premake5.lua` uses:
```lua
files { "Src/**.h", "Src/**.cpp" }
```
Any `.h` or `.cpp` added under `LXCore/Src/` is automatically included in the build. Only `LXCore.h` needs manual updating to expose new public types.

---

## Finding 6 — LXCore.h Currently Exposes Only Logging

**Decision**: `LXCore.h` MUST be updated to include the new VFS public entry point header after implementation.

**Evidence**:
```cpp
// LXCore.h — current content
#pragma once
#include "Core/Logging/Log.h"
#include "Core/Logging/LogMacros.h"
```

`ResourceSystem.h` and `WdfArchive.h` are NOT currently exposed through `LXCore.h`. The new `VirtualFileSystem.h` MUST be added to `LXCore.h` to satisfy the public-entry-point rule from the constitution.

---

## Finding 7 — CFileDiskStream Uses std::ifstream

**Decision**: `CFileDiskStream` wraps `std::ifstream` with UTF-8 → Wide conversion for Win32 compatibility, consistent with the pattern in `ResourceSystem.cpp` and `WdfArchive.cpp`.

**Evidence**: Both existing files use `static std::wstring ToWide(const std::string& utf8)` and pass it to `std::ifstream(ToWide(...))`. `CFileDiskStream` must replicate this pattern to handle non-ASCII paths.

---

## Finding 8 — Application Architecture Impact

**Decision**: `Application` exposes `GetVirtualFileSystem() const → const CVirtualFileSystem&`. This is a breaking change to `Application`'s public interface. The `Application.h` forward declaration and member type change from `ResourceSystem` to `CVirtualFileSystem`.

**Evidence**: `Application.h` currently declares `const ResourceSystem& GetResourceSystem() const`. No LXEngine or LXShell code currently calls `GetResourceSystem()` outside of `Application.cpp` itself (only `CreateResourceSystem` configures it). Safe to rename.

---

## Finding 9 — No Unit Test Framework

**Decision**: Verification will be performed via compile-time checks (build succeeds) and a targeted integration test in LXShell that mounts a test directory and reads a known resource.

**Rationale**: Phase 1 has no unit testing framework. Manual integration testing is the standard verification approach for this project.

---

## Summary: No NEEDS CLARIFICATION Items

All technical questions are resolved by direct codebase inspection:

| Question | Answer |
|---|---|
| WdfArchive API shape? | `HasEntry(fullPath)` / `ReadEntry(fullPath)` — confirmed from source |
| UID computation? | Internal to WdfArchive; VFS passes full normalized path |
| Module placement? | LXCore — consistent with existing WdfArchive + ResourceSystem |
| Premake changes needed? | No — glob auto-picks up new files |
| LXCore.h changes needed? | Yes — add VirtualFileSystem.h include |
| CFileDiskStream file access? | std::ifstream with ToWide() conversion — matches existing pattern |
| ResourceSystem relationship? | CVirtualFileSystem supersedes it; Application migrates to new class |
| Exists() gap? | CVirtualFileSystem::Exists() checks all mount types including archives |
