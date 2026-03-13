#include "Renderer/SpritePipelineBridge.h"

#include "Renderer/Backend/DX11/DX11SpritePipeline.h"

namespace LXEngine
{

class DX11SpritePipelineBridge final : public SpritePipelineBridge
{
public:
    bool Initialize(Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource) override
    {
        return m_Impl.Initialize(renderer, maxSpritesPerBatch, vertexShaderSource, pixelShaderSource);
    }

    void Shutdown() override
    {
        m_Impl.Shutdown();
    }

    bool IsInitialized() const override
    {
        return m_Impl.IsInitialized();
    }

    void BindBatchPipeline(Renderer& renderer) const override
    {
        m_Impl.BindBatchPipeline(renderer);
    }

    void UploadProjectionMatrix(Renderer& renderer, const float* matrixData, std::size_t matrixBytes) override
    {
        m_Impl.UploadProjectionMatrix(renderer, matrixData, matrixBytes);
    }

    void FlushBatch(Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const override
    {
        m_Impl.FlushBatch(renderer, vertices, spriteCount, texture);
    }

private:
    DX11SpritePipeline m_Impl;
};

std::unique_ptr<SpritePipelineBridge> CreateSpritePipelineBridge()
{
    return std::make_unique<DX11SpritePipelineBridge>();
}

} // namespace LXEngine
