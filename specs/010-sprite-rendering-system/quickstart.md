# Quickstart: Sprite Rendering System

**Feature**: 010-sprite-rendering-system
**Branch**: `010-sprite-rendering-system`
**Estimated Effort**: 6–8 hours

## Overview

Step-by-step implementation guide for the SpriteRenderer subsystem. Follow this order exactly — each step compiles before the next begins.

---

## Step 1: Add `d3dcompiler` to LXEngine build (5 min)

**File**: `LongXi/LXEngine/premake5.lua`

Add `"d3dcompiler"` to the `links` table:

```lua
links
{
    "LXCore",
    "d3d11",
    "dxgi",
    "dxguid",
    "d3dcompiler"   -- ADD THIS
}
```

Run `Win-Generate Project.bat` to regenerate the solution.

**Verification**: Project regenerates without error.

---

## Step 2: Create `Math/Math.h` — Vector2 and Color (15 min)

**File**: `LongXi/LXEngine/Src/Math/Math.h` (new file)

```cpp
#pragma once

namespace LongXi
{

struct Vector2
{
    float x;
    float y;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

} // namespace LongXi
```

**Verification**: Compiles as a standalone include.

---

## Step 3: Add `GetDevice()` and `GetContext()` to DX11Renderer (15 min)

**File**: `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`

Add to the public section after `IsInitialized()`:

```cpp
// Device and context accessors for subsystems (e.g., SpriteRenderer)
ID3D11Device*        GetDevice()  const { return m_Device.Get();  }
ID3D11DeviceContext* GetContext() const { return m_Context.Get(); }
```

**Verification**: Build succeeds. Existing code is unchanged.

---

## Step 4: Create `SpriteRenderer.h` (20 min)

**File**: `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h` (new file)

```cpp
#pragma once

#include "Math/Math.h"
#include <wrl/client.h>
#include <d3d11.h>

namespace LongXi
{

class DX11Renderer;
class Texture;

// Maximum sprites batched per draw call
static constexpr int MAX_SPRITES_PER_BATCH = 1024;

struct SpriteVertex
{
    Vector2 Position;
    Vector2 UV;
    Color   Color;
};

class SpriteRenderer
{
  public:
    SpriteRenderer();
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    bool Initialize(DX11Renderer& renderer, int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    void OnResize(int width, int height);

    void Begin();
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size);
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size,
                    Vector2 uvMin, Vector2 uvMax, LongXi::Color color);
    void End();

  private:
    void FlushBatch();
    void UpdateProjection(int width, int height);

    DX11Renderer* m_Renderer = nullptr;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_PixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_InputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_BlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_SamplerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_RasterizerState;

    SpriteVertex m_VertexData[MAX_SPRITES_PER_BATCH * 4];
    int          m_SpriteCount    = 0;
    Texture*     m_CurrentTexture = nullptr;
    bool         m_Initialized    = false;
    bool         m_InBatch        = false;

    float m_ProjectionMatrix[16] = {};
};

} // namespace LongXi
```

**Verification**: Header compiles.

---

## Step 5: Create `SpriteRenderer.cpp` — Initialize (60 min)

**File**: `LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp` (new file)

### 5a: HLSL Shaders (embedded strings)

```cpp
static const char* s_VertexShaderSrc = R"(
#pragma pack_matrix(row_major)
cbuffer ProjectionBuffer : register(b0)
{
    float4x4 g_Projection;
};
struct VSInput  { float2 Pos : POSITION; float2 UV : TEXCOORD0; float4 Col : COLOR0; };
struct VSOutput { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; float4 Col : COLOR0; };
VSOutput VS(VSInput input)
{
    VSOutput o;
    o.Pos = mul(float4(input.Pos, 0.0f, 1.0f), g_Projection);
    o.UV  = input.UV;
    o.Col = input.Col;
    return o;
}
)";

static const char* s_PixelShaderSrc = R"(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);
struct PSInput { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; float4 Col : COLOR0; };
float4 PS(PSInput input) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, input.UV) * input.Col;
}
)";
```

### 5b: Projection matrix computation

Maps screen space [0,W]×[0,H] to clip space [-1,1]×[1,-1]:

```cpp
void SpriteRenderer::UpdateProjection(int width, int height)
{
    float w = static_cast<float>(width);
    float h = static_cast<float>(height);

    // Row-major 4x4 orthographic projection
    // (matches HLSL with #pragma pack_matrix(row_major))
    float proj[16] = {
         2.0f / w,  0.0f,     0.0f, 0.0f,
         0.0f,     -2.0f / h, 0.0f, 0.0f,
         0.0f,      0.0f,     1.0f, 0.0f,
        -1.0f,      1.0f,     0.0f, 1.0f,
    };
    memcpy(m_ProjectionMatrix, proj, sizeof(proj));
}
```

