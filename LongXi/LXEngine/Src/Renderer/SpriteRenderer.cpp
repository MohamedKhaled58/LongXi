#include "Renderer/SpriteRenderer.h"

#include "Core/Logging/LogMacros.h"
#include "Profiling/ProfileScope.h"
#include "Renderer/SpritePipelineBridge.h"
#include "Texture/Texture.h"

#include <cstring>

// ============================================================================
// Embedded HLSL Shaders
// ============================================================================

static const char* s_VertexShaderSrc = R"(
#pragma pack_matrix(row_major)

cbuffer ProjectionBuffer : register(b0)
{
    float4x4 g_Projection;
};

struct VSInput
{
    float2 Pos : POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

VSOutput VS(VSInput input)
{
    VSOutput o;
    o.Pos = mul(float4(input.Pos, 0.0f, 1.0f), g_Projection);
    o.UV  = input.UV;
    o.Col = input.Col;
    return o;
}
)";

static const char* s_PixelShaderSrc = R"(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
    float4 Col : COLOR0;
};

float4 PS(PSInput input) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, input.UV) * input.Col;
}
)";

namespace LongXi
{

class SpriteRenderer::Impl
{
public:
    std::unique_ptr<SpritePipelineBridge> Pipeline = CreateSpritePipelineBridge();
};

SpriteRenderer::SpriteRenderer()
    : m_Renderer(nullptr)
    , m_Impl(std::make_unique<Impl>())
    , m_SpriteCount(0)
    , m_CurrentTexture(nullptr)
    , m_Initialized(false)
    , m_InBatch(false)
{
    memset(m_ProjectionMatrix, 0, sizeof(m_ProjectionMatrix));
}

SpriteRenderer::~SpriteRenderer()
{
    Shutdown();
}

bool SpriteRenderer::Initialize(Renderer& renderer, int width, int height)
{
    m_Renderer = &renderer;
    if (!m_Impl)
    {
        m_Impl = std::make_unique<Impl>();
    }

    if (!m_Impl->Pipeline || !m_Impl->Pipeline->Initialize(renderer, MAX_SPRITES_PER_BATCH, s_VertexShaderSrc, s_PixelShaderSrc))
    {
        LX_ENGINE_ERROR("[SpriteRenderer] Initialization failed - sprite rendering disabled");
        m_Initialized = false;
        return false;
    }

    UpdateProjection(width, height);

    m_Initialized = true;
    LX_ENGINE_INFO("[SpriteRenderer] Initialized");
    return true;
}

void SpriteRenderer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    if (m_Impl && m_Impl->Pipeline)
    {
        m_Impl->Pipeline->Shutdown();
    }

    m_Initialized = false;
    LX_ENGINE_INFO("[SpriteRenderer] Shutdown");
}

bool SpriteRenderer::IsInitialized() const
{
    return m_Initialized;
}

void SpriteRenderer::OnResize(int width, int height)
{
    if (!m_Initialized)
    {
        return;
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }
    UpdateProjection(width, height);
}

void SpriteRenderer::UpdateProjection(int width, int height)
{
    float w = static_cast<float>(width);
    float h = static_cast<float>(height);

    float proj[16] = {
        2.0f / w,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        -2.0f / h,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        -1.0f,
        1.0f,
        0.0f,
        1.0f,
    };
    memcpy(m_ProjectionMatrix, proj, sizeof(proj));

    if (m_Impl && m_Impl->Pipeline && m_Renderer)
    {
        m_Impl->Pipeline->UploadProjectionMatrix(*m_Renderer, m_ProjectionMatrix, sizeof(m_ProjectionMatrix));
    }
}

void SpriteRenderer::FlushBatch()
{
    if (m_SpriteCount == 0 || !m_CurrentTexture || !m_Impl || !m_Impl->Pipeline || !m_Renderer)
    {
        return;
    }

    LX_PROFILE_SCOPE("SpriteRenderer.FlushBatch");

    // Backend pipeline performs upload/bind via renderer resource APIs.
    m_Impl->Pipeline->FlushBatch(*m_Renderer, m_VertexData, m_SpriteCount, *m_CurrentTexture);

    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;
}

void SpriteRenderer::Begin()
{
    if (!m_Initialized || m_InBatch || !m_Impl || !m_Impl->Pipeline || !m_Renderer)
    {
        return;
    }

    LX_PROFILE_SCOPE("SpriteRenderer.Begin");

    m_InBatch = true;
    m_SpriteCount = 0;
    m_CurrentTexture = nullptr;

    m_Impl->Pipeline->BindBatchPipeline(*m_Renderer);
}

void SpriteRenderer::End()
{
    if (!m_InBatch)
    {
        return;
    }

    LX_PROFILE_SCOPE("SpriteRenderer.End");

    FlushBatch();
    m_InBatch = false;
}

void SpriteRenderer::DrawSprite(const Texture* texture, Vector2 position, Vector2 size)
{
    DrawSprite(texture, position, size, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
}

void SpriteRenderer::DrawSprite(const Texture* texture, Vector2 position, Vector2 size, Vector2 uvMin, Vector2 uvMax, LongXi::Color color)
{
    if (!m_Initialized || !m_InBatch)
    {
        LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite called outside Begin/End block");
        return;
    }

    if (!texture)
    {
        LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite: null texture");
        return;
    }

    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    if (m_CurrentTexture != nullptr && m_CurrentTexture != texture)
    {
        FlushBatch();
    }
    m_CurrentTexture = texture;

    if (m_SpriteCount >= MAX_SPRITES_PER_BATCH)
    {
        FlushBatch();
        m_CurrentTexture = texture;
    }

    SpriteVertex* v = &m_VertexData[m_SpriteCount * 4];

    v[0].Position = {position.x, position.y};
    v[0].UV = {uvMin.x, uvMin.y};
    v[0].Color = color;

    v[1].Position = {position.x + size.x, position.y};
    v[1].UV = {uvMax.x, uvMin.y};
    v[1].Color = color;

    v[2].Position = {position.x, position.y + size.y};
    v[2].UV = {uvMin.x, uvMax.y};
    v[2].Color = color;

    v[3].Position = {position.x + size.x, position.y + size.y};
    v[3].UV = {uvMax.x, uvMax.y};
    v[3].Color = color;

    ++m_SpriteCount;
}

} // namespace LongXi
