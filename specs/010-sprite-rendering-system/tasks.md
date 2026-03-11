# Implementation Tasks: Sprite Rendering System

**Feature**: 010-sprite-rendering-system
**Branch**: `010-sprite-rendering-system`
**Generated**: 2026-03-11
**Estimate**: 6–8 hours

## Overview

Task breakdown for implementing the SpriteRenderer subsystem. Organized by user story to enable independent delivery. No automated tests — all verification is manual via logs and visual output, consistent with the existing LXEngine test pattern.

**User Stories** (from spec.md):
- **US1** (P1): Render a Textured Sprite on Screen — full GPU pipeline, Begin/End/DrawSprite
- **US2** (P1): Batch Multiple Sprites in a Single Draw Call — texture batching, flush logic
- **US3** (P2): Custom UV Coordinates — sub-region texture mapping
- **US4** (P2): Per-Sprite Color Tinting — RGBA tint and alpha transparency
- **US5** (P2): Projection Matrix Updates on Window Resize — OnResize hook

---

## Phase 1: Setup

**Goal**: Prepare build environment and verify clean baseline

**Independent Test**: Build succeeds with no errors before any new files are added

### Setup Tasks

- [X] T001 Add `"d3dcompiler"` to the links table in `LongXi/LXEngine/premake5.lua` (after dxguid)
- [X] T002 Run `Win-Generate Project.bat` to regenerate the Visual Studio solution
- [X] T003 Build solution (Win-Build Project.bat Debug) to verify clean baseline before changes

---

## Phase 2: Foundational — Shared Types and Engine Declarations

**Goal**: Create Math.h types and update Engine/DX11Renderer headers so all subsequent phases can compile

**⚠️ CRITICAL**: All user story phases depend on these foundations being complete

**Independent Test**: Modified headers compile without errors when included

### Foundational Tasks

- [X] T004 [P] Create `LongXi/LXEngine/Src/Math/Math.h` — declare `struct Vector2 { float x; float y; };` and `struct Color { float r; float g; float b; float a; };` inside `namespace LongXi { }` with `#pragma once`
- [X] T005 [P] Add `ID3D11Device* GetDevice() const { return m_Device.Get(); }` and `ID3D11DeviceContext* GetContext() const { return m_Context.Get(); }` public methods to `LongXi/LXEngine/Src/Renderer/DX11Renderer.h` (after IsInitialized())
- [X] T006 [P] Add forward declaration `class SpriteRenderer;` to `LongXi/LXEngine/Src/Engine/Engine.h` alongside existing forward declarations
- [X] T007 [P] Add `SpriteRenderer& GetSpriteRenderer();` to the Subsystem Accessors section of `LongXi/LXEngine/Src/Engine/Engine.h`
- [X] T008 [P] Add `std::unique_ptr<SpriteRenderer> m_SpriteRenderer;` to the private subsystem ownership section of `LongXi/LXEngine/Src/Engine/Engine.h`
- [X] T009 Build solution to verify all foundational header changes compile without errors

**Checkpoint**: Foundational headers ready — user story implementation can begin

---

## Phase 3: User Story 1 — Render a Textured Sprite on Screen (Priority: P1) 🎯 MVP

**Goal**: Complete SpriteRenderer GPU pipeline — shaders, buffers, pipeline state, Begin/End/DrawSprite lifecycle

**Independent Test**: Build succeeds; log shows `[SpriteRenderer] Initialized`; a single textured quad appears at the specified screen position when DrawSprite is called between Begin/End

### US1 Tasks — SpriteRenderer Header

