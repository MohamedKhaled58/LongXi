# Data Model: Sprite Rendering System

**Feature**: 010-sprite-rendering-system
**Date**: 2026-03-11

## Overview

This document defines all entities, types, relationships, and state machines introduced by the Sprite Rendering System.

---

## Entity 1: Vector2

**Location**: `LongXi/LXEngine/Src/Math/Math.h`
**Namespace**: `LongXi`
**Kind**: POD struct

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| x | float | Horizontal component |
| y | float | Vertical component |

**Usage**: Screen-space position (pixels from top-left), sprite size (pixels), UV coordinates (0.0–1.0 normalized).

**Constraints**:
- No validation — callers are responsible for valid values
- UV usage: `x` and `y` must be in [0.0, 1.0] for correct texture sampling; values outside this range produce clamped samples (clamp sampler)
- Size usage: zero or negative width/height produce degenerate quads (no visible output); SpriteRenderer guards this

---

## Entity 2: Color

**Location**: `LongXi/LXEngine/Src/Math/Math.h`
**Namespace**: `LongXi`
**Kind**: POD struct

**Fields**:

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| r | float | [0.0, 1.0] | Red channel |
| g | float | [0.0, 1.0] | Green channel |
| b | float | [0.0, 1.0] | Blue channel |
| a | float | [0.0, 1.0] | Alpha (opacity: 0.0 = transparent, 1.0 = opaque) |

**Default**: `{1.0f, 1.0f, 1.0f, 1.0f}` — opaque white (no tint)

**Constraints**: Values outside [0.0, 1.0] are clamped by the GPU blend unit. No CPU-side validation required.

---

## Entity 3: SpriteVertex

**Location**: `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`
**Namespace**: `LongXi`
**Kind**: POD struct (GPU vertex layout)

**Fields**:

| Field | Type | HLSL Semantic | Description |
|-------|------|---------------|-------------|
| Position | Vector2 | POSITION | Screen-space pixel coordinate of vertex |
| UV | Vector2 | TEXCOORD0 | Normalized texture coordinate [0,1] |
| Color | Color | COLOR0 | Per-vertex RGBA tint |

**Size**: 32 bytes (2×float2 + float4)
**Total buffer capacity**: `MAX_SPRITES_PER_BATCH × 4 vertices × 32 bytes = 131,072 bytes (128 KB)`

**Quad vertex layout** (for one sprite at position P, size S):
```
Vertex 0 (TL): Position={P.x,       P.y      }, UV={UVMin.x, UVMin.y}
Vertex 1 (TR): Position={P.x + S.x, P.y      }, UV={UVMax.x, UVMin.y}
Vertex 2 (BL): Position={P.x,       P.y + S.y}, UV={UVMin.x, UVMax.y}
Vertex 3 (BR): Position={P.x + S.x, P.y + S.y}, UV={UVMax.x, UVMax.y}
```
All four vertices share the same Color value.

---

## Entity 4: Sprite (Batch Entry)

**Location**: Internal to `SpriteRenderer.cpp` (not exposed in public header)
**Namespace**: `LongXi`
**Kind**: POD struct (CPU batch buffer entry)

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| texture | `Texture*` | Non-owning pointer to texture (TextureManager owns lifetime) |
| position | Vector2 | Screen-space top-left position in pixels |
| size | Vector2 | Width and height in pixels |
| uvMin | Vector2 | Top-left UV (default: {0.0, 0.0}) |
| uvMax | Vector2 | Bottom-right UV (default: {1.0, 1.0}) |
| color | Color | Per-sprite RGBA tint (default: {1,1,1,1}) |

**Lifetime**: Exists only within a single `Begin()`→`End()` block. Batch buffer is reset at the start of each `Begin()` call.

**Constraints**:
- `texture` must never be null (guarded by DrawSprite before insertion)
- If `size.x <= 0 || size.y <= 0`, the sprite is skipped (no visible geometry)

---

## Entity 5: SpriteRenderer

**Location**: `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h/.cpp`
**Namespace**: `LongXi`
**Kind**: Subsystem class (owned by Engine via `std::unique_ptr`)

### Owned GPU Resources

| Resource | D3D11 Type | Usage | Created In |
|----------|------------|-------|-----------|
| Vertex Shader | `ID3D11VertexShader` | Transforms vertices to clip space | Initialize() |
| Pixel Shader | `ID3D11PixelShader` | Samples texture × vertex color | Initialize() |
| Input Layout | `ID3D11InputLayout` | Binds SpriteVertex format to IA | Initialize() |
| Vertex Buffer | `ID3D11Buffer` | DYNAMIC, 128KB, holds 4096 vertices | Initialize() |
| Index Buffer | `ID3D11Buffer` | IMMUTABLE, UINT16, 6144 indices | Initialize() |
| Constant Buffer | `ID3D11Buffer` | 64 bytes, holds 4×4 projection matrix | Initialize() |
| Blend State | `ID3D11BlendState` | SrcAlpha/InvSrcAlpha blend | Initialize() |
| Depth State | `ID3D11DepthStencilState` | Depth test+write disabled | Initialize() |
| Sampler State | `ID3D11SamplerState` | Linear filter, clamp address | Initialize() |
| Rasterizer State | `ID3D11RasterizerState` | No cull, solid fill | Initialize() |

### Member State

