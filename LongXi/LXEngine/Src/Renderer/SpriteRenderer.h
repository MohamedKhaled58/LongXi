#pragma once

#include "Math/Math.h"

#include <d3d11.h>
#include <wrl/client.h>

// =============================================================================
// SpriteRenderer — Batched screen-space textured quad renderer
// Manages GPU pipeline state, sprite batching, and DX11 draw submission.
// Owned and initialized by Engine as subsystem #5.
// =============================================================================

namespace LongXi
{

class DX11Renderer;
class Texture;

static constexpr int MAX_SPRITES_PER_BATCH = 1024;

struct SpriteVertex
{
    Vector2 Position;
    Vector2 UV;
    Color Color;
};

class SpriteRenderer
{
  public:
    SpriteRenderer();
    ~SpriteRenderer();

    // Non-copyable, non-movable
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Initialize GPU pipeline state. Returns false on failure (graceful — engine continues).
    bool Initialize(DX11Renderer& renderer, int width, int height);

    // Release all GPU resources. Safe to call even if Initialize() failed.
    void Shutdown();

    // Returns true if Initialize() succeeded and SpriteRenderer is functional.
    bool IsInitialized() const;

    // =========================================================================
    // Resize
    // =========================================================================

    // Update orthographic projection matrix. Ignored if width <= 0 || height <= 0.
    void OnResize(int width, int height);

    // =========================================================================
    // Frame Rendering
    // =========================================================================

    // Begin sprite submission block. Must be called before DrawSprite().
    void Begin();

    // Draw textured quad at screen-space position with full UV and white tint.
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size);

    // Draw textured quad with explicit UV sub-region and RGBA color tint.
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size, Vector2 uvMin, Vector2 uvMax, LongXi::Color color);

    // Flush all pending sprites to GPU and end submission block.
    void End();

  private:
    void FlushBatch();
    void UpdateProjection(int width, int height);

  private:
    DX11Renderer* m_Renderer = nullptr;

    // GPU resources (ComPtr RAII)
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

    // CPU-side batch buffer
    SpriteVertex m_VertexData[MAX_SPRITES_PER_BATCH * 4];

    // Batch state
    int m_SpriteCount = 0;
    Texture* m_CurrentTexture = nullptr;

    // Lifecycle state
    bool m_Initialized = false;
    bool m_InBatch = false;

    // Current orthographic projection (row-major 4x4)
    float m_ProjectionMatrix[16] = {};
};

} // namespace LongXi
