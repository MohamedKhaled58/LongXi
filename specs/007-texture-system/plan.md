# Implementation Plan: Texture System

**Branch**: `007-texture-system` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/007-texture-system/spec.md`

---

## Summary

Implement a GPU texture loading and caching system in `LXEngine`. The system introduces six new files in `LXEngine/Src/Texture/`, extends `DX11Renderer` with a `CreateTexture` factory, adds `TextureManager` to `Application`'s lifecycle, and exposes it through `LXEngine.h`. Textures are loaded through `CVirtualFileSystem`, decoded from DDS or TGA, uploaded to the GPU as `ID3D11ShaderResourceView` ComPtrs, and cached by normalized path. GPU resource lifetime is managed entirely by ComPtr RAII — no explicit `DestroyTexture` call is needed.

---

## Technical Context

**Language/Version**: C++23 (MSVC v145) — from constitution Article III
**Primary Dependencies**: LXEngine (DX11Renderer), LXCore (CVirtualFileSystem, LX_ENGINE_* macros), WRL ComPtr (`<wrl/client.h>`), D3D11 (`<d3d11.h>`, already linked)
**Storage**: GPU `ID3D11ShaderResourceView` via ComPtr (RAII); CPU `unordered_map<string, shared_ptr<Texture>>` cache
**Testing**: Manual build + integration test (mount a texture directory, call `LoadTexture`, verify non-null return and GPU SRV)
**Target Platform**: Windows (Win32) — D3D11 only
**Project Type**: Static library (LXEngine) — premake glob auto-includes all new files
**Performance Goals**: Cache hit = O(1) unordered_map lookup; first load = one `ReadAll` + one decode + one `CreateTexture2D` + `CreateShaderResourceView`
**Constraints**: Phase 1 single-threaded (constitution Article IX); GPU resource release before device teardown (TextureManager before Renderer in shutdown); `TextureFormat.h` must have zero DX11 dependencies
**Scale/Scope**: Unbounded cache for Phase 1; typical usage: hundreds of textures per scene

---

## Constitution Check

| Article | Gate | Status | Notes |
|---|---|---|---|
| III — Platform/Tooling | C++23, MSVC, Win32, static lib | PASS | All texture files go into LXEngine (static lib) |
| III — Module Structure | LXEngine = engine/runtime systems | PASS | TextureManager depends on DX11Renderer + VFS — correct placement in LXEngine |
| III — Dependency Direction | LXShell > LXEngine > LXCore | PASS | TextureManager uses CVirtualFileSystem (LXCore) — correct direction |
| IV — Entrypoint Discipline | Application stays thin | PASS | CreateTextureManager() is a thin delegator |
| IV — Module Boundaries | No cross-layer coupling | PASS | TextureFormat.h has zero DX11 includes; DX11 coupling isolated to DX11Renderer.h |
| IV — Abstraction Discipline | Abstractions earned | PASS | TextureLoader, TextureData, TextureFormat all earned by concrete need |
| IV — No global state | No hidden global state | PASS | Replaces g_lpTex[10240] global array; TextureManager owned by Application |
| VIII — Change Scope | Focused on stated problem | PASS | Texture loading only; no sprite/animation/render-target scope creep |
| IX — Threading | Phase 1 single-threaded | PASS | No sync primitives needed |
| XII — Phase 1 Scope | Resource/filesystem foundation | PASS | Texture loading is core asset foundation; within Phase 1 |

**All gates pass. No violations.**

---

## Project Structure

### Documentation (this feature)

```text
specs/007-texture-system/
├── plan.md              ← this file
├── spec.md              ← feature specification
├── research.md          ← Phase 0 findings
├── data-model.md        ← entity definitions
├── quickstart.md        ← implementation guide
├── contracts/
│   └── texture-api.md   ← public API contract
└── checklists/
    └── requirements.md  ← spec quality checklist
```

### Source Code Changes

```text
LongXi/LXEngine/Src/Texture/              NEW directory (6 files)
├── TextureFormat.h                        NEW: TextureFormat enum (zero DX11 deps)
├── TextureData.h                          NEW: TextureData struct
├── TextureLoader.h / TextureLoader.cpp    NEW: static DDS + TGA decoder
├── Texture.h / Texture.cpp                NEW: ComPtr-owning GPU texture object
└── TextureManager.h / TextureManager.cpp  NEW: VFS + cache + GPU bridge

LongXi/LXEngine/Src/Renderer/
├── DX11Renderer.h    UPDATED: RendererTextureHandle alias + CreateTexture()
└── DX11Renderer.cpp  UPDATED: implement CreateTexture()

LongXi/LXEngine/Src/Application/
├── Application.h     UPDATED: TextureManager member + accessor + factory
└── Application.cpp   UPDATED: CreateTextureManager() + shutdown ordering

