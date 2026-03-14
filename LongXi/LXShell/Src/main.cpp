// =============================================================================
// LXShell — Client Application
// =============================================================================

#include <Animation/AnimationClip.h>
#include <Animation/AnimationPlayer.h>
#include <Application/Application.h>
#include <Application/EntryPoint.h>
#include <Assets/C3/C3RuntimeLoader.h>
#include <Assets/C3/RuntimeMesh.h>
#include <Assets/ResourceManager.h>
#include <Core/FileSystem/VirtualFileSystem.h>
#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>
#include <Hero/LXHero.h>
#include <Map/MapCamera.h>
#include <Map/MapSystem.h>
#include <Renderer/LXHeroRenderer.h>
#include <Renderer/RendererTypes.h>
#include <Renderer/SpriteRenderer.h>
#include <Scene/Camera.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>
#include <Texture/Texture.h>
#include <Texture/TextureManager.h>
#include <Window/Win32Window.h>

#if defined(LX_DEBUG) || defined(LX_DEV)
#include <imgui.h>

#include "DebugUI/DebugUI.h"
#include "ImGui/ImGuiLayer.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Input/InputSystem.h"

namespace LXShell
{

using LXCore::CVirtualFileSystem;
using LXEngine::AnimationClip;
using LXEngine::AnimationClipResource;
using LXEngine::AnimationPlayer;
using LXEngine::C3LoadRequest;
using LXEngine::C3LoadResult;
using LXEngine::C3ResourceType;
using LXEngine::C3RuntimeLoader;
using LXEngine::Camera;
using LXEngine::Engine;
using LXEngine::Key;
using LXEngine::LXHero;
using LXEngine::LXHeroRenderer;
using LXEngine::ResourceManager;
using LXEngine::RuntimeMesh;
using LXEngine::Scene;
using LXEngine::SceneNode;
using LXEngine::SkeletonResource;
using LXEngine::SpriteRenderer;
using LXEngine::Texture;
using LXEngine::TextureManager;
using LXMap::MapCamera;

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

LXCore::Matrix4 MakeScreenToNdc(float width, float height)
{
    if (width <= 0.0f)
    {
        width = 1.0f;
    }
    if (height <= 0.0f)
    {
        height = 1.0f;
    }

    LXCore::Matrix4 proj = {};
    proj.m[0]            = 2.0f / width;
    proj.m[5]            = -2.0f / height;
    proj.m[10]           = 1.0f;
    proj.m[12]           = -1.0f;
    proj.m[13]           = 1.0f;
    proj.m[15]           = 1.0f;
    return proj;
}

LXCore::Matrix4 MakeMapWorldToScreen(const MapCamera& camera, float viewportWidth, float viewportHeight)
{
    const float zoom    = camera.GetZoom();
    const float centerX = camera.GetViewCenterWorldX();
    const float centerY = camera.GetViewCenterWorldY();

    const float tx = -centerX * zoom + viewportWidth * 0.5f;
    const float ty = -centerY * zoom + viewportHeight * 0.5f;

    LXCore::Matrix4 matrix = {};
    matrix.m[0]            = zoom;
    matrix.m[5]            = zoom;
    matrix.m[9]            = -zoom;
    matrix.m[12]           = tx;
    matrix.m[13]           = ty;
    matrix.m[15]           = 1.0f;
    return matrix;
}

void BuildMapViewProjection(
    const MapCamera& camera, float viewportWidth, float viewportHeight, LXCore::Matrix4& outView, LXCore::Matrix4& outProj)
{
    outView = MakeMapWorldToScreen(camera, viewportWidth, viewportHeight);
    outProj = MakeScreenToNdc(viewportWidth, viewportHeight);
}

} // namespace

