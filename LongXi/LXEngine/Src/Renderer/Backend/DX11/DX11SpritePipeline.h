#pragma once

#include <cstddef>
#include <d3d11.h>
#include <wrl/client.h>

#include "Renderer/RendererTypes.h"

namespace LXEngine
{

class Renderer;
class Texture;
struct SpriteVertex;

class DX11SpritePipeline
{
public:
    bool Initialize(Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource);
    void Shutdown();
    bool IsInitialized() const;

    bool ValidateRendererState(Renderer& renderer) const;
    void BindBatchPipeline(Renderer& renderer) const;
    void UploadProjectionMatrix(Renderer& renderer, const float* matrixData, std::size_t matrixBytes);
    void FlushBatch(Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_PixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_InputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_BlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_SamplerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_RasterizerState;

    RendererVertexBufferHandle   m_VertexBufferHandle   = {};
    RendererIndexBufferHandle    m_IndexBufferHandle    = {};
    RendererConstantBufferHandle m_ConstantBufferHandle = {};
    Renderer*                    m_Renderer             = nullptr;

    int  m_MaxSpritesPerBatch = 0;
    bool m_Initialized        = false;
};

} // namespace LXEngine
