#pragma once

#include <Windows.h>

#include "Profiling/ProfilerCollector.h"
#include "Renderer/Renderer.h"
#include "Timing/TimingService.h"

#include <functional>
#include <memory>
#include <string>

namespace LongXi
{

class InputSystem;
class CVirtualFileSystem;
class TextureManager;
class SpriteRenderer;
class Scene;

class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    bool Initialize(HWND windowHandle, int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    void Update();
    void Render();
    void Present();
    void OnResize(int width, int height);

    void ExecuteExternalRenderPass(const ExternalPassCallback& callback);
    void ExecuteSpritePass(const std::function<void(SpriteRenderer&)>& callback);

    void MountDirectory(const std::string& path);
    void MountWdf(const std::string& path);

    Renderer& GetRenderer();
    InputSystem& GetInput();
    CVirtualFileSystem& GetVFS();
    TextureManager& GetTextureManager();
    SpriteRenderer& GetSpriteRenderer();
    Scene& GetScene();

    void* GetRendererDeviceHandle() const;
    void* GetRendererContextHandle() const;
    int GetRendererViewportWidth() const;
    int GetRendererViewportHeight() const;

    void SetFrameLimiterEnabled(bool enabled);
    bool IsFrameLimiterEnabled() const;
    void SetFrameRateLimit(double targetFramesPerSecond);
    double GetFrameRateLimit() const;
    void SetMaxDeltaTime(double maxDeltaSeconds);
    double GetMaxDeltaTime() const;

    const TimingSnapshot& GetTimingSnapshot() const;
    const FrameProfileSnapshot& GetLastFrameProfileSnapshot() const;
    static bool IsProfilingEnabled();

private:
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_Input;
    std::unique_ptr<CVirtualFileSystem> m_VFS;
    std::unique_ptr<TextureManager> m_TextureManager;
    std::unique_ptr<SpriteRenderer> m_SpriteRenderer;
    std::unique_ptr<Scene> m_Scene;
    TimingService m_TimingService;
    ProfilerCollector m_ProfilerCollector;

    bool m_Initialized = false;
};

} // namespace LongXi