### 5c: Index buffer pre-generation

```cpp
// Inside Initialize(), when creating index buffer:
uint16_t indices[MAX_SPRITES_PER_BATCH * 6];
for (int i = 0; i < MAX_SPRITES_PER_BATCH; ++i)
{
    uint16_t base = static_cast<uint16_t>(i * 4);
    indices[i * 6 + 0] = base + 0;
    indices[i * 6 + 1] = base + 1;
    indices[i * 6 + 2] = base + 2;
    indices[i * 6 + 3] = base + 2;
    indices[i * 6 + 4] = base + 1;
    indices[i * 6 + 5] = base + 3;
}
```

### 5d: FlushBatch implementation

```cpp
void SpriteRenderer::FlushBatch()
{
    if (m_SpriteCount == 0 || !m_CurrentTexture)
        return;

    auto* ctx = m_Renderer->GetContext();

    // Upload vertex data (MAP_WRITE_DISCARD avoids GPU sync stall)
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(ctx->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, m_VertexData, m_SpriteCount * 4 * sizeof(SpriteVertex));
        ctx->Unmap(m_VertexBuffer.Get(), 0);
    }

    // Bind texture
    ID3D11ShaderResourceView* srv = m_CurrentTexture->GetHandle().Get();
    ctx->PSSetShaderResources(0, 1, &srv);
    LX_ENGINE_INFO("[SpriteRenderer] Texture bound");

    // Draw
    ctx->DrawIndexed(m_SpriteCount * 6, 0, 0);
    LX_ENGINE_INFO("[SpriteRenderer] Batch flushed ({} sprites)", m_SpriteCount);

    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;
}
```

**Verification**: Build succeeds with no errors or warnings.

---

## Step 6: Implement `Begin()` and `End()` (15 min)

```cpp
void SpriteRenderer::Begin()
{
    if (!m_Initialized || m_InBatch) return;
    m_InBatch = true;
    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;

    auto* ctx = m_Renderer->GetContext();

    // Bind pipeline state
    UINT stride = sizeof(SpriteVertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(m_InputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
    ctx->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
    ctx->OMSetBlendState(m_BlendState.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_DepthState.Get(), 0);
    ctx->RSSetState(m_RasterizerState.Get());
}

void SpriteRenderer::End()
{
    if (!m_Initialized || !m_InBatch) return;
    FlushBatch();
    m_InBatch = false;
}
```

---

## Step 7: Implement `DrawSprite()` (20 min)

```cpp
void SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size)
{
    DrawSprite(texture, position, size,
               {0.0f, 0.0f}, {1.0f, 1.0f},
               {1.0f, 1.0f, 1.0f, 1.0f});
}

void SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size,
                                Vector2 uvMin, Vector2 uvMax, LongXi::Color color)
{
    if (!m_Initialized || !m_InBatch) { LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite called outside Begin/End"); return; }
    if (!texture)                      { LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite: null texture"); return; }
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    // Flush if texture changed
    if (m_CurrentTexture != texture)
    {
        FlushBatch();
        m_CurrentTexture = texture;
    }

    // Flush if batch full
    if (m_SpriteCount >= MAX_SPRITES_PER_BATCH)
        FlushBatch();

    // Build 4 vertices
    SpriteVertex* v = &m_VertexData[m_SpriteCount * 4];
    v[0] = { {position.x,          position.y         }, {uvMin.x, uvMin.y}, color };
    v[1] = { {position.x + size.x, position.y         }, {uvMax.x, uvMin.y}, color };
    v[2] = { {position.x,          position.y + size.y}, {uvMin.x, uvMax.y}, color };
    v[3] = { {position.x + size.x, position.y + size.y}, {uvMax.x, uvMax.y}, color };
    ++m_SpriteCount;
}
```

---

## Step 8: Update Engine to own SpriteRenderer (30 min)

**File**: `LongXi/LXEngine/Src/Engine/Engine.h`

Add forward declaration and accessor:
```cpp
class SpriteRenderer;  // forward declare

// In public:
SpriteRenderer& GetSpriteRenderer();

// In private:
std::unique_ptr<SpriteRenderer> m_SpriteRenderer;
```

**File**: `LongXi/LXEngine/Src/Engine/Engine.cpp`

