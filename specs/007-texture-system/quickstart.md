# Quickstart: Texture System Implementation Guide

**Branch**: `007-texture-system`
**Date**: 2026-03-10
**Audience**: Implementer

---

## What Gets Built

**New directory** (6 new files):
```
LongXi/LXEngine/Src/Texture/
├── TextureFormat.h        ← TextureFormat enum only; zero DX11 deps
├── TextureData.h          ← TextureData struct; includes TextureFormat.h
├── TextureLoader.h/.cpp   ← static LoadDDS / LoadTGA
├── Texture.h/.cpp         ← Texture class; ComPtr RAII GPU owner
└── TextureManager.h/.cpp  ← TextureManager; VFS + cache + GPU bridge
```

**Modified files** (5 existing):
```
LongXi/LXEngine/Src/Renderer/DX11Renderer.h/.cpp   ← add RendererTextureHandle + CreateTexture
LongXi/LXEngine/Src/Application/Application.h/.cpp  ← add m_TextureManager + lifecycle
LongXi/LXEngine/LXEngine.h                          ← add TextureManager include
```

No premake changes. Glob auto-picks up new files.

---

## Implementation Order

Build in dependency order to avoid incomplete-type errors at compile time.

### Step 1 — `TextureFormat.h`

Pure enum, no includes except `<cstdint>` if needed. Place in `LXEngine/Src/Texture/`.

```cpp
#pragma once
namespace LongXi
{
enum class TextureFormat { RGBA8, DXT1, DXT3, DXT5 };
} // namespace LongXi
```

---

### Step 2 — `TextureData.h`

Struct only, no .cpp needed.

```cpp
#pragma once
#include "Core/TextureFormat.h"   // or relative path
#include <cstdint>
#include <vector>
namespace LongXi
{
struct TextureData
{
    uint32_t             Width;
    uint32_t             Height;
    TextureFormat        Format;
    std::vector<uint8_t> Pixels;
};
} // namespace LongXi
```

---

### Step 3 — `TextureLoader.h/.cpp`

Static class. No DX11. No VFS. Takes bytes, returns `TextureData`.

**`LoadDDS` implementation notes:**
- Bytes 0–3: must equal `{ 'D', 'D', 'S', ' ' }` (0x20534444 little-endian). Fail if not.
- Bytes 4–127: `DDS_HEADER` (use `<ddraw.h>` or declare manually — `dwSize`, `dwFlags`, `dwHeight`, `dwWidth`, `dwPitchOrLinearSize`, `dwDepth`, `dwMipMapCount`, reserved, `ddpfPixelFormat`, `ddsCaps`)
- Check `ddpfPixelFormat.dwFlags & DDPF_FOURCC` for compressed; compare `dwFourCC` against `MAKEFOURCC('D','X','T','1')` etc.
- For uncompressed: check `DDPF_RGB | DDPF_ALPHAPIXELS` and `dwRGBBitCount == 32`
- Pixel data starts at byte 128
- Expected size for DXT1: `max(1,(W+3)/4) * max(1,(H+3)/4) * 8`
- Expected size for DXT3/5: `max(1,(W+3)/4) * max(1,(H+3)/4) * 16`
- Expected size for RGBA8: `W * H * 4`
- Copy into `TextureData::Pixels`

**`LoadTGA` implementation notes:**
- Header (18 bytes): `idLength` [0], `colorMapType` [1], `imageType` [2], (skip color map 5 bytes), `xOrigin` [8–9], `yOrigin` [10–11], `width` [12–13], `height` [14–15], `bitDepth` [16], `imageDescriptor` [17]
- Accept `imageType` 2 (uncompressed RGB/RGBA) or 10 (RLE-compressed)
- Skip `idLength` bytes after header
- Skip color map if `colorMapType != 0` (use color map field sizes from header)
- Accept `bitDepth` 24 or 32
- Decode pixels:
  - Type 2: `width * height * (bitDepth/8)` bytes, direct copy
  - Type 10: RLE — read packet byte; bit 7 = 1 → run packet (1 pixel repeated count+1 times); bit 7 = 0 → raw packet (count+1 sequential pixels)
- If 24-bit: expand each pixel to RGBA by inserting `0xFF` as alpha
- Flip rows: row 0 of output = last row of decoded buffer
- Output: `Width`, `Height`, `Format = RGBA8`

---

### Step 4 — `Texture.h/.cpp`

Holds the ComPtr. Destructor is `= default`. Constructor is private; `TextureManager` uses `make_shared` via friend or factory.

**Pattern**: Make `TextureManager` a `friend` of `Texture`, or use a public constructor with a tag type:

```cpp
// Option: private constructor, TextureManager as friend
class Texture
{
    friend class TextureManager;
    Texture(RendererTextureHandle handle, uint32_t w, uint32_t h, TextureFormat fmt);
    ...
};
```

Include `DX11Renderer.h` in `Texture.h` for `RendererTextureHandle`.

---

### Step 5 — `DX11Renderer.h/.cpp` additions

**In `DX11Renderer.h`:**
1. Add type alias (after existing ComPtr members): `using RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;`
2. Add public method declaration: `RendererTextureHandle CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* pixels);`
3. Add `#include "Texture/TextureFormat.h"` (needed for the `TextureFormat` parameter type)

