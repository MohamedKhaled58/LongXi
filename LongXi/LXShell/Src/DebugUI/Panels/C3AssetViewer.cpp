#define NOMINMAX
#include "C3AssetViewer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <numbers>
#include <string>
#include <vector>

#include "Animation/AnimationClip.h"
#include "Animation/AnimationPlayer.h"
#include "Assets/C3/C3MeshProcessor.h"
#include "Assets/C3/C3RuntimeLoader.h"
#include "Assets/C3/C3RuntimeTypes.h"
#include "Assets/C3/C3Types.h"
#include "Assets/C3/RuntimeMesh.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Engine.h"
#include "Hero/LXHero.h"
#include "Renderer/LXHeroRenderer.h"
#include "Renderer/Renderer.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LXShell
{

using LXCore::Matrix4;
using LXCore::Vector3;
using LXEngine::AnimationClip;
using LXEngine::AnimationClipResource;
using LXEngine::AnimationPlayer;
using LXEngine::C3LoadRequest;
using LXEngine::C3LoadResult;
using LXEngine::C3MeshProcessor;
using LXEngine::C3RuntimeLoader;
using LXEngine::Engine;
using LXEngine::FourCCToString;
using LXEngine::LXHero;
using LXEngine::LXHeroRenderer;
using LXEngine::MeshBounds;
using LXEngine::MeshStats;
using LXEngine::Renderer;
using LXEngine::RendererColor;
using LXEngine::RendererTextureHandle;
using LXEngine::RuntimeMesh;
using LXEngine::SkeletonResource;
using LXEngine::Texture;
using LXEngine::TextureManager;

namespace
{

struct PreviewTarget
{
    uint32_t              width  = 0;
    uint32_t              height = 0;
    RendererTextureHandle color{};
    RendererTextureHandle depth{};
    void*                 imguiTextureId = nullptr;
};

struct PreviewState
{
    bool  enabled      = true;
    bool  animate      = true;
    int   direction    = 0;
    float zoom         = 1.0f;
    float scale        = 1.0f;
    float portraitZoom = 1.15f;

    PreviewTarget mainTarget;
    PreviewTarget portraitTarget;

    RendererTextureHandle        fallbackTexture{};
    std::unique_ptr<RuntimeMesh> mesh;
    std::shared_ptr<Texture>     texture;
    std::shared_ptr<Texture>     portraitFrameTexture;
    void*                        portraitFrameTextureId = nullptr;
    char                         portraitFramePath[256] = "data/main/rolefacebk.dds";
    bool                         portraitFrameAttempted = false;

    SkeletonResource skeleton;
    AnimationClip    clip;
    AnimationPlayer  player;
    bool             hasSkeleton = false;
    bool             hasClip     = false;

    LXHeroRenderer renderer;
    LXHero         hero;

    Vector3 boundsCenter{0.0f, 0.0f, 0.0f};
    float   boundsRadius = 1.0f;

    int selectedMeshIndex      = -1;
    int selectedSkeletonIndex  = -1;
    int selectedAnimationIndex = -1;
    int selectedAnimationSlot  = 0;

    void ReleaseResources(Renderer& renderer);
};

struct ViewerState
{
    char         inputBuffer[256] = {0};
    bool         hasResult        = false;
    std::string  lastError;
    C3LoadResult result;
    PreviewState preview;
};

static ViewerState g_ViewerState;

bool IsNumeric(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }
    return std::all_of(value.begin(),
                       value.end(),
                       [](unsigned char c)
                       {
                           return std::isdigit(c) != 0;
                       });
}

float DegreesToRadians(float degrees)
{
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

float Dot(const Vector3& lhs, const Vector3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vector3 Normalize(const Vector3& value)
{
    const float lengthSq = Dot(value, value);
    if (lengthSq <= 0.000001f)
    {
        return {0.0f, 0.0f, 0.0f};
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

Matrix4 MakeLookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
{
    Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    Vector3 right   = Normalize(Cross(up, forward));
    if (Dot(right, right) <= 0.000001f)
    {
        right = {1.0f, 0.0f, 0.0f};
    }
    Vector3 upVector = Normalize(Cross(forward, right));

    Matrix4 view = {};
    view.m[0]    = right.x;
    view.m[1]    = upVector.x;
    view.m[2]    = forward.x;
    view.m[3]    = 0.0f;

    view.m[4] = right.y;
    view.m[5] = upVector.y;
    view.m[6] = forward.y;
    view.m[7] = 0.0f;

    view.m[8]  = right.z;
    view.m[9]  = upVector.z;
    view.m[10] = forward.z;
    view.m[11] = 0.0f;

    view.m[12] = -Dot(eye, right);
    view.m[13] = -Dot(eye, upVector);
    view.m[14] = -Dot(eye, forward);
    view.m[15] = 1.0f;
    return view;
}

Matrix4 MakePerspective(float fovDegrees, float aspect, float nearPlane, float farPlane)
{
    Matrix4 proj = {};
    if (aspect <= 0.0f)
    {
        aspect = 1.0f;
    }

    const float fovRadians = DegreesToRadians(fovDegrees);
    const float yScale     = 1.0f / std::tan(fovRadians * 0.5f);
    const float xScale     = yScale / aspect;
    const float depthRange = farPlane - nearPlane;

    proj.m[0]  = xScale;
    proj.m[5]  = yScale;
    proj.m[10] = farPlane / depthRange;
    proj.m[11] = 1.0f;
    proj.m[14] = (-nearPlane * farPlane) / depthRange;
    return proj;
}

void EnsureFallbackTexture(Renderer& renderer, PreviewState& preview)
{
    if (preview.fallbackTexture.IsValid())
    {
        return;
    }

    constexpr uint32_t kWhitePixel = 0xFFFFFFFF;

    LXEngine::RendererTextureDesc desc = {};
    desc.Width                         = 1;
    desc.Height                        = 1;
    desc.Format                        = LXCore::TextureFormat::RGBA8;
    desc.Usage                         = LXEngine::RendererResourceUsage::Static;
    desc.CpuAccess                     = LXEngine::RendererCpuAccessFlags::None;
    desc.BindFlags                     = LXEngine::RendererBindFlags::ShaderResource;
    desc.InitialData                   = &kWhitePixel;
    desc.InitialDataSize               = sizeof(kWhitePixel);

    preview.fallbackTexture = renderer.CreateTexture(desc);
}

void ReleasePreviewMesh(Renderer& renderer, PreviewState& preview)
{
    if (preview.mesh)
    {
        preview.mesh->Release(renderer);
        preview.mesh.reset();
    }

    preview.texture.reset();
    preview.hasSkeleton = false;
    preview.hasClip     = false;
    preview.player.Reset();
    preview.hero                   = {};
    preview.selectedSkeletonIndex  = -1;
    preview.selectedAnimationIndex = -1;
}

void ReleasePreviewTarget(Renderer& renderer, PreviewTarget& target)
{
    if (target.color.IsValid())
    {
        renderer.DestroyTexture(target.color);
        target.color = {};
    }

    if (target.depth.IsValid())
    {
        renderer.DestroyTexture(target.depth);
        target.depth = {};
    }

    target.imguiTextureId = nullptr;
    target.width          = 0;
    target.height         = 0;
}

void PreviewState::ReleaseResources(Renderer& renderer)
{
    ReleasePreviewMesh(renderer, *this);
    ReleasePreviewTarget(renderer, mainTarget);
    ReleasePreviewTarget(renderer, portraitTarget);

    if (fallbackTexture.IsValid())
    {
        renderer.DestroyTexture(fallbackTexture);
        fallbackTexture = {};
    }

    portraitFrameTexture.reset();
    portraitFrameTextureId = nullptr;
    portraitFrameAttempted = false;
}

void EnsurePreviewTarget(Renderer& renderer, PreviewTarget& target, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (target.color.IsValid() && target.depth.IsValid() && target.width == width && target.height == height)
    {
        return;
    }

    ReleasePreviewTarget(renderer, target);

    LXEngine::RendererTextureDesc colorDesc = {};
    colorDesc.Width                         = width;
    colorDesc.Height                        = height;
    colorDesc.Format                        = LXCore::TextureFormat::RGBA8;
    colorDesc.Usage                         = LXEngine::RendererResourceUsage::Static;
    colorDesc.CpuAccess                     = LXEngine::RendererCpuAccessFlags::None;
    colorDesc.BindFlags                     = LXEngine::RendererBindFlags::ShaderResource | LXEngine::RendererBindFlags::RenderTarget;

    target.color = renderer.CreateTexture(colorDesc);

    LXEngine::RendererTextureDesc depthDesc = {};
    depthDesc.Width                         = width;
    depthDesc.Height                        = height;
    depthDesc.Format                        = LXCore::TextureFormat::Depth24Stencil8;
    depthDesc.Usage                         = LXEngine::RendererResourceUsage::Static;
    depthDesc.CpuAccess                     = LXEngine::RendererCpuAccessFlags::None;
    depthDesc.BindFlags                     = LXEngine::RendererBindFlags::DepthStencil;

    target.depth = renderer.CreateTexture(depthDesc);

    if (!target.color.IsValid() || !target.depth.IsValid())
    {
        ReleasePreviewTarget(renderer, target);
        return;
    }

    target.imguiTextureId = renderer.GetNativeTextureHandle(target.color);
    if (!target.imguiTextureId)
    {
        ReleasePreviewTarget(renderer, target);
        return;
    }

    target.width  = width;
    target.height = height;
}

void EnsurePortraitFrameTexture(Engine& engine, PreviewState& preview)
{
    if (preview.portraitFrameTexture)
    {
        if (!preview.portraitFrameTextureId)
        {
            preview.portraitFrameTextureId = engine.GetRenderer().GetNativeTextureHandle(preview.portraitFrameTexture->GetHandle());
        }
        return;
    }

    if (preview.portraitFrameAttempted)
    {
        return;
    }

    preview.portraitFrameAttempted = true;

    TextureManager& textureManager = engine.GetTextureManager();
    auto            tryLoad        = [&engine, &textureManager, &preview](const char* path) -> bool
    {
        if (!path || path[0] == '\0')
        {
            return false;
        }
        if (!engine.GetVFS().Exists(path))
        {
            return false;
        }

        std::shared_ptr<Texture> texture = textureManager.LoadTexture(path);
        if (!texture)
        {
            return false;
        }

        preview.portraitFrameTexture   = texture;
        preview.portraitFrameTextureId = engine.GetRenderer().GetNativeTextureHandle(texture->GetHandle());
        return true;
    };

    if (tryLoad(preview.portraitFramePath))
    {
        return;
    }

    const char* defaultPath   = "data/main/rolefacebk.dds";
    const bool  allowFallback = (preview.portraitFramePath[0] == '\0') || (std::strcmp(preview.portraitFramePath, defaultPath) == 0);

    if (!allowFallback)
    {
        return;
    }

    const char* candidates[] = {
        "data/main/rolefacebk.dds",
        "data/main/rolefacebk.tga",
        "data/main1/rolefacebk.dds",
        "data/main1/rolefacebk.tga",
    };

    for (const char* path : candidates)
    {
        if (tryLoad(path))
        {
            break;
        }
    }
}

bool BuildPreviewAssets(Engine& engine, PreviewState& preview, const C3LoadResult& result)
{
    Renderer& renderer = engine.GetRenderer();

    if (!preview.renderer.IsInitialized())
    {
        if (!preview.renderer.Initialize(renderer))
        {
            LX_WARN("[C3Viewer] Failed to initialize preview renderer");
            return false;
        }
    }

    ReleasePreviewMesh(renderer, preview);

    if (result.meshes.empty())
    {
        return false;
    }

    if (preview.selectedMeshIndex < 0 || preview.selectedMeshIndex >= static_cast<int>(result.meshes.size()))
    {
        preview.selectedMeshIndex = C3MeshProcessor::SelectBestMeshIndex(result.meshes);
    }

    if (preview.selectedMeshIndex < 0)
    {
        return false;
    }

    const auto& meshResource = result.meshes[preview.selectedMeshIndex];

    preview.mesh = std::make_unique<RuntimeMesh>();
    if (!preview.mesh->Initialize(renderer, meshResource))
    {
        preview.mesh.reset();
        return false;
    }

    EnsureFallbackTexture(renderer, preview);

    RendererTextureHandle textureHandle = preview.fallbackTexture;
    if (!meshResource.textureName.empty())
    {
        TextureManager& textureManager = engine.GetTextureManager();
        preview.texture                = textureManager.LoadTexture(meshResource.textureName);
        if (preview.texture)
        {
            textureHandle = preview.texture->GetHandle();
        }
        else
        {
            LX_WARN("[C3Viewer] Failed to load preview texture '{}'", meshResource.textureName);
        }
    }

    if (textureHandle.IsValid())
    {
        preview.mesh->SetDefaultTexture(textureHandle);
    }

    const MeshStats stats        = C3MeshProcessor::ComputeMeshStats(meshResource);
    const uint32_t  requiredBone = (stats.weightedInfluences > 0) ? (stats.maxBoneIndex + 1u) : 0u;

    preview.hasSkeleton = false;
    preview.hasClip     = false;
    preview.player.Reset();

    int      preferredAnimationIndex = -1;
    uint32_t preferredAnimationBones = 0;
    if (!result.animations.empty())
    {
        const int slotIndex = std::clamp(preview.selectedAnimationSlot, 0, 2);
        if (slotIndex < static_cast<int>(result.animations.size()))
        {
            preferredAnimationIndex = slotIndex;
            preferredAnimationBones = result.animations[slotIndex].boneCount;
        }
    }

    if (!result.skeletons.empty())
    {
        preview.selectedSkeletonIndex = C3MeshProcessor::SelectSkeletonIndex(result.skeletons, requiredBone, preferredAnimationBones);
        if (preview.selectedSkeletonIndex >= 0)
        {
            preview.skeleton    = result.skeletons[preview.selectedSkeletonIndex];
            preview.hasSkeleton = true;
        }
    }

    if (preview.hasSkeleton && !result.animations.empty())
    {
        int preferredIndex = -1;
        if (preferredAnimationIndex >= 0)
        {
            const auto& candidate = result.animations[preferredAnimationIndex];
            if (candidate.boneCount == preview.skeleton.boneCount)
            {
                preferredIndex = preferredAnimationIndex;
            }
        }

        if (preferredIndex < 0)
        {
            preferredIndex = C3MeshProcessor::SelectAnimationIndex(result.animations, preview.skeleton.boneCount);
        }

        preview.selectedAnimationIndex = preferredIndex;
        if (preview.selectedAnimationIndex >= 0)
        {
            preview.hasClip = preview.clip.Initialize(result.animations[preview.selectedAnimationIndex]);
            if (!preview.hasClip)
            {
                preview.selectedAnimationIndex = -1;
            }
        }
    }

    if (preview.hasSkeleton)
    {
        preview.player.SetSkeleton(&preview.skeleton);
        if (preview.hasClip)
        {
            preview.player.SetClip(&preview.clip);
            preview.player.SetLooping(true);
            preview.player.Reset();
            preview.player.Sample();
        }
    }

    MeshBounds bounds = C3MeshProcessor::ComputeBounds(meshResource);
    if (bounds.valid)
    {
        preview.boundsCenter = {
            (bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f, (bounds.min.z + bounds.max.z) * 0.5f};
        const Vector3 extent = {
            (bounds.max.x - bounds.min.x) * 0.5f, (bounds.max.y - bounds.min.y) * 0.5f, (bounds.max.z - bounds.min.z) * 0.5f};
        preview.boundsRadius = std::max({extent.x, extent.y, extent.z, 0.1f});
    }
    else
    {
        preview.boundsCenter = {0.0f, 0.0f, 0.0f};
        preview.boundsRadius = 1.0f;
    }

    preview.hero                 = LXHero{};
    preview.hero.bodyMesh        = preview.mesh.get();
    preview.hero.direction       = static_cast<uint8_t>(std::clamp(preview.direction, 0, 7));
    preview.hero.scale           = preview.scale;
    preview.hero.worldX          = 0.0f;
    preview.hero.worldY          = 0.0f;
    preview.hero.height          = 0.0f;
    preview.hero.animationPlayer = preview.hasClip ? &preview.player : nullptr;

    return true;
}

bool RenderPreview(Engine&                                   engine,
                   PreviewState&                             preview,
                   PreviewTarget&                            target,
                   uint32_t                                  width,
                   uint32_t                                  height,
                   bool                                      portraitMode,
                   const LXHeroRenderer::HeroRenderLighting& lighting,
                   const RendererColor&                      clearColor,
                   bool                                      advanceAnimation)
{
    if (!preview.enabled || !preview.mesh || !target.color.IsValid() || !target.depth.IsValid())
    {
        return false;
    }

    if (width == 0 || height == 0)
    {
        return false;
    }

    if (advanceAnimation && preview.animate && preview.hasClip)
    {
        preview.player.Advance(static_cast<float>(engine.GetTimingSnapshot().DeltaTimeSeconds));
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float radius = std::max(preview.boundsRadius * preview.scale, 0.1f);
    const float fov    = portraitMode ? 28.0f : 40.0f;

    float distance   = radius / std::sin(DegreesToRadians(fov) * 0.5f);
    distance         = std::max(distance * (portraitMode ? 1.2f : 1.35f), radius * (portraitMode ? 1.6f : 2.0f));
    const float zoom = portraitMode ? std::max(preview.portraitZoom, 0.25f) : std::max(preview.zoom, 0.25f);
    distance         = distance / zoom;

    Vector3     center       = preview.boundsCenter;
    const float eyeOffset    = portraitMode ? (radius * 0.55f) : (radius * 0.25f);
    const float targetOffset = portraitMode ? (radius * 0.45f) : (radius * 0.15f);

    Vector3 eye   = {center.x, center.y + eyeOffset, center.z - distance};
    Vector3 focus = {center.x, center.y + targetOffset, center.z};
    Vector3 up    = {0.0f, 1.0f, 0.0f};

    Matrix4 view       = MakeLookAt(eye, focus, up);
    float   nearPlane  = std::max(0.01f, distance * 0.1f);
    float   farPlane   = std::max(nearPlane + 1.0f, distance * 4.0f);
    Matrix4 projection = MakePerspective(fov, aspect, nearPlane, farPlane);

    preview.hero.direction = static_cast<uint8_t>(std::clamp(preview.direction, 0, 7));
    preview.hero.scale     = preview.scale;

    engine.ExecuteExternalRenderPass(
        [&engine, &preview, &target, width, height, view, projection, lighting, clearColor]()
        {
            Renderer& renderer   = engine.GetRenderer();
            const int fullWidth  = renderer.GetViewportWidth();
            const int fullHeight = renderer.GetViewportHeight();

            if (!renderer.SetRenderTarget(target.color, target.depth))
            {
                return;
            }

            renderer.SetViewport({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
            renderer.ClearRenderTarget(target.color, clearColor);
            renderer.ClearDepthStencil(target.depth, 1.0f, 0);

            preview.renderer.Render(preview.hero, view, projection, lighting);

            renderer.SetRenderTarget();
            renderer.SetViewport({0.0f, 0.0f, static_cast<float>(fullWidth), static_cast<float>(fullHeight), 0.0f, 1.0f});
        });

    return true;
}

} // namespace

void C3AssetViewer::Render(Engine& engine)
{
    ViewerState& state = g_ViewerState;

    if (!ImGui::Begin("C3 Asset Viewer"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Load C3 asset by virtual path or numeric file ID.");
    ImGui::InputText("Path / File ID", state.inputBuffer, sizeof(state.inputBuffer));
    ImGui::SameLine();

    bool rebuildPreview = false;

    if (ImGui::Button("Load"))
    {
        std::string input = state.inputBuffer;
        state.lastError.clear();
        state.result    = C3LoadResult{};
        state.hasResult = false;

        if (input.empty())
        {
            state.lastError = "Provide a path or numeric file ID.";
        }
        else
        {
            C3LoadRequest request;
            if (IsNumeric(input))
            {
                request.fileId = static_cast<uint32_t>(std::stoul(input));
            }
            else
            {
                request.virtualPath = input;
            }

            C3RuntimeLoader loader;
            if (loader.LoadFromVfs(engine.GetVFS(), request, state.result))
            {
                state.hasResult                 = true;
                state.preview.selectedMeshIndex = C3MeshProcessor::SelectBestMeshIndex(state.result.meshes);
                rebuildPreview                  = true;
            }
            else
            {
                state.lastError = state.result.error.empty() ? "Load failed" : state.result.error;
            }
        }
    }

    if (!state.lastError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", state.lastError.c_str());
    }

    if (!state.hasResult)
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Columns(2, "C3ViewerColumns");
    ImGui::SetColumnWidth(0, 360.0f);

    ImGui::Text("Chunks");
    ImGui::BulletText("Meshes: %zu", state.result.meshes.size());
    ImGui::BulletText("Skeletons: %zu", state.result.skeletons.size());
    ImGui::BulletText("Animations: %zu", state.result.animations.size());
    ImGui::BulletText("Particles: %zu", state.result.particles.size());
    ImGui::BulletText("Cameras: %zu", state.result.cameras.size());
    ImGui::BulletText("Unknown: %zu", state.result.unknownChunks.size());

    if (!state.result.unknownChunks.empty() && ImGui::TreeNode("Unknown Chunks"))
    {
        for (const auto& chunk : state.result.unknownChunks)
        {
            const std::string type = FourCCToString(chunk.info.type);
            ImGui::BulletText("%s (%u bytes)", type.c_str(), chunk.info.size);
        }
        ImGui::TreePop();
    }

    if (!state.result.meshes.empty())
    {
        ImGui::Separator();
        ImGui::Text("Mesh Selection");

        const int meshCount = static_cast<int>(state.result.meshes.size());
        if (state.preview.selectedMeshIndex < 0 || state.preview.selectedMeshIndex >= meshCount)
        {
            state.preview.selectedMeshIndex = C3MeshProcessor::SelectBestMeshIndex(state.result.meshes);
            rebuildPreview                  = true;
        }

        std::string currentLabel;
        if (state.preview.selectedMeshIndex >= 0)
        {
            const auto& mesh = state.result.meshes[static_cast<size_t>(state.preview.selectedMeshIndex)];
            currentLabel     = mesh.name.empty() ? ("Mesh " + std::to_string(state.preview.selectedMeshIndex)) : mesh.name;
        }
        else
        {
            currentLabel = "None";
        }

        if (ImGui::BeginCombo("Preview Mesh", currentLabel.c_str()))
        {
            for (int i = 0; i < meshCount; ++i)
            {
                const auto& mesh     = state.result.meshes[static_cast<size_t>(i)];
                std::string label    = mesh.name.empty() ? ("Mesh " + std::to_string(i)) : mesh.name;
                const bool  selected = (i == state.preview.selectedMeshIndex);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    state.preview.selectedMeshIndex = i;
                    rebuildPreview                  = true;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    if (!state.result.meshes.empty() && ImGui::TreeNode("Meshes"))
    {
        for (size_t i = 0; i < state.result.meshes.size(); ++i)
        {
            const auto& mesh = state.result.meshes[i];
            ImGui::BulletText("Mesh %zu: vertices=%zu indices=%zu morphTargets=%zu",
                              i,
                              mesh.vertices.size(),
                              mesh.indices.size(),
                              mesh.morphTargets.size());
        }
        ImGui::TreePop();
    }

    if (!state.result.skeletons.empty() && ImGui::TreeNode("Skeletons"))
    {
        for (size_t i = 0; i < state.result.skeletons.size(); ++i)
        {
            const auto& skeleton = state.result.skeletons[i];
            ImGui::BulletText("Skeleton %zu: bones=%u", i, skeleton.boneCount);
            if (ImGui::TreeNode(("Hierarchy##" + std::to_string(i)).c_str()))
            {
                for (size_t b = 0; b < skeleton.parentIndices.size(); ++b)
                {
                    ImGui::BulletText("Bone %zu -> Parent %d", b, skeleton.parentIndices[b]);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (!state.result.animations.empty() && ImGui::TreeNode("Animations"))
    {
        for (size_t i = 0; i < state.result.animations.size(); ++i)
        {
            const auto& clip = state.result.animations[i];
            ImGui::BulletText("Clip %zu: frames=%u bones=%u duration=%.2fs", i, clip.frameCount, clip.boneCount, clip.durationSeconds);
        }
        ImGui::TreePop();
    }

    if (!state.result.warnings.empty() && ImGui::TreeNode("Warnings"))
    {
        for (const auto& warning : state.result.warnings)
        {
            ImGui::BulletText("%s", warning.c_str());
        }
        ImGui::TreePop();
    }

    ImGui::NextColumn();

    ImGui::Text("Preview Controls");
    ImGui::Separator();

    ImGui::Checkbox("Animate", &state.preview.animate);

    const char* animationLabels[] = {"Idle", "Walk", "Attack"};
    ImGui::BeginDisabled(state.result.animations.empty());
    if (ImGui::Combo("Animation", &state.preview.selectedAnimationSlot, animationLabels, IM_ARRAYSIZE(animationLabels)))
    {
        rebuildPreview = true;
    }
    ImGui::EndDisabled();

    if (!state.result.animations.empty())
    {
        if (state.preview.selectedAnimationIndex >= 0)
        {
            ImGui::Text("Clip Index: %d / %zu", state.preview.selectedAnimationIndex, state.result.animations.size());
        }
        else
        {
            ImGui::Text("Clip Index: none");
        }
    }

    if (state.preview.hasSkeleton)
    {
        ImGui::Text("Skeleton: %d bones (index %d)", state.preview.skeleton.boneCount, state.preview.selectedSkeletonIndex);
    }
    else if (!state.result.skeletons.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "Skeleton: none matched");
    }

    if (state.preview.selectedAnimationIndex >= 0)
    {
        const auto& clip = state.result.animations[static_cast<size_t>(state.preview.selectedAnimationIndex)];
        ImGui::Text("Clip Bones: %u, Frames: %u", clip.boneCount, clip.frameCount);
    }
    else if (!state.result.animations.empty() && state.preview.hasSkeleton)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "No animation matches skeleton (%u bones)", state.preview.skeleton.boneCount);
    }

    ImGui::SliderInt("Direction", &state.preview.direction, 0, 7);
    ImGui::SliderFloat("Zoom", &state.preview.zoom, 0.5f, 2.5f, "%.2f");
    ImGui::SliderFloat("Portrait Zoom", &state.preview.portraitZoom, 0.5f, 2.5f, "%.2f");
    ImGui::SliderFloat("Scale", &state.preview.scale, 0.2f, 2.0f, "%.2f");

    if (rebuildPreview || (!state.preview.mesh && !state.result.meshes.empty()))
    {
        BuildPreviewAssets(engine, state.preview, state.result);
    }

    ImGui::Separator();
    ImGui::Text("Status Portrait");
    ImGui::Separator();

    bool framePathChanged = ImGui::InputText("Frame Path", state.preview.portraitFramePath, sizeof(state.preview.portraitFramePath));
    ImGui::SameLine();
    if (ImGui::Button("Reload Frame") || framePathChanged)
    {
        state.preview.portraitFrameTexture.reset();
        state.preview.portraitFrameTextureId = nullptr;
        state.preview.portraitFrameAttempted = false;
    }
    if (state.preview.portraitFrameAttempted && !state.preview.portraitFrameTexture)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Frame texture not found in VFS");
    }

    const RendererColor portraitClear = {0.10f, 0.09f, 0.11f, 1.0f};
    const RendererColor previewClear  = {0.05f, 0.05f, 0.08f, 0.0f};

    bool advancedAnimation = false;

    EnsurePortraitFrameTexture(engine, state.preview);
    float portraitAspect = 1.0f;
    if (state.preview.portraitFrameTexture)
    {
        const float texW = static_cast<float>(state.preview.portraitFrameTexture->GetWidth());
        const float texH = static_cast<float>(state.preview.portraitFrameTexture->GetHeight());
        if (texW > 0.0f && texH > 0.0f)
        {
            portraitAspect = texW / texH;
        }
    }

    ImVec2 portraitAvail  = ImGui::GetContentRegionAvail();
    float  portraitWidth  = std::min(portraitAvail.x, 220.0f);
    float  portraitHeight = (portraitAspect > 0.0f) ? (portraitWidth / portraitAspect) : portraitWidth;
    portraitHeight        = std::max(portraitHeight, 140.0f);
    ImVec2 portraitSize(portraitWidth, portraitHeight);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    ImGui::BeginChild("C3PortraitFrame", portraitSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 portraitImageSize = ImGui::GetContentRegionAvail();
    if (portraitImageSize.x > 4.0f && portraitImageSize.y > 4.0f)
    {
        EnsurePreviewTarget(engine.GetRenderer(),
                            state.preview.portraitTarget,
                            static_cast<uint32_t>(portraitImageSize.x),
                            static_cast<uint32_t>(portraitImageSize.y));
        if (RenderPreview(engine,
                          state.preview,
                          state.preview.portraitTarget,
                          static_cast<uint32_t>(portraitImageSize.x),
                          static_cast<uint32_t>(portraitImageSize.y),
                          true,
                          LXHeroRenderer::HeroRenderLighting::Portrait(),
                          portraitClear,
                          !advancedAnimation))
        {
            advancedAnimation = true;
        }
    }

    if (state.preview.portraitTarget.imguiTextureId)
    {
        ImVec2 imagePos = ImGui::GetCursorScreenPos();
        ImGui::Image(state.preview.portraitTarget.imguiTextureId, portraitImageSize);
        if (state.preview.portraitFrameTextureId)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2      frameMax = {imagePos.x + portraitImageSize.x, imagePos.y + portraitImageSize.y};
            drawList->AddImage(state.preview.portraitFrameTextureId, imagePos, frameMax);
        }
    }
    else
    {
        ImGui::Text("No portrait available");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Text("Live Preview");
    ImGui::Separator();

    ImVec2 avail      = ImGui::GetContentRegionAvail();
    float  previewHgt = std::min(avail.y, std::max(200.0f, avail.x * 1.2f));
    ImVec2 previewSize(avail.x, previewHgt);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    ImGui::BeginChild("C3PreviewFrame", previewSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 imageSize = ImGui::GetContentRegionAvail();
    if (imageSize.x > 4.0f && imageSize.y > 4.0f)
    {
        EnsurePreviewTarget(
            engine.GetRenderer(), state.preview.mainTarget, static_cast<uint32_t>(imageSize.x), static_cast<uint32_t>(imageSize.y));
        if (RenderPreview(engine,
                          state.preview,
                          state.preview.mainTarget,
                          static_cast<uint32_t>(imageSize.x),
                          static_cast<uint32_t>(imageSize.y),
                          false,
                          LXHeroRenderer::HeroRenderLighting::Flat(),
                          previewClear,
                          !advancedAnimation))
        {
            advancedAnimation = true;
        }
    }

    if (state.preview.mainTarget.imguiTextureId)
    {
        ImGui::Image(state.preview.mainTarget.imguiTextureId, imageSize);
    }
    else
    {
        ImGui::Text("No preview available");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Columns(1);
    ImGui::End();
}

void C3AssetViewer::ReleaseResources(Engine& engine)
{
    Renderer& renderer = engine.GetRenderer();
    g_ViewerState.preview.ReleaseResources(renderer);
    g_ViewerState = ViewerState{};
}

} // namespace LXShell
