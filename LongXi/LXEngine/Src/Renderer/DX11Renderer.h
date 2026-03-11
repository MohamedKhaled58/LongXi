#pragma once

#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>

#include "Math/Math.h"
#include "Renderer/RendererTypes.h"
#include "Texture/TextureFormat.h"

namespace LongXi
{

// Renderer texture handle type: ComPtr-wrapped D3D11 shader resource view.
using RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;

class DX11Renderer
{
  public:
    DX11Renderer();
    ~DX11Renderer();

    DX11Renderer(const DX11Renderer&) = delete;
    DX11Renderer& operator=(const DX11Renderer&) = delete;

    bool Initialize(HWND hwnd, int width, int height);
    void BeginFrame();
    bool BeginPass(RenderPassType passType);
    void EndPass();
    void ExecuteExternalPass(const ExternalPassCallback& callback);
    void EndFrame();
    void Present();
    void OnResize(int width, int height);
    void Shutdown();

    void SetViewProjection(const Matrix4& view, const Matrix4& projection);

    bool IsInitialized() const
    {
        return m_IsInitialized;
    }

    FrameLifecyclePhase GetLifecyclePhase() const
    {
        return m_LifecyclePhase;
    }

    RenderPassType GetActivePass() const
    {
        return m_ActivePass;
    }

    RendererRecoveryMode GetRecoveryMode() const
    {
        return m_RecoveryMode;
    }

    // Opaque native handles for shell-side integrations.
    void* GetNativeDeviceHandle() const
    {
        return m_Device.Get();
    }

    void* GetNativeContextHandle() const
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

    int GetViewportWidth() const
    {
        return m_ViewportWidth;
    }

    int GetViewportHeight() const
    {
        return m_ViewportHeight;
    }

    HWND GetWindowHandle() const
    {
        return m_WindowHandle;
    }

    RendererTextureHandle CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* pixels);

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

  private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_DepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DepthStencilView;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_DefaultRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_DefaultBlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DefaultDepthState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_NoDepthState;

    bool m_IsInitialized = false;
    HWND m_WindowHandle = nullptr;
    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;

    Matrix4 m_CurrentViewMatrix = {};
    Matrix4 m_CurrentProjectionMatrix = {};

    FrameLifecyclePhase m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    RenderPassType m_ActivePass = RenderPassType::None;
    RendererRecoveryMode m_RecoveryMode = RendererRecoveryMode::Normal;

    bool m_HasPendingResize = false;
    int m_PendingResizeWidth = 0;
    int m_PendingResizeHeight = 0;
};

} // namespace LongXi
