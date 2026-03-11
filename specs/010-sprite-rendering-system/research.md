# Research: Sprite Rendering System

**Feature**: 010-sprite-rendering-system
**Date**: 2026-03-11
**Phase**: 0 - Outline & Research

## Overview

This document consolidates research findings for implementing the SpriteRenderer subsystem. All decisions are grounded in the existing LXEngine codebase and the constraints of the Long Xi Constitution (DX11, C++23, single-threaded, no premature abstraction).

---

## Research Area 1: DX11Renderer Device/Context Access

**Question**: How does SpriteRenderer access the `ID3D11Device*` and `ID3D11DeviceContext*` required to create GPU resources and issue draw calls?

**Codebase finding**: `DX11Renderer` stores `m_Device` and `m_Context` as private `ComPtr<>` members with **no public accessors**. SpriteRenderer cannot access them directly.

**Decision**: Add non-const accessor methods to `DX11Renderer`:
```cpp
ID3D11Device*        GetDevice()  const;   // returns m_Device.Get()
ID3D11DeviceContext* GetContext() const;   // returns m_Context.Get()
```

**Rationale**:
- Follows the established pattern in Engine.h (public accessors for all subsystems)
- Raw pointer return is appropriate — caller never owns the device/context lifetime
- Const methods are safe since D3D11 operations mutate GPU state, not the ComPtr itself
- Minimal change to DX11Renderer — two one-line methods

**Alternatives considered**:
- Pass raw pointers directly to `SpriteRenderer::Initialize()` — rejected because SpriteRenderer also needs the context during `Begin()`/`End()`/flush calls, so storing a reference to DX11Renderer is cleaner than storing two raw pointers
- DX11Renderer factory methods for resource creation — rejected as premature abstraction; SpriteRenderer is responsible for its own GPU resource lifecycle

---

## Research Area 2: Texture SRV Access Pattern

**Question**: How does SpriteRenderer retrieve the `ID3D11ShaderResourceView*` from a `Texture` to bind it to the pixel shader?

**Codebase finding**: `Texture::GetHandle()` returns `const RendererTextureHandle&` which is `Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>`. SpriteRenderer calls `.Get()` on this ComPtr to obtain the raw pointer for `PSSetShaderResources`.

**Decision**: Use `texture->GetHandle().Get()` for SRV binding.

**Rationale**:
- No changes required to Texture class
- ComPtr `.Get()` is zero-overhead raw pointer access
- `ID3D11ShaderResourceView*` is the correct type for `PSSetShaderResources`

---

## Research Area 3: HLSL Shader Strategy

**Question**: How should SpriteRenderer compile and manage HLSL shaders? Inline strings vs. external .hlsl files vs. pre-compiled .cso files?

**Decision**: Embed HLSL source as string constants in `SpriteRenderer.cpp`, compile at runtime via `D3DCompile()`.

**Rationale**:
- Consistent with the existing project pattern (no external shader files currently exist)
- No build system changes required for shader compilation pipeline
- D3D11 feature level 11_0 guarantees shader model 4_0 support
- Avoids introducing FXC/DXC build step and managing .cso file distribution
- Acceptable for Phase 1 — shader compile time is negligible (< 50ms for two simple shaders)

**Alternatives considered**:
- Pre-compiled .cso via FXC: Faster startup, but requires build system pipeline changes and managing compiled artefacts alongside source — deferred to future optimization spec
- External .hlsl files loaded from VFS: Clean separation but adds loading dependency and VFS must be initialized before SpriteRenderer — rejected to avoid ordering constraint

**D3DCompile dependency**:
- `d3dcompiler.lib` must be added to `LongXi/LXEngine/premake5.lua` `links` table
- `#include <d3dcompiler.h>` in SpriteRenderer.cpp only (not exposed in header)
- On Windows 10+, D3DCompiler_47.dll ships with the OS — no runtime redistribution needed

---

## Research Area 4: Orthographic Projection Matrix

**Question**: What is the correct orthographic projection matrix for top-left origin screen space, and how must it be laid out in the constant buffer for HLSL?

**Decision**: Use a hand-computed 4×4 row-major projection matrix, transpose before upload (or use `#pragma pack_matrix(row_major)` in HLSL).

**Matrix (maps screen [0,W]×[0,H] to clip [-1,1]×[1,-1]):**

```
C++ row-major layout:
  Row 0: [ 2/W,    0,  0,  0 ]
  Row 1: [   0, -2/H,  0,  0 ]
  Row 2: [   0,    0,  1,  0 ]
  Row 3: [  -1,    1,  0,  1 ]

Where W = viewport width, H = viewport height
X clip = (2/W) * x - 1
Y clip = (-2/H) * y + 1  (Y is flipped: top=+1, bottom=-1)
```

**HLSL handling**: Add `#pragma pack_matrix(row_major)` to the vertex shader before the `cbuffer` declaration. This tells HLSL to interpret the uploaded float array as row-major, matching the C++ memory layout exactly with no transposition required.

**Alternatives considered**:
- Upload column-major (transpose in C++): Works but adds per-resize CPU work; the `#pragma pack_matrix` approach is zero-cost
- Use DirectXMath `XMMatrixOrthographicOffCenterLH`: Correct result but adds DirectXMath dependency — rejected per No-Math-Library assumption in spec

