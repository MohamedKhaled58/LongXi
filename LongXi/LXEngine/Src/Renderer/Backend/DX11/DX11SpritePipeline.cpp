#include "Renderer/Backend/DX11/DX11SpritePipeline.h"

#include "Core/Logging/LogMacros.h"
#include "Renderer/DX11Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/Texture.h"

#include <d3dcompiler.h>
#include <cstring>
#include <cstdint>
#include <vector>

namespace LongXi
{

bool DX11SpritePipeline::Initialize(DX11Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource)
{
    Shutdown();

    if (!renderer.IsInitialized() || renderer.GetContext() == nullptr || maxSpritesPerBatch <= 0 || !vertexShaderSource || !pixelShaderSource)
    {
        return false;
    }

    ID3D11Device* device = renderer.GetDevice();
    if (!device)
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(vertexShaderSource, strlen(vertexShaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[SpriteRenderer] Vertex shader compile failed: {}", msg);
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VertexShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateVertexShader failed (hr={})", hr);
        Shutdown();
        return false;
    }

    hr = D3DCompile(pixelShaderSource, strlen(pixelShaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr))
    {
        const char* msg = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
        LX_ENGINE_ERROR("[SpriteRenderer] Pixel shader compile failed: {}", msg);
        Shutdown();
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PixelShader);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreatePixelShader failed (hr={})", hr);
        Shutdown();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_InputLayout);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateInputLayout failed (hr={})", hr);
        Shutdown();
        return false;
    }

    {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.ByteWidth = static_cast<UINT>(maxSpritesPerBatch * 4 * sizeof(SpriteVertex));
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = device->CreateBuffer(&vbDesc, nullptr, &m_VertexBuffer);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBuffer (vertex) failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        std::vector<uint16_t> indices(static_cast<std::size_t>(maxSpritesPerBatch) * 6U);
        for (int i = 0; i < maxSpritesPerBatch; ++i)
        {
            uint16_t base = static_cast<uint16_t>(i * 4);
            indices[static_cast<std::size_t>(i) * 6 + 0] = static_cast<uint16_t>(base + 0);
            indices[static_cast<std::size_t>(i) * 6 + 1] = static_cast<uint16_t>(base + 1);
            indices[static_cast<std::size_t>(i) * 6 + 2] = static_cast<uint16_t>(base + 2);
            indices[static_cast<std::size_t>(i) * 6 + 3] = static_cast<uint16_t>(base + 2);
            indices[static_cast<std::size_t>(i) * 6 + 4] = static_cast<uint16_t>(base + 1);
            indices[static_cast<std::size_t>(i) * 6 + 5] = static_cast<uint16_t>(base + 3);
        }

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices.data();

        hr = device->CreateBuffer(&ibDesc, &ibData, &m_IndexBuffer);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateBuffer (index) failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

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
            Shutdown();
            return false;
        }
    }

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
            Shutdown();
            return false;
        }
    }

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
            Shutdown();
            return false;
        }
    }

    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&samplerDesc, &m_SamplerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateSamplerState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.DepthClipEnable = true;

        hr = device->CreateRasterizerState(&rasterizerDesc, &m_RasterizerState);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[SpriteRenderer] CreateRasterizerState failed (hr={})", hr);
            Shutdown();
            return false;
        }
    }

    m_MaxSpritesPerBatch = maxSpritesPerBatch;
    m_Initialized = true;
    return true;
}

void DX11SpritePipeline::Shutdown()
{
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

    m_MaxSpritesPerBatch = 0;
    m_Initialized = false;
}

bool DX11SpritePipeline::IsInitialized() const
{
    return m_Initialized;
}

bool DX11SpritePipeline::ValidateRendererState(DX11Renderer& renderer) const
{
    return m_Initialized && renderer.IsInitialized() && renderer.GetContext() != nullptr;
}

void DX11SpritePipeline::BindBatchPipeline(DX11Renderer& renderer) const
{
    if (!ValidateRendererState(renderer))
    {
        return;
    }

    ID3D11DeviceContext* context = renderer.GetContext();

    UINT stride = sizeof(SpriteVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
    context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    context->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    context->OMSetBlendState(m_BlendState.Get(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_DepthState.Get(), 0);
    context->RSSetState(m_RasterizerState.Get());
}

void DX11SpritePipeline::UploadProjectionMatrix(DX11Renderer& renderer, const float* matrixData, std::size_t matrixBytes)
{
    if (!ValidateRendererState(renderer) || !matrixData || matrixBytes == 0)
    {
        return;
    }

    ID3D11DeviceContext* context = renderer.GetContext();
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(m_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, matrixData, matrixBytes);
        context->Unmap(m_ConstantBuffer.Get(), 0);
    }
}

void DX11SpritePipeline::FlushBatch(DX11Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const
{
    if (!ValidateRendererState(renderer) || !vertices || spriteCount <= 0)
    {
        return;
    }

    ID3D11DeviceContext* context = renderer.GetContext();

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        const std::size_t byteCount = static_cast<std::size_t>(spriteCount) * 4 * sizeof(SpriteVertex);
        memcpy(mapped.pData, vertices, byteCount);
        context->Unmap(m_VertexBuffer.Get(), 0);
    }

    ID3D11ShaderResourceView* textureView = texture.GetHandle().Get();
    context->PSSetShaderResources(0, 1, &textureView);
    context->DrawIndexed(static_cast<UINT>(spriteCount) * 6, 0, 0);
}

} // namespace LongXi
