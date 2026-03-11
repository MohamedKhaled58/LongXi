# Implementation Plan: Sprite Rendering System

**Branch**: `010-sprite-rendering-system` | **Date**: 2026-03-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-sprite-rendering-system/spec.md`

## Summary

Introduce `SpriteRenderer` as Engine subsystem #5, providing a batched textured-quad renderer for screen-space 2D drawing. The system uses a dynamic D3D11 vertex buffer, a static index buffer, runtime-compiled HLSL shaders, and an orthographic projection matrix. Sprites are batched by texture and flushed via `DrawIndexed`. Integration points: DX11Renderer (device/context), Texture (SRV handle), Engine (subsystem lifecycle + Render/OnResize hooks). Also introduces `Math/Math.h` (Vector2, Color) as a shared LXEngine math header.

## Technical Context

**Language/Version**: C++23, MSVC v145
**Primary Dependencies**: DirectX 11 (d3d11, dxgi, d3dcompiler), WRL ComPtr, existing LXEngine subsystems (DX11Renderer, TextureManager, Texture)
**Storage**: N/A (GPU buffers managed by D3D11 and ComPtr RAII)
**Testing**: Manual visual + log-driven (no automated test harness; consistent with existing codebase pattern)
**Target Platform**: Windows 10+, x64
**Project Type**: Static library extension (LXEngine)
**Performance Goals**: Batch 1024 sprites per draw call; flush latency < 1ms; no per-frame heap allocations
**Constraints**: Single-threaded (Phase 1 rule); no new modules/projects; no external math library; d3dcompiler runtime available on Windows 10+
**Scale/Scope**: Single subsystem, ~400 lines of new C++, 2 new source files, 1 new header (Math.h), modifications to 4 existing files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Article | Requirement | Status | Notes |
|---------|-------------|--------|-------|
| III — Platform | DirectX 11 only | ✅ PASS | SpriteRenderer uses D3D11 exclusively |
| III — Module Structure | Static libs, LXEngine layer | ✅ PASS | New files added to LXEngine; no new module |
| III — Dependency Direction | LXShell → LXEngine → LXCore | ✅ PASS | No reverse dependencies introduced |
| III — Runtime | Single-threaded Phase 1 | ✅ PASS | All sprite operations on main thread |
| IV — Entrypoint Discipline | Engine::Initialize() stays thin | ✅ PASS | SpriteRenderer init is a 3-line delegation |
| IV — Module Boundaries | No cross-layer coupling | ✅ PASS | SpriteRenderer only accesses DX11Renderer and Texture |
| IV — Abstraction Discipline | No premature abstraction | ✅ PASS | Math.h is earned (Vector2/Color used immediately by SpriteVertex and DrawSprite API) |
| IV — Ownership | Rendering MUST NOT own gameplay truth | ✅ PASS | SpriteRenderer is pure presentation layer |
| V — Multiplayer Readiness | Presentation separable from state | ✅ PASS | Sprite positions are caller-supplied; SpriteRenderer holds no game state |
| IX — Threading | No premature threading | ✅ PASS | No background threads; MT-ready (no global mutable state) |
| XII — Phase 1 Scope | DX11 rendering path | ✅ PASS | Sprite rendering is within Phase 1 scope (DirectX 11 initialization and render path) |

**Complexity Tracking**: No constitution violations. No justification table required.

## Project Structure

### Documentation (this feature)

```text
specs/010-sprite-rendering-system/
├── plan.md              # This file
├── research.md          # Phase 0 — all decisions resolved
├── data-model.md        # Phase 1 — entities, state machine, modified types
├── quickstart.md        # Phase 1 — step-by-step implementation guide
├── contracts/
│   └── sprite-renderer-api.md   # Public API surface contract
└── tasks.md             # Phase 2 output (/speckit.tasks — not yet created)
```

### Source Code (new files)

```text
LongXi/LXEngine/Src/
├── Math/
│   └── Math.h                          # NEW — Vector2, Color (shared LXEngine math types)
└── Renderer/
    ├── DX11Renderer.h                  # MODIFIED — add GetDevice(), GetContext()
    ├── SpriteRenderer.h                # NEW — SpriteRenderer class declaration
    └── SpriteRenderer.cpp              # NEW — full implementation + embedded HLSL
