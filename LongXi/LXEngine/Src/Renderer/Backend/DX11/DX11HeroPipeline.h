#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace LXEngine
{

class Renderer;

class DX11HeroPipeline
{
public:
    bool Initialize(Renderer& renderer);
    void Shutdown();
    bool IsInitialized() const;

    void BindOpaque(ID3D11DeviceContext* ctx) const;
    void BindTransparent(ID3D11DeviceContext* ctx) const;

    ID3D11VertexShader* GetVertexShader() const;
    ID3D11PixelShader*  GetPixelShader() const;
    ID3D11InputLayout*  GetInputLayout() const;
    ID3D11SamplerState* GetSampler() const;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_PixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_InputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_OpaqueBlendState;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_TransparentBlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_OpaqueDepthState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_TransparentDepthState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_SamplerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_RasterizerState;
    bool                                            m_Initialized = false;
};

} // namespace LXEngine
