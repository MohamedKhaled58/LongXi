# Feature Specification: Texture System

**Feature Branch**: `007-texture-system`
**Created**: 2026-03-10
**Status**: Draft

---

## Clarifications

### Session 2026-03-10

- Q: How should `Texture` release its GPU resource — ComPtr RAII or explicit renderer callback? → A: ComPtr RAII. `Texture` stores `ComPtr<ID3D11ShaderResourceView>` and holds no renderer reference. When `Texture` destructs, the ComPtr releases the SRV, which transitively releases the underlying `ID3D11Texture2D` if no other references exist. `DestroyTexture()` is removed from the spec as unnecessary.
- Q: Should `Application` gain a `TextureManager` member and accessor as part of Spec 007? → A: Yes. Spec 007 adds `m_TextureManager`, `CreateTextureManager()`, and `GetTextureManager()` to `Application`, consistent with how VFS, Renderer, and InputSystem were integrated in previous specs.
- Q: Where is `RendererTextureHandle` defined — `TextureFormat.h` or `DX11Renderer.h`? → A: `DX11Renderer.h`. `TextureFormat.h` contains only the `TextureFormat` enum (no DX11 headers). `Texture.h` and `TextureManager.h` include both. This keeps DX11 includes contained within the renderer layer and avoids circular dependencies.

---

## Overview

The Texture System provides GPU-ready texture resources to all engine systems. It loads texture files from the Virtual File System (Spec 006), decodes them from DDS or TGA format, uploads them to the GPU through the renderer, and caches them so each unique texture is only loaded once.

This mirrors the original client's `c3_texture` module (`Texture_Load` / `C3Texture` struct) and the `C3DTexture` / `CMyBitmap` wrapper layer from `3DBaseCode`, modernized from D3D9 global arrays to a RAII-managed, reference-counted design.

### Scope (In)
- Loading DDS and TGA texture files from VFS
- Uploading decoded pixel data to the GPU as shader-accessible textures
- Path-keyed texture cache with reference-counted lifetime
- Clean API for rendering systems to request textures by virtual path
- GPU texture handle type (`RendererTextureHandle`) exposed by the renderer
- `TextureFormat` enum covering formats present in the original client's DDS assets

### Scope (Out)
- MSK alpha mask compositor (present in `CMyBitmap` reference; deferred to a later spec)
- Animated textures / sprite sheets (deferred)
- Render target / off-screen textures (separate responsibility)
- Texture streaming or async loading (Phase 2 concern)
- Mipmap generation policy (treated as implementation detail; DDS mipmaps are read from file)
- `3dtexture.ini` ID-to-path lookup table (used by the original client; not required for the foundation layer)

---

## Architecture Diagram

```
Rendering / UI / Gameplay Systems
            |
            | LoadTexture("texture/terrain/grass.dds")
            v
+----------------------------------+
|        TextureManager            |
|  m_Cache: map<path, shared_ptr>  |
+-----+----------+--------+--------+
      |          |        |
  Cache hit   Cache miss  ClearCache()
      |          |
      v          v
  Return        CVirtualFileSystem::ReadAll(path)
  cached            |
  texture           v
               TextureLoader::LoadDDS() or LoadTGA()
                       |
                       v  TextureData { pixels, width, height, format }
                       |
               DX11Renderer::CreateTexture()
                       |
                       v
               Texture { m_Handle, m_Width, m_Height, m_Format }
                       |
                       v
               shared_ptr<Texture> returned to caller
               + stored in m_Cache
```

---

## User Scenarios & Testing

### Scenario 1 — Texture Load from VFS (Priority: P1)

A rendering system requests a texture by virtual path. The Texture System loads it from the VFS, decodes it, uploads it to the GPU, and returns a usable handle.

**Acceptance**:
1. **Given** a DDS file is mounted in the VFS, **When** `LoadTexture("texture/terrain/grass.dds")` is called, **Then** a non-null `shared_ptr<Texture>` is returned with correct width, height, and format.
2. **Given** a TGA file is mounted in the VFS, **When** `LoadTexture("ui/button.tga")` is called, **Then** a non-null `shared_ptr<Texture>` is returned.
3. **Given** the file is in a WDF archive, **When** `LoadTexture` is called, **Then** the result is identical to loading from a loose file at the same path.

