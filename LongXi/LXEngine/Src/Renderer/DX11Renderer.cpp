#include "Renderer/DX11Renderer.h"
#include "Core/Logging/LogMacros.h"

#include <stdint.h>

namespace LongXi
{

// Debug configuration
#ifdef LX_DEBUG
#define LX_DX11_ENABLE_DEBUG_LAYER 1
#else
#define LX_DX11_ENABLE_DEBUG_LAYER 0
#endif

// Cornflower blue clear color (traditional DX bootstrap color)
static const float CLEAR_COLOR[4] = {0.392f, 0.584f, 0.929f, 1.0f};

// ============================================================================
// Constructor / Destructor
// ============================================================================

DX11Renderer::DX11Renderer() : m_IsInitialized(false) {}

DX11Renderer::~DX11Renderer()
{
    Shutdown();
}

// ============================================================================
// Initialize
// ============================================================================

bool DX11Renderer::Initialize(HWND hwnd, int width, int height)
{
    if (m_IsInitialized)
    {
        LX_ENGINE_ERROR("DX11Renderer already initialized");
        return false;
    }

    if (!hwnd)
    {
        LX_ENGINE_ERROR("Invalid window handle passed to DX11Renderer::Initialize");
        return false;
    }

    if (width <= 0 || height <= 0)
    {
        LX_ENGINE_ERROR("Invalid window dimensions: {}x{}", width, height);
        return false;
    }

    LX_ENGINE_INFO("Initializing DX11 renderer...");

    // Configure swap chain descriptor
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

    // Set device creation flags
    UINT createFlags = 0;
#if LX_DX11_ENABLE_DEBUG_LAYER
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Request feature level 11_0 only
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
    UINT featureLevelCount = 1;

    D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    // Create device and swap chain in one call
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,                  // Default adapter
                                               D3D_DRIVER_TYPE_HARDWARE, // Hardware rendering
                                               nullptr,                  // No software rasterizer
                                               createFlags,              // Creation flags
                                               featureLevels,            // Requested feature levels
                                               featureLevelCount,        // Number of feature levels
                                               D3D11_SDK_VERSION,        // SDK version
                                               &swapChainDesc,           // Swap chain descriptor
                                               &m_SwapChain,             // Output swap chain
                                               &m_Device,                // Output device
                                               &createdFeatureLevel,     // Actual feature level created
                                               &m_Context                // Output device context
    );

    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("D3D11CreateDeviceAndSwapChain failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    // Verify we got at least feature level 11_0
    if (createdFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        LX_ENGINE_ERROR("Insufficient DX11 feature level: {}", static_cast<int>(createdFeatureLevel));
        Shutdown();
        return false;
    }

    // Create render target view from back buffer
    if (!CreateRenderTarget())
    {
        LX_ENGINE_ERROR("Failed to create render target view");
        Shutdown();
        return false;
    }

    m_IsInitialized = true;
    LX_ENGINE_INFO("DX11 renderer initialized (Feature Level: 11_0)");
    return true;
}

// ============================================================================
// CreateRenderTarget
// ============================================================================

bool DX11Renderer::CreateRenderTarget()
{
    // Get back buffer from swap chain
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("Failed to get back buffer from swap chain (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    // Create render target view
    hr = m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTargetView);
    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("Failed to create render target view (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    return true;
}
// ============================================================================
// ReleaseRenderTarget
// ============================================================================

void DX11Renderer::ReleaseRenderTarget()
{
    // Unbind from pipeline before releasing — DXGI requires all references
    // to back buffer resources be released before ResizeBuffers
    if (m_Context)
    {
        ID3D11RenderTargetView* nullRTV = nullptr;
        m_Context->OMSetRenderTargets(1, &nullRTV, nullptr);
    }
    m_RenderTargetView.Reset();
}

// ============================================================================
// BeginFrame
// ============================================================================

void DX11Renderer::BeginFrame()
{
    if (!m_IsInitialized)
    {
        return;
    }

    // Bind render target
    m_Context->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), nullptr);

