// =============================================================================
// LXShell — Client Application
// =============================================================================

#include <Application/Application.h>
#include <Application/EntryPoint.h>
#include <Assets/C3/C3RuntimeLoader.h>
#include <Assets/C3/RuntimeMesh.h>
#include <Core/FileSystem/VirtualFileSystem.h>
#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>
#include <Hero/LXHero.h>
#include <Renderer/LXHeroRenderer.h>
#include <Renderer/SpriteRenderer.h>
#include <Scene/Camera.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>
#include <Texture/Texture.h>
#include <Texture/TextureManager.h>
#include <Window/Win32Window.h>

#if defined(LX_DEBUG) || defined(LX_DEV)
#include "DebugUI/DebugUI.h"
#include "ImGui/ImGuiLayer.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "Input/InputSystem.h"

namespace LXShell
{

using LXCore::CVirtualFileSystem;
using LXEngine::Camera;
using LXEngine::C3LoadRequest;
using LXEngine::C3LoadResult;
using LXEngine::C3RuntimeLoader;
using LXEngine::Engine;
using LXEngine::LXHero;
using LXEngine::LXHeroRenderer;
using LXEngine::Key;
using LXEngine::RuntimeMesh;
using LXEngine::Scene;
using LXEngine::SceneNode;
using LXEngine::SpriteRenderer;
using LXEngine::Texture;
using LXEngine::TextureManager;

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
            if (m_HeroMesh.IsValid())
            {
                m_HeroMesh.Release(engine.GetRenderer());
            }
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
            const size_t lineEnd = content.find('\n', offset);
            std::string_view line =
                (lineEnd == std::string_view::npos) ? content.substr(offset) : content.substr(offset, lineEnd - offset);
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

        constexpr uint32_t   kHeroMeshId       = 1111000;
        constexpr uint32_t   kHeroTextureId    = 2111300;
        constexpr const char kMeshMapIni[]     = "ini/3dobj.ini";
        constexpr const char kTextureMapIni[]  = "ini/3dtexture.ini";

        std::string meshPath;
        if (!TryLookupIniPath(vfs, kMeshMapIni, kHeroMeshId, meshPath))
        {
            LX_WARN("[HeroTest] Mesh id {} not found in {}", kHeroMeshId, kMeshMapIni);
            return;
        }

        C3RuntimeLoader loader;
        C3LoadRequest   request;
        request.virtualPath = meshPath;

        C3LoadResult result;
        if (!loader.LoadFromVfs(vfs, request, result) || result.meshes.empty())
        {
            LX_WARN("[HeroTest] Failed to load C3 mesh (id={}, path={})", kHeroMeshId, meshPath);
            return;
        }

        if (!m_HeroMesh.Initialize(engine.GetRenderer(), result.meshes[0]))
        {
            LX_WARN("[HeroTest] Failed to initialize runtime mesh from C3 data");
            return;
        }

        std::string texturePath;
        if (TryLookupIniPath(vfs, kTextureMapIni, kHeroTextureId, texturePath))
        {
            auto texture = textureManager.LoadTexture(texturePath);
            if (texture)
            {
                m_HeroMesh.SetDefaultTexture(texture->GetHandle());
            }
            else
            {
                LX_WARN("[HeroTest] Failed to load texture '{}'", texturePath);
            }
        }
        else
        {
            LX_WARN("[HeroTest] Texture id {} not found in {}", kHeroTextureId, kTextureMapIni);
        }

        m_Hero           = LXHero{};
        m_Hero.bodyMesh  = &m_HeroMesh;
        m_Hero.direction = 0;
        m_Hero.scale     = 0.75f;
        m_Hero.worldX    = 0.0f;
        m_Hero.worldY    = 0.0f;
        m_Hero.height    = 0.0f;
        m_HeroReady      = true;

        LX_INFO("[HeroTest] Hero setup complete (meshId={}, textureId={})", kHeroMeshId, kHeroTextureId);
    }

    void RenderHero(Engine& engine)
    {
        if (!m_HeroReady || !m_HeroRenderer.IsInitialized())
        {
            return;
        }

        Scene&  scene  = engine.GetScene();
        Camera& camera = scene.GetActiveCamera();
        camera.SyncDirtyMatricesForRender(engine.GetRendererViewportWidth(), engine.GetRendererViewportHeight());

        const LXCore::Matrix4 view = camera.GetViewMatrix();
        const LXCore::Matrix4 proj = camera.GetProjectionMatrix();

        engine.ExecuteExternalRenderPass(
            [this, view, proj]()
            {
                m_HeroRenderer.Render(m_Hero, view, proj);
            });
    }

    LXHeroRenderer m_HeroRenderer;
    RuntimeMesh    m_HeroMesh;
    LXHero         m_Hero;
    bool           m_HeroReady = false;

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