class TestApplication : public LXEngine::Application
{
public:
    bool Initialize() override
    {
        if (!Application::Initialize())
        {
            return false;
        }

        Engine& engine = GetEngine();
        if (engine.IsInitialized())
        {
            SetupValidationScene();
#if defined(LX_DEBUG) || defined(LX_DEV)
            if (!m_ImGuiLayer.Initialize(engine, GetWindowHandle()))
            {
                LX_WARN("[Application] ImGuiLayer initialization failed, running without developer UI");
            }
            else
            {
                WireImGuiCallbacks();
            }
#endif
        }

        return true;
    }

    int Run() override
    {
        LX_INFO("Entering main loop");

        Engine& engine = GetEngine();
        if (!engine.IsInitialized())
        {
            LX_ERROR("Cannot run: engine is not initialized");
            return 1;
        }

        MSG msg = {};
        while (true)
        {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    LX_INFO("Exiting main loop");
                    return static_cast<int>(msg.wParam);
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            engine.Update();
            engine.Render();
            RenderHero(engine);

#if defined(LX_DEBUG) || defined(LX_DEV)
            if (m_ImGuiLayer.IsInitialized())
            {
                RenderValidationSprite(engine);

                const bool isF6Down = engine.GetInput().IsKeyDown(LXEngine::Key::F6);
                if (isF6Down && !m_F6WasDown)
                {
                    m_DebugUI.ToggleProfilerPanel();
                    LX_INFO("[DebugUI] Profiler panel toggled (F6)");
                }
                m_F6WasDown = isF6Down;

                m_ImGuiLayer.BeginFrame();
                m_DebugUI.UpdateViewModels(engine);
                m_DebugUI.RenderPanels(engine);
                RenderHeroDebugPanel(engine);
                m_ImGuiLayer.EndFrame();
                engine.ExecuteExternalRenderPass(
                    [this]()
                    {
                        m_ImGuiLayer.RenderDrawData();
                    });
            }
#endif

            engine.Present();
        }
    }

    void Shutdown() override
    {
        Engine& engine = GetEngine();
        if (engine.IsInitialized())
        {
            // m_HeroMesh is owned by ResourceManager cache, no need to release
            m_HeroMesh = nullptr;
            m_HeroRenderer.Shutdown();
        }
#if defined(LX_DEBUG) || defined(LX_DEV)
        m_ImGuiLayer.Shutdown();
#endif
        Application::Shutdown();
    }

private:
    static std::string_view TrimView(std::string_view value)
    {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
        {
            ++start;
        }

        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return value.substr(start, end - start);
    }

    static bool TryParseUInt32(std::string_view value, uint32_t& outValue)
    {
        if (value.empty())
        {
            return false;
        }

        const auto result = std::from_chars(value.data(), value.data() + value.size(), outValue);
        return result.ec == std::errc() && result.ptr == value.data() + value.size();
    }

    static bool TryLookupIniPath(CVirtualFileSystem& vfs, const char* iniPath, uint32_t id, std::string& outPath)
    {
        outPath.clear();
        const std::vector<uint8_t> fileData = vfs.ReadAll(iniPath);
        if (fileData.empty())
        {
            LX_WARN("[HeroTest] Missing INI mapping file: {}", iniPath);
            return false;
        }

        std::string_view content(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        size_t           offset = 0;

        while (offset < content.size())
        {
            const size_t     lineEnd = content.find('\n', offset);
            std::string_view line = (lineEnd == std::string_view::npos) ? content.substr(offset) : content.substr(offset, lineEnd - offset);
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }

            line = TrimView(line);
            if (!line.empty() && line.front() != '[' && line.front() != ';' && line.front() != '#')
            {
                const size_t equalsPos = line.find('=');
                if (equalsPos != std::string_view::npos)
                {
                    const std::string_view key   = TrimView(line.substr(0, equalsPos));
                    const std::string_view value = TrimView(line.substr(equalsPos + 1));
                    uint32_t               keyId = 0;
                    if (TryParseUInt32(key, keyId) && keyId == id && !value.empty())
                    {
                        outPath.assign(value.begin(), value.end());
                        return true;
                    }
                }
            }

            if (lineEnd == std::string_view::npos)
            {
                break;
            }
            offset = lineEnd + 1;
        }

        return false;
    }