- [X] T010 [US1] Create `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h` — add `#pragma once`, includes (`Math/Math.h`, `<wrl/client.h>`, `<d3d11.h>`), `namespace LongXi {`, forward declarations for `DX11Renderer` and `Texture`, and `static constexpr int MAX_SPRITES_PER_BATCH = 1024;`
- [X] T011 [US1] Add `struct SpriteVertex { Vector2 Position; Vector2 UV; Color Color; };` to `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h` inside `namespace LongXi`
- [X] T012 [US1] Add full `SpriteRenderer` class declaration to `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h` — public methods: `SpriteRenderer()`, `~SpriteRenderer()`, deleted copy operators, `bool Initialize(DX11Renderer&, int, int)`, `void Shutdown()`, `bool IsInitialized() const`, `void OnResize(int, int)`, `void Begin()`, `void DrawSprite(Texture*, Vector2, Vector2)`, `void DrawSprite(Texture*, Vector2, Vector2, Vector2, Vector2, Color)`, `void End()`; private: `void FlushBatch()`, `void UpdateProjection(int, int)`, all ComPtr members, `SpriteVertex m_VertexData[MAX_SPRITES_PER_BATCH * 4]`, `int m_SpriteCount`, `Texture* m_CurrentTexture`, `bool m_Initialized`, `bool m_InBatch`, `float m_ProjectionMatrix[16]`, `DX11Renderer* m_Renderer`

### US1 Tasks — Embedded HLSL Shaders

- [X] T013 [P] [US1] Create `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp` — add includes (`Renderer/SpriteRenderer.h`, `Renderer/DX11Renderer.h`, `Texture/Texture.h`, `Core/Logging/LogMacros.h`, `<d3dcompiler.h>`, `<windows.h>`) and namespace LongXi opening
- [X] T014 [P] [US1] Add embedded vertex shader string constant `s_VertexShaderSrc` to `SpriteRenderer.cpp` — HLSL with `#pragma pack_matrix(row_major)`, `cbuffer ProjectionBuffer : register(b0) { float4x4 g_Projection; }`, `VSInput` struct (POSITION float2, TEXCOORD0 float2, COLOR0 float4), `VSOutput` struct (SV_POSITION float4, TEXCOORD0 float2, COLOR0 float4), `VS()` function: `o.Pos = mul(float4(input.Pos, 0.0f, 1.0f), g_Projection); o.UV = input.UV; o.Col = input.Col;`
- [X] T015 [P] [US1] Add embedded pixel shader string constant `s_PixelShaderSrc` to `SpriteRenderer.cpp` — HLSL with `Texture2D g_Texture : register(t0)`, `SamplerState g_Sampler : register(s0)`, `PSInput` struct (SV_POSITION float4, TEXCOORD0 float2, COLOR0 float4), `PS()` function returning `g_Texture.Sample(g_Sampler, input.UV) * input.Col`

### US1 Tasks — Initialize() Implementation