    // Clear to cornflower blue
    m_Context->ClearRenderTargetView(m_RenderTargetView.Get(), CLEAR_COLOR);
}

// ============================================================================
// EndFrame
// ============================================================================

void DX11Renderer::EndFrame()
{
    if (!m_IsInitialized)
    {
        return;
    }

    // Present with VSync (SyncInterval = 1)
    m_SwapChain->Present(1, 0);
}

// ============================================================================
// OnResize
// ============================================================================

void DX11Renderer::OnResize(int width, int height)
{
    if (!m_IsInitialized)
    {
        return;
    }

    // Skip zero-area resize (minimized window)
    if (width <= 0 || height <= 0)
    {
        return;
    }

    // Release existing render target
    ReleaseRenderTarget();

    // Resize swap chain buffers
    HRESULT hr = m_SwapChain->ResizeBuffers(0,                   // Preserve buffer count
                                            width,               // New width
                                            height,              // New height
                                            DXGI_FORMAT_UNKNOWN, // Preserve format
                                            0                    // No flags
    );

    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("ResizeBuffers failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return;
    }

    // Recreate render target view
    if (!CreateRenderTarget())
    {
        LX_ENGINE_ERROR("Failed to recreate render target after resize");
    }
}

// ============================================================================
// CreateTexture
// ============================================================================

RendererTextureHandle DX11Renderer::CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* pixels)
{
    if (!m_IsInitialized)
    {
        LX_ENGINE_ERROR("[Texture] Cannot create texture: renderer not initialized");
        return RendererTextureHandle();
    }

    if (width == 0 || height == 0)
    {
        LX_ENGINE_ERROR("[Texture] Invalid texture dimensions: {}x{}", width, height);
        return RendererTextureHandle();
    }

    if (!pixels)
    {
        LX_ENGINE_ERROR("[Texture] Null pixel data pointer");
        return RendererTextureHandle();
    }

    // Map TextureFormat to DXGI_FORMAT
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
        LX_ENGINE_ERROR("[Texture] Unknown texture format: {}", static_cast<uint32_t>(format));
        return RendererTextureHandle();
    }

    // Compute row pitch
    UINT rowPitch;
    if (format == TextureFormat::RGBA8)
    {
        // Uncompressed: width * 4 bytes
        rowPitch = width * 4;
    }
    else
    {
        // Compressed (DXT): block-row formula
        // blockCols = max(1, (width + 3) / 4)
        // rowPitch = blockCols * blockSize
        UINT blockCols = (width + 3) / 4;
        if (blockCols == 0)
            blockCols = 1;

        if (format == TextureFormat::DXT1)
        {
            rowPitch = blockCols * 8;
        }
        else // DXT3 or DXT5
        {
            rowPitch = blockCols * 16;
        }
    }

    // Create texture description
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = dxgiFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    // Initialize subresource data
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = rowPitch;
    initData.SysMemSlicePitch = 0; // Not used for 2D textures

    // Create texture
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTex;
    HRESULT hr = m_Device->CreateTexture2D(&texDesc, &initData, &pTex);

    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Texture] CreateTexture2D failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return RendererTextureHandle();
    }

    // Create shader resource view
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;
    hr = m_Device->CreateShaderResourceView(pTex.Get(), nullptr, &pSRV);

    if (FAILED(hr))
    {
        LX_ENGINE_ERROR("[Texture] CreateShaderResourceView failed (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return RendererTextureHandle();
    }

    // Release Texture2D immediately — SRV holds the sole reference
    // When SRV is destroyed, Texture2D will be freed automatically
    pTex.Reset();

    return pSRV;
}

// ============================================================================
// Shutdown
// ============================================================================

void DX11Renderer::Shutdown()
{
    if (!m_IsInitialized)
    {
        return;
    }

    LX_ENGINE_INFO("DX11 renderer shutting down...");

    // Release in reverse-creation order
    ReleaseRenderTarget();
    m_SwapChain.Reset();
    m_Context.Reset();
    m_Device.Reset();

    m_IsInitialized = false;
    LX_ENGINE_INFO("DX11 renderer shutdown complete");
}

} // namespace LongXi
