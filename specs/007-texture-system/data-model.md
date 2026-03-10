# Data Model: Texture System

**Branch**: `007-texture-system`
**Date**: 2026-03-10

---

## Entity Map

```
Application
│
└── m_TextureManager: unique_ptr<TextureManager>
          │
          ├── m_Vfs:       CVirtualFileSystem&   (reference, not owned)
          ├── m_Renderer:  DX11Renderer&          (reference, not owned)
          └── m_Cache:     unordered_map<string, shared_ptr<Texture>>
                                                       │
                                              shared_ptr<Texture>
                                                       │
                                                    Texture
                                                       │
                                            m_Handle: ComPtr<ID3D11ShaderResourceView>
                                            m_Width:  uint32_t
                                            m_Height: uint32_t
                                            m_Format: TextureFormat

TextureLoader (static — no instances)
    LoadDDS(bytes) → TextureData
    LoadTGA(bytes) → TextureData

TextureData (transient — CPU only, not stored after GPU upload)
    Width:   uint32_t
    Height:  uint32_t
    Format:  TextureFormat
    Pixels:  vector<uint8_t>
```

---

## Entities

### `TextureFormat` (enum, in `TextureFormat.h`)

Pure enum, no DX11 includes.

| Value | Integer | Meaning |
|---|---|---|
| `RGBA8` | 0 | Uncompressed; 4 bytes/pixel |
| `DXT1` | 1 | BC1 compressed; 8 bytes/4×4 block |
| `DXT3` | 2 | BC2 compressed; 16 bytes/4×4 block |
| `DXT5` | 3 | BC3 compressed; 16 bytes/4×4 block |

**DXGI mapping** (used only in `DX11Renderer::CreateTexture`):
- `RGBA8` → `DXGI_FORMAT_R8G8B8A8_UNORM`
- `DXT1` → `DXGI_FORMAT_BC1_UNORM`
- `DXT3` → `DXGI_FORMAT_BC2_UNORM`
- `DXT5` → `DXGI_FORMAT_BC3_UNORM`

---

### `TextureData` (struct, in `TextureData.h`)

Transient CPU-side decoded data. Created by `TextureLoader`, consumed by `DX11Renderer::CreateTexture`, then discarded.

| Field | Type | Invariant |
|---|---|---|
| `Width` | `uint32_t` | > 0 on success |
| `Height` | `uint32_t` | > 0 on success |
| `Format` | `TextureFormat` | Valid enum value |
| `Pixels` | `vector<uint8_t>` | Non-empty on success; compressed blocks for DXT formats |

**Row pitch for `Pixels`:**
- `RGBA8`: `Width × 4` bytes per row
- `DXT1`: `max(1, (Width+3)/4) × 8` bytes per block-row
- `DXT3/DXT5`: `max(1, (Width+3)/4) × 16` bytes per block-row

---

### `Texture` (class, in `Texture.h/.cpp`)

Owns a single GPU shader resource. Lifetime managed by `shared_ptr<Texture>`.

| Field | Type | Notes |
|---|---|---|
| `m_Handle` | `RendererTextureHandle` (`ComPtr<ID3D11ShaderResourceView>`) | Owns GPU resource; auto-releases on destruction |
| `m_Width` | `uint32_t` | Pixel width |
| `m_Height` | `uint32_t` | Pixel height |
| `m_Format` | `TextureFormat` | Source format |

**Invariants:**
- `m_Handle` is non-null for a valid `Texture`; empty only if construction failed (invalid instance not held in cache)
- `m_Width > 0` and `m_Height > 0`
- Not copyable; not movable (callers use `shared_ptr<Texture>`)
- `~Texture() = default` — ComPtr RAII releases GPU resource automatically

---

### `TextureLoader` (static class, in `TextureLoader.h/.cpp`)

Stateless. No constructor/destructor. No DX11 dependencies.

| Method | Input | Output | Failure |
|---|---|---|---|
| `LoadDDS(data, out)` | Raw DDS file bytes | `TextureData` filled | Returns `false`; `out` unchanged |
| `LoadTGA(data, out)` | Raw TGA file bytes | `TextureData` filled (always RGBA8) | Returns `false`; `out` unchanged |