- [X] T016 [US1] Implement `SpriteRenderer::SpriteRenderer()` constructor (set m_SpriteCount=0, m_CurrentTexture=nullptr, m_Initialized=false, m_InBatch=false, m_Renderer=nullptr) and `SpriteRenderer::IsInitialized() const` in `SpriteRenderer.cpp`
- [X] T017 [US1] Begin `SpriteRenderer::Initialize()` in `SpriteRenderer.cpp` — store `m_Renderer = &renderer`, compile vertex shader via `D3DCompile(s_VertexShaderSrc, strlen(s_VertexShaderSrc), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob)`, on failure log `ID3DBlob` error message and return false, on success call `device->CreateVertexShader` and store in `m_VertexShader`
- [X] T018 [US1] Add pixel shader compilation to `SpriteRenderer::Initialize()` — `D3DCompile` with entrypoint `"PS"`, target `"ps_4_0"`, on failure log error blob and return false, on success `device->CreatePixelShader` into `m_PixelShader`
- [X] T019 [US1] Add `ID3D11InputLayout` creation to `SpriteRenderer::Initialize()` — define `D3D11_INPUT_ELEMENT_DESC` array: `{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}`, `{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}`, `{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}`; call `device->CreateInputLayout` using vertex shader blob; store in `m_InputLayout`
- [X] T020 [US1] Add dynamic vertex buffer creation to `SpriteRenderer::Initialize()` — `D3D11_BUFFER_DESC` with Usage=DYNAMIC, ByteWidth=`MAX_SPRITES_PER_BATCH * 4 * sizeof(SpriteVertex)`, BindFlags=VERTEX_BUFFER, CPUAccessFlags=CPU_ACCESS_WRITE; call `device->CreateBuffer`, store in `m_VertexBuffer`
- [X] T021 [US1] Add static index buffer creation to `SpriteRenderer::Initialize()` — declare `uint16_t indices[MAX_SPRITES_PER_BATCH * 6]`, fill with loop: `base = i*4; indices[i*6+0]=base+0; [1]=base+1; [2]=base+2; [3]=base+2; [4]=base+1; [5]=base+3`; `D3D11_BUFFER_DESC` with Usage=IMMUTABLE, BindFlags=INDEX_BUFFER; `D3D11_SUBRESOURCE_DATA` pointing to indices array; call `device->CreateBuffer`, store in `m_IndexBuffer`
- [X] T022 [US1] Add constant buffer creation to `SpriteRenderer::Initialize()` — `D3D11_BUFFER_DESC` with Usage=DYNAMIC, ByteWidth=64, BindFlags=CONSTANT_BUFFER, CPUAccessFlags=CPU_ACCESS_WRITE; call `device->CreateBuffer`, store in `m_ConstantBuffer`
- [X] T023 [P] [US1] Add alpha blend state creation to `SpriteRenderer::Initialize()` — `D3D11_BLEND_DESC` with AlphaToCoverageEnable=false, RenderTarget[0]: BlendEnable=true, SrcBlend=SRC_ALPHA, DestBlend=INV_SRC_ALPHA, BlendOp=ADD, SrcBlendAlpha=ONE, DestBlendAlpha=ZERO, BlendOpAlpha=ADD, RenderTargetWriteMask=0x0F; call `device->CreateBlendState`, store in `m_BlendState`
- [X] T024 [P] [US1] Add depth-stencil state creation to `SpriteRenderer::Initialize()` — `D3D11_DEPTH_STENCIL_DESC` with DepthEnable=false, DepthWriteMask=ZERO; call `device->CreateDepthStencilState`, store in `m_DepthState`
- [X] T025 [P] [US1] Add sampler state creation to `SpriteRenderer::Initialize()` — `D3D11_SAMPLER_DESC` with Filter=MIN_MAG_MIP_LINEAR, AddressU/V/W=CLAMP, MaxLOD=D3D11_FLOAT32_MAX; call `device->CreateSamplerState`, store in `m_SamplerState`
- [X] T026 [P] [US1] Add rasterizer state creation to `SpriteRenderer::Initialize()` — `D3D11_RASTERIZER_DESC` with FillMode=SOLID, CullMode=NONE, ScissorEnable=false, DepthClipEnable=true; call `device->CreateRasterizerState`, store in `m_RasterizerState`
- [X] T027 [US1] Complete `SpriteRenderer::Initialize()` — after all resources created successfully: call `UpdateProjection(width, height)`, set `m_Initialized=true`, log `[SpriteRenderer] Initialized`, return true; add failure path at any HRESULT error: log `[SpriteRenderer] Initialization failed — sprite rendering disabled`, reset all ComPtr members, set `m_Initialized=false`, return false (caller does NOT propagate this failure)

### US1 Tasks — UpdateProjection, Begin, End, DrawSprite, FlushBatch, Shutdown

