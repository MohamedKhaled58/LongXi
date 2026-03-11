#pragma once

#include "Math/Math.h"
#include "Renderer/RendererTypes.h"

#include <Windows.h>
#include <cstdint>
#include <memory>

namespace LongXi
{

class Renderer
{
public:
    virtual ~Renderer() = default;

    virtual bool Initialize(HWND hwnd, int width, int height) = 0;
    virtual void BeginFrame() = 0;
    virtual bool BeginPass(RenderPassType passType) = 0;
    virtual void EndPass() = 0;
    virtual void ExecuteExternalPass(const ExternalPassCallback& callback) = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    virtual void OnResize(int width, int height) = 0;
    virtual void Shutdown() = 0;

    virtual void Clear(const RendererColor& color) = 0;
    virtual void SetViewport(const RendererViewport& viewport) = 0;
    virtual void SetRenderTarget() = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) = 0;
    virtual void SetViewProjection(const Matrix4& view, const Matrix4& projection) = 0;

    virtual RendererTextureHandle CreateTexture(const RendererTextureDesc& desc) = 0;
    virtual bool DestroyTexture(RendererTextureHandle handle) = 0;

    virtual RendererVertexBufferHandle CreateVertexBuffer(const RendererBufferDesc& desc) = 0;
    virtual RendererIndexBufferHandle CreateIndexBuffer(const RendererBufferDesc& desc) = 0;
    virtual RendererConstantBufferHandle CreateConstantBuffer(const RendererBufferDesc& desc) = 0;
    virtual bool DestroyBuffer(RendererBufferHandle handle) = 0;

    virtual RendererShaderHandle CreateVertexShader(const RendererShaderDesc& desc) = 0;
    virtual RendererShaderHandle CreatePixelShader(const RendererShaderDesc& desc) = 0;
    virtual bool DestroyShader(RendererShaderHandle handle) = 0;

    virtual bool UpdateBuffer(const RendererBufferUpdateRequest& request) = 0;
    virtual bool MapBuffer(const RendererMapRequest& request, RendererMappedResource& mapped) = 0;
    virtual bool UnmapBuffer(RendererBufferHandle handle) = 0;

    virtual bool BindVertexBuffer(RendererVertexBufferHandle handle, uint32_t stride, uint32_t offset) = 0;
    virtual bool BindIndexBuffer(RendererIndexBufferHandle handle, RendererIndexFormat format, uint32_t offset) = 0;
    virtual bool BindConstantBuffer(RendererConstantBufferHandle handle, RendererShaderStage stage, uint32_t slot) = 0;
    virtual bool BindShader(RendererShaderHandle handle) = 0;
    virtual bool BindTexture(RendererTextureHandle handle, RendererShaderStage stage, uint32_t slot) = 0;

    virtual RendererResult GetLastResourceResult() const = 0;

    virtual bool IsInitialized() const = 0;
    virtual FrameLifecyclePhase GetLifecyclePhase() const = 0;
    virtual RenderPassType GetActivePass() const = 0;
    virtual RendererRecoveryMode GetRecoveryMode() const = 0;
    virtual int GetViewportWidth() const = 0;
    virtual int GetViewportHeight() const = 0;

    virtual void* GetNativeDeviceHandle() const = 0;
    virtual void* GetNativeContextHandle() const = 0;
};

std::unique_ptr<Renderer> CreateRenderer();

} // namespace LongXi