---

### Scenario 2 — Cache Hit (Priority: P2)

A second request for the same path returns the already-loaded texture without any disk or GPU operations.

**Acceptance**:
1. **Given** `LoadTexture("texture/terrain/grass.dds")` was called once, **When** called again with the same path, **Then** the returned pointer is equal to the first, and no re-decode or GPU upload occurs.
2. **Given** paths differ only in casing or slash direction (`Texture/Terrain/Grass.DDS` vs `texture/terrain/grass.dds`), **When** both are requested, **Then** only one GPU upload occurs and both return the same texture.

---

### Scenario 3 — Error Safety (Priority: P3)

Missing, unsupported, or corrupt texture files fail gracefully without crashing.

**Acceptance**:
1. **Given** the path does not exist in the VFS, **When** `LoadTexture` is called, **Then** it returns `nullptr` and logs an error.
2. **Given** the file data is corrupt (invalid DDS header), **When** `LoadTexture` is called, **Then** it returns `nullptr` and logs an error.
3. **Given** the format is unsupported, **When** `LoadTexture` is called, **Then** it returns `nullptr` and logs an error.

---

### Edge Cases

- Empty path → `LoadTexture` returns `nullptr` immediately; no VFS lookup attempted.
- Path normalizes to empty (only slashes/whitespace) → `nullptr` returned.
- DDS file with no pixel data (0-byte content section) → decode fails, returns `nullptr`.
- TGA file with 0 width or 0 height → decode fails, returns `nullptr`.
- `ClearCache()` called while callers hold a `shared_ptr<Texture>` → cache entry removed; the `Texture` object lives until all holders drop their reference.
- `LoadTexture` called after `ClearCache()` for a previously loaded path → reloads from VFS as a fresh load.

---

## Functional Requirements

- **FR-001**: The Texture System MUST expose a `TextureManager` class as the single entry point for all texture operations.
- **FR-002**: `TextureManager::LoadTexture(path)` MUST load a texture from the VFS, decode it, and upload it to the GPU if it is not already cached. If already cached, it MUST return the cached instance.
- **FR-003**: `TextureManager::GetTexture(path)` MUST return the cached texture for the given path, or `nullptr` if not yet loaded. It MUST NOT trigger a VFS load.
- **FR-004**: `TextureManager::ClearCache()` MUST release all internally held texture references. Callers holding `shared_ptr<Texture>` copies continue to own their instances until they drop them.
- **FR-005**: The Texture System MUST normalize all caller-provided paths using the same rules as CVirtualFileSystem before cache lookup and VFS access.
- **FR-006**: The Texture System MUST support DDS files including compressed variants (DXT1 / DXT3 / DXT5) and uncompressed RGBA8.
- **FR-007**: The Texture System MUST support TGA files (uncompressed RGBA8 or RGB24 decoded to RGBA8).
- **FR-008**: `TextureManager` MUST read file bytes through `CVirtualFileSystem::ReadAll()` exclusively — no direct filesystem access.
- **FR-009**: The Texture System MUST expose a `TextureFormat` enum covering: `RGBA8`, `DXT1`, `DXT3`, `DXT5`.
- **FR-010**: The Texture System MUST define a `TextureData` struct holding decoded pixel data (width, height, format, pixels) as an intermediate between decoding and GPU upload.
- **FR-011**: The Texture System MUST define a `TextureLoader` class with static methods `LoadDDS` and `LoadTGA` that populate `TextureData` from raw file bytes.
- **FR-012**: The Texture System MUST define a `Texture` class that holds a GPU texture handle, width, height, and format. The `Texture` object MUST own the GPU resource and release it on destruction.
- **FR-013**: `DX11Renderer` MUST expose a `CreateTexture(width, height, format, pixels) → RendererTextureHandle` method used exclusively by the Texture System for GPU upload.
- **FR-014**: GPU resource lifetime is managed by ComPtr RAII. `Texture` holds a `ComPtr<ID3D11ShaderResourceView>` member (`m_Handle`). When `Texture` is destroyed, the ComPtr destructor releases the SRV, which releases the underlying `ID3D11Texture2D` if no other references exist. `DX11Renderer` does NOT expose a `DestroyTexture` method — explicit teardown is unnecessary.
- **FR-015**: The Texture System MUST log a message for every successful texture load (first-time load), every cache hit, and every failure.
- **FR-016**: All failures (missing file, decode error, GPU creation error) MUST return `nullptr` from `LoadTexture` without crashing or throwing.
- **FR-017**: The cache MUST store entries keyed by normalized path (lowercase, forward-slash, no leading slash) so case-variant paths resolve to the same entry.
- **FR-018**: `Application` MUST own one `TextureManager` instance (`m_TextureManager`), initialize it via `CreateTextureManager()` after both `DX11Renderer` and `CVirtualFileSystem` are ready, expose it via `GetTextureManager()`, and reset it in `Shutdown()` before the renderer is torn down.