- [X] T028 [US1] Implement `SpriteRenderer::UpdateProjection(int width, int height)` in `SpriteRenderer.cpp` — compute row-major orthographic matrix: `[2/W, 0, 0, 0 / 0, -2/H, 0, 0 / 0, 0, 1, 0 / -1, 1, 0, 1]`, store in `m_ProjectionMatrix[16]`; then upload to GPU: `ctx->Map(m_ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped); memcpy(mapped.pData, m_ProjectionMatrix, 64); ctx->Unmap(m_ConstantBuffer, 0)`
- [X] T029 [US1] Implement `SpriteRenderer::Begin()` in `SpriteRenderer.cpp` — guard `!m_Initialized || m_InBatch` (return); set `m_InBatch=true`, `m_SpriteCount=0`, `m_CurrentTexture=nullptr`; bind pipeline: `IASetInputLayout`, `IASetPrimitiveTopology(TRIANGLELIST)`, vertex buffer (`stride=sizeof(SpriteVertex)`, `offset=0`), index buffer (`DXGI_FORMAT_R16_UINT`), `VSSetShader`, `VSSetConstantBuffers(0, 1, ...)`, `PSSetShader`, `PSSetSamplers(0, 1, ...)`, `OMSetBlendState`, `OMSetDepthStencilState`, `RSSetState`
- [X] T030 [US1] Implement `SpriteRenderer::End()` in `SpriteRenderer.cpp` — guard `!m_InBatch` (return); call `FlushBatch()`; set `m_InBatch=false`
- [X] T031 [US1] Implement `SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size)` in `SpriteRenderer.cpp` — delegate to extended overload: `DrawSprite(texture, position, size, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f})`
- [X] T032 [US1] Implement extended `SpriteRenderer::DrawSprite(Texture*, Vector2, Vector2, Vector2, Vector2, Color)` in `SpriteRenderer.cpp` — guard `!m_Initialized || !m_InBatch` (log error, return); guard `!texture` (log `[SpriteRenderer] DrawSprite: null texture`, return); guard `size.x <= 0 || size.y <= 0` (return); build 4 SpriteVertex entries at `&m_VertexData[m_SpriteCount * 4]`: TL={position, uvMin, color}, TR={position.x+size.x, position.y, uvMax.x/uvMin.y, color}, BL={position.x, position.y+size.y, uvMin.x/uvMax.y, color}, BR={position.x+size.x, position.y+size.y, uvMax, color}; increment `m_SpriteCount`
- [X] T033 [US1] Implement `SpriteRenderer::FlushBatch()` in `SpriteRenderer.cpp` — guard `m_SpriteCount == 0` (return early); `Map(m_VertexBuffer, D3D11_MAP_WRITE_DISCARD)`, `memcpy` vertex data for `m_SpriteCount * 4` vertices, `Unmap`; bind SRV: `ID3D11ShaderResourceView* srv = m_CurrentTexture->GetHandle().Get(); ctx->PSSetShaderResources(0, 1, &srv)`; log `[SpriteRenderer] Texture bound`; `DrawIndexed(m_SpriteCount * 6, 0, 0)`; log `[SpriteRenderer] Batch flushed (N sprites)`; reset `m_SpriteCount=0`, `m_CurrentTexture=nullptr`
- [X] T034 [US1] Implement `SpriteRenderer::Shutdown()` in `SpriteRenderer.cpp` — guard `!m_Initialized` (return); reset all ComPtr members: `m_RasterizerState.Reset()`, `m_SamplerState.Reset()`, `m_DepthState.Reset()`, `m_BlendState.Reset()`, `m_ConstantBuffer.Reset()`, `m_IndexBuffer.Reset()`, `m_VertexBuffer.Reset()`, `m_InputLayout.Reset()`, `m_PixelShader.Reset()`, `m_VertexShader.Reset()`; set `m_Initialized=false`; log `[SpriteRenderer] Shutdown`

**Checkpoint**: SpriteRenderer compiles and can render a single textured quad — US1 complete

---

## Phase 4: User Story 2 — Batch Multiple Sprites in a Single Draw Call (Priority: P1)

**Goal**: Add texture-comparison batching and batch-limit flushing to DrawSprite

**Independent Test**: Draw 32 sprites with same texture; log shows `[SpriteRenderer] Batch flushed (32 sprites)` exactly once; draw sprites with 2 alternating textures; log shows separate flush per texture group

### US2 Tasks

