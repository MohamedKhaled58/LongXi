#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "Core/Math/Math.h"
#include "Renderer/Backend/DX11/DX11Buffers.h"
#include "Renderer/Backend/DX11/DX11ResourceTables.h"
#include "Renderer/Backend/DX11/DX11Shaders.h"
#include "Renderer/Backend/DX11/DX11Textures.h"
#include "Renderer/Renderer.h"

namespace LXEngine
{

class DX11Renderer final : public Renderer
{
public:
    DX11Renderer();
    ~DX11Renderer() override;

    DX11Renderer(const DX11Renderer&)            = delete;
    DX11Renderer& operator=(const DX11Renderer&) = delete;

    bool Initialize(HWND hwnd, int width, int height) override;
    void BeginFrame() override;
    bool BeginPass(RenderPassType passType) override;
    void EndPass() override;
    void ExecuteExternalPass(const ExternalPassCallback& callback) override;
    void EndFrame() override;
    void Present() override;
    void OnResize(int width, int height) override;
    void Shutdown() override;

    void                         Clear(const RendererColor& color) override;
    void                         SetViewport(const RendererViewport& viewport) override;
    void                         SetRenderTarget() override;
    void                         DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) override;
    void                         SetViewProjection(const LXCore::Matrix4& view, const LXCore::Matrix4& projection) override;
    RendererTextureHandle        CreateTexture(const RendererTextureDesc& desc) override;
    bool                         DestroyTexture(RendererTextureHandle handle) override;
    RendererVertexBufferHandle   CreateVertexBuffer(const RendererBufferDesc& desc) override;
    RendererIndexBufferHandle    CreateIndexBuffer(const RendererBufferDesc& desc) override;
    RendererConstantBufferHandle CreateConstantBuffer(const RendererBufferDesc& desc) override;
    bool                         DestroyBuffer(RendererBufferHandle handle) override;
    RendererShaderHandle         CreateVertexShader(const RendererShaderDesc& desc) override;
    RendererShaderHandle         CreatePixelShader(const RendererShaderDesc& desc) override;
    bool                         DestroyShader(RendererShaderHandle handle) override;
    bool                         UpdateBuffer(const RendererBufferUpdateRequest& request) override;
    bool                         MapBuffer(const RendererMapRequest& request, RendererMappedResource& mapped) override;
    bool                         UnmapBuffer(RendererBufferHandle handle) override;
    bool                         BindVertexBuffer(RendererVertexBufferHandle handle, uint32_t stride, uint32_t offset) override;
    bool                         BindIndexBuffer(RendererIndexBufferHandle handle, RendererIndexFormat format, uint32_t offset) override;
    bool                         BindConstantBuffer(RendererConstantBufferHandle handle, RendererShaderStage stage, uint32_t slot) override;
    bool                         BindShader(RendererShaderHandle handle) override;
    bool                         BindTexture(RendererTextureHandle handle, RendererShaderStage stage, uint32_t slot) override;

    bool IsInitialized() const override
    {
        return m_IsInitialized;
    }

    FrameLifecyclePhase GetLifecyclePhase() const override
    {
        return m_LifecyclePhase;
    }

    RenderPassType GetActivePass() const override
    {
        return m_ActivePass;
    }

    RendererRecoveryMode GetRecoveryMode() const override
    {
        return m_RecoveryMode;
    }

    RendererResult GetLastResourceResult() const override
    {
        return m_LastResourceResult;
    }

    // Opaque native handles for shell-side integrations.
    void* GetNativeDeviceHandle() const override
    {
        return m_Device.Get();
    }

    void* GetNativeContextHandle() const override
    {
        return m_Context.Get();
    }

    // Renderer-module typed accessors.
    ID3D11Device* GetDevice() const
    {
        return m_Device.Get();
    }

    ID3D11DeviceContext* GetContext() const
    {
        return m_Context.Get();
    }

    int GetViewportWidth() const override
    {
        return m_ViewportWidth;
    }

    int GetViewportHeight() const override
    {
        return m_ViewportHeight;
    }

    HWND GetWindowHandle() const
    {
        return m_WindowHandle;
    }

    ID3D11ShaderResourceView* ResolveShaderResourceView(RendererTextureHandle handle, RendererResultCode& outError) const;
    ID3D11Buffer*             ResolveBuffer(RendererBufferHandle handle, RendererResultCode& outError) const;

private:
    bool CreateRenderTarget();
    bool CreateDepthBuffer();
    bool CreateDefaultStates();
    void ReleaseRenderTarget();
    bool ApplyResizeNow(int width, int height);
    void QueueResize(int width, int height);
    void ApplyPendingResizeIfNeeded();
    void EnterRecoveryMode(const char* reason, HRESULT hr);
    void HandleContractViolation(const char* operation, const char* expectedState);
    void ApplyPassState(RenderPassType passType);
    void SetResourceResult(RendererResultCode code);
    bool ValidateDrawState() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Device>           m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_DepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DepthStencilView;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_DefaultRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_DefaultBlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DefaultDepthState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_NoDepthState;

    DX11ResourceTables m_ResourceTables;
    DX11Textures       m_Textures;
    DX11Buffers        m_Buffers;
    DX11Shaders        m_Shaders;

    bool m_IsInitialized  = false;
    HWND m_WindowHandle   = nullptr;
    int  m_ViewportWidth  = 0;
    int  m_ViewportHeight = 0;

    LXCore::Matrix4 m_CurrentViewMatrix       = {};
    LXCore::Matrix4 m_CurrentProjectionMatrix = {};

    FrameLifecyclePhase  m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    RenderPassType       m_ActivePass     = RenderPassType::None;
    RendererRecoveryMode m_RecoveryMode   = RendererRecoveryMode::Normal;

    bool m_HasPendingResize    = false;
    int  m_PendingResizeWidth  = 0;
    int  m_PendingResizeHeight = 0;

    RendererResult m_LastResourceResult                 = {};
    uint64_t       m_FrameIndex                         = 0;
    mutable bool   m_HasLoggedInvalidDrawStateThisFrame = false;
};

} // namespace LXEngine
