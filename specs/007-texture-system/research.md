# Research: Texture System

**Branch**: `007-texture-system`
**Date**: 2026-03-10
**Status**: Complete — no NEEDS CLARIFICATION items

---

## Finding 1 — LXEngine Module: Auto-Glob + Existing DX11 Links

**Decision**: New `LongXi/LXEngine/Src/Texture/` directory is auto-included. No premake changes required.

**Evidence**: `LXEngine/premake5.lua` uses `files { "Src/**.h", "Src/**.cpp" }` glob. Links already include `d3d11`, `dxgi`, `dxguid` — all DX11 texture APIs are available without new link entries.

---

## Finding 2 — RendererTextureHandle Definition (Confirmed via Spec Clarification)

**Decision**: `RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>` is defined in `DX11Renderer.h`. `TextureFormat.h` contains only the enum (zero DX11 dependencies).

**Evidence**: `DX11Renderer.h` already includes `<wrl/client.h>` and `<d3d11.h>`. The type alias slots in naturally alongside existing ComPtr members (`m_Device`, `m_Context`, etc.).

**Rationale**: Keeping DX11 headers inside `DX11Renderer.h` prevents DX11 pollution from spreading into `TextureFormat.h` and `TextureData.h`, which are pure data types.

---

## Finding 3 — D3D11 Texture Creation Pattern

**Decision**: `CreateTexture` in `DX11Renderer` follows this exact D3D11 RAII sequence:

```
1. D3D11_TEXTURE2D_DESC desc = { width, height, 1 mip, 1 arraySize, DXGI_FORMAT, IMMUTABLE usage, SRV bind flag }
2. D3D11_SUBRESOURCE_DATA initData = { pixels, rowPitch, slicePitch }
3. m_Device->CreateTexture2D(&desc, &initData, &pTex) → ID3D11Texture2D* pTex
4. m_Device->CreateShaderResourceView(pTex, nullptr, &pSRV) → ID3D11ShaderResourceView* pSRV
5. pTex->Release() immediately → SRV now holds the sole reference to the Texture2D
6. Return ComPtr wrapping pSRV
```

When the returned `ComPtr<ID3D11ShaderResourceView>` is later destroyed (or Reset()), it calls `Release()` on the SRV. The SRV's internal `Release()` drops its reference to the Texture2D, which then reaches ref count 0 and is destroyed. Full GPU memory is reclaimed. No separate `DestroyTexture` call is needed.

**Row pitch calculation for `D3D11_SUBRESOURCE_DATA.SysMemPitch`:**
- `RGBA8`: `width * 4`
- `DXT1` (BC1): `max(1, (width + 3) / 4) * 8`
- `DXT3` (BC2): `max(1, (width + 3) / 4) * 16`
- `DXT5` (BC3): `max(1, (width + 3) / 4) * 16`

---

## Finding 4 — GPU Resource Release: ComPtr RAII (Spec Clarification Q1)

**Decision**: `Texture`'s GPU resource lifetime is managed entirely by the `ComPtr<ID3D11ShaderResourceView>` member. The destructor is `= default`. No renderer back-pointer needed. `DestroyTexture` is removed.

**Rationale**: ComPtr already handles `Release()` via RAII. The D3D11 creation pattern (step 5 above: release Texture2D immediately) ensures that when the SRV ComPtr is destroyed, both the SRV and the Texture2D are freed. This is the idiomatic WRL pattern used throughout the renderer.

---

## Finding 5 — Application Integration (Spec Clarification Q2)

**Decision**: `Application` gains `m_TextureManager: unique_ptr<TextureManager>`, `CreateTextureManager()`, and `GetTextureManager()`.

**Shutdown order** (confirmed correct for GPU safety):
```
m_TextureManager.reset()   ← 1st: ComPtr SRVs released while device still alive
m_VirtualFileSystem.reset() ← 2nd: WDF file handles released
m_InputSystem.reset()       ← 3rd
m_Renderer.reset()          ← 4th: DX11 device destroyed LAST
m_Window.reset()            ← 5th: window handle released
```

GPU resources MUST be released before the DX11 device. Placing TextureManager reset first ensures no "live D3D11 objects" warnings and no use-after-free.

**Evidence from Application.cpp Shutdown pattern**: existing code already resets VFS before Renderer. TextureManager slots in at position 0 (first reset).

---

## Finding 6 — LXEngine.h Needs TextureManager Entry

**Decision**: `LXEngine.h` gains `#include "Texture/TextureManager.h"` to expose `TextureManager` as part of the LXEngine public surface.

**Evidence**: `LXEngine.h` currently includes `Application/Application.h` and `Window/Win32Window.h`. TextureManager is a new engine subsystem at the same level.

---

## Finding 7 — DDS Pixel Data Offset and Mipmap Chain

**Decision**: Read only mipmap level 0. Pixel data starts at byte offset 128 (4 magic + 124 header). For DXT compressed formats, the pixel section contains compressed block data directly usable as `SysMemPitch` row-by-row.

**Evidence**: Standard DDS spec. Original client used `D3DX_DEFAULT` mip levels (3) but auto-generated from level 0. We upload level 0 only; GPU mipmap generation is deferred.

---

## Finding 8 — TGA Row Flip

**Decision**: Standard TGA files store rows bottom-to-top. After decoding, rows must be reversed before upload to match DirectX's top-to-bottom convention.

**Evidence**: TGA spec §8: "image origin" field. Type 2 (uncompressed) typically has origin bit = 0 (bottom-left). DirectX `CreateTexture2D` expects top-to-bottom row order.

---

## Summary: All NEEDS CLARIFICATION Items Resolved

| Item | Resolution |
|---|---|
| GPU release mechanism | ComPtr RAII; `Texture::~Texture() = default` |
| Application integration | FR-018 + Application Integration section in spec |
| RendererTextureHandle location | `DX11Renderer.h` only; `TextureFormat.h` is DX11-free |
| D3D11 creation pattern | CreateTexture2D → CreateSRV → Release Texture2D immediately |
| LXEngine premake | No changes needed (glob auto-includes) |
| DX11 links | Already present (d3d11, dxgi, dxguid) |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
