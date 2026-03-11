#pragma once

#include "Renderer/Backend/DX11/DX11ResourceTables.h"

namespace LongXi
{

class DX11Buffers
{
public:
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, DX11ResourceTables* resourceTables);
    void Shutdown();

    RendererVertexBufferHandle CreateVertexBuffer(const RendererBufferDesc& desc, RendererResult& outResult);
    RendererIndexBufferHandle CreateIndexBuffer(const RendererBufferDesc& desc, RendererResult& outResult);
    RendererConstantBufferHandle CreateConstantBuffer(const RendererBufferDesc& desc, RendererResult& outResult);
    bool DestroyBuffer(RendererBufferHandle handle, RendererResult& outResult);

    bool UpdateBuffer(const RendererBufferUpdateRequest& request, RendererResult& outResult);
    bool MapBuffer(const RendererMapRequest& request, RendererMappedResource& mapped, RendererResult& outResult);
    bool UnmapBuffer(RendererBufferHandle handle, RendererResult& outResult);

    ID3D11Buffer* ResolveBuffer(RendererBufferHandle handle, RendererResultCode& outError) const;

private:
    RendererBufferHandle RegisterBuffer(const RendererBufferDesc& desc, ID3D11Buffer* buffer, RendererResult& outResult);
    bool ValidateDescriptor(const RendererBufferDesc& desc, RendererResult& outResult) const;

private:
    ID3D11Device* m_Device = nullptr;
    ID3D11DeviceContext* m_Context = nullptr;
    DX11ResourceTables* m_ResourceTables = nullptr;
};

} // namespace LongXi