---

## Research Area 5: Vertex Buffer Strategy

**Question**: What is the correct D3D11 vertex buffer configuration for dynamic per-frame sprite data upload?

**Decision**: `D3D11_USAGE_DYNAMIC`, `D3D11_CPU_ACCESS_WRITE`, capacity for `MAX_SPRITES_PER_BATCH * 4` vertices.

**Upload pattern each flush**:
1. `Map(buffer, D3D11_MAP_WRITE_DISCARD, ...)` → get CPU pointer
2. `memcpy` sprite vertex data
3. `Unmap(buffer, 0)`

**Rationale**:
- `D3D11_MAP_WRITE_DISCARD` avoids GPU/CPU synchronization stalls — driver allocates new backing memory
- Dynamic vertex buffer is the standard approach for CPU-built geometry uploaded once per frame
- Capacity fixed at `MAX_SPRITES_PER_BATCH * 4 = 4096 vertices` (512KB at 128 bytes/vertex) — fits well within GPU buffer constraints

**Alternatives considered**:
- `D3D11_USAGE_DEFAULT` + `UpdateSubresource`: Causes a full copy through a staging buffer on some drivers — less efficient than `Map/Unmap` for frequently changing data
- Staging buffer + copy: More control but unnecessary complexity for Phase 1 single-threaded use

---

## Research Area 6: Index Buffer Strategy

**Question**: Should the index buffer be static or dynamic? What index pattern is used for quads?

**Decision**: Static index buffer (`D3D11_USAGE_IMMUTABLE`) with pre-generated indices for `MAX_SPRITES_PER_BATCH` quads.

**Index pattern** (two triangles per quad, counter-clockwise front face):
```
Quad vertices: 0(TL), 1(TR), 2(BL), 3(BR)
Triangle 1: 0, 1, 2   (top-left triangle)
Triangle 2: 2, 1, 3   (bottom-right triangle)
Index pattern per quad: [ 4n+0, 4n+1, 4n+2, 4n+2, 4n+1, 4n+3 ]
Total indices: MAX_SPRITES_PER_BATCH * 6 = 6144
Index type: UINT16 (max index 4095, fits in uint16_t)
```

**Rationale**:
- Immutable index buffer is created once, zero per-frame upload cost
- UINT16 indices (not UINT32) halve the index buffer bandwidth — max index value is 4095 which fits uint16_t
- Counter-clockwise winding is standard D3D convention

---

## Research Area 7: Pipeline State (Blend, Depth, Sampler, Rasterizer)

**Question**: What D3D11 pipeline state objects are required for correct 2D sprite rendering?

**Decision summary**:

| State | Configuration |
|-------|--------------|
| Blend | Alpha blend: `SrcAlpha / InvSrcAlpha`, blend op `Add`, both color and alpha |
| Depth-Stencil | Depth test: disabled; depth write: disabled |
| Sampler | Filter: `MIN_MAG_MIP_LINEAR`; Address: `CLAMP` on U and V |
| Rasterizer | Cull mode: `NONE` (sprites may be flipped); Fill: `SOLID`; scissor: disabled |

**All state objects are created once during `Initialize()` and bound during each `Begin()` call.**

---

## Research Area 8: SpriteRenderer Initialization Signature

**Question**: What parameters should `SpriteRenderer::Initialize()` accept?

**Decision**: `bool Initialize(DX11Renderer& renderer, int width, int height)`

- `DX11Renderer&` stored as member reference for device/context access throughout lifetime
- `width`, `height` used to compute initial orthographic projection matrix
- Returns `bool` — false on any GPU resource creation failure (graceful degradation per spec FR-005)
- Engine::Initialize() DOES NOT fail if SpriteRenderer::Initialize() returns false (non-fatal)

---

## Summary of All Decisions

| Area | Decision | Key Rationale |
|------|----------|---------------|
| Device/context access | Add `GetDevice()`/`GetContext()` to DX11Renderer | Follows existing accessor pattern, minimal change |
| Texture SRV | `texture->GetHandle().Get()` | No Texture class changes required |
| HLSL strategy | Inline strings, compile via D3DCompile at runtime | No build pipeline changes, acceptable for Phase 1 |
| Projection matrix | Hand-computed row-major + `#pragma pack_matrix(row_major)` | Zero-cost, no external math library |
| Vertex buffer | DYNAMIC + MAP_WRITE_DISCARD | Standard pattern for CPU-built GPU geometry |
| Index buffer | IMMUTABLE, UINT16, pre-generated | Zero per-frame cost, lower bandwidth |
| Pipeline state | Alpha blend, no depth, linear-clamp sampler, no cull | Correct for 2D screen-space sprites |
| Init signature | `Initialize(DX11Renderer&, int, int)` → bool | Consistent with subsystem pattern |
| d3dcompiler | Add to LXEngine premake5.lua links | Runtime HLSL compilation dependency |
| Math types | `LongXi/LXEngine/Src/Math/Math.h` (Vector2, Color) | Shared LXEngine header, no new module |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
