#pragma once

#include "Renderer/Backend/DX11/DX11ResourceTables.h"

namespace LXEngine
{

class DX11Textures
{
public:
    void Initialize(ID3D11Device* device, DX11ResourceTables* resourceTables);
    void Shutdown();

    RendererTextureHandle CreateTexture(const RendererTextureDesc& desc, RendererResult& outResult);
    bool                  DestroyTexture(RendererTextureHandle handle, RendererResult& outResult);

    ID3D11ShaderResourceView* ResolveShaderResourceView(RendererTextureHandle handle, RendererResultCode& outError) const;

private:
    ID3D11Device*       m_Device         = nullptr;
    DX11ResourceTables* m_ResourceTables = nullptr;
};

} // namespace LXEngine