```cpp
#include "Renderer/SpriteRenderer.h"

// In Initialize() after TextureManager:
LX_ENGINE_INFO("[Engine] Initializing sprite renderer");
m_SpriteRenderer = std::make_unique<SpriteRenderer>();
if (!m_SpriteRenderer->Initialize(*m_Renderer, width, height))
{
    LX_ENGINE_WARN("[Engine] SpriteRenderer initialization failed — sprite rendering disabled");
    // Non-fatal: do not return false
}

// In Shutdown() BEFORE TextureManager reset:
if (m_SpriteRenderer) { m_SpriteRenderer->Shutdown(); m_SpriteRenderer.reset(); }

// In Render() after the scene placeholder:
if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized())
{
    m_SpriteRenderer->Begin();
    // Game/test code calls DrawSprite via Engine::GetSpriteRenderer()
    m_SpriteRenderer->End();
}

// In OnResize() after Renderer:
if (m_SpriteRenderer && m_SpriteRenderer->IsInitialized())
    m_SpriteRenderer->OnResize(width, height);
```

**Verification**: Build succeeds.

---

## Step 9: Update LXEngine.h (5 min)

**File**: `LongXi/LXEngine/LXEngine.h`

```cpp
#include "Renderer/SpriteRenderer.h"
#include "Math/Math.h"
```

---

## Step 10: Add sprite test to LXShell (20 min)

**File**: `LongXi/LXShell/Src/main.cpp`

In `TestApplication::Initialize()`, after `TestTextureSystem()`:

```cpp
void TestSpriteSystem()
{
    LX_ENGINE_INFO("==============================================");
    LX_ENGINE_INFO("SPRITE SYSTEM TEST");
    LX_ENGINE_INFO("==============================================");

    Engine& engine = GetEngine();
    auto tex = engine.GetTextureManager().LoadTexture("1.dds");
    if (!tex)
    {
        LX_ENGINE_ERROR("SPRITE TEST: Failed to load test texture");
        return;
    }

    // One-shot draw test — SpriteRenderer::Begin/End is called by Engine::Render()
    // For the test, override Render() or simply verify via logs
    LX_ENGINE_INFO("SPRITE TEST: Texture ready for sprite rendering");
    LX_ENGINE_INFO("==============================================");
    LX_ENGINE_INFO("SPRITE SYSTEM TEST COMPLETE");
    LX_ENGINE_INFO("==============================================");
}
```

**Note**: Since `Engine::Render()` owns the Begin/End loop, the test application draws sprites by overriding a virtual `OnRender()` method (to be added in a future spec) OR by temporarily drawing in the Initialize phase as a one-shot log test. For Phase 1, verifying that `[SpriteRenderer] Initialized` appears in the log is sufficient proof of SC-008.

---

## Step 11: Build and Verify (20 min)

```
Win-Build Project.bat
Build\Debug\Executables\LXShell.exe
```

**Expected log output**:
```
[LXEngine] [info] [Engine] Initializing sprite renderer
[LXEngine] [info] [SpriteRenderer] Initialized
```

**Verification Checklist**:

- ✅ `[SpriteRenderer] Initialized` appears in log (SC-008)
- ✅ No D3D11 debug layer errors during init
- ✅ Engine starts successfully even if SpriteRenderer were to fail (SC-011)
- ✅ `Engine::GetSpriteRenderer()` returns valid reference (SC-010 — Application unchanged)
- ✅ Log shows `[Engine] Initializing sprite renderer` as step 5
- ✅ `Win-Format Code.bat` applied before commit

---

## Common Issues

### Issue: `D3DCompile` unresolved external
**Fix**: Verify `d3dcompiler` is in `premake5.lua` links and solution is regenerated

### Issue: Shader compile error at runtime
**Fix**: SpriteRenderer will log `[SpriteRenderer] Initialization failed` and disable. Check HLSL source string for syntax errors.

### Issue: Black sprites (no texture visible)
**Fix**: Verify `PSSetShaderResources` is called with the SRV; check UV coordinates (UVMin/UVMax should be {0,0} to {1,1} for full texture)

### Issue: Inverted Y coordinates
**Fix**: Verify projection matrix Y row has `-2/H` sign (negative Y scale flips screen-space to clip-space)

### Issue: Transparency not working
**Fix**: Verify blend state uses `D3D11_BLEND_SRC_ALPHA` / `D3D11_BLEND_INV_SRC_ALPHA` and is bound via `OMSetBlendState` in `Begin()`

---

## Success Criteria Reference

| SC | Test |
|----|------|
| SC-008 | Log shows `[SpriteRenderer] Initialized` |
| SC-009 | D3D11 debug layer reports no live objects after shutdown |
| SC-010 | Application class has zero new members |
| SC-011 | App starts if Initialize() returns false (manually force a shader compile error to test) |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
