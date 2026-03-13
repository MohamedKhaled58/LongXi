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
    bool Initialize(Renderer& renderer);
    void Shutdown();
    bool IsInitialized() const;

    void Render(const LXHero& hero, const LXCore::Matrix4& view, const LXCore::Matrix4& projection);

private:
    struct CbPerObject
    {
        LXCore::Matrix4 World;
        LXCore::Matrix4 View;
        LXCore::Matrix4 Projection;
    };

    struct CbBones
    {
        LXCore::Matrix4 BoneMatrices[64];
    };

    static LXCore::Matrix4 ComputeWorldMatrix(const LXHero& hero);
    void                   UploadBoneMatrices(const LXHero& hero);
    void                   RenderParts(const LXHero& hero, bool transparentPass);
    void                   RenderMesh(RuntimeMesh& mesh, bool transparentPass);

private:
    DX11HeroPipeline             m_Pipeline;
    RendererConstantBufferHandle m_CbPerObject{};
    RendererConstantBufferHandle m_CbBones{};
    RendererTextureHandle        m_FallbackTexture{};
    Renderer*                    m_Renderer    = nullptr;
    bool                         m_Initialized = false;
    bool                         m_LoggedMissingTexture = false;
};

} // namespace LXEngine
