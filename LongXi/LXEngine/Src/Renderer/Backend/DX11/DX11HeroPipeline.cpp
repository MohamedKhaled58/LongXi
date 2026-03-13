#include "Renderer/Backend/DX11/DX11HeroPipeline.h"

#include <cstring>
#include <d3dcompiler.h>

#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"

namespace LXEngine
{

namespace
{

static const char* s_HeroVS = R"(
#pragma pack_matrix(row_major)

cbuffer cbBones : register(b0)
{
    float4x4 BoneMatrices[64];
};

cbuffer cbPerObject : register(b1)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
};

struct VSInput
{
    float3 Pos      : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
    uint4  BlendIdx : BLENDINDICES;
    float4 BlendWt  : BLENDWEIGHT;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VSOutput VS(VSInput v)
{
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        skinnedPos += v.BlendWt[i] * mul(float4(v.Pos, 1.0f), BoneMatrices[v.BlendIdx[i]]);
    }

    VSOutput o;
    o.Pos = mul(mul(mul(skinnedPos, World), View), Projection);
    o.UV  = v.UV;
    return o;
}
)";

static const char* s_HeroPS = R"(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

float4 PS(PSInput p) : SV_Target
{
    return g_Texture.Sample(g_Sampler, p.UV);
}
)";

} // namespace

bool DX11HeroPipeline::Initialize(Renderer& renderer)
{
    Shutdown();

    if (!renderer.IsInitialized())
    {
        return false;
    }

    ID3D11Device* device = static_cast<ID3D11Device*>(renderer.GetNativeDeviceHandle());
    if (!device)
    {
        LX_ENGINE_ERROR("[HeroRenderer] Missing DX11 device");
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(s_HeroVS, std::strlen(s_HeroVS), nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[HeroRenderer] Vertex shader compile failed: {}", msg);
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VertexShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[HeroRenderer] CreateVertexShader failed (hr={})", hr);
        Shutdown();
        return false;
    }

    hr = D3DCompile(s_HeroPS, std::strlen(s_HeroPS), nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[HeroRenderer] Pixel shader compile failed: {}", msg);
        Shutdown();
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[HeroRenderer] CreatePixelShader failed (hr={})", hr);
        Shutdown();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = device->CreateInputLayout(layout, 5, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_InputLayout);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[HeroRenderer] CreateInputLayout failed (hr={})", hr);
        Shutdown();
        return false;
    }

    {
        D3D11_BLEND_DESC blendDesc                      = {};
        blendDesc.AlphaToCoverageEnable                 = false;
        blendDesc.IndependentBlendEnable                = false;
        blendDesc.RenderTarget[0].BlendEnable           = false;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;

        hr = device->CreateBlendState(&blendDesc, &m_OpaqueBlendState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateOpaqueBlendState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        D3D11_BLEND_DESC blendDesc                      = {};
        blendDesc.AlphaToCoverageEnable                 = false;
        blendDesc.IndependentBlendEnable                = false;
        blendDesc.RenderTarget[0].BlendEnable           = true;
        blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;

        hr = device->CreateBlendState(&blendDesc, &m_TransparentBlendState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateTransparentBlendState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable              = true;
        dsDesc.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc                = D3D11_COMPARISON_LESS_EQUAL;
        dsDesc.StencilEnable            = false;

        hr = device->CreateDepthStencilState(&dsDesc, &m_OpaqueDepthState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateOpaqueDepthState failed (hr={})", hr);
            Shutdown();
            return false;
        }

        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        hr                    = device->CreateDepthStencilState(&dsDesc, &m_TransparentDepthState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateTransparentDepthState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD             = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&samplerDesc, &m_SamplerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateSamplerState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode              = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode              = D3D11_CULL_BACK;
        rasterizerDesc.DepthClipEnable       = true;

        hr = device->CreateRasterizerState(&rasterizerDesc, &m_RasterizerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[HeroRenderer] CreateRasterizerState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    m_Initialized = true;
    return true;
}

void DX11HeroPipeline::Shutdown()
{
    m_RasterizerState.Reset();
    m_SamplerState.Reset();
    m_OpaqueDepthState.Reset();
    m_TransparentDepthState.Reset();
    m_TransparentBlendState.Reset();
    m_OpaqueBlendState.Reset();
    m_InputLayout.Reset();
    m_PixelShader.Reset();
    m_VertexShader.Reset();
    m_Initialized = false;
}

bool DX11HeroPipeline::IsInitialized() const
{
    return m_Initialized;
}

void DX11HeroPipeline::BindOpaque(ID3D11DeviceContext* ctx) const
{
    if (!ctx || !m_Initialized)
    {
        return;
    }

    ctx->IASetInputLayout(m_InputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
    ctx->OMSetBlendState(m_OpaqueBlendState.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_OpaqueDepthState.Get(), 0);
    ctx->RSSetState(m_RasterizerState.Get());
}

void DX11HeroPipeline::BindTransparent(ID3D11DeviceContext* ctx) const
{
    if (!ctx || !m_Initialized)
    {
        return;
    }

    ctx->IASetInputLayout(m_InputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
    ctx->OMSetBlendState(m_TransparentBlendState.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_TransparentDepthState.Get(), 0);
    ctx->RSSetState(m_RasterizerState.Get());
}

ID3D11VertexShader* DX11HeroPipeline::GetVertexShader() const
{
    return m_VertexShader.Get();
}

ID3D11PixelShader* DX11HeroPipeline::GetPixelShader() const
{
    return m_PixelShader.Get();
}

ID3D11InputLayout* DX11HeroPipeline::GetInputLayout() const
{
    return m_InputLayout.Get();
}

ID3D11SamplerState* DX11HeroPipeline::GetSampler() const
{
    return m_SamplerState.Get();
}

} // namespace LXEngine
