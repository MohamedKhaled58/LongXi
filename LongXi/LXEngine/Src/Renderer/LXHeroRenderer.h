#pragma once

#include "Core/Math/Math.h"
#include "Hero/LXHero.h"
#include "Renderer/Backend/DX11/DX11HeroPipeline.h"
#include "Renderer/RendererTypes.h"

namespace LXEngine
{

class Renderer;
class RuntimeMesh;

class LXHeroRenderer
{
public:
    static constexpr uint32_t kMaxBones = 256;

    bool Initialize(Renderer& renderer);
    void Shutdown();
    bool IsInitialized() const;

    struct HeroRenderLighting
    {
        LXCore::Vector3 LightDirection{0.0f, -0.6f, -0.8f};
        float           Padding0 = 0.0f;
        LXCore::Color   Ambient{1.0f, 1.0f, 1.0f, 1.0f};
        LXCore::Color   Diffuse{0.0f, 0.0f, 0.0f, 1.0f};
        LXCore::Color   Tint{1.0f, 1.0f, 1.0f, 1.0f};

        static HeroRenderLighting Flat();
        static HeroRenderLighting Portrait();
    };

    void Render(const LXHero& hero, const LXCore::Matrix4& view, const LXCore::Matrix4& projection);
    void Render(const LXHero& hero, const LXCore::Matrix4& view, const LXCore::Matrix4& projection, const HeroRenderLighting& lighting);

private:
    struct CbPerObject
    {
        LXCore::Matrix4 World;
        LXCore::Matrix4 View;
        LXCore::Matrix4 Projection;
    };

    struct CbBones
    {
        LXCore::Matrix4 BoneMatrices[kMaxBones];
    };

    struct CbLighting
    {
        LXCore::Vector3 LightDirection{0.0f, -0.6f, -0.8f};
        float           Padding0 = 0.0f;
        LXCore::Color   Ambient{1.0f, 1.0f, 1.0f, 1.0f};
        LXCore::Color   Diffuse{0.0f, 0.0f, 0.0f, 1.0f};
        LXCore::Color   Tint{1.0f, 1.0f, 1.0f, 1.0f};
    };

    static LXCore::Matrix4 ComputeWorldMatrix(const LXHero& hero);
    void                   UploadBoneMatrices(const LXHero& hero);
    void                   UploadLighting(const HeroRenderLighting& lighting);
    void                   RenderParts(const LXHero& hero, bool transparentPass);
    void                   RenderMesh(RuntimeMesh& mesh, bool transparentPass);

private:
    DX11HeroPipeline             m_Pipeline;
    RendererConstantBufferHandle m_CbPerObject{};
    RendererConstantBufferHandle m_CbBones{};
    RendererConstantBufferHandle m_CbLighting{};
    RendererTextureHandle        m_FallbackTexture{};
    Renderer*                    m_Renderer             = nullptr;
    bool                         m_Initialized          = false;
    bool                         m_LoggedMissingTexture = false;
};

} // namespace LXEngine
