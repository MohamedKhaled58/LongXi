# Contract: Texture System Public API

**Module**: LXEngine
**Headers**: `LongXi/LXEngine/Src/Texture/TextureManager.h`, `Texture.h`, `TextureFormat.h`
**Public entry**: `LongXi/LXEngine/LXEngine.h`
**Date**: 2026-03-10

---

## TextureManager

Owned by `Application`. Accessed via `Application::GetTextureManager()`.

### `LoadTexture`

```cpp
std::shared_ptr<Texture> LoadTexture(const std::string& path);
```

- `path`: Virtual path to a DDS or TGA file (e.g., `"texture/terrain/grass.dds"`).
- Normalizes `path` (lowercase, forward-slash, no leading slash) before lookup.
- Returns cached `shared_ptr<Texture>` if `path` was previously loaded successfully.
- On cache miss: reads via VFS, decodes, uploads to GPU, stores in cache, returns `shared_ptr<Texture>`.
- Returns `nullptr` on: empty path, VFS miss, unsupported extension, decode failure, GPU upload failure.
- Logs INFO on load and cache hit; ERROR/WARN on failure.
- Must be called after `Application::Initialize()` completes.

### `GetTexture`

```cpp
std::shared_ptr<Texture> GetTexture(const std::string& path) const;
```

- Returns cached texture if loaded, `nullptr` if not. No VFS access.
- Does NOT log on miss.

### `ClearCache`

```cpp
void ClearCache();
```

- Removes all entries from the internal cache.
- Callers holding `shared_ptr<Texture>` retain their instances until released.
- Logs INFO: `[Texture] Cache cleared (<N> entries released)`.

---

## Texture

Not constructible by callers. Received only via `shared_ptr<Texture>` from `TextureManager`.

```cpp
uint32_t              GetWidth()  const;
uint32_t              GetHeight() const;
TextureFormat         GetFormat() const;
RendererTextureHandle GetHandle() const;  // ComPtr<ID3D11ShaderResourceView>
```

- `GetHandle()` returns the shader resource view for binding at render time.
- GPU resource is owned by the `Texture` object; callers must not call `Release()` on the handle.

---

## TextureFormat (enum)

```cpp
enum class TextureFormat { RGBA8, DXT1, DXT3, DXT5 };
```

Defined in `TextureFormat.h`. No DX11 includes.

---

## DX11Renderer additions

```cpp
RendererTextureHandle CreateTexture(
    uint32_t       width,
    uint32_t       height,
    TextureFormat  format,
    const void*    pixels
);
```

- `pixels`: Row-major pixel data. For compressed formats, raw block data.
- Returns non-null `ComPtr<ID3D11ShaderResourceView>` on success; null on failure.
- Called only by `TextureManager` — not part of the general rendering API.
- `RendererTextureHandle` is defined in `DX11Renderer.h` as `ComPtr<ID3D11ShaderResourceView>`.

---

## Application additions

```cpp
const TextureManager& GetTextureManager() const;
```

- Returns the active `TextureManager` for the application lifetime.
- Valid after `Initialize()` returns `true`; undefined before.

---

## Usage Example

```cpp
// After Application::Initialize():
const TextureManager& tm = app.GetTextureManager();

auto grass = tm.LoadTexture("texture/terrain/grass.dds");
if (!grass)
{
    // texture unavailable — handle gracefully
}
else
{
    // bind for rendering:
    auto srv = grass->GetHandle();  // ID3D11ShaderResourceView*
    // d3dContext->PSSetShaderResources(0, 1, srv.GetAddressOf());
}

// Cache hit on second call:
auto grass2 = tm.LoadTexture("texture/terrain/grass.dds");
// grass2.get() == grass.get() (same Texture object)
```

---

## Error Behavior

| Condition | Return | Log |
|---|---|---|
| `LoadTexture("")` | `nullptr` | `[Texture] WARN LoadTexture called with empty path` |
| File not in VFS | `nullptr` | `[Texture] ERROR File not found: <path>` |
| Bad extension | `nullptr` | `[Texture] ERROR Unsupported format: <path>` |
| DDS/TGA decode failure | `nullptr` | `[Texture] ERROR Failed to decode: <path>` |
| GPU upload failure | `nullptr` | `[Texture] ERROR GPU upload failed: <path>` |
| `GetTexture` on un-loaded path | `nullptr` | (no log) |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