    void SetupValidationScene()
    {
        LX_INFO("==============================================");
        LX_INFO("VALIDATION SCENE SETUP");
        LX_INFO("==============================================");

        Engine&                     engine         = GetEngine();
        LXEngine::Scene&            scene          = engine.GetScene();
        LXEngine::TextureManager&   textureManager = engine.GetTextureManager();
        LXCore::CVirtualFileSystem& vfs            = engine.GetVFS();
        bool                        mapLoaded      = false;

        auto rootNode = std::make_unique<SceneNode>();
        rootNode->SetName("ValidationRoot");
        rootNode->SetPosition({0.0f, 0.0f, 0.0f});
        LX_INFO("[ValidationScene] Validation root created");

#if defined(LX_DEBUG) || defined(LX_DEV)
        std::shared_ptr<LXEngine::Texture> testTexture;
        const char*                        resolvedPath = nullptr;

        if (testTexture)
        {
            m_ValidationTexture = testTexture;
            auto spriteNode     = std::make_unique<SceneNode>();
            spriteNode->SetName("TestSpriteNode");
            spriteNode->SetPosition({0.0f, 0.0f, 0.0f});
            spriteNode->SetScale({1.0f, 1.0f, 1.0f});
            rootNode->AddChild(std::move(spriteNode));
            LX_INFO("[ValidationScene] Test sprite node attached (texture: {})", resolvedPath ? resolvedPath : "unknown");
        }
#endif

        scene.AddNode(std::move(rootNode));

        constexpr std::array<const char*, 2> mapCandidates = {
            //"map/map/newplain.dmap",
            "map/map/desert.dmap",
            "map/map/island.dmap",
        };

        for (const char* candidate : mapCandidates)
        {
            const bool isMapId = std::all_of(candidate,
                                             candidate + std::strlen(candidate),
                                             [](unsigned char ch)
                                             {
                                                 return std::isdigit(ch) != 0;
                                             });
            if (!isMapId && !vfs.Exists(candidate))
            {
                continue;
            }

            if (engine.LoadMap(candidate))
            {
                LX_INFO("[ValidationScene] Map attached to engine map system (source: {})", candidate);
                mapLoaded = true;
                break;
            }
        }

        if (!mapLoaded)
        {
            LX_WARN("[ValidationScene] No compatible .dmap map package found in mounted VFS paths");
        }

        Camera& camera = scene.GetActiveCamera();
        camera.SetPosition({0.0f, 0.0f, -5.0f});
        camera.SetRotation({0.0f, 0.0f, 0.0f});
        camera.SetFOV(60.0f);
        camera.SetNearFar(0.1f, 1000.0f);

        SetupHeroFromC3(engine, textureManager, vfs);

        LX_INFO("==============================================");
        LX_INFO("VALIDATION SCENE SETUP COMPLETE");
        LX_INFO("==============================================");
    }

