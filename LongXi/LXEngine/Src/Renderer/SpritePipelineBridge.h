#pragma once

#include <cstddef>
#include <memory>

namespace LongXi
{

class Renderer;
class Texture;
struct SpriteVertex;

class SpritePipelineBridge
{
public:
    virtual ~SpritePipelineBridge() = default;

    virtual bool Initialize(Renderer& renderer, int maxSpritesPerBatch, const char* vertexShaderSource, const char* pixelShaderSource) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual void BindBatchPipeline(Renderer& renderer) const = 0;
    virtual void UploadProjectionMatrix(Renderer& renderer, const float* matrixData, std::size_t matrixBytes) = 0;
    virtual void FlushBatch(Renderer& renderer, const SpriteVertex* vertices, int spriteCount, const Texture& texture) const = 0;
};

std::unique_ptr<SpritePipelineBridge> CreateSpritePipelineBridge();

} // namespace LongXi