LongXi/LXEngine/
└── LXEngine.h        UPDATED: add TextureManager include
```

No premake changes. LXEngine glob Src/**.h / Src/**.cpp auto-includes new Texture/ files.

---

## Phase 0: Research Summary

All questions resolved. See [research.md](research.md).

| Question | Finding |
|---|---|
| GPU release | ComPtr RAII; ~Texture() = default; no DestroyTexture |
| D3D11 creation | CreateTexture2D -> CreateSRV -> release Texture2D (SRV holds sole ref) |
| Row pitch (compressed) | DXT1: max(1,(W+3)/4)*8; DXT3/5: max(1,(W+3)/4)*16; RGBA8: W*4 |
| Shutdown order | TextureManager first -> VFS -> Input -> Renderer -> Window |
| LXEngine premake | No changes; glob auto-includes |
| RendererTextureHandle location | DX11Renderer.h only; TextureFormat.h DX11-free |
| TGA row orientation | Bottom-left -> must flip rows for D3D11 top-left convention |
| DDS pixel offset | Byte 128 (4 magic + 124 header) |

---

## Phase 1: Design Artifacts

- **Data model**: [data-model.md](data-model.md)
- **API contract**: [contracts/texture-api.md](contracts/texture-api.md)
- **Implementation guide**: [quickstart.md](quickstart.md)

---

## Implementation Tasks

### Task 1 — TextureFormat.h

**File**: `LXEngine/Src/Texture/TextureFormat.h`

Define `enum class TextureFormat { RGBA8, DXT1, DXT3, DXT5 }`. Zero DX11 or external dependencies. `namespace LongXi`.

---

### Task 2 — TextureData.h

**File**: `LXEngine/Src/Texture/TextureData.h`

Define `TextureData` struct: `Width`, `Height`, `Format`, `Pixels`. Include `TextureFormat.h`, `<cstdint>`, `<vector>`. No .cpp needed.

---

### Task 3 — TextureLoader.h/.cpp

**Files**: `LXEngine/Src/Texture/TextureLoader.h/.cpp`

Static class. `LoadDDS` and `LoadTGA`. No DX11. No VFS.

DDS: validate magic, parse DDS_HEADER, detect FOURCC / uncompressed flags, copy pixel data from byte 128.
TGA: parse 18-byte header, type 2/10, 24/32-bit, RLE decompress for type 10, flip rows bottom-to-top.
Both: validate buffer size; return false without modifying `out` on failure.

---

### Task 4 — Texture.h/.cpp

**Files**: `LXEngine/Src/Texture/Texture.h/.cpp`

- `m_Handle: RendererTextureHandle` (ComPtr) — auto-releases
- `~Texture() = default`
- Private constructor; `friend class TextureManager`
- Include `DX11Renderer.h` for `RendererTextureHandle`
- Getters: `GetWidth`, `GetHeight`, `GetFormat`, `GetHandle`

---

### Task 5 — DX11Renderer extensions

**Files**: `LXEngine/Src/Renderer/DX11Renderer.h/.cpp`

DX11Renderer.h:
1. Add `#include "Texture/TextureFormat.h"`
2. Add `using RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;`
3. Add public `RendererTextureHandle CreateTexture(uint32_t w, uint32_t h, TextureFormat fmt, const void* pixels);`

DX11Renderer.cpp:
- Map TextureFormat -> DXGI_FORMAT
- Compute rowPitch per format (see research.md for formulas)
- CreateTexture2D + CreateShaderResourceView -> release Texture2D handle -> return SRV ComPtr
- Log LX_ENGINE_ERROR on FAILED(hr)

---

### Task 6 — TextureManager.h/.cpp

**Files**: `LXEngine/Src/Texture/TextureManager.h/.cpp`

- Constructor: `TextureManager(CVirtualFileSystem&, DX11Renderer&)`
- `Normalize(path)`: 8-step algorithm (lowercase, backslash->slash, strip leading slash, collapse //, reject .., collapse .)
- `LoadTexture(path)`: normalize -> cache lookup -> VFS ReadAll -> extension detect -> decode -> GPU upload -> insert cache -> return
- `GetTexture(path)`: normalize -> cache lookup
- `ClearCache()`: `m_Cache.clear()` + log

---

### Task 7 — Application migration

**Files**: `LXEngine/Src/Application/Application.h/.cpp`

Application.h:
- Add `class TextureManager;` forward declaration
- Add `std::unique_ptr<TextureManager> m_TextureManager;`
- Add `const TextureManager& GetTextureManager() const;`
- Add `bool CreateTextureManager();` (private)

Application.cpp:
- Add `#include "Texture/TextureManager.h"`
- `CreateTextureManager()`: construct with `*m_VirtualFileSystem, *m_Renderer`
- `Initialize()`: call `CreateTextureManager()` after `CreateVirtualFileSystem()`
- `Shutdown()`: `m_TextureManager.reset()` as FIRST reset (before VFS, before Renderer)

---

### Task 8 — LXEngine.h update

**File**: `LXEngine/LXEngine.h`

Add `#include "Texture/TextureManager.h"`.

---

### Task 9 — Verification

1. `Win-Build Project.bat` — zero errors, zero warnings
2. `Win-Format Code.bat` — all new files formatted
3. Application starts — no D3D11 live-object warnings in debug layer
4. `GetTextureManager().LoadTexture("...")` returns non-null for mounted DDS
5. Second `LoadTexture` call on same path returns same `Texture*`
6. Missing path returns `nullptr` and logs error

---

## Constitution Check (Post-Design)

All gates confirmed after design phase:
- No new external dependencies (DX11 already linked; no new third-party libraries)
- Premake unchanged
- `TextureFormat.h` verified DX11-free
- Shutdown order protects against D3D11 live-object warnings
- No global state introduced

---

## Risks and Notes

| Risk | Mitigation |
|---|---|
| Texture constructor visibility | `friend class TextureManager` in Texture class |
| Circular include: DX11Renderer.h <-> TextureFormat.h | TextureFormat.h has no DX11 deps; DX11Renderer.h includes it; no cycle |
| TGA row flip omitted | Explicit step documented in quickstart.md; verify in integration test |
| DXT row pitch miscalculation | Block-row formula in research.md; corrupted GPU texture is visible symptom |
| D3D11 live-object warnings | TextureManager reset first in Shutdown(); verified in Task 9 |
| Normalize duplication (also in CVirtualFileSystem) | Acceptable for Phase 1; consolidation is follow-up work |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