    void SetupHeroFromC3(Engine& engine, TextureManager& textureManager, CVirtualFileSystem& vfs)
    {
        if (!m_HeroRenderer.Initialize(engine.GetRenderer()))
        {
            LX_WARN("[HeroTest] Failed to initialize hero renderer");
            return;
        }

        ResourceManager&   resourceManager = engine.GetResourceManager();
        constexpr uint32_t kHeroMeshId     = 4000000;
        constexpr uint32_t kHeroTextureId  = 4000000;

        // Load mesh using ResourceManager (cache owns the mesh, we store pointer)
        m_HeroMesh = resourceManager.GetMesh(kHeroMeshId);
        if (!m_HeroMesh)
        {
            LX_WARN("[HeroTest] Failed to load mesh id {} from ResourceManager", kHeroMeshId);
            return;
        }

        // Load texture using ResourceManager and apply to mesh
        LXEngine::RendererTextureHandle textureHandle = resourceManager.GetTexture(kHeroTextureId);
        if (textureHandle.IsValid())
        {
            // Apply texture override to the cached mesh
            m_HeroMesh->SetDefaultTexture(textureHandle);
        }
        else
        {
            LX_WARN("[HeroTest] Texture id {} not found or failed to load (using fallback)", kHeroTextureId);
        }

        m_Hero           = LXHero{};
        m_Hero.bodyMesh  = m_HeroMesh;
        m_Hero.direction = 0;
        m_Hero.scale     = 1.0f;
        m_Hero.worldX    = 1000.0f;
        m_Hero.worldY    = 1000.0f;
        m_Hero.height    = 10.0f;
        m_HeroReady      = true;

        m_HeroLookType = static_cast<uint32_t>(kHeroMeshId / 1000000ull);
        m_HeroAction   = 100;
        m_HeroMotionId = static_cast<uint64_t>(m_HeroLookType) * 1000000ull + static_cast<uint64_t>(m_HeroAction);
        LoadHeroMotionFromIni(engine, m_HeroMotionId, vfs);

        LX_INFO("[HeroTest] Hero setup complete (meshId={}, textureId={})", kHeroMeshId, kHeroTextureId);
    }