**DDS decode state machine:**
1. Validate magic (`"DDS "`, 4 bytes)
2. Read `DDS_HEADER` (124 bytes at offset 4)
3. Check `ddpfPixelFormat.dwFlags`:
   - `DDPF_FOURCC` set → read `dwFourCC`: `'DXT1'` / `'DXT3'` / `'DXT5'`
   - `DDPF_RGB | DDPF_ALPHAPIXELS`, `dwRGBBitCount == 32` → `RGBA8`
   - Otherwise → fail
4. Pixel data at byte offset 128 (4 + 124)
5. Compute expected pixel data size; validate against remaining buffer size
6. Copy pixel bytes into `TextureData::Pixels`

**TGA decode state machine:**
1. Read 18-byte header: `imageType` at byte 2, `width`/`height` at bytes 12–15 (little-endian), `bitDepth` at byte 16
2. Skip image ID field (`idLength` bytes at offset 0)
3. Skip color map (if present)
4. Accept `imageType` 2 (uncompressed) or 10 (RLE)
5. Decode pixels:
   - Type 2: direct copy
   - Type 10: RLE decompress (packet byte: high bit = RLE vs raw; low 7 bits + 1 = count)
6. Expand 24-bit RGB → 32-bit RGBA (insert `0xFF` alpha)
7. Flip rows (bottom-left origin → top-left for DirectX)
8. Output: `Width`, `Height`, `Format = RGBA8`, `Pixels`

---

### `TextureManager` (class, in `TextureManager.h/.cpp`)

Primary engine subsystem for texture access. Owned by `Application`.

| Field | Type | Notes |
|---|---|---|
| `m_Vfs` | `CVirtualFileSystem&` | Injected; not owned |
| `m_Renderer` | `DX11Renderer&` | Injected; not owned |
| `m_Cache` | `unordered_map<string, shared_ptr<Texture>>` | Key = normalized path |

**Lifecycle:**
- Constructed in `Application::CreateTextureManager()` after VFS and Renderer are ready
- Destroyed in `Application::Shutdown()` before Renderer is torn down
- `ClearCache()` callable at any time; callers holding `shared_ptr<Texture>` retain their instances

**Cache invariant:** Every entry in `m_Cache` has a non-null `shared_ptr<Texture>` value. A failed `LoadTexture` is never inserted into the cache.

---

## State Transitions

### `TextureManager` cache state

```
[EMPTY]
    │
    ├─ LoadTexture(path) — miss → load from VFS → decode → GPU upload
    │                          → [CACHED entry added]
    │
    ├─ LoadTexture(path) — hit → return cached shared_ptr
    │                          → [state unchanged]
    │
    └─ ClearCache()            → [EMPTY]
```

### `Texture` GPU resource state

```
[Constructed by TextureManager]
    m_Handle = non-null ComPtr<ID3D11ShaderResourceView>
    │
    ├─ GetHandle() called by rendering system → [state unchanged]
    │
    └─ shared_ptr ref count drops to 0 → [Destructor called]
           ComPtr::~ComPtr() → Release() on SRV
           → SRV releases Texture2D reference
           → Texture2D freed if ref count reaches 0
           → GPU memory reclaimed
```

---

## Relationships

- `TextureManager` → `CVirtualFileSystem`: uses (reads files); does not own
- `TextureManager` → `DX11Renderer`: uses (creates GPU textures); does not own
- `TextureManager` → `Texture`: creates and caches via `shared_ptr`
- `Texture` → `RendererTextureHandle`: owns (RAII lifetime)
- `TextureLoader` → `TextureData`: produces (pure function)
- `DX11Renderer::CreateTexture` → `TextureData`: consumes pixels to upload

---

## Validation Rules

| Rule | Source |
|---|---|
| Cache key must be normalized (lowercase, forward-slash, no leading slash) | FR-017 |
| Failed loads are never inserted into cache | Cache invariant |
| `Texture::m_Handle` must be non-null (otherwise no Texture is constructed) | FR-012 |
| `TextureData::Width > 0` and `Height > 0` | TextureLoader invariant |
| TextureManager destroyed before DX11Renderer | FR-018 / shutdown ordering |
| `RendererTextureHandle` defined only in `DX11Renderer.h` | Spec clarification Q3 |
