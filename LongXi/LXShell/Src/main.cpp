// =============================================================================
// LXShell — Client Application
// =============================================================================

#include <Application/Application.h>
#include <Application/EntryPoint.h>
#include <Core/FileSystem/VirtualFileSystem.h>
#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>
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
#include <cstring>
#include <memory>

#include "Input/InputSystem.h"

namespace LXShell
{

using LXCore::CVirtualFileSystem;
using LXEngine::Camera;
using LXEngine::Engine;
using LXEngine::Key;
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
#if defined(LX_DEBUG) || defined(LX_DEV)
        m_ImGuiLayer.Shutdown();
#endif
        Application::Shutdown();
    }

private:
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

        LX_INFO("==============================================");
        LX_INFO("VALIDATION SCENE SETUP COMPLETE");
        LX_INFO("==============================================");
    }

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
