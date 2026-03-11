#include "Renderer/Backend/DX11/DX11SpritePipeline.h"

#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Texture/Texture.h"

#include <d3dcompiler.h>
#include <cstring>
#include <cstdint>
#include <vector>

namespace LongXi
{

bool DX11SpritePipeline::Initialize(Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource)
{
    Shutdown();
    m_Renderer = &renderer;

    if (!renderer.IsInitialized() || maxSpritesPerBatch <= 0 || !vertexShaderSource || !pixelShaderSource)
    {
        m_Renderer = nullptr;
        return false;
    }

    ID3D11Device* device = static_cast<ID3D11Device*>(renderer.GetNativeDeviceHandle());
    ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(renderer.GetNativeContextHandle());
    if (!device || !context)
    {
        m_Renderer = nullptr;
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

    RendererBufferDesc vertexBufferDesc = {};
    vertexBufferDesc.Type = RendererBufferType::Vertex;
    vertexBufferDesc.ByteSize = static_cast<uint32_t>(maxSpritesPerBatch * 4 * sizeof(SpriteVertex));
    vertexBufferDesc.Stride = sizeof(SpriteVertex);
    vertexBufferDesc.Usage = RendererResourceUsage::Dynamic;
    vertexBufferDesc.CpuAccess = RendererCpuAccessFlags::Write;
    vertexBufferDesc.BindFlags = RendererBindFlags::VertexBuffer;
    m_VertexBufferHandle = renderer.CreateVertexBuffer(vertexBufferDesc);
    if (!m_VertexBufferHandle.IsValid())
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateVertexBuffer failed via renderer API");
        Shutdown();
        return false;
    }

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

    RendererBufferDesc indexBufferDesc = {};
    indexBufferDesc.Type = RendererBufferType::Index;
    indexBufferDesc.ByteSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t));
    indexBufferDesc.Stride = sizeof(uint16_t);
    indexBufferDesc.Usage = RendererResourceUsage::Static;
    indexBufferDesc.CpuAccess = RendererCpuAccessFlags::None;
    indexBufferDesc.BindFlags = RendererBindFlags::IndexBuffer;
    indexBufferDesc.InitialData = indices.data();
    indexBufferDesc.InitialDataSize = indexBufferDesc.ByteSize;
    m_IndexBufferHandle = renderer.CreateIndexBuffer(indexBufferDesc);
    if (!m_IndexBufferHandle.IsValid())
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateIndexBuffer failed via renderer API");
        Shutdown();
        return false;
    }

    RendererBufferDesc constantBufferDesc = {};
    constantBufferDesc.Type = RendererBufferType::Constant;
    constantBufferDesc.ByteSize = 64;
    constantBufferDesc.Stride = 16;
    constantBufferDesc.Usage = RendererResourceUsage::Dynamic;
    constantBufferDesc.CpuAccess = RendererCpuAccessFlags::Write;
    constantBufferDesc.BindFlags = RendererBindFlags::ConstantBuffer;
    m_ConstantBufferHandle = renderer.CreateConstantBuffer(constantBufferDesc);
    if (!m_ConstantBufferHandle.IsValid())
    {
        LX_ENGINE_ERROR("[SpriteRenderer] CreateConstantBuffer failed via renderer API");
        Shutdown();
        return false;
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
    if (m_Renderer && m_Renderer->IsInitialized())
    {
        if (m_VertexBufferHandle.IsValid())
        {
            m_Renderer->DestroyBuffer(ToBufferHandle(m_VertexBufferHandle));
        }

        if (m_IndexBufferHandle.IsValid())
        {
            m_Renderer->DestroyBuffer(ToBufferHandle(m_IndexBufferHandle));
        }

        if (m_ConstantBufferHandle.IsValid())
        {
            m_Renderer->DestroyBuffer(ToBufferHandle(m_ConstantBufferHandle));
        }
    }

    m_VertexBufferHandle = {};
    m_IndexBufferHandle = {};
    m_ConstantBufferHandle = {};
    m_Renderer = nullptr;
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

bool DX11SpritePipeline::ValidateRendererState(Renderer& renderer) const
{
    return m_Initialized && renderer.IsInitialized() && renderer.GetNativeContextHandle() != nullptr && m_VertexBufferHandle.IsValid() && m_IndexBufferHandle.IsValid() &&
           m_ConstantBufferHandle.IsValid();
}

void DX11SpritePipeline::BindBatchPipeline(Renderer& renderer) const
{
    if (!ValidateRendererState(renderer))
    {
        return;
    }

    ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(renderer.GetNativeContextHandle());

    if (!renderer.BindVertexBuffer(m_VertexBufferHandle, sizeof(SpriteVertex), 0))
    {
        return;
    }

    if (!renderer.BindIndexBuffer(m_IndexBufferHandle, RendererIndexFormat::UInt16, 0))
    {
        return;
    }

    if (!renderer.BindConstantBuffer(m_ConstantBufferHandle, RendererShaderStage::Vertex, 0))
    {
        return;
    }

    context->IASetInputLayout(m_InputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    context->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    context->OMSetBlendState(m_BlendState.Get(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_DepthState.Get(), 0);
    context->RSSetState(m_RasterizerState.Get());
}

void DX11SpritePipeline::UploadProjectionMatrix(Renderer& renderer, const float* matrixData, std::size_t matrixBytes)
{
    if (!ValidateRendererState(renderer) || !matrixData || matrixBytes == 0)
    {
        return;
    }

    RendererBufferUpdateRequest updateRequest = {};
    updateRequest.Handle = ToBufferHandle(m_ConstantBufferHandle);
    updateRequest.Data = matrixData;
    updateRequest.ByteOffset = 0;
    updateRequest.ByteSize = static_cast<uint32_t>(matrixBytes);

    renderer.UpdateBuffer(updateRequest);
}

void DX11SpritePipeline::FlushBatch(Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const
{
    if (!ValidateRendererState(renderer) || !vertices || spriteCount <= 0)
    {
        return;
    }

    const std::size_t byteCount = static_cast<std::size_t>(spriteCount) * 4 * sizeof(SpriteVertex);
    RendererBufferUpdateRequest updateRequest = {};
    updateRequest.Handle = ToBufferHandle(m_VertexBufferHandle);
    updateRequest.Data = vertices;
    updateRequest.ByteOffset = 0;
    updateRequest.ByteSize = static_cast<uint32_t>(byteCount);

    if (!renderer.UpdateBuffer(updateRequest))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] Failed to upload sprite batch through renderer update API");
        return;
    }

    if (!renderer.BindTexture(texture.GetHandle(), RendererShaderStage::Pixel, 0))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] Failed to bind sprite texture handle");
        return;
    }

    renderer.DrawIndexed(static_cast<uint32_t>(spriteCount) * 6, 0, 0);
}

} // namespace LongXi
