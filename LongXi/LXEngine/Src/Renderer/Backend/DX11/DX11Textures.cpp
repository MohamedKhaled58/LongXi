#include "Renderer/Backend/DX11/DX11Textures.h"

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

using LXCore::TextureFormat;

namespace
{

DXGI_FORMAT ToDxgiTextureFormat(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::RGBA8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::DXT1:
            return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::DXT3:
            return DXGI_FORMAT_BC2_UNORM;
        case TextureFormat::DXT5:
            return DXGI_FORMAT_BC3_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

UINT ComputeDefaultRowPitch(const RendererTextureDesc& desc)
{
    if (desc.Format == TextureFormat::RGBA8)
    {
        return desc.Width * 4;
    }

    UINT blockCols = (desc.Width + 3) / 4;
    if (blockCols == 0)
    {
        blockCols = 1;
    }

    if (desc.Format == TextureFormat::DXT1)
    {
        return blockCols * 8;
    }

    return blockCols * 16;
}

bool ValidateCpuAccess(RendererResourceUsage usage, RendererCpuAccessFlags access)
{
    switch (usage)
    {
        case RendererResourceUsage::Static:
            return access == RendererCpuAccessFlags::None;
        case RendererResourceUsage::Dynamic:
            return access == RendererCpuAccessFlags::Write;
        case RendererResourceUsage::Staging:
            return access != RendererCpuAccessFlags::None;
        default:
            break;
    }

    return false;
}

D3D11_USAGE ToD3D11Usage(const RendererTextureDesc& desc)
{
    switch (desc.Usage)
    {
        case RendererResourceUsage::Static:
            return desc.InitialData ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
        case RendererResourceUsage::Dynamic:
            return D3D11_USAGE_DYNAMIC;
        case RendererResourceUsage::Staging:
            return D3D11_USAGE_STAGING;
        default:
            break;
    }

    return D3D11_USAGE_DEFAULT;
}

UINT ToD3D11CpuAccess(RendererCpuAccessFlags access)
{
    UINT result = 0;
    if (HasAnyCpuAccess(access, RendererCpuAccessFlags::Read))
    {
        result |= D3D11_CPU_ACCESS_READ;
    }
    if (HasAnyCpuAccess(access, RendererCpuAccessFlags::Write))
    {
        result |= D3D11_CPU_ACCESS_WRITE;
    }
    return result;
}

UINT ToD3D11BindFlags(RendererBindFlags bindFlags)
{
    UINT result = 0;
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::ShaderResource))
    {
        result |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::RenderTarget))
    {
        result |= D3D11_BIND_RENDER_TARGET;
    }
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::DepthStencil))
    {
        result |= D3D11_BIND_DEPTH_STENCIL;
    }
    return result;
}

} // namespace

void DX11Textures::Initialize(ID3D11Device* device, DX11ResourceTables* resourceTables)
{
    m_Device         = device;
    m_ResourceTables = resourceTables;
}

void DX11Textures::Shutdown()
{
    m_Device         = nullptr;
    m_ResourceTables = nullptr;
}

RendererTextureHandle DX11Textures::CreateTexture(const RendererTextureDesc& desc, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_Device || !m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return {};
    }

    if (desc.Width == 0 || desc.Height == 0)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return {};
    }

    if (!ValidateCpuAccess(desc.Usage, desc.CpuAccess))
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return {};
    }

    if (desc.Usage != RendererResourceUsage::Staging && desc.BindFlags == RendererBindFlags::None)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return {};
    }

    const DXGI_FORMAT dxgiFormat = ToDxgiTextureFormat(desc.Format);
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
    {
        outResult.Code = RendererResultCode::Unsupported;
        return {};
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width                = desc.Width;
    textureDesc.Height               = desc.Height;
    textureDesc.MipLevels            = 1;
    textureDesc.ArraySize            = 1;
    textureDesc.Format               = dxgiFormat;
    textureDesc.SampleDesc.Count     = 1;
    textureDesc.Usage                = ToD3D11Usage(desc);
    textureDesc.BindFlags            = (desc.Usage == RendererResourceUsage::Staging) ? 0 : ToD3D11BindFlags(desc.BindFlags);
    textureDesc.CPUAccessFlags       = ToD3D11CpuAccess(desc.CpuAccess);

    D3D11_SUBRESOURCE_DATA  initData    = {};
    D3D11_SUBRESOURCE_DATA* initDataPtr = nullptr;

    if (desc.InitialData)
    {
        initData.pSysMem     = desc.InitialData;
        initData.SysMemPitch = (desc.InitialRowPitch > 0) ? desc.InitialRowPitch : ComputeDefaultRowPitch(desc);
        initDataPtr          = &initData;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    const HRESULT                           textureHr = m_Device->CreateTexture2D(&textureDesc, initDataPtr, &texture);
    if (FAILED(textureHr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateTexture2D failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(textureHr));
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    if (HasAnyBindFlag(desc.BindFlags, RendererBindFlags::ShaderResource))
    {
        const HRESULT srvHr = m_Device->CreateShaderResourceView(texture.Get(), nullptr, &shaderResourceView);
        if (FAILED(srvHr))
        {
            LX_ENGINE_ERROR("[Renderer] CreateShaderResourceView failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(srvHr));
            outResult.Code = RendererResultCode::BackendFailure;
            return {};
        }
    }

    DX11TextureRecord record  = {};
    record.Usage              = desc.Usage;
    record.BindFlags          = desc.BindFlags;
    record.Texture            = texture;
    record.ShaderResourceView = shaderResourceView;

    RendererTextureHandle handle = m_ResourceTables->RegisterTexture(record);
    if (!handle.IsValid())
    {
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    LX_ENGINE_INFO("[Renderer] Texture created (slot={}, generation={})", handle.Id.Slot, handle.Id.Generation);
    outResult.Code = RendererResultCode::Success;
    return handle;
}

bool DX11Textures::DestroyTexture(RendererTextureHandle handle, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    RendererResultCode destroyResult = RendererResultCode::Success;
    if (!m_ResourceTables->DestroyTexture(handle, destroyResult))
    {
        outResult.Code = destroyResult;
        return false;
    }

    LX_ENGINE_INFO("[Renderer] Texture destroyed (slot={})", handle.Id.Slot);
    return true;
}

ID3D11ShaderResourceView* DX11Textures::ResolveShaderResourceView(RendererTextureHandle handle, RendererResultCode& outError) const
{
    if (!m_ResourceTables)
    {
        outError = RendererResultCode::NotInitialized;
        return nullptr;
    }

    const DX11TextureRecord* record = nullptr;
    if (!m_ResourceTables->ResolveTexture(handle, record, outError))
    {
        return nullptr;
    }

    outError = RendererResultCode::Success;
    return record->ShaderResourceView.Get();
}

} // namespace LXEngine