---

## Key Entities

### `TextureFormat` (enum)

Maps directly to DX11 DXGI format variants and the compressed formats present in the original client's DDS assets.

| Value | Source Format | Notes |
|---|---|---|
| `RGBA8` | Uncompressed TGA or DDS | 4 bytes/pixel; standard RGBA |
| `DXT1` | DDS BC1 | 4:1 compression; 1-bit alpha or no alpha |
| `DXT3` | DDS BC2 | 4:1 compression; explicit 4-bit alpha |
| `DXT5` | DDS BC3 | 4:1 compression; interpolated alpha |

---

### `TextureData` (struct)

Intermediate representation of decoded texture pixels, produced by `TextureLoader` and consumed by `DX11Renderer::CreateTexture()`. Not a GPU resource — pure CPU data.

```
struct TextureData
{
    uint32_t             Width;
    uint32_t             Height;
    TextureFormat        Format;
    std::vector<uint8_t> Pixels;   // row-major; for DXT formats: compressed blocks
};
```

Invariants:
- `Width > 0` and `Height > 0` on success
- `Pixels` is non-empty on success
- For DXT formats, `Pixels` contains the raw compressed block data as read from the DDS file

---

### `TextureLoader` (static utility class)

Stateless; all methods are static. Responsible only for decoding bytes → `TextureData`. Does not touch VFS or GPU.

```
class TextureLoader
{
public:
    // Decode a DDS file from raw bytes.
    // Supports: uncompressed RGBA8, DXT1, DXT3, DXT5.
    // Returns false and leaves `out` unmodified on any failure.
    static bool LoadDDS(const std::vector<uint8_t>& data, TextureData& out);

    // Decode a TGA file from raw bytes.
    // Decodes to RGBA8 regardless of source bit depth.
    // Returns false and leaves `out` unmodified on any failure.
    static bool LoadTGA(const std::vector<uint8_t>& data, TextureData& out);
};
```

**DDS parsing rules** (derived from reference client `D3DXCreateTextureFromFileInMemoryEx` behavior):
- Magic: bytes 0–3 must equal `0x44445320` ("DDS ")
- Header: standard `DDS_HEADER` at offset 4 (124 bytes)
- Pixel format FOURCC `DXT1` → `TextureFormat::DXT1`
- Pixel format FOURCC `DXT3` → `TextureFormat::DXT3`
- Pixel format FOURCC `DXT5` → `TextureFormat::DXT5`
- No FOURCC, `DDPF_RGB | DDPF_ALPHAPIXELS` with 32 bpp → `TextureFormat::RGBA8`
- Any other pixel format → decode fails (unsupported)
- Mipmaps: read level 0 only from the Pixels section; mipmap chain is ignored
- Pixel data begins at offset 4 + 124 = 128 for non-DXT10 headers

**TGA parsing rules**:
- Header: 18 bytes; image type 2 (uncompressed RGB) or 10 (RLE RGB)
- Bit depths 24-bit (RGB) and 32-bit (RGBA) supported
- 24-bit input: expand to RGBA8 by appending `0xFF` alpha per pixel
- RLE decompression required for image type 10
- Origin: bottom-left (standard TGA) → flip rows to top-left on decode
- Any other image type → decode fails

