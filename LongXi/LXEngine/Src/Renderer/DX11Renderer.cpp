#include "Renderer/DX11Renderer.h"

#include "Core/Logging/LogMacros.h"

#include <cassert>
#include <cstring>

namespace LongXi
{

#ifdef LX_DEBUG
#define LX_DX11_ENABLE_DEBUG_LAYER 1
#else
#define LX_DX11_ENABLE_DEBUG_LAYER 0
#endif

static const float CLEAR_COLOR[4] = {0.5f, 0.5f, 0.9f, 1.0f};

static bool IsDeviceLost(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

static Matrix4 MakeIdentityMatrix()
{
    Matrix4 matrix = {};
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    return matrix;
}

DX11Renderer::DX11Renderer()
    : m_CurrentViewMatrix(MakeIdentityMatrix())
    , m_CurrentProjectionMatrix(MakeIdentityMatrix())
{
}

DX11Renderer::~DX11Renderer()
{
    Shutdown();
}

bool DX11Renderer::Initialize(HWND hwnd, int width, int height)
{
    if (m_IsInitialized)
    {
        LX_ENGINE_ERROR("[Renderer] Initialize called more than once");
        return false;
    }

    if (!hwnd)
    {
        LX_ENGINE_ERROR("[Renderer] Initialize failed: invalid window handle");
        return false;
    }

    if (width <= 0 || height <= 0)
    {
        LX_ENGINE_ERROR("[Renderer] Initialize failed: invalid dimensions {}x{}", width, height);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
#if LX_DX11_ENABLE_DEBUG_LAYER
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, featureLevels, 1, D3D11_SDK_VERSION, &swapChainDesc, &m_SwapChain, &m_Device, &createdFeatureLevel, &m_Context);

    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] D3D11CreateDeviceAndSwapChain failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    if (createdFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        LX_ENGINE_ERROR("[Renderer] Insufficient feature level: {}", static_cast<int>(createdFeatureLevel));
        Shutdown();
        return false;
    }

    m_WindowHandle = hwnd;
    m_ViewportWidth = width;
    m_ViewportHeight = height;

    if (!CreateRenderTarget() || !CreateDepthBuffer() || !CreateDefaultStates())
    {
        LX_ENGINE_ERROR("[Renderer] Failed to create baseline resources/state");
        Shutdown();
        return false;
    }

    m_IsInitialized = true;
    m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    m_ActivePass = RenderPassType::None;
    m_RecoveryMode = RendererRecoveryMode::Normal;

    LX_ENGINE_INFO("[Renderer] Device initialized");
    return true;
}

bool DX11Renderer::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] GetBuffer failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    hr = m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTargetView);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateRenderTargetView failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    return true;
}

bool DX11Renderer::CreateDepthBuffer()
{
    if (m_ViewportWidth <= 0 || m_ViewportHeight <= 0)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = static_cast<UINT>(m_ViewportWidth);
    depthDesc.Height = static_cast<UINT>(m_ViewportHeight);
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_Device->CreateTexture2D(&depthDesc, nullptr, &m_DepthStencilBuffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] Create depth buffer failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    hr = m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, &m_DepthStencilView);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] CreateDepthStencilView failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    return true;
}

bool DX11Renderer::CreateDefaultStates()
{
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.DepthClipEnable = TRUE;

    HRESULT hr = m_Device->CreateRasterizerState(&rasterDesc, &m_DefaultRasterizerState);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] Create default rasterizer state failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_Device->CreateBlendState(&blendDesc, &m_DefaultBlendState);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] Create default blend state failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    hr = m_Device->CreateDepthStencilState(&depthDesc, &m_DefaultDepthState);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] Create default depth state failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    hr = m_Device->CreateDepthStencilState(&depthDesc, &m_NoDepthState);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] Create no-depth state failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    return true;
}

void DX11Renderer::ReleaseRenderTarget()
{
    if (m_Context)
    {
        ID3D11RenderTargetView* nullRTV = nullptr;
        m_Context->OMSetRenderTargets(1, &nullRTV, nullptr);
    }

    m_DepthStencilView.Reset();
    m_DepthStencilBuffer.Reset();
    m_RenderTargetView.Reset();
}

void DX11Renderer::QueueResize(int width, int height)
{
    m_HasPendingResize = true;
    m_PendingResizeWidth = width;
    m_PendingResizeHeight = height;
}

