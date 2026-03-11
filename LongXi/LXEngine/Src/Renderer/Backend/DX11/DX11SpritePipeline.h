#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <cstddef>

namespace LongXi
{

class DX11Renderer;
class Texture;
struct SpriteVertex;

class DX11SpritePipeline
{
  public:
    bool Initialize(DX11Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource);
    void Shutdown();
    bool IsInitialized() const;

    bool ValidateRendererState(DX11Renderer& renderer) const;
    void BindBatchPipeline(DX11Renderer& renderer) const;
    void UploadProjectionMatrix(DX11Renderer& renderer, const float* matrixData, std::size_t matrixBytes);
    void FlushBatch(DX11Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const;

  private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_SamplerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RasterizerState;

    int m_MaxSpritesPerBatch = 0;
    bool m_Initialized = false;
};

} // namespace LongXi
