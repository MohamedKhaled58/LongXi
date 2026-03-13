#include "Renderer/LXHeroRenderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Animation/AnimationPlayer.h"
#include "Core/Graphics/TextureFormat.h"
#include "Assets/C3/RuntimeMesh.h"
#include "Core/Logging/LogMacros.h"
#include "Renderer/Renderer.h"

namespace LXEngine
{

namespace
{

LXCore::Matrix4 MakeIdentity()
{
    LXCore::Matrix4 matrix = {};
    matrix.m[0]            = 1.0f;
    matrix.m[5]            = 1.0f;
    matrix.m[10]           = 1.0f;
    matrix.m[15]           = 1.0f;
    return matrix;
}

LXCore::Matrix4 Multiply(const LXCore::Matrix4& lhs, const LXCore::Matrix4& rhs)
{
    LXCore::Matrix4 result = {};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += lhs.m[row * 4 + k] * rhs.m[k * 4 + col];
            }
            result.m[row * 4 + col] = sum;
        }
    }
    return result;
}

LXCore::Matrix4 MakeScale(float x, float y, float z)
{
    LXCore::Matrix4 matrix = {};
    matrix.m[0]            = x;
    matrix.m[5]            = y;
    matrix.m[10]           = z;
    matrix.m[15]           = 1.0f;
    return matrix;
}

LXCore::Matrix4 MakeTranslate(float x, float y, float z)
{
    LXCore::Matrix4 matrix = MakeIdentity();
    matrix.m[12]           = x;
    matrix.m[13]           = y;
    matrix.m[14]           = z;
    return matrix;
}

LXCore::Matrix4 MakeRotateX(float radians)
{
    LXCore::Matrix4 matrix = MakeIdentity();
    const float     c      = std::cos(radians);
    const float     s      = std::sin(radians);
    matrix.m[5]            = c;
    matrix.m[6]            = s;
    matrix.m[9]            = -s;
    matrix.m[10]           = c;
    return matrix;
}

LXCore::Matrix4 MakeRotateZ(float radians)
{
    LXCore::Matrix4 matrix = MakeIdentity();
    const float     c      = std::cos(radians);
    const float     s      = std::sin(radians);
    matrix.m[0]            = c;
    matrix.m[1]            = s;
    matrix.m[4]            = -s;
    matrix.m[5]            = c;
    return matrix;
}

float DegreesToRadians(float degrees)
{
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

} // namespace

bool LXHeroRenderer::Initialize(Renderer& renderer)
{
    Shutdown();

    if (!renderer.IsInitialized())
    {
        return false;
    }

    if (!m_Pipeline.Initialize(renderer))
    {
        LX_ENGINE_ERROR("[HeroRenderer] Pipeline initialization failed");
        return false;
    }

    RendererBufferDesc perObjectDesc = {};
    perObjectDesc.Type               = RendererBufferType::Constant;
    perObjectDesc.ByteSize           = sizeof(CbPerObject);
    perObjectDesc.Stride             = 16;
    perObjectDesc.Usage              = RendererResourceUsage::Dynamic;
    perObjectDesc.CpuAccess          = RendererCpuAccessFlags::Write;
    perObjectDesc.BindFlags          = RendererBindFlags::ConstantBuffer;

    m_CbPerObject = renderer.CreateConstantBuffer(perObjectDesc);
    if (!m_CbPerObject.IsValid())
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to create per-object constant buffer");
        Shutdown();
        return false;
    }

    RendererBufferDesc bonesDesc = {};
    bonesDesc.Type               = RendererBufferType::Constant;
    bonesDesc.ByteSize           = sizeof(CbBones);
    bonesDesc.Stride             = 16;
    bonesDesc.Usage              = RendererResourceUsage::Dynamic;
    bonesDesc.CpuAccess          = RendererCpuAccessFlags::Write;
    bonesDesc.BindFlags          = RendererBindFlags::ConstantBuffer;

    m_CbBones = renderer.CreateConstantBuffer(bonesDesc);
    if (!m_CbBones.IsValid())
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to create bone constant buffer");
        Shutdown();
        return false;
    }

    const uint32_t whitePixel = 0xFFFFFFFFu;
    RendererTextureDesc fallbackDesc = {};
    fallbackDesc.Width               = 1;
    fallbackDesc.Height              = 1;
    fallbackDesc.Format              = LXCore::TextureFormat::RGBA8;
    fallbackDesc.Usage               = RendererResourceUsage::Static;
    fallbackDesc.BindFlags           = RendererBindFlags::ShaderResource;
    fallbackDesc.InitialData         = &whitePixel;
    fallbackDesc.InitialDataSize     = sizeof(whitePixel);
    fallbackDesc.InitialRowPitch     = sizeof(whitePixel);

    m_FallbackTexture = renderer.CreateTexture(fallbackDesc);
    if (!m_FallbackTexture.IsValid())
    {
        LX_ENGINE_WARN("[HeroRenderer] Failed to create fallback texture");
    }

    m_Renderer    = &renderer;
    m_Initialized = true;
    return true;
}