bool DX11Renderer::ApplyResizeNow(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    ReleaseRenderTarget();

    const HRESULT hr = m_SwapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Renderer] ResizeBuffers failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    if (!CreateRenderTarget() || !CreateDepthBuffer())
    {
        return false;
    }

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_ViewportWidth);
    viewport.Height = static_cast<float>(m_ViewportHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_Context->RSSetViewports(1, &viewport);

    LX_ENGINE_INFO("[Renderer] Swapchain resized: {}x{}", width, height);
    LX_ENGINE_INFO("[Renderer] Viewport updated: {}x{}", width, height);
    return true;
}

void DX11Renderer::ApplyPendingResizeIfNeeded()
{
    if (!m_HasPendingResize)
    {
        return;
    }

    if (m_PendingResizeWidth <= 0 || m_PendingResizeHeight <= 0)
    {
        return;
    }

    if (ApplyResizeNow(m_PendingResizeWidth, m_PendingResizeHeight))
    {
        m_HasPendingResize = false;
        m_PendingResizeWidth = 0;
        m_PendingResizeHeight = 0;
    }
}

void DX11Renderer::HandleContractViolation(const char* operation, const char* expectedState)
{
    LX_ENGINE_ERROR("[Renderer] Contract violation in {} (expected: {})", operation, expectedState);
#if defined(LX_DEBUG) || defined(LX_DEV)
    assert(false && "Renderer contract violation");
#endif
}

void DX11Renderer::EnterRecoveryMode(const char* reason, HRESULT hr)
{
    LX_ENGINE_ERROR("[Renderer] Entering recovery mode: {} (HRESULT: 0x{:08X})", reason, static_cast<uint32_t>(hr));
    m_RecoveryMode = RendererRecoveryMode::SafeNoRender;
    m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    m_ActivePass = RenderPassType::None;
}

void DX11Renderer::BeginFrame()
{
    if (!m_IsInitialized)
    {
        return;
    }

    if (m_LifecyclePhase != FrameLifecyclePhase::NotStarted && m_LifecyclePhase != FrameLifecyclePhase::FrameEnded)
    {
        HandleContractViolation("BeginFrame", "NotStarted or FrameEnded");
        return;
    }

    if (m_RecoveryMode == RendererRecoveryMode::SafeNoRender)
    {
        if (m_HasPendingResize && m_PendingResizeWidth > 0 && m_PendingResizeHeight > 0)
        {
            m_RecoveryMode = RendererRecoveryMode::RecoveryPending;
        }
        else
        {
            return;
        }
    }

    if (m_RecoveryMode == RendererRecoveryMode::RecoveryPending)
    {
        m_RecoveryMode = RendererRecoveryMode::Reinitializing;
        if (!ApplyResizeNow(m_PendingResizeWidth, m_PendingResizeHeight))
        {
            m_RecoveryMode = RendererRecoveryMode::SafeNoRender;
            return;
        }

        m_HasPendingResize = false;
        m_PendingResizeWidth = 0;
        m_PendingResizeHeight = 0;
        m_RecoveryMode = RendererRecoveryMode::Normal;
        LX_ENGINE_INFO("[Renderer] Recovery completed");
    }

    ApplyPendingResizeIfNeeded();

    if (!m_RenderTargetView || !m_DepthStencilView)
    {
        HandleContractViolation("BeginFrame", "Valid render/depth targets");
        return;
    }

    SetRenderTarget();
    SetViewport({0.0f, 0.0f, static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight), 0.0f, 1.0f});

    m_Context->RSSetState(m_DefaultRasterizerState.Get());
    m_Context->OMSetBlendState(m_DefaultBlendState.Get(), nullptr, 0xFFFFFFFF);
    m_Context->OMSetDepthStencilState(m_DefaultDepthState.Get(), 0);

    Clear({CLEAR_COLOR[0], CLEAR_COLOR[1], CLEAR_COLOR[2], CLEAR_COLOR[3]});
    m_Context->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_LifecyclePhase = FrameLifecyclePhase::InFrame;
    m_ActivePass = RenderPassType::None;
}

void DX11Renderer::Clear(const RendererColor& color)
{
    if (!m_IsInitialized || !m_RenderTargetView)
    {
        return;
    }

    const float clearColor[4] = {color.R, color.G, color.B, color.A};
    m_Context->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);
}

