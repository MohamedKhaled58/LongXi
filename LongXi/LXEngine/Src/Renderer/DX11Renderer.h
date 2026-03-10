#pragma once

#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>

// =============================================================================
// DX11Renderer — Minimal DirectX 11 render bootstrap
// Manages device, swap chain, and render target for single-window rendering.
// =============================================================================

namespace LongXi
{

class DX11Renderer
{
  public:
    DX11Renderer();
    ~DX11Renderer();

    // Disable copy
    DX11Renderer(const DX11Renderer&) = delete;
    DX11Renderer& operator=(const DX11Renderer&) = delete;

    // Core lifecycle
    bool Initialize(HWND hwnd, int width, int height);
    void BeginFrame();
    void EndFrame();
    void OnResize(int width, int height);
    void Shutdown();

    bool IsInitialized() const
    {
        return m_IsInitialized;
    }

  private:
    bool CreateRenderTarget();
    void ReleaseRenderTarget();

  private:
    // DX11 COM objects (managed by ComPtr for RAII)
    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView;

    // State tracking
    bool m_IsInitialized;
};

} // namespace LongXi