```

### Modified Existing Files

```text
LongXi/LXEngine/Src/Engine/Engine.h    # MODIFIED — add SpriteRenderer forward decl + member + accessor
LongXi/LXEngine/Src/Engine/Engine.cpp  # MODIFIED — add init/shutdown/render/resize hooks
LongXi/LXEngine/LXEngine.h             # MODIFIED — add SpriteRenderer.h + Math.h includes
LongXi/LXEngine/premake5.lua           # MODIFIED — add d3dcompiler link
LongXi/LXShell/Src/main.cpp            # MODIFIED — add sprite test call
```

**Structure Decision**: Single project extension. No new Premake projects. All new source files land in `LXEngine/Src/` under appropriate subdirectories, picked up by the existing `Src/**.h` / `Src/**.cpp` wildcard glob.

---

## Phase 0: Research Findings Summary

All research complete. See [research.md](./research.md) for full rationale.

| Decision | Choice |
|----------|--------|
| Device/context access | Add `GetDevice()` / `GetContext()` to DX11Renderer |
| Texture SRV | `Texture::GetHandle().Get()` — no Texture changes needed |
| HLSL strategy | Inline string literals, D3DCompile at runtime |
| Projection matrix | Hand-computed row-major; `#pragma pack_matrix(row_major)` in HLSL |
| Vertex buffer | `D3D11_USAGE_DYNAMIC`, `MAP_WRITE_DISCARD` per flush |
| Index buffer | `D3D11_USAGE_IMMUTABLE`, UINT16, pre-generated |
| Pipeline states | Alpha blend, depth disabled, linear-clamp sampler, no-cull rasterizer |
| Init signature | `Initialize(DX11Renderer&, int, int) → bool` (non-fatal failure) |
| Math types | New `LongXi/LXEngine/Src/Math/Math.h` — Vector2, Color |

---

## Phase 1: Design Decisions

### Initialization Order in Engine

```
Step 1: DX11Renderer
Step 2: InputSystem
Step 3: VirtualFileSystem
Step 4: TextureManager
Step 5: SpriteRenderer  ← NEW (non-fatal if Initialize() returns false)
```

### Shutdown Order in Engine (reverse)

```
Step 0: SpriteRenderer  ← NEW (first — releases GPU resources before device)
Step 1: TextureManager
Step 2: VirtualFileSystem
Step 3: InputSystem
Step 4: DX11Renderer    ← last (device destroyed last)
```

### Render Frame Order

```
DX11Renderer::BeginFrame()
  → [future Scene::Render()]
  → SpriteRenderer::Begin()
      → game/test DrawSprite calls
  → SpriteRenderer::End()
DX11Renderer::EndFrame()
```

### GPU Resource Budget (per SpriteRenderer instance)

| Resource | Size |
|----------|------|
| Vertex buffer (dynamic) | 4096 vertices × 32 bytes = 128 KB |
| Index buffer (immutable) | 6144 × 2 bytes = 12 KB |
| Constant buffer | 64 bytes (4×4 float) |
| Vertex shader bytecode | ~1 KB (estimated) |
| Pixel shader bytecode | ~1 KB (estimated) |
| **Total** | **~142 KB GPU** |

### Key Constraint: Engine::Render() owns Begin()/End()

The Engine::Render() method owns the Begin()/End() lifecycle. Client code (TestApplication, future game systems) calls `DrawSprite()` but does NOT call `Begin()`/`End()` directly. This requires a mechanism for client code to issue sprite draw calls within the Engine::Render() frame.

**Phase 1 approach**: Engine::Render() calls a protected virtual `OnRender()` method on Application (or directly runs a test sequence from Engine::Render()). This is a deferred decision — `/speckit.tasks` will address the exact hook mechanism. For now, the test in LXShell validates initialization and logging; actual sprite visibility is validated by adding a temporary DrawSprite call inside Engine::Render() for the test build.

---

## Agent Context Update

Run after Phase 1 design to update agent context with new technology:
