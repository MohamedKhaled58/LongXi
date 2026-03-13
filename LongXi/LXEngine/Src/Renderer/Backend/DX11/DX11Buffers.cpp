#include "Renderer/Backend/DX11/DX11Buffers.h"

#include <algorithm>
#include <cstring>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

namespace
{

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

D3D11_USAGE ToD3D11Usage(RendererResourceUsage usage, bool hasInitialData)
{
    switch (usage)
    {
        case RendererResourceUsage::Static:
            return hasInitialData ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
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

UINT ToD3D11Bind(RendererBindFlags bindFlags)
{
    UINT result = 0;
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::VertexBuffer))
    {
        result |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::IndexBuffer))
    {
        result |= D3D11_BIND_INDEX_BUFFER;
    }
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::ConstantBuffer))
    {
        result |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if (HasAnyBindFlag(bindFlags, RendererBindFlags::ShaderResource))
    {
        result |= D3D11_BIND_SHADER_RESOURCE;
    }
    return result;
}

RendererBindFlags DefaultBindFlagsForType(RendererBufferType type)
{
    switch (type)
    {
        case RendererBufferType::Vertex:
            return RendererBindFlags::VertexBuffer;
        case RendererBufferType::Index:
            return RendererBindFlags::IndexBuffer;
        case RendererBufferType::Constant:
            return RendererBindFlags::ConstantBuffer;
        default:
            break;
    }

    return RendererBindFlags::None;
}

D3D11_MAP ToD3D11Map(RendererMapMode mode)
{
    switch (mode)
    {
        case RendererMapMode::WriteDiscard:
            return D3D11_MAP_WRITE_DISCARD;
        case RendererMapMode::WriteNoOverwrite:
            return D3D11_MAP_WRITE_NO_OVERWRITE;
        case RendererMapMode::Read:
            return D3D11_MAP_READ;
        default:
            break;
    }

    return D3D11_MAP_WRITE_DISCARD;
}

} // namespace

void DX11Buffers::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, DX11ResourceTables* resourceTables)
{
    m_Device         = device;
    m_Context        = context;
    m_ResourceTables = resourceTables;
}

void DX11Buffers::Shutdown()
{
    m_Device         = nullptr;
    m_Context        = nullptr;
    m_ResourceTables = nullptr;
}

bool DX11Buffers::ValidateDescriptor(const RendererBufferDesc& desc, RendererResult& outResult) const
{
    outResult.Code = RendererResultCode::Success;

    if (desc.ByteSize == 0)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return false;
    }

    if (!ValidateCpuAccess(desc.Usage, desc.CpuAccess))
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return false;
    }

    if (desc.Type == RendererBufferType::Constant && (desc.ByteSize % 16) != 0)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return false;
    }

    if ((desc.Type == RendererBufferType::Vertex || desc.Type == RendererBufferType::Index) && desc.Stride == 0)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return false;
    }

    if (desc.Usage == RendererResourceUsage::Static && desc.InitialData == nullptr)
    {
        outResult.Code = RendererResultCode::InvalidDescriptor;
        return false;
    }

    return true;
}

RendererBufferHandle DX11Buffers::RegisterBuffer(const RendererBufferDesc& desc, ID3D11Buffer* buffer, RendererResult& outResult)
{
    if (!m_ResourceTables || !buffer)
    {
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    DX11BufferRecord record = {};
    record.Type             = desc.Type;
    record.ByteSize         = desc.ByteSize;
    record.Usage            = desc.Usage;
    record.CpuAccess        = desc.CpuAccess;
    record.BindFlags        = desc.BindFlags;
    record.Buffer           = buffer;

    switch (desc.Type)
    {
        case RendererBufferType::Vertex:
            return ToBufferHandle(m_ResourceTables->RegisterVertexBuffer(record));
        case RendererBufferType::Index:
            return ToBufferHandle(m_ResourceTables->RegisterIndexBuffer(record));
        case RendererBufferType::Constant:
            return ToBufferHandle(m_ResourceTables->RegisterConstantBuffer(record));
        default:
            break;
    }

    outResult.Code = RendererResultCode::InvalidDescriptor;
    return {};
}

RendererVertexBufferHandle DX11Buffers::CreateVertexBuffer(const RendererBufferDesc& desc, RendererResult& outResult)
{
    RendererBufferDesc typedDesc = desc;
    typedDesc.Type               = RendererBufferType::Vertex;
    typedDesc.BindFlags          = (typedDesc.BindFlags == RendererBindFlags::None) ? RendererBindFlags::VertexBuffer : typedDesc.BindFlags;

    if (!m_Device || !m_ResourceTables || !ValidateDescriptor(typedDesc, outResult))
    {
        if (!m_Device || !m_ResourceTables)
        {
            outResult.Code = RendererResultCode::NotInitialized;
        }
        return {};
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage             = ToD3D11Usage(typedDesc.Usage, typedDesc.InitialData != nullptr);
    bufferDesc.ByteWidth         = typedDesc.ByteSize;
    bufferDesc.BindFlags         = ToD3D11Bind(typedDesc.BindFlags);
    bufferDesc.CPUAccessFlags    = ToD3D11CpuAccess(typedDesc.CpuAccess);

    D3D11_SUBRESOURCE_DATA  initData    = {};
    D3D11_SUBRESOURCE_DATA* initDataPtr = nullptr;
    if (typedDesc.InitialData)
    {
        initData.pSysMem = typedDesc.InitialData;
        initDataPtr      = &initData;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    const HRESULT                        hr = m_Device->CreateBuffer(&bufferDesc, initDataPtr, &buffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateBuffer(Vertex) failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    const RendererBufferHandle genericHandle = RegisterBuffer(typedDesc, buffer.Get(), outResult);
    if (!genericHandle.IsValid())
    {
        if (outResult.Code == RendererResultCode::Success)
        {
            outResult.Code = RendererResultCode::BackendFailure;
        }
        return {};
    }

    LX_ENGINE_INFO("[Renderer] Vertex buffer created (slot={}, generation={})", genericHandle.Id.Slot, genericHandle.Id.Generation);
    return {genericHandle.Id};
}

RendererIndexBufferHandle DX11Buffers::CreateIndexBuffer(const RendererBufferDesc& desc, RendererResult& outResult)
{
    RendererBufferDesc typedDesc = desc;
    typedDesc.Type               = RendererBufferType::Index;
    typedDesc.BindFlags          = (typedDesc.BindFlags == RendererBindFlags::None) ? RendererBindFlags::IndexBuffer : typedDesc.BindFlags;

    if (!m_Device || !m_ResourceTables || !ValidateDescriptor(typedDesc, outResult))
    {
        if (!m_Device || !m_ResourceTables)
        {
            outResult.Code = RendererResultCode::NotInitialized;
        }
        return {};
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage             = ToD3D11Usage(typedDesc.Usage, typedDesc.InitialData != nullptr);
    bufferDesc.ByteWidth         = typedDesc.ByteSize;
    bufferDesc.BindFlags         = ToD3D11Bind(typedDesc.BindFlags);
    bufferDesc.CPUAccessFlags    = ToD3D11CpuAccess(typedDesc.CpuAccess);

    D3D11_SUBRESOURCE_DATA  initData    = {};
    D3D11_SUBRESOURCE_DATA* initDataPtr = nullptr;
    if (typedDesc.InitialData)
    {
        initData.pSysMem = typedDesc.InitialData;
        initDataPtr      = &initData;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    const HRESULT                        hr = m_Device->CreateBuffer(&bufferDesc, initDataPtr, &buffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateBuffer(Index) failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    const RendererBufferHandle genericHandle = RegisterBuffer(typedDesc, buffer.Get(), outResult);
    if (!genericHandle.IsValid())
    {
        if (outResult.Code == RendererResultCode::Success)
        {
            outResult.Code = RendererResultCode::BackendFailure;
        }
        return {};
    }

    LX_ENGINE_INFO("[Renderer] Index buffer created (slot={}, generation={})", genericHandle.Id.Slot, genericHandle.Id.Generation);
    return {genericHandle.Id};
}

RendererConstantBufferHandle DX11Buffers::CreateConstantBuffer(const RendererBufferDesc& desc, RendererResult& outResult)
{
    RendererBufferDesc typedDesc = desc;
    typedDesc.Type               = RendererBufferType::Constant;
    typedDesc.BindFlags = (typedDesc.BindFlags == RendererBindFlags::None) ? RendererBindFlags::ConstantBuffer : typedDesc.BindFlags;
    typedDesc.Stride    = 16;

    if (!m_Device || !m_ResourceTables || !ValidateDescriptor(typedDesc, outResult))
    {
        if (!m_Device || !m_ResourceTables)
        {
            outResult.Code = RendererResultCode::NotInitialized;
        }
        return {};
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage             = ToD3D11Usage(typedDesc.Usage, typedDesc.InitialData != nullptr);
    bufferDesc.ByteWidth         = typedDesc.ByteSize;
    bufferDesc.BindFlags         = ToD3D11Bind(typedDesc.BindFlags);
    bufferDesc.CPUAccessFlags    = ToD3D11CpuAccess(typedDesc.CpuAccess);

    D3D11_SUBRESOURCE_DATA  initData    = {};
    D3D11_SUBRESOURCE_DATA* initDataPtr = nullptr;
    if (typedDesc.InitialData)
    {
        initData.pSysMem = typedDesc.InitialData;
        initDataPtr      = &initData;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    const HRESULT                        hr = m_Device->CreateBuffer(&bufferDesc, initDataPtr, &buffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateBuffer(Constant) failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        outResult.Code = RendererResultCode::BackendFailure;
        return {};
    }

    const RendererBufferHandle genericHandle = RegisterBuffer(typedDesc, buffer.Get(), outResult);
    if (!genericHandle.IsValid())
    {
        if (outResult.Code == RendererResultCode::Success)
        {
            outResult.Code = RendererResultCode::BackendFailure;
        }
        return {};
    }

    LX_ENGINE_INFO("[Renderer] Constant buffer created (slot={}, generation={})", genericHandle.Id.Slot, genericHandle.Id.Generation);
    return {genericHandle.Id};
}

bool DX11Buffers::DestroyBuffer(RendererBufferHandle handle, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    RendererResultCode destroyCode = RendererResultCode::Success;
    if (!m_ResourceTables->DestroyBuffer(handle, destroyCode))
    {
        outResult.Code = destroyCode;
        return false;
    }

    LX_ENGINE_INFO("[Renderer] Buffer destroyed (slot={}, type={})", handle.Id.Slot, static_cast<uint32_t>(handle.Type));
    return true;
}

bool DX11Buffers::UpdateBuffer(const RendererBufferUpdateRequest& request, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_Context || !m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    if (!request.Handle.IsValid() || !request.Data || request.ByteSize == 0)
    {
        outResult.Code = RendererResultCode::InvalidArgument;
        return false;
    }

    DX11BufferRecord*  record      = nullptr;
    RendererResultCode resolveCode = RendererResultCode::Success;
    if (!m_ResourceTables->ResolveBuffer(request.Handle, record, resolveCode))
    {
        outResult.Code = resolveCode;
        return false;
    }

    if (record->Usage == RendererResourceUsage::Static)
    {
        outResult.Code = RendererResultCode::InvalidOperation;
        return false;
    }

    if (request.ByteOffset + request.ByteSize > record->ByteSize)
    {
        outResult.Code = RendererResultCode::InvalidArgument;
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped  = {};
    D3D11_MAP                mapType = D3D11_MAP_WRITE_DISCARD;
    if (record->Usage == RendererResourceUsage::Staging)
    {
        mapType = D3D11_MAP_WRITE;
    }

    if (FAILED(m_Context->Map(record->Buffer.Get(), 0, mapType, 0, &mapped)))
    {
        outResult.Code = RendererResultCode::BackendFailure;
        return false;
    }

    std::memcpy(static_cast<uint8_t*>(mapped.pData) + request.ByteOffset, request.Data, request.ByteSize);
    m_Context->Unmap(record->Buffer.Get(), 0);
    outResult.Code = RendererResultCode::Success;
    return true;
}

bool DX11Buffers::MapBuffer(const RendererMapRequest& request, RendererMappedResource& mapped, RendererResult& outResult)
{
    mapped         = {};
    outResult.Code = RendererResultCode::Success;

    if (!m_Context || !m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    DX11BufferRecord*  record      = nullptr;
    RendererResultCode resolveCode = RendererResultCode::Success;
    if (!m_ResourceTables->ResolveBuffer(request.Handle, record, resolveCode))
    {
        outResult.Code = resolveCode;
        return false;
    }

    if (request.Mode == RendererMapMode::Read && !HasAnyCpuAccess(record->CpuAccess, RendererCpuAccessFlags::Read))
    {
        outResult.Code = RendererResultCode::InvalidOperation;
        return false;
    }

    if ((request.Mode == RendererMapMode::WriteDiscard || request.Mode == RendererMapMode::WriteNoOverwrite) &&
        !HasAnyCpuAccess(record->CpuAccess, RendererCpuAccessFlags::Write))
    {
        outResult.Code = RendererResultCode::InvalidOperation;
        return false;
    }

    RendererResultCode mapMarkCode = RendererResultCode::Success;
    if (!m_ResourceTables->MarkBufferMapped(request.Handle, mapMarkCode))
    {
        outResult.Code = mapMarkCode;
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE dxMapped = {};
    const HRESULT            mapHr    = m_Context->Map(record->Buffer.Get(), 0, ToD3D11Map(request.Mode), 0, &dxMapped);
    if (FAILED(mapHr))
    {
        RendererResultCode rollbackCode = RendererResultCode::Success;
        m_ResourceTables->MarkBufferUnmapped(request.Handle, rollbackCode);
        outResult.Code = RendererResultCode::BackendFailure;
        return false;
    }

    mapped.Data       = dxMapped.pData;
    mapped.RowPitch   = dxMapped.RowPitch;
    mapped.DepthPitch = dxMapped.DepthPitch;
    outResult.Code    = RendererResultCode::Success;
    return true;
}

bool DX11Buffers::UnmapBuffer(RendererBufferHandle handle, RendererResult& outResult)
{
    outResult.Code = RendererResultCode::Success;

    if (!m_Context || !m_ResourceTables)
    {
        outResult.Code = RendererResultCode::NotInitialized;
        return false;
    }

    DX11BufferRecord*  record      = nullptr;
    RendererResultCode resolveCode = RendererResultCode::Success;
    if (!m_ResourceTables->ResolveBuffer(handle, record, resolveCode))
    {
        outResult.Code = resolveCode;
        return false;
    }

    RendererResultCode unmapMarkCode = RendererResultCode::Success;
    if (!m_ResourceTables->MarkBufferUnmapped(handle, unmapMarkCode))
    {
        outResult.Code = unmapMarkCode;
        return false;
    }

    m_Context->Unmap(record->Buffer.Get(), 0);
    outResult.Code = RendererResultCode::Success;
    return true;
}

ID3D11Buffer* DX11Buffers::ResolveBuffer(RendererBufferHandle handle, RendererResultCode& outError) const
{
    if (!m_ResourceTables)
    {
        outError = RendererResultCode::NotInitialized;
        return nullptr;
    }

    const DX11BufferRecord* record = nullptr;
    if (!m_ResourceTables->ResolveBuffer(handle, record, outError))
    {
        return nullptr;
    }

    outError = RendererResultCode::Success;
    return record->Buffer.Get();
}

} // namespace LXEngine