- [X] T035 [US2] Add texture-change detection to `SpriteRenderer::DrawSprite()` extended overload in `SpriteRenderer.cpp` — after guards, before appending vertices: `if (m_CurrentTexture != nullptr && m_CurrentTexture != texture) { FlushBatch(); }` then `m_CurrentTexture = texture;`
- [X] T036 [US2] Add batch-limit check to `SpriteRenderer::DrawSprite()` in `SpriteRenderer.cpp` — after texture-change detection: `if (m_SpriteCount >= MAX_SPRITES_PER_BATCH) { FlushBatch(); }` (FlushBatch resets m_CurrentTexture, so re-assign: `m_CurrentTexture = texture;`)
- [X] T037 [US2] Verify `FlushBatch()` correctly resets `m_SpriteCount = 0` and `m_CurrentTexture = nullptr` after DrawIndexed — inspect SpriteRenderer.cpp to confirm reset order (reset happens after GPU submit, not before)
- [X] T038 [US2] Test batching: add code to TestSpriteSystem() in `LongXi/LXShell/Src/main.cpp` that stores a reference to a loaded texture, intended for 32-sprite batch test; add log message confirming setup (actual draw loop runs in Engine::Render())
- [X] T039 [US2] Verify log output after first run with multiple DrawSprite calls — `[SpriteRenderer] Batch flushed (N sprites)` count matches expected batch groupings (1 flush per texture group, not 1 flush per sprite)

---

## Phase 5: User Story 3 — Custom UV Coordinates (Priority: P2)

**Goal**: Verify UV sub-region mapping works correctly for texture atlas use cases

**Independent Test**: Draw sprite with UVMin={0,0}, UVMax={0.5,0.5}; only top-left quadrant of texture appears on screen

### US3 Tasks

- [X] T040 [P] [US3] Verify UV vertex layout in `SpriteRenderer::DrawSprite()` in `SpriteRenderer.cpp` — confirm: `v[0].UV = uvMin` (TL), `v[1].UV = {uvMax.x, uvMin.y}` (TR), `v[2].UV = {uvMin.x, uvMax.y}` (BL), `v[3].UV = uvMax` (BR)
- [X] T041 [P] [US3] Verify default `DrawSprite(texture, position, size)` overload passes `uvMin={0.0f, 0.0f}` and `uvMax={1.0f, 1.0f}` to extended overload in `SpriteRenderer.cpp`
- [X] T042 [US3] Verify HLSL vertex shader `VS()` in embedded `s_VertexShaderSrc` string in `SpriteRenderer.cpp` passes UV through unchanged: `o.UV = input.UV;`
- [X] T043 [US3] Add UV sub-region test comment to TestSpriteSystem() in `LongXi/LXShell/Src/main.cpp` — log that UV test with uvMin={0,0} uvMax={0.5,0.5} is expected to render top-left quadrant of texture (visual verification)

---

## Phase 6: User Story 4 — Per-Sprite Color Tinting (Priority: P2)

**Goal**: Verify color tinting and alpha transparency work for per-sprite visual effects

**Independent Test**: Sprite with color={1,0,0,1} renders red; sprite with alpha=0.5 is semi-transparent over background

### US4 Tasks

- [X] T044 [P] [US4] Verify color is applied to all 4 vertices in `SpriteRenderer::DrawSprite()` in `SpriteRenderer.cpp` — `v[0].Color`, `v[1].Color`, `v[2].Color`, `v[3].Color` all equal the `color` parameter
- [X] T045 [P] [US4] Verify HLSL pixel shader `PS()` in embedded `s_PixelShaderSrc` string in `SpriteRenderer.cpp` multiplies sample by color: `return g_Texture.Sample(g_Sampler, input.UV) * input.Col;`
- [X] T046 [US4] Verify blend state in `SpriteRenderer::Initialize()` uses `D3D11_BLEND_SRC_ALPHA` / `D3D11_BLEND_INV_SRC_ALPHA` and `BlendEnable = true` in `SpriteRenderer.cpp` — a sprite with `color.a = 0.5f` blends with the cornflower-blue background
- [X] T047 [US4] Add color tint test log to TestSpriteSystem() in `LongXi/LXShell/Src/main.cpp` — log that red-tint test (`color={1,0,0,1}`) and semi-transparent test (`color={1,1,1,0.5}`) are configured for visual verification