---

### `Texture` (class)

Represents a single GPU texture resource. Owns the GPU handle; the handle is released in the destructor.

```
class Texture
{
public:
    // Destructor is trivial — m_Handle (ComPtr) auto-releases the SRV on destruction,
    // which transitively releases the ID3D11Texture2D if no other references exist.
    ~Texture() = default;

    uint32_t              GetWidth()  const;
    uint32_t              GetHeight() const;
    TextureFormat         GetFormat() const;
    RendererTextureHandle GetHandle() const;

    // Not copyable — GPU resource ownership is exclusive to this instance
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

private:
    RendererTextureHandle m_Handle;  // ComPtr<ID3D11ShaderResourceView>; RAII lifetime
    uint32_t              m_Width;
    uint32_t              m_Height;
    TextureFormat         m_Format;
};
```

`Texture` objects are only created by `TextureManager`. Callers receive `shared_ptr<Texture>` and never construct `Texture` directly. The ownership chain is:

```
TextureManager
      │
      └─ shared_ptr<Texture>  (cache + returned to callers)
                │
                └─ Texture
                      │
                      └─ ComPtr<ID3D11ShaderResourceView>  (m_Handle)
                                │
                                └─ GPU resource lifetime
```

---

### `TextureManager` (class)

The primary entry point for all texture operations. Owned by `Application` (one instance per application lifetime).

```
class TextureManager
{
public:
    // Construct with references to the VFS and renderer needed for loading.
    TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer);
    ~TextureManager();

    // Load texture by virtual path. Returns cached instance on repeated calls.
    // Returns nullptr on any failure (missing file, decode error, GPU error).
    std::shared_ptr<Texture> LoadTexture(const std::string& path);

    // Return cached texture for path without triggering a load.
    // Returns nullptr if not in cache.
    std::shared_ptr<Texture> GetTexture(const std::string& path) const;

    // Release all cached texture entries.
    // Existing shared_ptr holders retain their Texture instances until released.
    void ClearCache();

private:
    std::string Normalize(const std::string& path) const;

    CVirtualFileSystem& m_Vfs;
    DX11Renderer&       m_Renderer;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Cache;
};
```

---

### `RendererTextureHandle` (type alias, defined in `DX11Renderer.h`)

Opaque handle to a GPU shader resource. Defined exclusively in `DX11Renderer.h` to keep DX11 headers (`<wrl/client.h>`, `<d3d11.h>`) isolated to the renderer layer. `TextureFormat.h` does NOT define or include this type.

```
// In DX11Renderer.h:
using RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;
```

The handle is a ComPtr — it auto-releases the SRV on destruction (RAII). `Texture.h` and `TextureManager.h` include `DX11Renderer.h` to access this type. Callers outside the texture system use `Texture::GetHandle()` only when binding a texture for rendering; they do not interact with `RendererTextureHandle` directly.

---

## Texture Loading Pipeline

```
LoadTexture(callerPath)
    │
    ├─ 1. Normalize(callerPath) → normalizedPath
    │         lowercase, forward-slash, no leading slash
    │
    ├─ 2. Cache lookup: m_Cache.find(normalizedPath)
    │         HIT  → log "[Texture] Cache hit: path" → return cached shared_ptr
    │         MISS → continue
    │
    ├─ 3. VFS read: m_Vfs.ReadAll(normalizedPath) → vector<uint8_t>
    │         empty result → log "[Texture] File not found: path" → return nullptr
    │
    ├─ 4. Detect format from file extension (lowercase suffix)
    │         ".dds" → TextureLoader::LoadDDS(data, textureData)
    │         ".tga" → TextureLoader::LoadTGA(data, textureData)
    │         other  → log "[Texture] Unsupported format: path" → return nullptr
    │
    ├─ 5. Decode: TextureLoader returns bool
    │         false → log "[Texture] Failed to decode: path" → return nullptr
    │
    ├─ 6. GPU upload: m_Renderer.CreateTexture(width, height, format, pixels)
    │         null handle → log "[Texture] GPU upload failed: path" → return nullptr
    │
    ├─ 7. Construct Texture object with handle, width, height, format
    │
    ├─ 8. Store in m_Cache[normalizedPath]
    │
    └─ 9. Log "[Texture] Loaded: path" → return shared_ptr<Texture>
```

