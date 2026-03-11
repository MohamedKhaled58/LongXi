// =============================================================================
// LXShell — Client Application
// =============================================================================

#include <Application/Application.h>
#include <Application/EntryPoint.h>
#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>
#include <Scene/Camera.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>
#include <Texture/TextureManager.h>
#include <Window/Win32Window.h>

#if defined(LX_DEBUG) || defined(LX_DEV)
#include "DebugUI/DebugUI.h"
#include "ImGui/ImGuiLayer.h"
#endif

namespace LongXi
{

class TestApplication : public Application
{
  public:
    bool Initialize() override
    {
        if (!Application::Initialize())
        {
            return false;
        }

#if defined(LX_DEBUG) || defined(LX_DEV)
        Engine& engine = GetEngine();
        if (engine.IsInitialized())
        {
            if (!m_ImGuiLayer.Initialize(engine, GetWindowHandle()))
            {
                LX_ENGINE_WARN("[Application] ImGuiLayer initialization failed, running without developer UI");
            }
            else
            {
                WireImGuiCallbacks();
                SetupValidationScene();
            }
        }
#endif

        return true;
    }

    int Run() override
    {
        LX_ENGINE_INFO("Entering main loop");

        Engine& engine = GetEngine();
        if (!engine.IsInitialized())
        {
            LX_ENGINE_ERROR("Cannot run: engine is not initialized");
            return 1;
        }

        MSG msg = {};
        while (true)
        {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    LX_ENGINE_INFO("Exiting main loop");
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
                m_ImGuiLayer.BeginFrame();
                m_DebugUI.UpdateViewModels(engine);
                m_DebugUI.RenderPanels(engine);
                m_ImGuiLayer.EndFrame();
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
#if defined(LX_DEBUG) || defined(LX_DEV)
    void WireImGuiCallbacks()
    {
        GetWindow().OnRawMessage = [this](UINT msg, WPARAM wParam, LPARAM lParam) -> bool
        {
            const bool consumed = m_ImGuiLayer.HandleWin32Message(static_cast<uint32_t>(msg), static_cast<uint64_t>(wParam), static_cast<int64_t>(lParam));
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

    void SetupValidationScene()
    {
        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("VALIDATION SCENE SETUP");
        LX_ENGINE_INFO("==============================================");

        Engine& engine = GetEngine();
        Scene& scene = engine.GetScene();
        TextureManager& textureManager = engine.GetTextureManager();

        auto rootNode = std::make_unique<SceneNode>();
        rootNode->SetName("ValidationRoot");
        rootNode->SetPosition({0.0f, 0.0f, 0.0f});
        LX_ENGINE_INFO("[ValidationScene] Validation root created");

        static const char* TEST_TEXTURE_PATH = "Data/texture/test.dds";
        auto testTexture = textureManager.LoadTexture(TEST_TEXTURE_PATH);

        if (!testTexture)
        {
            LX_ENGINE_WARN("[ValidationScene] '{}' not found, falling back to '1.dds'", TEST_TEXTURE_PATH);
            testTexture = textureManager.LoadTexture("1.dds");
        }

        if (testTexture)
        {
            auto spriteNode = std::make_unique<SceneNode>();
            spriteNode->SetName("TestSpriteNode");
            spriteNode->SetPosition({0.0f, 0.0f, 0.0f});
            spriteNode->SetScale({1.0f, 1.0f, 1.0f});
            rootNode->AddChild(std::move(spriteNode));
            LX_ENGINE_INFO("[ValidationScene] Test sprite node attached");
        }
        else
        {
            LX_ENGINE_WARN("[ValidationScene] No test texture available");
        }

        scene.AddNode(std::move(rootNode));

        Camera& camera = scene.GetActiveCamera();
        camera.SetPosition({0.0f, 0.0f, -5.0f});
        camera.SetRotation({0.0f, 0.0f, 0.0f});
        camera.SetFOV(60.0f);
        camera.SetNearFar(0.1f, 1000.0f);

        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("VALIDATION SCENE SETUP COMPLETE");
        LX_ENGINE_INFO("==============================================");
    }

    ImGuiLayer m_ImGuiLayer;
    DebugUI m_DebugUI;
#endif
};

} // namespace LongXi

LongXi::Application* CreateApplication()
{
    return new LongXi::TestApplication();
}