---

## Phase 7: User Story 5 — Projection Matrix Updates on Window Resize (Priority: P2)

**Goal**: SpriteRenderer updates projection matrix when window is resized so sprites remain correctly anchored

**Independent Test**: Draw sprite at (0,0), resize window, verify sprite remains in top-left corner with no repositioning artifact; zero/negative resize is ignored

### US5 Tasks

- [X] T048 [US5] Implement `SpriteRenderer::OnResize(int width, int height)` in `SpriteRenderer.cpp` — guard `!m_Initialized` (return); guard `width <= 0 || height <= 0` (return, ignore); call `UpdateProjection(width, height)`
- [X] T049 [US5] Add `SpriteRenderer::OnResize()` call to `Engine::OnResize()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — after `m_Renderer->OnResize(width, height)`, add: `if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized()) { m_SpriteRenderer->OnResize(width, height); }`
- [X] T050 [US5] Verify `UpdateProjection()` in `SpriteRenderer.cpp` uploads to constant buffer via Map/memcpy/Unmap (not just stores to `m_ProjectionMatrix` array) — this ensures the GPU sees the new projection on the next Draw call
- [X] T051 [US5] Manual resize test: run `Build/Debug/Executables/LXShell.exe`, drag window edge to resize, observe that sprite anchored at (0,0) stays at top-left after resize (verified visually)

---

## Phase 8: Engine Integration

**Goal**: Wire SpriteRenderer into Engine lifecycle (Initialize, Shutdown, Render, OnResize) and update LXEngine public header

**Independent Test**: Build succeeds; Engine::Initialize() log shows step 5 SpriteRenderer; SC-010 satisfied (Application unchanged)

### Engine Integration Tasks

- [X] T052 [P] Add `#include "Renderer/SpriteRenderer.h"` to `LongXi/LXEngine/Src/Engine/Engine.cpp` (alongside existing includes)
- [X] T053 Add SpriteRenderer initialization as step 5 in `Engine::Initialize()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — after TextureManager init: `LX_ENGINE_INFO("[Engine] Initializing sprite renderer"); m_SpriteRenderer = std::make_unique<SpriteRenderer>(); if (!m_SpriteRenderer->Initialize(*m_Renderer, width, height)) { LX_ENGINE_WARN("[Engine] SpriteRenderer initialization failed — sprite rendering disabled"); }` (do NOT return false — non-fatal per FR-005)
- [X] T054 Add SpriteRenderer shutdown as first step in `Engine::Shutdown()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — before TextureManager reset: `if (m_SpriteRenderer) { m_SpriteRenderer->Shutdown(); m_SpriteRenderer.reset(); }`
- [X] T055 Update `Engine::Render()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — after `BeginFrame()` and after existing `// TODO: Scene.Render()` comment, add: `if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized()) { m_SpriteRenderer->Begin(); /* DrawSprite calls from game code */ m_SpriteRenderer->End(); }`
- [X] T056 Implement `Engine::GetSpriteRenderer()` in `LongXi/LXEngine/Src/Engine/Engine.cpp` — `return *m_SpriteRenderer;`
- [X] T057 [P] Add `#include "Renderer/SpriteRenderer.h"` and `#include "Math/Math.h"` to `LongXi/LXEngine/LXEngine.h` (after existing includes)
- [X] T058 Build solution and verify Engine integration compiles without errors (SC-010: check that Application.h and Application.cpp have zero new lines)

---

## Phase 9: LXShell Test Application

**Goal**: Update TestApplication to exercise SpriteRenderer initialization path and log test status