void LXHeroRenderer::Shutdown()
{
    if (m_Renderer && m_Renderer->IsInitialized())
    {
        if (m_CbPerObject.IsValid())
        {
            m_Renderer->DestroyBuffer(ToBufferHandle(m_CbPerObject));
        }
        if (m_CbBones.IsValid())
        {
            m_Renderer->DestroyBuffer(ToBufferHandle(m_CbBones));
        }
        if (m_FallbackTexture.IsValid())
        {
            m_Renderer->DestroyTexture(m_FallbackTexture);
        }
    }

    m_CbPerObject = {};
    m_CbBones     = {};
    m_FallbackTexture = {};
    m_Renderer    = nullptr;
    m_Pipeline.Shutdown();
    m_Initialized = false;
    m_LoggedMissingTexture = false;
}

bool LXHeroRenderer::IsInitialized() const
{
    return m_Initialized;
}

LXCore::Matrix4 LXHeroRenderer::ComputeWorldMatrix(const LXHero& hero)
{
    const uint8_t dir = static_cast<uint8_t>(hero.direction % 8u);
    if (hero.scale <= 0.0f)
    {
        LX_ENGINE_WARN("[HeroRenderer] Invalid scale {} (expected > 0)", hero.scale);
    }

    const float rotZ = DegreesToRadians((-45.0f * dir) - 45.0f);
    const float rotX = DegreesToRadians(-37.0f);

    LXCore::Matrix4 world = MakeIdentity();
    world                 = Multiply(world, MakeScale(hero.scale, hero.scale, hero.scale));
    world                 = Multiply(world, MakeTranslate(0.0f, 0.0f, hero.height));
    world                 = Multiply(world, MakeRotateZ(rotZ));
    world                 = Multiply(world, MakeRotateX(rotX));
    world                 = Multiply(world, MakeTranslate(hero.worldX, hero.worldY, 0.0f));
    return world;
}

void LXHeroRenderer::UploadBoneMatrices(const LXHero& hero)
{
    if (!m_Renderer || !m_CbBones.IsValid())
    {
        return;
    }

    RendererMapRequest     mapRequest = {};
    RendererMappedResource mapped     = {};
    mapRequest.Handle                 = ToBufferHandle(m_CbBones);
    mapRequest.Mode                   = RendererMapMode::WriteDiscard;

    if (!m_Renderer->MapBuffer(mapRequest, mapped) || !mapped.Data)
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to map bone constant buffer");
        return;
    }

    CbBones* bones = static_cast<CbBones*>(mapped.Data);
    for (int i = 0; i < 64; ++i)
    {
        bones->BoneMatrices[i] = MakeIdentity();
    }

    if (hero.animationPlayer)
    {
        const auto&  matrices  = hero.animationPlayer->GetFinalBoneMatrices();
        const size_t copyCount = std::min<size_t>(matrices.size(), 64);
        for (size_t i = 0; i < copyCount; ++i)
        {
            bones->BoneMatrices[i] = matrices[i];
        }

        if (matrices.size() > 64)
        {
            LX_ENGINE_WARN("[HeroRenderer] Bone count {} exceeds 64; truncating", matrices.size());
        }
    }

    m_Renderer->UnmapBuffer(ToBufferHandle(m_CbBones));
}