---

## Format Support Rules

### DDS

DDS files are the primary format in the original client (all WDF-packaged textures use DDS). Support priority is highest.

| DDS Format | `TextureFormat` | Notes |
|---|---|---|
| RGBA8 (uncompressed, 32 bpp) | `RGBA8` | Header: `DDPF_RGB | DDPF_ALPHAPIXELS`, `RGBBitCount=32` |
| DXT1 (BC1) | `DXT1` | FOURCC `'DXT1'`; used for terrain, environment |
| DXT3 (BC2) | `DXT3` | FOURCC `'DXT3'`; used for UI with explicit alpha |
| DXT5 (BC3) | `DXT5` | FOURCC `'DXT5'`; used for characters, detailed alpha |
| Any other | Unsupported | Returns decode failure |

DX10 extended headers are NOT supported in this spec (not present in original client assets).

### TGA

TGA files are used for some UI elements and development assets. Decoded always to `RGBA8`.

| TGA Source | Decoded Format | Notes |
|---|---|---|
| 24-bit RGB | `RGBA8` | Alpha channel filled with `0xFF` |
| 32-bit RGBA | `RGBA8` | Direct copy |
| RLE-compressed | `RGBA8` | Decompressed before output |
| Indexed / other | Unsupported | Returns decode failure |

### MSK (Alpha Mask — Noted, Not Implemented)

The original client's `CMyBitmap` loads a `.msk` file alongside some textures for per-pixel transparency compositing. This is a separate alpha mask image, not a standard texture format. MSK support is deferred to a future spec (sprite/UI compositor layer). `TextureLoader` does not handle `.msk` files.

---

## VFS Integration

`TextureManager` holds a reference to `CVirtualFileSystem` (injected at construction). All file reads go through `CVirtualFileSystem::ReadAll(normalizedPath)`.

**Rationale:** This ensures textures are found whether they are in WDF archives or loose override directories, and that the patch priority order is respected — matching the original client behavior where `g_objDnFile.GetMPtr()` loaded from the DNP archive.

`TextureManager` never constructs path strings from disk roots or archive paths directly. The VFS layer handles all source resolution.

---

## Renderer Integration

### New API Added to DX11Renderer

One method is added to `DX11Renderer` to support GPU texture creation:

```
// Create a GPU shader resource from pixel data.
// pixels: row-major; for DXT formats, compressed block data.
// Returns a null (empty) ComPtr if creation fails.
// D3D11 creation pattern: CreateTexture2D → CreateShaderResourceView →
// release the Texture2D handle immediately (SRV holds the remaining reference).
RendererTextureHandle CreateTexture(
    uint32_t       width,
    uint32_t       height,
    TextureFormat  format,
    const void*    pixels
);
```

`DestroyTexture` is **not** part of the renderer API. GPU resource lifetime is managed entirely by ComPtr RAII on the `Texture` object (see FR-014).

**TextureFormat → DXGI format mapping** (informs `CreateTexture` implementation):

| `TextureFormat` | `DXGI_FORMAT` |
|---|---|
| `RGBA8` | `DXGI_FORMAT_R8G8B8A8_UNORM` |
| `DXT1` | `DXGI_FORMAT_BC1_UNORM` |
| `DXT3` | `DXGI_FORMAT_BC2_UNORM` |
| `DXT5` | `DXGI_FORMAT_BC3_UNORM` |

`TextureManager` calls `CreateTexture` once per unique texture path. The returned ComPtr is stored in the `Texture` object; it is released automatically when the `Texture` destructs.

---

## Caching Model

The cache is an `unordered_map<string, shared_ptr<Texture>>` keyed by normalized path.

**Lookup:** `GetTexture` and `LoadTexture` both normalize the path before map lookup. Case and slash differences are transparent to callers.

**Strong references:** The cache holds `shared_ptr<Texture>`. Returned handles are also `shared_ptr<Texture>` copies. A texture remains alive as long as either the cache OR any caller holds a reference.