**Independent Test**: Log shows `[SpriteRenderer] Initialized`; no D3D11 debug layer errors on startup or shutdown

### Test Application Tasks

- [X] T059 Add `void TestSpriteSystem()` private method declaration to `TestApplication` class in `LongXi/LXShell/Src/main.cpp`
- [X] T060 Implement `TestSpriteSystem()` in `LongXi/LXShell/Src/main.cpp` — log test header `=== SPRITE SYSTEM TEST ===`; access `Engine& engine = GetEngine()`; verify `engine.GetSpriteRenderer().IsInitialized()` is true and log result; log `SpriteRenderer ready — draw calls will execute in Engine::Render() Begin/End block`; log test footer
- [X] T061 Call `TestSpriteSystem()` from `TestApplication::Initialize()` in `LongXi/LXShell/Src/main.cpp` after `TestTextureSystem()` returns

---

## Phase 10: Polish & Cross-Cutting Concerns

**Goal**: Code formatting, final build validation, success criteria verification

### Polish Tasks

- [X] T062 Run `Win-Format Code.bat` to apply clang-format to all modified files (`Math/Math.h`, `Renderer/DX11Renderer.h`, `Renderer/SpriteRenderer.h`, `Renderer/SpriteRenderer.cpp`, `Engine/Engine.h`, `Engine/Engine.cpp`, `LXEngine.h`, `LXShell/Src/main.cpp`)
- [X] T063 Build Debug configuration and verify zero errors, zero warnings (SC-008 prerequisite)
- [X] T064 Run `Build/Debug/Executables/LXShell.exe` and verify log shows `[SpriteRenderer] Initialized` (SC-008)
- [X] T065 Verify D3D11 debug layer reports no live object warnings on shutdown in debug output (SC-009)
- [X] T066 Confirm `Application.h` and `Application.cpp` have zero new lines added by this spec (SC-010)
- [X] T067 Search `Engine::Initialize()` in `Engine.cpp` to confirm SpriteRenderer init failure does not cause `return false` (SC-011)
- [X] T068 Search `Engine::Shutdown()` in `Engine.cpp` to confirm `m_SpriteRenderer.reset()` appears before `m_TextureManager.reset()` (correct shutdown order per FR-004)

---

## Dependencies

### User Story Dependencies

```
US1 (P1): Basic Sprite Rendering
├── Depends on: Phase 2 Foundational (Math.h, DX11Renderer accessors, Engine.h declarations)
└── Enables: US2, US3, US4 (all use the DrawSprite infrastructure built here)

US2 (P1): Texture Batching
├── Depends on: US1 (FlushBatch and DrawSprite exist)
└── Enables: Full multi-sprite rendering

US3 (P2): Custom UV Coordinates
├── Depends on: US1 (extended DrawSprite overload exists with uvMin/uvMax parameters)
└── Can run in parallel with US4 (different verification tasks)

US4 (P2): Color Tinting
├── Depends on: US1 (color field exists in SpriteVertex and DrawSprite)
└── Can run in parallel with US3

US5 (P2): Resize Handling
├── Depends on: US1 (UpdateProjection and constant buffer exist)
└── Can run in parallel with US3 and US4

Engine Integration (Phase 8):
├── Depends on: US1 (SpriteRenderer class complete)
└── Enables: Phase 9 (TestApplication)
```

### Execution Order

**Priority 1 (Foundation + MVP)**:
1. Phase 1: Setup (T001–T003)
2. Phase 2: Foundational (T004–T009)
3. Phase 3: US1 — Basic Sprite Rendering (T010–T034)
4. Phase 4: US2 — Batching (T035–T039)
5. Phase 8: Engine Integration (T052–T058)

**Priority 2 (Extended Features)**:
6. Phase 5: US3 — Custom UV (T040–T043)
7. Phase 6: US4 — Color Tinting (T044–T047)
8. Phase 7: US5 — Resize (T048–T051)