    bool LoadHeroMotionFromIni(Engine& engine, uint64_t motionId, CVirtualFileSystem& vfs)
    {
        m_HeroAnimationResources.clear();
        m_HeroSkeletons.clear();
        m_HeroHasSkeleton = false;
        m_HeroHasClip     = false;

        ResourceManager& resourceManager = engine.GetResourceManager();
        std::string      motionPath;
        if (!resourceManager.TryResolveMotionPath(motionId, motionPath) || motionPath.empty())
        {
            LX_WARN("[HeroTest] Motion ID {} not found in ini/3dmotion.ini", motionId);
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        C3RuntimeLoader loader;
        C3LoadRequest   request;
        request.virtualPath    = motionPath;
        request.requestedTypes = {C3ResourceType::Skeleton, C3ResourceType::Animation};

        C3LoadResult result;
        if (!loader.LoadFromVfs(vfs, request, result))
        {
            LX_WARN("[HeroTest] Failed to load motion data from {}", motionPath);
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        if (result.skeletons.empty() || result.animations.empty())
        {
            LX_WARN("[HeroTest] Missing skeleton/animation in motion file {}", motionPath);
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        m_HeroSkeletons          = std::move(result.skeletons);
        m_HeroAnimationResources = std::move(result.animations);
        m_HeroAnimationIndex     = std::min<int>(m_HeroAnimationIndex, static_cast<int>(m_HeroAnimationResources.size() - 1));

        return ApplyHeroAnimationSelection();
    }

    bool ApplyHeroAnimationSelection()
    {
        if (m_HeroAnimationResources.empty() || m_HeroSkeletons.empty())
        {
            m_HeroHasClip          = false;
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        if (m_HeroAnimationIndex < 0 || m_HeroAnimationIndex >= static_cast<int>(m_HeroAnimationResources.size()))
        {
            m_HeroAnimationIndex = 0;
        }

        const AnimationClipResource& clipResource = m_HeroAnimationResources[static_cast<size_t>(m_HeroAnimationIndex)];

        size_t skeletonIndex = 0;
        if (m_HeroAnimationIndex < static_cast<int>(m_HeroSkeletons.size()) &&
            m_HeroSkeletons[static_cast<size_t>(m_HeroAnimationIndex)].boneCount == clipResource.boneCount)
        {
            skeletonIndex = static_cast<size_t>(m_HeroAnimationIndex);
        }
        else
        {
            for (size_t i = 0; i < m_HeroSkeletons.size(); ++i)
            {
                if (m_HeroSkeletons[i].boneCount == clipResource.boneCount)
                {
                    skeletonIndex = i;
                    break;
                }
            }
        }

        m_HeroSkeleton    = m_HeroSkeletons[skeletonIndex];
        m_HeroHasSkeleton = (m_HeroSkeleton.boneCount > 0);
        if (!m_HeroHasSkeleton)
        {
            m_HeroHasClip          = false;
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        m_HeroClip    = AnimationClip{};
        m_HeroHasClip = m_HeroClip.Initialize(clipResource);
        if (!m_HeroHasClip)
        {
            LX_WARN("[HeroTest] Failed to initialize animation clip index {}", m_HeroAnimationIndex);
            m_Hero.animationPlayer = nullptr;
            return false;
        }

        m_HeroPlayer.SetSkeleton(&m_HeroSkeleton);
        m_HeroPlayer.SetClip(&m_HeroClip);
        m_HeroPlayer.SetLooping(true);
        m_HeroPlayer.SetUseDirectMatrices(m_HeroDirectMotion);
        m_HeroPlayer.Reset();
        m_HeroPlayer.Sample();
        m_Hero.animationPlayer = &m_HeroPlayer;
        return true;
    }

    void RenderHero(Engine& engine)
    {
        if (!m_HeroReady || !m_HeroRenderer.IsInitialized())
        {
            return;
        }

        if (m_HeroAnimate && m_Hero.animationPlayer)
        {
            const float deltaSeconds = static_cast<float>(engine.GetTimingSnapshot().DeltaTimeSeconds);
            m_HeroPlayer.Advance(deltaSeconds * m_HeroAnimSpeed);
        }

        const int viewportWidth  = engine.GetRendererViewportWidth();
        const int viewportHeight = engine.GetRendererViewportHeight();

        LXCore::Matrix4 view = MakeIdentity();
        LXCore::Matrix4 proj = MakeIdentity();

        if (engine.IsMapReady())
        {
            const MapCamera& mapCamera = engine.GetMapSystem().GetCamera();
            BuildMapViewProjection(mapCamera, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), view, proj);
        }
        else
        {
            Scene&  scene  = engine.GetScene();
            Camera& camera = scene.GetActiveCamera();
            camera.SyncDirtyMatricesForRender(viewportWidth, viewportHeight);
            view = camera.GetViewMatrix();
            proj = camera.GetProjectionMatrix();
        }

        engine.ExecuteExternalRenderPass(
            [this, view, proj]()
            {
                m_HeroRenderer.Render(m_Hero, view, proj);
            });
    }

#if defined(LX_DEBUG) || defined(LX_DEV)
    void RenderHeroDebugPanel(Engine& engine)
    {
        if (!ImGui::Begin("Hero Tools"))
        {
            ImGui::End();
            return;
        }

        if (!m_HeroReady)
        {
            ImGui::TextUnformatted("Hero not loaded.");
            ImGui::End();
            return;
        }

        static bool  followHero         = false;
        static float rotationDegrees[3] = {0.0f, 0.0f, 0.0f};
        static bool  flipAxis[3]        = {false, false, false};

        ImGui::Text("Hero World: (%.1f, %.1f)", m_Hero.worldX, m_Hero.worldY);
        ImGui::Separator();
        ImGui::DragFloat("World X", &m_Hero.worldX, 1.0f);
        ImGui::DragFloat("World Y", &m_Hero.worldY, 1.0f);
        ImGui::DragFloat("Height", &m_Hero.height, 0.5f);
        ImGui::SliderFloat("Scale", &m_Hero.scale, 0.1f, 5.0f);
        int direction = static_cast<int>(m_Hero.direction);
        if (ImGui::SliderInt("Direction", &direction, 0, 7))
        {
            m_Hero.direction = static_cast<uint8_t>(direction);
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Extra Rotation (degrees)");
        ImGui::DragFloat3("Rotate XYZ", rotationDegrees, 0.5f);
        ImGui::TextUnformatted("Flip Axes");
        ImGui::Checkbox("Flip X", &flipAxis[0]);
        ImGui::SameLine();
        ImGui::Checkbox("Flip Y", &flipAxis[1]);
        ImGui::SameLine();
        ImGui::Checkbox("Flip Z", &flipAxis[2]);
        if (ImGui::Button("Reset Rotation"))
        {
            rotationDegrees[0] = 0.0f;
            rotationDegrees[1] = 0.0f;
            rotationDegrees[2] = 0.0f;
            flipAxis[0]        = false;
            flipAxis[1]        = false;
            flipAxis[2]        = false;
        }

        m_HeroRenderer.SetExtraRotationDegrees({rotationDegrees[0], rotationDegrees[1], rotationDegrees[2]});
        m_HeroRenderer.SetExtraAxisScale({flipAxis[0] ? -1.0f : 1.0f, flipAxis[1] ? -1.0f : 1.0f, flipAxis[2] ? -1.0f : 1.0f});

        ImGui::Separator();
        ImGui::TextUnformatted("Animation");
        ImGui::Checkbox("Direct Matrices (Legacy)", &m_HeroDirectMotion);
        if (m_Hero.animationPlayer)
        {
            m_HeroPlayer.SetUseDirectMatrices(m_HeroDirectMotion);
        }
        ImGui::InputScalar("Look Type", ImGuiDataType_U32, &m_HeroLookType);
        ImGui::InputScalar("Action", ImGuiDataType_U32, &m_HeroAction);

        const uint64_t derivedMotionId = static_cast<uint64_t>(m_HeroLookType) * 1000000ull + static_cast<uint64_t>(m_HeroAction);
        char           motionKey[32]   = {};
        std::snprintf(motionKey, sizeof(motionKey), "%010llu", static_cast<unsigned long long>(derivedMotionId));
        ImGui::Text("Motion ID = %llu  (ini key: %s)", static_cast<unsigned long long>(derivedMotionId), motionKey);

        if (ImGui::Button("Load Motion (Look + Action)"))
        {
            m_HeroMotionId = derivedMotionId;
            LoadHeroMotionFromIni(engine, m_HeroMotionId, engine.GetVFS());
        }

        ImGui::Separator();
        ImGui::InputScalar("Motion ID", ImGuiDataType_U64, &m_HeroMotionId);
        if (ImGui::Button("Load Motion (ini/3dmotion.ini)"))
        {
            LoadHeroMotionFromIni(engine, m_HeroMotionId, engine.GetVFS());
        }
        if (m_HeroAnimationResources.empty())
        {
            ImGui::TextUnformatted("No animation data loaded.");
        }
        else
        {
            ImGui::Checkbox("Animate", &m_HeroAnimate);
            ImGui::DragFloat("Speed", &m_HeroAnimSpeed, 0.05f, 0.0f, 4.0f);

            int       clipIndex = m_HeroAnimationIndex;
            const int clipCount = static_cast<int>(m_HeroAnimationResources.size());
            if (clipCount > 0 && ImGui::SliderInt("Clip Index", &clipIndex, 0, clipCount - 1))
            {
                m_HeroAnimationIndex = clipIndex;
                ApplyHeroAnimationSelection();
            }
            const AnimationClipResource& clip = m_HeroAnimationResources[static_cast<size_t>(m_HeroAnimationIndex)];
            ImGui::Text("Clips: %d (clip bones: %u, frames: %u)", clipCount, clip.boneCount, clip.frameCount);
            ImGui::Text("Skeleton bones: %u", m_HeroSkeleton.boneCount);
        }

        if (ImGui::Button("Copy Values"))
        {
            char buffer[256] = {};
            std::snprintf(buffer,
                          sizeof(buffer),
                          "worldX=%.3f, worldY=%.3f, height=%.3f, scale=%.3f, direction=%u, rotX=%.3f, rotY=%.3f, rotZ=%.3f, flipX=%d, "
                          "flipY=%d, flipZ=%d",
                          m_Hero.worldX,
                          m_Hero.worldY,
                          m_Hero.height,
                          m_Hero.scale,
                          static_cast<unsigned int>(m_Hero.direction),
                          rotationDegrees[0],
                          rotationDegrees[1],
                          rotationDegrees[2],
                          flipAxis[0] ? 1 : 0,
                          flipAxis[1] ? 1 : 0,
                          flipAxis[2] ? 1 : 0);
            ImGui::SetClipboardText(buffer);
            LX_INFO("[HeroTools] {}", buffer);
        }

        if (engine.IsMapReady())
        {
            if (ImGui::Button("Focus Hero"))
            {
                engine.GetMapSystem().GetCamera().SetViewCenter(m_Hero.worldX, m_Hero.worldY);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Follow Hero", &followHero);
            if (followHero)
            {
                engine.GetMapSystem().GetCamera().SetViewCenter(m_Hero.worldX, m_Hero.worldY);
            }
        }
        else
        {
            ImGui::TextUnformatted("Map not ready.");
        }

        ImGui::End();
    }
#endif
    LXHeroRenderer                     m_HeroRenderer;
    RuntimeMesh*                       m_HeroMesh = nullptr;
    LXHero                             m_Hero;
    bool                               m_HeroReady = false;
    AnimationPlayer                    m_HeroPlayer;
    AnimationClip                      m_HeroClip;
    SkeletonResource                   m_HeroSkeleton;
    std::vector<SkeletonResource>      m_HeroSkeletons;
    std::vector<AnimationClipResource> m_HeroAnimationResources;
    int                                m_HeroAnimationIndex = 0;
    uint64_t                           m_HeroMotionId       = 0;
    uint32_t                           m_HeroLookType       = 2;
    uint32_t                           m_HeroAction         = 100;
    bool                               m_HeroDirectMotion   = true;
    bool                               m_HeroHasSkeleton    = false;
    bool                               m_HeroHasClip        = false;
    bool                               m_HeroAnimate        = true;
    float                              m_HeroAnimSpeed      = 1.0f;

#if defined(LX_DEBUG) || defined(LX_DEV)
    void WireImGuiCallbacks()
    {
        GetWindow().OnRawMessage = [this](UINT msg, WPARAM wParam, LPARAM lParam) -> bool
        {
            const bool consumed =
                m_ImGuiLayer.HandleWin32Message(static_cast<uint32_t>(msg), static_cast<uint64_t>(wParam), static_cast<int64_t>(lParam));
            m_DebugUI.SetLastInputConsumedByDebugUI(consumed);
            return consumed;
        };

        // Resize order: Engine first, then ImGui viewport-dependent state.
        GetWindow().OnResize = [this](int w, int h)
        {
            Engine& engine = GetEngine();
            if (engine.IsInitialized())
            {
                engine.OnResize(w, h);
            }
            m_ImGuiLayer.OnResize(w, h);
        };

        GetWindow().OnMouseWheel = [this](int delta)
        {
            if (m_ImGuiLayer.WantsMouseCapture())
            {
                return;
            }

            Engine& engine = GetEngine();
            if (engine.IsInitialized())
            {
                engine.GetInput().OnMouseWheel(delta);
            }
        };
    }

    void RenderValidationSprite(Engine& engine)
    {
        if (!m_ValidationTexture)
        {
            return;
        }

        LXEngine::SpriteRenderer& spriteRenderer = engine.GetSpriteRenderer();
        if (!spriteRenderer.IsInitialized())
        {
            return;
        }

        engine.ExecuteSpritePass(
            [this](LXEngine::SpriteRenderer& activeSpriteRenderer)
            {
                // Draw one visible debug sprite so texture/VFS/render validation is immediate.
                activeSpriteRenderer.DrawSprite(m_ValidationTexture.get(), {100.0f, 100.0f}, {256.0f, 256.0f});
            });
    }

    ImGuiLayer                         m_ImGuiLayer;
    DebugUI                            m_DebugUI;
    std::shared_ptr<LXEngine::Texture> m_ValidationTexture;
    bool                               m_F6WasDown = false;
#endif
};

} // namespace LXShell

LXEngine::Application* LXEngine::CreateApplication()
{
    return new LXShell::TestApplication();
}