void DX11Renderer::SetViewport(const RendererViewport& viewport)
{
    if (!m_IsInitialized)
    {
        return;
    }

    D3D11_VIEWPORT dxViewport = {};
    dxViewport.TopLeftX = viewport.X;
    dxViewport.TopLeftY = viewport.Y;
    dxViewport.Width = viewport.Width;
    dxViewport.Height = viewport.Height;
    dxViewport.MinDepth = viewport.MinDepth;
    dxViewport.MaxDepth = viewport.MaxDepth;
    m_Context->RSSetViewports(1, &dxViewport);

    if (viewport.Width > 0.0f && viewport.Height > 0.0f)
    {
        m_ViewportWidth = static_cast<int>(viewport.Width);
        m_ViewportHeight = static_cast<int>(viewport.Height);
    }
}

void DX11Renderer::SetRenderTarget()
{
    if (!m_IsInitialized || !m_RenderTargetView || !m_DepthStencilView)
    {
        return;
    }

    m_Context->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), m_DepthStencilView.Get());
}

void DX11Renderer::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex)
{
    if (!m_IsInitialized || m_LifecyclePhase != FrameLifecyclePhase::InPass)
    {
        return;
    }

    m_Context->DrawIndexed(indexCount, startIndex, baseVertex);
}

void DX11Renderer::ApplyPassState(RenderPassType passType)
{
    switch (passType)
    {
        case RenderPassType::Scene:
            m_Context->OMSetDepthStencilState(m_DefaultDepthState.Get(), 0);
            break;
        case RenderPassType::Sprite:
        case RenderPassType::DebugUI:
        case RenderPassType::External:
            m_Context->OMSetDepthStencilState(m_NoDepthState.Get(), 0);
            break;
        case RenderPassType::None:
        default:
            break;
    }
}

bool DX11Renderer::BeginPass(RenderPassType passType)
{
    if (!m_IsInitialized)
    {
        return false;
    }

    if (m_LifecyclePhase != FrameLifecyclePhase::InFrame || m_ActivePass != RenderPassType::None)
    {
        HandleContractViolation("BeginPass", "InFrame with no active pass");
        return false;
    }

    ApplyPassState(passType);
    m_ActivePass = passType;
    m_LifecyclePhase = FrameLifecyclePhase::InPass;
    return true;
}

void DX11Renderer::EndPass()
{
    if (!m_IsInitialized)
    {
        return;
    }

    if (m_LifecyclePhase != FrameLifecyclePhase::InPass)
    {
        HandleContractViolation("EndPass", "InPass");
        return;
    }

    m_ActivePass = RenderPassType::None;
    m_LifecyclePhase = FrameLifecyclePhase::InFrame;

    // Restore baseline depth state between passes.
    m_Context->OMSetDepthStencilState(m_DefaultDepthState.Get(), 0);
}

void DX11Renderer::ExecuteExternalPass(const ExternalPassCallback& callback)
{
    if (!callback)
    {
        return;
    }

    if (!BeginPass(RenderPassType::External))
    {
        return;
    }

    callback();
    EndPass();
}

void DX11Renderer::EndFrame()
{
    if (!m_IsInitialized)
    {
        return;
    }

    if (m_LifecyclePhase == FrameLifecyclePhase::InPass)
    {
        HandleContractViolation("EndFrame", "InFrame with no active pass");
        return;
    }

    if (m_LifecyclePhase != FrameLifecyclePhase::InFrame)
    {
        HandleContractViolation("EndFrame", "InFrame");
        return;
    }

    m_LifecyclePhase = FrameLifecyclePhase::FrameEnded;
}

void DX11Renderer::Present()
{
    if (!m_IsInitialized)
    {
        return;
    }

    if (m_LifecyclePhase != FrameLifecyclePhase::FrameEnded)
    {
        HandleContractViolation("Present", "FrameEnded");
        return;
    }

    if (m_RecoveryMode != RendererRecoveryMode::Normal)
    {
        m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
        return;
    }

    const HRESULT hr = m_SwapChain->Present(1, 0);
    if (FAILED(hr))
    {
        if (IsDeviceLost(hr))
        {
            const HRESULT reason = m_Device ? m_Device->GetDeviceRemovedReason() : hr;
            EnterRecoveryMode("Present device lost/reset", reason);
            return;
        }

        EnterRecoveryMode("Present failed", hr);
        return;
    }

    m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    m_ActivePass = RenderPassType::None;
}

