#include "Renderer/SpriteRenderer.h"
#include "Renderer/DX11Renderer.h"
#include "Texture/Texture.h"
#include "Core/Logging/LogMacros.h"

#include <d3dcompiler.h>
#include <windows.h>
#include <cstring>

// ============================================================================
// Embedded HLSL Shaders
// ============================================================================

static const char* s_VertexShaderSrc = R"(
#pragma pack_matrix(row_major)

cbuffer ProjectionBuffer : register(b0)
{
    float4x4 g_Projection;
};

struct VSInput
{
    float2 Pos : POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

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

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

float4 PS(PSInput input) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, input.UV) * input.Col;
}
)";

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

SpriteRenderer::SpriteRenderer() : m_Renderer(nullptr), m_SpriteCount(0), m_CurrentTexture(nullptr), m_Initialized(false), m_InBatch(false)
{
    memset(m_ProjectionMatrix, 0, sizeof(m_ProjectionMatrix));
}

SpriteRenderer::~SpriteRenderer()
{
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SpriteRenderer::Initialize(DX11Renderer& renderer, int width, int height)
{
    m_Renderer = &renderer;
    ID3D11Device* device = renderer.GetDevice();

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    // ---- Compile vertex shader ----
    HRESULT hr = D3DCompile(s_VertexShaderSrc, strlen(s_VertexShaderSrc), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[SpriteRenderer] Vertex shader compile failed: {}", msg);
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
        m_Initialized = false;
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VertexShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateVertexShader failed (hr={})", hr);
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
        m_Initialized = false;
        return false;
    }

    // ---- Compile pixel shader ----
    hr = D3DCompile(s_PixelShaderSrc, strlen(s_PixelShaderSrc), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[SpriteRenderer] Pixel shader compile failed: {}", msg);
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
        m_VertexShader.Reset();
        m_Initialized = false;
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreatePixelShader failed (hr={})", hr);
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
        m_VertexShader.Reset();
        m_Initialized = false;
        return false;
    }

    // ---- Input layout ----
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_InputLayout);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateInputLayout failed (hr={})", hr);
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
        m_PixelShader.Reset();
        m_VertexShader.Reset();
        m_Initialized = false;
        return false;
    }

    // ---- Dynamic vertex buffer ----
    {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.ByteWidth = MAX_SPRITES_PER_BATCH * 4 * sizeof(SpriteVertex);
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = device->CreateBuffer(&vbDesc, nullptr, &m_VertexBuffer);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBuffer (vertex) failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Static index buffer ----
    {
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

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.ByteWidth = sizeof(indices);
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices;

        hr = device->CreateBuffer(&ibDesc, &ibData, &m_IndexBuffer);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBuffer (index) failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Constant buffer (64 bytes = float4x4) ----
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = 64;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = device->CreateBuffer(&cbDesc, nullptr, &m_ConstantBuffer);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBuffer (constant) failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_IndexBuffer.Reset();
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Alpha blend state ----
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = false;
        blendDesc.IndependentBlendEnable = false;
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;

        hr = device->CreateBlendState(&blendDesc, &m_BlendState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBlendState failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_ConstantBuffer.Reset();
            m_IndexBuffer.Reset();
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Depth-stencil state (depth disabled) ----
    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = false;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.StencilEnable = false;

        hr = device->CreateDepthStencilState(&dsDesc, &m_DepthState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateDepthStencilState failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_BlendState.Reset();
            m_ConstantBuffer.Reset();
            m_IndexBuffer.Reset();
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Linear-clamp sampler ----
    {
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&sampDesc, &m_SamplerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateSamplerState failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_DepthState.Reset();
            m_BlendState.Reset();
            m_ConstantBuffer.Reset();
            m_IndexBuffer.Reset();
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Rasterizer state (no cull, solid fill) ----
    {
        D3D11_RASTERIZER_DESC rasDesc = {};
        rasDesc.FillMode = D3D11_FILL_SOLID;
        rasDesc.CullMode = D3D11_CULL_NONE;
        rasDesc.FrontCounterClockwise = false;
        rasDesc.DepthClipEnable = true;
        rasDesc.ScissorEnable = false;

        hr = device->CreateRasterizerState(&rasDesc, &m_RasterizerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateRasterizerState failed (hr={})", hr);
            LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed — sprite rendering disabled");
            m_SamplerState.Reset();
            m_DepthState.Reset();
            m_BlendState.Reset();
            m_ConstantBuffer.Reset();
            m_IndexBuffer.Reset();
            m_VertexBuffer.Reset();
            m_InputLayout.Reset();
            m_PixelShader.Reset();
            m_VertexShader.Reset();
            m_Initialized = false;
            return false;
        }
    }

    // ---- Upload initial projection matrix ----
    UpdateProjection(width, height);

    m_Initialized = true;
    LX_ENGINE_INFO("[SpriteRenderer] Initialized");
    return true;
}

void SpriteRenderer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    m_RasterizerState.Reset();
    m_SamplerState.Reset();
    m_DepthState.Reset();
    m_BlendState.Reset();
    m_ConstantBuffer.Reset();
    m_IndexBuffer.Reset();
    m_VertexBuffer.Reset();
    m_InputLayout.Reset();
    m_PixelShader.Reset();
    m_VertexShader.Reset();

    m_Initialized = false;
    LX_ENGINE_INFO("[SpriteRenderer] Shutdown");
}

bool SpriteRenderer::IsInitialized() const
{
    return m_Initialized;
}

// ============================================================================
// Resize
// ============================================================================

void SpriteRenderer::OnResize(int width, int height)
{
    if (!m_Initialized)
    {
        return;
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }
    UpdateProjection(width, height);
}

// ============================================================================
// Private Helpers
// ============================================================================

void SpriteRenderer::UpdateProjection(int width, int height)
{
    float w = static_cast<float>(width);
    float h = static_cast<float>(height);

    // Row-major orthographic projection
    // Maps [0,W] x [0,H] to clip space [-1,1] x [1,-1] (Y flipped for top-left origin)
    // Used with #pragma pack_matrix(row_major) in HLSL so no CPU-side transpose needed
    float proj[16] = {
        2.0f / w,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        -2.0f / h,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        -1.0f,
        1.0f,
        0.0f,
        1.0f,
    };
    memcpy(m_ProjectionMatrix, proj, sizeof(proj));

    // Upload to constant buffer
    if (m_ConstantBuffer && m_Renderer)
    {
        ID3D11DeviceContext* ctx = m_Renderer->GetContext();
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(ctx->Map(m_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, m_ProjectionMatrix, sizeof(m_ProjectionMatrix));
            ctx->Unmap(m_ConstantBuffer.Get(), 0);
        }
    }
}

void SpriteRenderer::FlushBatch()
{
    if (m_SpriteCount == 0 || !m_CurrentTexture)
    {
        return;
    }

    ID3D11DeviceContext* ctx = m_Renderer->GetContext();

    // Upload vertex data (DISCARD avoids GPU/CPU stall)
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(ctx->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, m_VertexData, static_cast<size_t>(m_SpriteCount) * 4 * sizeof(SpriteVertex));
        ctx->Unmap(m_VertexBuffer.Get(), 0);
    }

    // Bind texture SRV
    ID3D11ShaderResourceView* srv = m_CurrentTexture->GetHandle().Get();
    ctx->PSSetShaderResources(0, 1, &srv);
    LX_ENGINE_INFO("[SpriteRenderer] Texture bound");

    // Submit draw call
    ctx->DrawIndexed(static_cast<UINT>(m_SpriteCount) * 6, 0, 0);
    LX_ENGINE_INFO("[SpriteRenderer] Batch flushed ({} sprites)", m_SpriteCount);

    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;
}

// ============================================================================
// Frame Rendering
// ============================================================================

void SpriteRenderer::Begin()
{
    if (!m_Initialized || m_InBatch)
    {
        return;
    }

    m_InBatch = true;
    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;

    ID3D11DeviceContext* ctx = m_Renderer->GetContext();

    // Bind vertex buffer
    UINT stride = sizeof(SpriteVertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);

    // Bind index buffer
    ctx->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Input layout and primitive topology
    ctx->IASetInputLayout(m_InputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Shaders
    ctx->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
    ctx->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    // Output merger states
    ctx->OMSetBlendState(m_BlendState.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_DepthState.Get(), 0);

    // Rasterizer
    ctx->RSSetState(m_RasterizerState.Get());
}

void SpriteRenderer::End()
{
    if (!m_InBatch)
    {
        return;
    }
    FlushBatch();
    m_InBatch = false;
}

void SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size)
{
    DrawSprite(texture, position, size, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
}

void SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size, Vector2 uvMin, Vector2 uvMax, LongXi::Color color)
{
    if (!m_Initialized || !m_InBatch)
    {
        LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite called outside Begin/End block");
        return;
    }

    if (!texture)
    {
        LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite: null texture");
        return;
    }

    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    // Flush if texture changed
    if (m_CurrentTexture != nullptr && m_CurrentTexture != texture)
    {
        FlushBatch();
    }
    m_CurrentTexture = texture;

    // Flush if batch is full
    if (m_SpriteCount >= MAX_SPRITES_PER_BATCH)
    {
        FlushBatch();
        m_CurrentTexture = texture;
    }

    // Build 4 vertices for this quad
    SpriteVertex* v = &m_VertexData[m_SpriteCount * 4];

    v[0].Position = {position.x, position.y}; // TL
    v[0].UV = {uvMin.x, uvMin.y};
    v[0].Color = color;

    v[1].Position = {position.x + size.x, position.y}; // TR
    v[1].UV = {uvMax.x, uvMin.y};
    v[1].Color = color;

    v[2].Position = {position.x, position.y + size.y}; // BL
    v[2].UV = {uvMin.x, uvMax.y};
    v[2].Color = color;

    v[3].Position = {position.x + size.x, position.y + size.y}; // BR
    v[3].UV = {uvMax.x, uvMax.y};
    v[3].Color = color;

    ++m_SpriteCount;
}

} // namespace LongXi
