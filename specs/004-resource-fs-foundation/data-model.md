# Data Model: Resource and Filesystem Foundation

**Branch**: `004-resource-fs-foundation`
**Source**: Derived from `spec.md` requirements + `research.md` findings

---

## Key Entities

### `ResourceSystem` Class

Central engine object in `LXCore` responsible for root registration, path normalization, path resolution, existence queries, and whole-file byte reads.

#### State Fields

| Field | Type | Description |
|-------|------|-------------|
| `m_Roots` | `std::vector<std::string>` | Registered resource root directories (absolute paths, forward-slash normalized). Searched in order; first match wins. |

#### Lifecycle Methods

| Method | Called By | Description |
|--------|-----------|-------------|
| `Initialize(const std::vector<std::string>& roots)` | `Application::CreateResourceSystem()` | Registers and validates root paths. Logs each registered root. Logs a warning for any root that does not exist on disk. |
| `Shutdown()` | `Application::Shutdown()` | Clears root registrations. Logs shutdown. |

#### Public Query Methods

| Method | Signature | Returns | Notes |
|--------|-----------|---------|-------|
| `Exists` | `bool Exists(const std::string& relativePath) const` | `true` if the path resolves to a readable file under any root | Does not open the file |
| `ReadFile` | `std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const` | Engaged optional with file bytes on success; `std::nullopt` on any failure | Caller owns the returned buffer |

#### Private Implementation Methods

| Method | Description |
|--------|-------------|
| `Normalize(const std::string& path) const` | Applies normalization rules (see below); returns empty string if path is rejected (e.g., traversal) |
| `Resolve(const std::string& normalizedPath) const` | Iterates `m_Roots`, concatenates each with the normalized path, checks existence via `std::filesystem::is_regular_file`; returns first absolute match as `std::string`, or `""` if none found |

---

## Path Normalization Rules

Applied by `Normalize()` in this order:

1. Trim leading and trailing ASCII whitespace
2. Replace all `\` with `/`
3. Collapse consecutive `//` into single `/`
4. Strip a single leading `/` if present (ensure relative)
5. Split on `/`; **reject** (return `""`) if any segment equals `..` — log `LX_CORE_WARN`
6. Collapse or ignore `.` segments (no-op navigation)
7. Strip trailing `/`
8. Return normalized relative path string

**Examples**:

| Input | Normalized Output | Notes |
|-------|-------------------|-------|
| `"textures/hero.dds"` | `"textures/hero.dds"` | Clean — no change |
| `"textures\\hero.dds"` | `"textures/hero.dds"` | Backslash replaced |
| `"//textures//hero.dds"` | `"textures/hero.dds"` | Leading slash + double slash collapsed |
| `"  textures/hero.dds  "` | `"textures/hero.dds"` | Whitespace trimmed |
| `"textures/../hero.dds"` | `""` (rejected) | Traversal rejected |
| `"./textures/hero.dds"` | `"textures/hero.dds"` | Single-dot collapsed |
| `""` | `""` (rejected) | Empty path rejected |

---

## Resolution Contract

`Resolve(normalizedPath)` searches `m_Roots` in registration order:

```
For each root in m_Roots:
    candidate = root + "/" + normalizedPath
    if std::filesystem::is_regular_file(ToWide(candidate)):
        return candidate
return ""
```

The first root that contains a matching regular file wins. If no root matches, resolution returns `""` and the caller (`ReadFile` or `Exists`) handles the miss.

**Archive extension boundary**: When archive-backed sources are added (future spec), `Resolve()` is the integration point. An archive source would be consulted after directory roots, or interleaved based on registration priority. The caller-facing `ReadFile` signature does not change.

---

## Return Type Contract

### `std::optional<std::vector<uint8_t>>`

| Condition | Return Value |
|-----------|-------------|
| File found and read successfully | `std::optional` containing `std::vector<uint8_t>` with file bytes |
| File found but zero bytes | `std::optional` containing empty `std::vector<uint8_t>` (not `nullopt`) |
| Path normalization rejected (e.g., `..`) | `std::nullopt` + `LX_CORE_WARN` log |
| Path not found in any root | `std::nullopt` + `LX_CORE_ERROR` log |
| File exists but cannot be opened (permission denied, etc.) | `std::nullopt` + `LX_CORE_ERROR` log |

---

## Application Integration Points

### `Application.h` changes

```
+ class ResourceSystem;  // forward declaration
+ std::unique_ptr<ResourceSystem> m_ResourceSystem;
+ const ResourceSystem& GetResourceSystem() const;
+ bool CreateResourceSystem();  // private
```

### `Application.cpp` changes

```
Initialize():
  + CreateResourceSystem()  ← called after CreateInputSystem()

CreateResourceSystem():
  + determine exe directory via GetModuleFileNameW
  + register exe_dir + "/Data" as primary root (if it exists)
  + register exe_dir as fallback root
  + call m_ResourceSystem->Initialize(roots)
  + log registered roots

Shutdown():
  + m_ResourceSystem->Shutdown()
  + m_ResourceSystem.reset()
  ← called before InputSystem and Renderer shutdown
```

---

## Diagnostic Log Contract

| Event | Level | Message format |
|-------|-------|----------------|
| Root registered (exists) | INFO | `"ResourceSystem: registered root '{}'"`  |
| Root registered (does not exist on disk) | WARN | `"ResourceSystem: root '{}' does not exist — no files will resolve from it"` |
| Path rejected (traversal) | WARN | `"ResourceSystem: rejected path with traversal sequence: '{}'"`  |
| Path not found | ERROR | `"ResourceSystem: file not found: '{}'"`  |
| File open failed | ERROR | `"ResourceSystem: failed to open: '{}'"` |
| Initialized | INFO | `"Resource system initialized ({} root(s))"` |
| Shutdown | INFO | `"Resource system shutdown"` |

---

## Constraints and Invariants

- `Initialize()` MUST be called before any `Exists()` or `ReadFile()` call
- Roots are fixed after `Initialize()`; no root changes during the session (this spec)
- All path input to public methods is normalized internally — callers need not pre-normalize
- `ToWide()` (file-scope static in `ResourceSystem.cpp`) is the only point where `std::string` → `std::wstring` conversion occurs; it does not appear in the public header
- `std::filesystem` headers are included only in `ResourceSystem.cpp`, not in `ResourceSystem.h`, to minimize header inclusion weight
- `ResourceSystem` has no dependency on `DX11Renderer`, `InputSystem`, or any gameplay type