| Member | Type | Description |
|--------|------|-------------|
| m_Renderer | `DX11Renderer&` | Reference to renderer for device/context access |
| m_VertexData | `SpriteVertex[4096]` | CPU-side vertex staging buffer |
| m_SpriteCount | `int` | Current number of sprites in the active batch |
| m_CurrentTexture | `Texture*` | Texture bound to the current batch (null = no batch active) |
| m_Initialized | `bool` | True if Initialize() succeeded |
| m_InBatch | `bool` | True between Begin() and End() calls |
| m_ProjectionMatrix | `float[16]` | Current orthographic projection (row-major) |

### Constant: `MAX_SPRITES_PER_BATCH = 1024`

---

## Entity 6: ProjectionConstantBuffer

**Location**: Internal to SpriteRenderer (GPU constant buffer, no public C++ type)
**Size**: 64 bytes (float4×4 = 16 floats × 4 bytes)
**HLSL register**: `b0`
**Shader stage**: Vertex shader only

**Layout** (row-major, matching C++ `float[16]` layout via `#pragma pack_matrix(row_major)`):
```
Row 0: [ 2/W,    0,  0, 0 ]
Row 1: [   0, -2/H,  0, 0 ]
Row 2: [   0,    0,  1, 0 ]
Row 3: [  -1,    1,  0, 1 ]
```
Updated on initialization and on every `OnResize()` call.

---

## State Machine: SpriteRenderer Lifecycle

```
[Uninitialized]
      │
      │ Initialize(renderer, w, h) → success
      ▼
[Initialized / Idle]  ←──────────────────────────────────┐
      │                                                   │
      │ Begin()                                           │ End() (batch flushed)
      ▼                                                   │
[In Batch]                                                │
      │                                                   │
      │ DrawSprite(...)                                   │
      │   ├── same texture, batch not full → append       │
      │   ├── different texture → flush current, start new│
      │   └── batch full (1024) → flush, start new        │
      ▼                                                   │
[In Batch]─────────────────────────────────────────────→─┘
      │
      │ Shutdown() [from any state]
      ▼
[Shutdown]

[Initialize failed] → [Disabled] → all calls are no-ops
```

### State Transitions

| Current State | Event | Next State | Action |
|--------------|-------|-----------|--------|
| Uninitialized | Initialize() success | Idle | Create GPU resources, compute projection |
| Uninitialized | Initialize() failure | Disabled | Log error, set m_Initialized=false |
| Idle | Begin() | In Batch | Reset batch (m_SpriteCount=0, m_CurrentTexture=null, m_InBatch=true) |
| In Batch | DrawSprite() (same texture, not full) | In Batch | Append 4 vertices to m_VertexData |
| In Batch | DrawSprite() (different texture) | In Batch | Flush current batch, reset, set new texture, append |
| In Batch | DrawSprite() (batch full) | In Batch | Flush, reset, append to new batch |
| In Batch | End() | Idle | Flush remaining batch (if any), set m_InBatch=false |
| Any enabled | OnResize(w>0, h>0) | Same | Recompute and upload projection matrix |
| Any enabled | OnResize(w≤0 or h≤0) | Same | Ignore (log warning) |
| Disabled | Any call | Disabled | Silent no-op |

---

## Entity Relationships

```
Engine
  └── owns (unique_ptr) ──────────────────────────────► SpriteRenderer
                                                              │
                                                              │ holds reference to
                                                              ▼
                                                         DX11Renderer
                                                         (GetDevice / GetContext)

Application / TestApplication
  └── calls ────────────────────────────────────────► Engine::GetSpriteRenderer()
                                                              │
                                                              └── DrawSprite(texture, ...)
                                                                        │
                                                                        │ non-owning ptr
                                                                        ▼
                                                                    Texture
                                                                    (owned by TextureManager)
                                                                    GetHandle().Get()
                                                                         │
                                                                         ▼
                                                               ID3D11ShaderResourceView*
```

---

## Modified Entities (Existing Codebase)

### DX11Renderer (modified)

Two public accessor methods added:

| Method | Return Type | Description |
|--------|-------------|-------------|
| `GetDevice()` | `ID3D11Device*` | Returns `m_Device.Get()` |
| `GetContext()` | `ID3D11DeviceContext*` | Returns `m_Context.Get()` |

### Engine (modified)

| Change | Details |
|--------|---------|
| New member | `std::unique_ptr<SpriteRenderer> m_SpriteRenderer` |
| `Initialize()` | Adds step 5: initialize SpriteRenderer (non-fatal failure) |
| `Shutdown()` | Adds step 0: reset SpriteRenderer first (before TextureManager) |
| `Render()` | Adds `m_SpriteRenderer->Begin()` / `End()` after scene placeholder |
| `OnResize()` | Adds `m_SpriteRenderer->OnResize(width, height)` call |
| New method | `SpriteRenderer& GetSpriteRenderer()` accessor |

### Engine.h (modified)

| Change | Details |
|--------|---------|
| Forward declaration | `class SpriteRenderer;` added |
| Public method | `SpriteRenderer& GetSpriteRenderer();` |
| Private member | `std::unique_ptr<SpriteRenderer> m_SpriteRenderer;` |

### LXEngine/premake5.lua (modified)

| Change | Details |
|--------|---------|
| links | Add `"d3dcompiler"` |

### LXEngine/LXEngine.h (modified)

| Change | Details |
|--------|---------|
| include | Add `#include "Renderer/SpriteRenderer.h"` |
| include | Add `#include "Math/Math.h"` |
