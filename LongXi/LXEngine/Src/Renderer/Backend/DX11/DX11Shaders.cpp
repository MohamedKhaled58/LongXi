#include "Renderer/Backend/DX11/DX11Shaders.h"

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

void DX11Shaders::Initialize(ID3D11Device* device, DX11ResourceTables* resourceTables)
{
    m_Device = device;
    m_ResourceTables = resourceTables;
}

void DX11Shaders::Shutdown()
{
    m_Device = nullptr;
    m_ResourceTables = nullptr;
}

RendererShaderHandle DX11Shaders::CreateShaderInternal(const RendererShaderDesc& desc, RendererShaderStage stage, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_Device || !m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return {};
    }

    if (!desc.Bytecode || desc.BytecodeSize == 0 || desc.Stage != stage)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return {};
    }

    DX11ShaderRecord record = {};
    record.Stage = stage;

    if (stage == RendererShaderStage::Vertex)
    {
        const HRESULT hr = m_Device->CreateVertexShader(desc.Bytecode, desc.BytecodeSize, nullptr, &record.VertexShader);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[Renderer] CreateVertexShader failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
            outResult.Code = RendererResultCode::BackendFailure;
            return {};
        }
    }
    else
    {
        const HRESULT hr = m_Device->CreatePixelShader(desc.Bytecode, desc.BytecodeSize, nullptr, &record.PixelShader);
        if (FAILED(hr))
        {
            LX_ENGINE_ERROR("[Renderer] CreatePixelShader failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
            outResult.Code = RendererResultCode::BackendFailure;
            return {};
        }
    }

    RendererShaderHandle handle = m_ResourceTables->RegisterShader(record);
    if (!handle.IsValid())
    {
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    LX_ENGINE_INFO("[Renderer] Shader created (slot={}, stage={})", handle.Id.Slot, static_cast<uint32_t>(handle.Stage));
    return handle;
}

RendererShaderHandle DX11Shaders::CreateVertexShader(const RendererShaderDesc& desc, RendererResult& outResult)
{
    return CreateShaderInternal(desc, RendererShaderStage::Vertex, outResult);
}

RendererShaderHandle DX11Shaders::CreatePixelShader(const RendererShaderDesc& desc, RendererResult& outResult)
{
    return CreateShaderInternal(desc, RendererShaderStage::Pixel, outResult);
}

bool DX11Shaders::DestroyShader(RendererShaderHandle handle, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    RendererResultCode destroyCode = RendererResultCode::Success;
    if (!m_ResourceTables->DestroyShader(handle, destroyCode))
    {
        outResult.Code = destroyCode;
        return false;
    }

    LX_ENGINE_INFO("[Renderer] Shader destroyed (slot={}, stage={})", handle.Id.Slot, static_cast<uint32_t>(handle.Stage));
    return true;
}

ID3D11VertexShader* DX11Shaders::ResolveVertexShader(RendererShaderHandle handle, RendererResultCode& outError) const
{
    if (!m_ResourceTables)
    {
        outError = RendererResultCode::NotInitialized;
        return nullptr;
    }

    const DX11ShaderRecord* record = nullptr;
    if (!m_ResourceTables->ResolveShader(handle, record, outError))
    {
        return nullptr;
    }

    if (record->Stage != RendererShaderStage::Vertex)
    {
        outError = RendererResultCode::InvalidHandle;
        return nullptr;
    }

    outError = RendererResultCode::Success;
    return record->VertexShader.Get();
}

ID3D11PixelShader* DX11Shaders::ResolvePixelShader(RendererShaderHandle handle, RendererResultCode& outError) const
{
    if (!m_ResourceTables)
    {
        outError = RendererResultCode::NotInitialized;
        return nullptr;
    }

    const DX11ShaderRecord* record = nullptr;
    if (!m_ResourceTables->ResolveShader(handle, record, outError))
    {
        return nullptr;
    }

    if (record->Stage != RendererShaderStage::Pixel)
    {
        outError = RendererResultCode::InvalidHandle;
        return nullptr;
    }

    outError = RendererResultCode::Success;
    return record->PixelShader.Get();
}

} // namespace LongXi