void DX11Renderer::OnResize(int width, int height)
{
    if (!m_IsInitialized)
    {
        return;
    }

    if (width <= 0 || height <= 0)
    {
        QueueResize(width, height);
        return;
    }

    if (m_LifecyclePhase == FrameLifecyclePhase::InFrame || m_LifecyclePhase == FrameLifecyclePhase::InPass)
    {
        QueueResize(width, height);
        return;
    }

    if (!ApplyResizeNow(width, height))
    {
        EnterRecoveryMode("OnResize failed", E_FAIL);
        QueueResize(width, height);
        return;
    }

    m_HasPendingResize = false;
    m_PendingResizeWidth = 0;
    m_PendingResizeHeight = 0;
}

RendererTextureHandle DX11Renderer::CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* pixels)
{
    if (!m_IsInitialized)
    {
        LX_ENGINE_ERROR("[Texture] Cannot create texture: renderer not initialized");
        return {};
    }

    if (width == 0 || height == 0 || !pixels)
    {
        LX_ENGINE_ERROR("[Texture] Invalid texture creation parameters");
        return {};
    }

    DXGI_FORMAT dxgiFormat;
    switch (format)
    {
        case TextureFormat::RGBA8:
            dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case TextureFormat::DXT1:
            dxgiFormat = DXGI_FORMAT_BC1_UNORM;
            break;
        case TextureFormat::DXT3:
            dxgiFormat = DXGI_FORMAT_BC2_UNORM;
            break;
        case TextureFormat::DXT5:
            dxgiFormat = DXGI_FORMAT_BC3_UNORM;
            break;
        default:
            LX_ENGINE_ERROR("[Texture] Unsupported texture format: {}", static_cast<uint32_t>(format));
            return {};
    }

    UINT rowPitch = 0;
    if (format == TextureFormat::RGBA8)
    {
        rowPitch = width * 4;
    }
    else
    {
        UINT blockCols = (width + 3) / 4;
        if (blockCols == 0)
        {
            blockCols = 1;
        }
        rowPitch = (format == TextureFormat::DXT1) ? (blockCols * 8) : (blockCols * 16);
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = dxgiFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = rowPitch;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = m_Device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Texture] CreateTexture2D failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return {};
    }

    ID3D11ShaderResourceView* rawSrv = nullptr;
    hr = m_Device->CreateShaderResourceView(texture.Get(), nullptr, &rawSrv);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Texture] CreateShaderResourceView failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return {};
    }

    RendererTextureHandle handle;
    handle.NativeResource = std::shared_ptr<void>(rawSrv,
                                                  [](void* resource)
                                                  {
                                                      if (resource)
                                                      {
                                                          static_cast<ID3D11ShaderResourceView*>(resource)->Release();
                                                      }
                                                  });

    return handle;
}

void DX11Renderer::SetViewProjection(const Matrix4& view, const Matrix4& projection)
{
    std::memcpy(m_CurrentViewMatrix.m, view.m, sizeof(m_CurrentViewMatrix.m));
    std::memcpy(m_CurrentProjectionMatrix.m, projection.m, sizeof(m_CurrentProjectionMatrix.m));
}

void DX11Renderer::Shutdown()
{
    if (!m_IsInitialized)
    {
        return;
    }

    ReleaseRenderTarget();

    m_NoDepthState.Reset();
    m_DefaultDepthState.Reset();
    m_DefaultBlendState.Reset();
    m_DefaultRasterizerState.Reset();

    m_SwapChain.Reset();
    m_Context.Reset();
    m_Device.Reset();

    m_ViewportWidth = 0;
    m_ViewportHeight = 0;
    m_CurrentViewMatrix = MakeIdentityMatrix();
    m_CurrentProjectionMatrix = MakeIdentityMatrix();

    m_LifecyclePhase = FrameLifecyclePhase::NotStarted;
    m_ActivePass = RenderPassType::None;
    m_RecoveryMode = RendererRecoveryMode::Normal;
    m_HasPendingResize = false;
    m_PendingResizeWidth = 0;
    m_PendingResizeHeight = 0;

    m_IsInitialized = false;
    LX_ENGINE_INFO("[Renderer] Shutdown complete");
}

} // namespace LongXi