void LXHeroRenderer::RenderMesh(RuntimeMesh& mesh, bool transparentPass)
{
    if (!m_Renderer)
    {
        return;
    }

    const uint32_t stride = sizeof(Vertex);
    if (!m_Renderer->BindVertexBuffer(mesh.GetVertexBuffer(), stride, 0))
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to bind vertex buffer");
        return;
    }

    if (!m_Renderer->BindIndexBuffer(mesh.GetIndexBuffer(), mesh.GetIndexFormat(), 0))
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to bind index buffer");
        return;
    }

    RendererTextureHandle textureHandle = mesh.GetActiveTexture();
    bool usingFallback                  = false;

    if (!textureHandle.IsValid())
    {
        textureHandle  = m_FallbackTexture;
        usingFallback = true;
    }

    if (!m_Renderer->BindTexture(textureHandle, RendererShaderStage::Pixel, 0))
    {
        if (!usingFallback && m_FallbackTexture.IsValid())
        {
            if (m_Renderer->BindTexture(m_FallbackTexture, RendererShaderStage::Pixel, 0))
            {
                usingFallback = true;
            }
            else
            {
                if (!m_LoggedMissingTexture)
                {
                    LX_ENGINE_WARN("[HeroRenderer] Failed to bind mesh texture or fallback texture");
                    m_LoggedMissingTexture = true;
                }
                return;
            }
        }
        else
        {
            if (!m_LoggedMissingTexture)
            {
                LX_ENGINE_WARN("[HeroRenderer] Failed to bind mesh texture or fallback texture");
                m_LoggedMissingTexture = true;
            }
            return;
        }
    }

    if (usingFallback && !m_LoggedMissingTexture)
    {
        const std::string& slotName = mesh.GetSlotName();
        if (!slotName.empty())
        {
            LX_ENGINE_WARN("[HeroRenderer] Missing texture for mesh '{}'; using fallback", slotName);
        }
        else
        {
            LX_ENGINE_WARN("[HeroRenderer] Missing mesh texture; using fallback");
        }
        m_LoggedMissingTexture = true;
    }

    const auto& subMeshes = mesh.GetSubMeshes();
    for (const auto& sub : subMeshes)
    {
        if (sub.isTransparent != transparentPass || sub.indexCount == 0)
        {
            continue;
        }
        m_Renderer->DrawIndexed(sub.indexCount, sub.startIndex, 0);
    }
}

void LXHeroRenderer::RenderParts(const LXHero& hero, bool transparentPass)
{
    if (hero.bodyMesh)
    {
        RenderMesh(*hero.bodyMesh, transparentPass);
    }

    if (hero.headMesh)
    {
        RenderMesh(*hero.headMesh, transparentPass);
    }

    for (uint32_t i = 0; i < LXHeroSlot::Count; ++i)
    {
        if (hero.slots[i].mesh)
        {
            RenderMesh(*hero.slots[i].mesh, transparentPass);
        }
    }
}

void LXHeroRenderer::Render(const LXHero& hero, const LXCore::Matrix4& view, const LXCore::Matrix4& projection)
{
    if (!IsInitialized() || !m_Renderer)
    {
        return;
    }

    const LXCore::Matrix4 world = ComputeWorldMatrix(hero);

    RendererMapRequest     mapRequest = {};
    RendererMappedResource mapped     = {};
    mapRequest.Handle                 = ToBufferHandle(m_CbPerObject);
    mapRequest.Mode                   = RendererMapMode::WriteDiscard;

    if (!m_Renderer->MapBuffer(mapRequest, mapped) || !mapped.Data)
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to map per-object constant buffer");
        return;
    }

    CbPerObject* perObject = static_cast<CbPerObject*>(mapped.Data);
    perObject->World       = world;
    perObject->View        = view;
    perObject->Projection  = projection;

    m_Renderer->UnmapBuffer(ToBufferHandle(m_CbPerObject));

    UploadBoneMatrices(hero);

    if (!m_Renderer->BindConstantBuffer(m_CbBones, RendererShaderStage::Vertex, 0))
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to bind bone constant buffer");
        return;
    }

    if (!m_Renderer->BindConstantBuffer(m_CbPerObject, RendererShaderStage::Vertex, 1))
    {
        LX_ENGINE_ERROR("[HeroRenderer] Failed to bind per-object constant buffer");
        return;
    }

    ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(m_Renderer->GetNativeContextHandle());
    m_Pipeline.BindOpaque(context);
    RenderParts(hero, false);

    m_Pipeline.BindTransparent(context);
    RenderParts(hero, true);
}

} // namespace LXEngine