**Eviction:** Textures are evicted only via `ClearCache()`, which calls `m_Cache.clear()`. If a caller holds a `shared_ptr<Texture>` at that point, the `Texture` object stays alive until the caller drops it — the GPU resource is not released prematurely.

**Reference model comparison to original client:**

| Original (`c3_texture`) | This spec |
|---|---|
| `nDupCount` manual ref count | `shared_ptr<Texture>` automatic ref count |
| Global `g_lpTex[10240]` array | `unordered_map<string, shared_ptr<Texture>>` |
| `Texture_Unload()` explicit decrement | Destructor auto-release via RAII |
| `stricmp` case-insensitive compare | Normalized path key (always lowercase) |
| `Texture_Load` is get-or-load | `LoadTexture` is get-or-load |

---

## Error Model

All failures return `nullptr` from `LoadTexture`. No exceptions propagate to callers.

| Failure Condition | Return | Log |
|---|---|---|
| Empty or invalid path | `nullptr` | `[Texture] WARN LoadTexture called with empty path` |
| VFS returns empty bytes | `nullptr` | `[Texture] ERROR File not found: <path>` |
| Unknown file extension | `nullptr` | `[Texture] ERROR Unsupported format: <path>` |
| DDS decode failure (bad magic) | `nullptr` | `[Texture] ERROR Failed to decode: <path>` |
| DDS unsupported pixel format | `nullptr` | `[Texture] ERROR Unsupported DDS format in: <path>` |
| TGA decode failure | `nullptr` | `[Texture] ERROR Failed to decode: <path>` |
| GPU CreateTexture returns null | `nullptr` | `[Texture] ERROR GPU upload failed: <path>` |

`GetTexture` never logs on cache miss — a miss is not an error.

---

## Logging Model

All log output uses `LX_ENGINE_INFO`, `LX_ENGINE_WARN`, `LX_ENGINE_ERROR` macros (LXEngine module). Every message is prefixed `[Texture]`.

| Event | Level | Example |
|---|---|---|
| First-time texture loaded | INFO | `[Texture] Loaded: texture/terrain/grass.dds (512x512 DXT5)` |
| Cache hit | INFO | `[Texture] Cache hit: texture/terrain/grass.dds` |
| File not found | ERROR | `[Texture] File not found: texture/terrain/missing.dds` |
| Unsupported format | ERROR | `[Texture] Unsupported format: texture/weird.bmp` |
| Decode failure | ERROR | `[Texture] Failed to decode: ui/button.tga` |
| GPU upload failure | ERROR | `[Texture] GPU upload failed: texture/terrain/grass.dds` |
| Empty path | WARN | `[Texture] LoadTexture called with empty path` |
| ClearCache called | INFO | `[Texture] Cache cleared (<N> entries released)` |

---

## Module Placement

The Texture System belongs in **LXEngine** (`LongXi/LXEngine/Src/Texture/`).

**Rationale:**
- `TextureManager` depends on both `CVirtualFileSystem` (LXCore) and `DX11Renderer` (LXEngine)
- LXEngine already contains `DX11Renderer`; texture management is an engine-level concern
- `LXCore → LXEngine` dependency direction is preserved; texture types do not belong in LXCore

**Proposed file layout:**

```
LongXi/LXEngine/Src/Texture/
├── TextureFormat.h          ← TextureFormat enum ONLY (no DX11 headers)
├── TextureData.h            ← TextureData struct (includes TextureFormat.h)
├── TextureLoader.h/.cpp     ← TextureLoader static class (DDS + TGA decode)
├── Texture.h/.cpp           ← Texture class; includes TextureFormat.h + DX11Renderer.h
└── TextureManager.h/.cpp    ← TextureManager class; includes Texture.h + DX11Renderer.h

LongXi/LXEngine/Src/Renderer/
└── DX11Renderer.h           ← defines RendererTextureHandle alias + CreateTexture()
```

**Include rule**: `RendererTextureHandle` is defined exclusively in `DX11Renderer.h`. `TextureFormat.h` has no DX11 dependencies — it is a pure enum header safe to include anywhere. Headers that need `RendererTextureHandle` (`Texture.h`, `TextureManager.h`) include `DX11Renderer.h` directly.

