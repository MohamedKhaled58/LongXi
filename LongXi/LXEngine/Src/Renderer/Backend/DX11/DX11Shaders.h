#pragma once

#include "Renderer/Backend/DX11/DX11ResourceTables.h"

namespace LXEngine
{

class DX11Shaders
{
public:
    void Initialize(ID3D11Device* device, DX11ResourceTables* resourceTables);
    void Shutdown();

    RendererShaderHandle CreateVertexShader(const RendererShaderDesc& desc, RendererResult& outResult);
    RendererShaderHandle CreatePixelShader(const RendererShaderDesc& desc, RendererResult& outResult);
    bool                 DestroyShader(RendererShaderHandle handle, RendererResult& outResult);

    ID3D11VertexShader* ResolveVertexShader(RendererShaderHandle handle, RendererResultCode& outError) const;
    ID3D11PixelShader*  ResolvePixelShader(RendererShaderHandle handle, RendererResultCode& outError) const;

private:
    RendererShaderHandle CreateShaderInternal(const RendererShaderDesc& desc, RendererShaderStage stage, RendererResult& outResult);

private:
    ID3D11Device*       m_Device         = nullptr;
    DX11ResourceTables* m_ResourceTables = nullptr;
};

} // namespace LXEngine