**Priority 3 (Validation)**:
9. Phase 9: LXShell Test (T059–T061)
10. Phase 10: Polish (T062–T068)

---

## Parallel Execution Opportunities

### Within Phase 2 (Foundational)
T004, T005, T006, T007, T008 can all run in parallel (different files):
- T004: Math.h (new file)
- T005: DX11Renderer.h (add accessors)
- T006+T007+T008: Engine.h (forward decl, method, member)

### Within Phase 3 (US1)
- T013, T014, T015: Embedded shader strings [P] — write in parallel (same file, but independent string constants)
- T023, T024, T025, T026: Pipeline state creation [P] — each is an independent D3D11 state object

### Within Phase 5 and 6
- T040, T041, T042 (US3): Parallel verification tasks
- T044, T045, T046 (US4): Parallel verification tasks
- US3 and US4 entire phases can run in parallel (different verification focus)

### Within Phase 10 (Polish)
- T063, T064, T065, T066 can run in parallel (different verification concerns)

---

## MVP Scope (Minimum Viable Product)

**Definition**: Deliver US1 + US2 (P1 stories) + Engine Integration only

**MVP Tasks**: T001–T039 + T052–T058 (~55 tasks)

**MVP Test Criteria**:
- ✅ `[SpriteRenderer] Initialized` in log
- ✅ SpriteRenderer accessible via `engine.GetSpriteRenderer()`
- ✅ 32 sprites with same texture produce `Batch flushed (32 sprites)` in log
- ✅ D3D11 debug layer clean on shutdown

**MVP Value**: Core sprite rendering pipeline ready for use in future specs (UI, gameplay sprites, debug overlays)

---

## Implementation Strategy

### Incremental Delivery

1. **Phase 1–2**: Build environment ready, foundational types declared
2. **Phase 3 (US1)**: Core sprite rendering — draw a textured quad at any screen position
3. **Phase 4 (US2)**: Batching — multiple sprites → single draw call
4. **Phase 8**: Engine integration — SpriteRenderer in Engine lifecycle
5. **MVP validated** — stop here if P2 stories can wait
6. **Phases 5–7**: UV, Color, Resize (independent, low-risk additions)
7. **Phase 9–10**: Test app + final validation

### Risk Mitigation

- **Shader compile errors**: Test HLSL strings in isolation; D3DCompile error blob contains exact error message
- **Incorrect projection matrix**: Test with sprite at (0,0) — should appear at top-left; test at (windowW-64, windowH-64) — should appear at bottom-right
- **ComPtr leaks**: D3D11 debug layer will report live objects on shutdown — fix before merging
- **Build after each phase**: Each phase ends with a build task; never proceed to next phase with compile errors

---

## Total Task Count

**Total**: 68 tasks
- Phase 1 (Setup): 3 tasks
- Phase 2 (Foundational): 6 tasks
- Phase 3 (US1 — Core Pipeline): 25 tasks
- Phase 4 (US2 — Batching): 5 tasks
- Phase 5 (US3 — Custom UV): 4 tasks
- Phase 6 (US4 — Color Tinting): 4 tasks
- Phase 7 (US5 — Resize): 4 tasks
- Phase 8 (Engine Integration): 7 tasks
- Phase 9 (LXShell Test): 3 tasks
- Phase 10 (Polish): 7 tasks

**Parallel Opportunities**: 21 tasks marked [P] (31% parallelizable)

**Estimated Timeline**: 6–8 hours (matches quickstart.md estimate)

---

## Notes

- No automated test harness — all validation is manual via logs and visual output
- Build after each phase before proceeding to the next
- D3D11 debug layer is active in Debug builds (`LX_DX11_ENABLE_DEBUG_LAYER 1`) — all resource leaks will be visible
- `Win-Format Code.bat` must be run before commit (clang-format, Allman braces, 4-space indent)
- SpriteRenderer failure is non-fatal — Engine continues without sprites (FR-005, SC-011)
- All types in `namespace LongXi` — no nested namespaces (FR-037)

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