`DX11Renderer.h/.cpp` gains `CreateTexture` only (no `DestroyTexture` — see FR-014).

`TextureManager` is exposed through `LXEngine.h`.

### Application Integration

`Application` gains the following as part of Spec 007, consistent with how every prior subsystem was integrated:

```
// Application.h additions:
class TextureManager;

// new private member:
std::unique_ptr<TextureManager> m_TextureManager;

// new public accessor:
const TextureManager& GetTextureManager() const;

// new private factory:
bool CreateTextureManager();
```

`CreateTextureManager()` is called in `Application::Initialize()` after both `DX11Renderer` and `CVirtualFileSystem` are ready (both are required dependencies). `m_TextureManager` is reset in `Application::Shutdown()` before the renderer is torn down, ensuring all GPU resources are released while the device is still valid.

---

## Reference Notes (from c3_texture analysis)

These notes document what was confirmed from the original client source and how it informs this spec.

| Original | Confidence | New Design Decision |
|---|---|---|
| `D3DXCreateTextureFromFileInMemoryEx` auto-detects DDS/TGA | Confirmed | We parse DDS/TGA manually using standard headers; extension determines parser |
| DXT1/DXT3/DXT5 formats present in WDF DDS assets | Confirmed | `TextureFormat` includes all three; DXT10 headers absent |
| Global `g_lpTex[10240]` array cache with `nDupCount` | Confirmed | Replaced by `unordered_map` + `shared_ptr` |
| `g_objDnFile.GetMPtr()` loads from DNP archive | Confirmed | CVirtualFileSystem::ReadAll replaces DnFile |
| `CMyBitmap` loads `.dds` + optional `.msk` alpha mask | Confirmed | MSK deferred; TextureLoader handles DDS and TGA only |
| Cache lookup uses `stricmp` (case-insensitive) | Confirmed | Path normalization to lowercase handles this |
| Textures pooled at `g_lpTex` slot; slot ID returned | Confirmed | We return `shared_ptr<Texture>` instead; no slot ID exposed |
| `D3DPOOL_MANAGED` default pool | Confirmed | DX11 equivalent: `D3D11_USAGE_DEFAULT` + `D3D11_BIND_SHADER_RESOURCE` |

---

## Success Criteria

- **SC-001**: All engine systems access textures exclusively through `TextureManager::LoadTexture` — no direct VFS or renderer texture creation calls from outside the texture system.
- **SC-002**: A texture requested twice returns the identical `shared_ptr` target in 100% of same-path requests (cache confirmed working).
- **SC-003**: A missing texture request returns `nullptr` in 100% of cases without crash or exception.
- **SC-004**: DXT1, DXT3, DXT5, and uncompressed DDS files from the original client's WDF archives all decode and upload successfully.
- **SC-005**: TGA files (24-bit and 32-bit) decode correctly to RGBA8 textures.
- **SC-006**: Every load, cache hit, and failure produces a log entry visible in the engine log.

---

## Assumptions

- DDS files in the original client's WDF archives do not use DX10 extended headers. `DDS_HEADER_DXT10` is not parsed.
- TGA files use image types 2 (uncompressed) or 10 (RLE). Indexed-color TGA (type 1) is not present in client assets.
- Mipmaps in DDS files: only level 0 is read and uploaded. GPU mipmap generation and multi-level upload are deferred.
- `CVirtualFileSystem::ReadAll()` returns an empty vector on failure (consistent with Spec 006 contract).
- `DX11Renderer::CreateTexture` has exclusive access to the DX11 device; no concurrent GPU creation from multiple threads. This is safe under Phase 1 single-threaded runtime.
- `TextureManager` is initialized after both `CVirtualFileSystem` and `DX11Renderer` are ready.

---

## Dependencies

- **Spec 006 (VFS)**: `CVirtualFileSystem::ReadAll()` for file access.
- **LXEngine / DX11Renderer**: Extended with `CreateTexture` / `DestroyTexture` in this spec.
- **LXCore Logging**: `LX_ENGINE_*` macros for all log output.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