**In `DX11Renderer.cpp`:**
```cpp
RendererTextureHandle DX11Renderer::CreateTexture(
    uint32_t width, uint32_t height, TextureFormat format, const void* pixels)
{
    // 1. Map TextureFormat → DXGI_FORMAT
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    switch (format)
    {
    case TextureFormat::RGBA8: dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
    case TextureFormat::DXT1:  dxgiFormat = DXGI_FORMAT_BC1_UNORM;       break;
    case TextureFormat::DXT3:  dxgiFormat = DXGI_FORMAT_BC2_UNORM;       break;
    case TextureFormat::DXT5:  dxgiFormat = DXGI_FORMAT_BC3_UNORM;       break;
    default: LX_ENGINE_ERROR("Unknown TextureFormat"); return {};
    }

    // 2. Compute row pitch
    UINT rowPitch = 0;
    switch (format)
    {
    case TextureFormat::RGBA8: rowPitch = width * 4; break;
    case TextureFormat::DXT1:  rowPitch = std::max(1u, (width + 3) / 4) * 8;  break;
    case TextureFormat::DXT3:
    case TextureFormat::DXT5:  rowPitch = std::max(1u, (width + 3) / 4) * 16; break;
    }

    // 3. CreateTexture2D
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = dxgiFormat;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = pixels;
    initData.SysMemPitch = rowPitch;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTex;
    HRESULT hr = m_Device->CreateTexture2D(&desc, &initData, &pTex);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("DX11Renderer::CreateTexture failed (HRESULT 0x{:08X})", (uint32_t)hr);
        return {};
    }

    // 4. CreateShaderResourceView; pTex goes out of scope (SRV holds sole ref)
    RendererTextureHandle srv;
    hr = m_Device->CreateShaderResourceView(pTex.Get(), nullptr, &srv);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("DX11Renderer::CreateShaderResourceView failed (0x{:08X})", (uint32_t)hr);
        return {};
    }

    return srv;  // ComPtr takes ownership; pTex released here
}
```

---

### Step 6 — `TextureManager.h/.cpp`

**`LoadTexture` core logic:**
```
1. normalized = Normalize(path)
2. if empty → WARN + return nullptr
3. it = m_Cache.find(normalized)
   if found → INFO "[Texture] Cache hit: ..." → return it->second
4. data = m_Vfs.ReadAll(normalized)
   if data.empty() → ERROR "[Texture] File not found: ..." → return nullptr
5. suffix = lowercase extension of normalized
   if ".dds" → TextureLoader::LoadDDS(data, td)
   if ".tga" → TextureLoader::LoadTGA(data, td)
   else → ERROR "[Texture] Unsupported format: ..." → return nullptr
   if decode failed → ERROR "[Texture] Failed to decode: ..." → return nullptr
6. handle = m_Renderer.CreateTexture(td.Width, td.Height, td.Format, td.Pixels.data())
   if handle == null → ERROR "[Texture] GPU upload failed: ..." → return nullptr
7. auto tex = shared_ptr<Texture>(new Texture(move(handle), td.Width, td.Height, td.Format))
   OR use friend + make_shared
8. m_Cache[normalized] = tex
9. INFO "[Texture] Loaded: ... (WxH FMT)" → return tex
```

**`Normalize`**: Copy `CVirtualFileSystem::Normalize` logic (lowercase, backslash→slash, strip leading slash, collapse //, reject ..).

---

### Step 7 — `Application.h/.cpp`

**Application.h:** add forward declaration `class TextureManager;` and:
```cpp
std::unique_ptr<TextureManager> m_TextureManager;
const TextureManager& GetTextureManager() const;
bool CreateTextureManager();
```

**Application.cpp `Initialize()`** — add after `CreateVirtualFileSystem()`:
```cpp
if (!CreateTextureManager())
{
    LX_ENGINE_ERROR("Texture manager creation failed — aborting initialization");
    // ... cleanup VFS, input, renderer, window
    return false;
}
```

**`CreateTextureManager()`:**
```cpp
bool Application::CreateTextureManager()
{
    m_TextureManager = std::make_unique<TextureManager>(
        *m_VirtualFileSystem, *m_Renderer);
    LX_ENGINE_INFO("Texture manager initialized");
    return true;
}
```

**Application.cpp `Shutdown()`** — insert as FIRST reset (before VFS):
```cpp
if (m_TextureManager)
    m_TextureManager.reset();  // Release all ComPtr SRVs before device teardown
```

---

### Step 8 — `LXEngine.h`

Add:
```cpp
#include "Texture/TextureManager.h"
```

---

## Verification Checklist

- [ ] Project builds without errors (`Win-Build Project.bat`)
- [ ] `LoadTexture("some/dds/path.dds")` returns non-null for a valid DDS in a mounted directory
- [ ] Second `LoadTexture` call on same path returns equal `shared_ptr` target (same `Texture*`)
- [ ] `LoadTexture("nonexistent.dds")` returns `nullptr` and logs `[Texture] ERROR File not found:`
- [ ] TGA load (`LoadTexture("some/file.tga")`) returns non-null with `RGBA8` format
- [ ] `GetTextureManager()` accessible after `Application::Initialize()`
- [ ] Shutdown: no D3D11 live object warnings in debug layer (TextureManager reset before Renderer)
- [ ] `Win-Format Code.bat` runs cleanly on all new files
