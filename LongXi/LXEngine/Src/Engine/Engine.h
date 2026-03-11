#pragma once

#include <Windows.h>
#include <chrono>
#include <memory>
#include <string>

namespace LongXi
{

// Forward declarations
class DX11Renderer;
class InputSystem;
class CVirtualFileSystem;
class TextureManager;
class SpriteRenderer;
class Scene;

// =============================================================================
// Engine — Central runtime coordinator class
// Owns and manages all engine subsystems (Renderer, Input, VFS, TextureManager)
// =============================================================================

class Engine
{
  public:
    Engine();
    ~Engine();

    // Disable copy and move
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Initialize all engine subsystems in dependency order
    // Order: Renderer → InputSystem → VFS → TextureManager
    bool Initialize(HWND windowHandle, int width, int height);

    // Shutdown all engine subsystems in reverse dependency order
    void Shutdown();

    // Check if Engine is initialized
    bool IsInitialized() const;

    // =========================================================================
    // Runtime Loop
    // =========================================================================

    // Advance one frame of engine runtime (input, game logic, etc.)
    void Update();

    // Render one frame
    void Render();

    // Handle window resize event
    void OnResize(int width, int height);

    // =========================================================================
    // VFS Configuration (called by Application before runtime loop)
    // =========================================================================

    void MountDirectory(const std::string& path);
    void MountWdf(const std::string& path);

    // =========================================================================
    // Subsystem Accessors
    // =========================================================================

    DX11Renderer& GetRenderer();
    InputSystem& GetInput();
    CVirtualFileSystem& GetVFS();
    TextureManager& GetTextureManager();
    SpriteRenderer& GetSpriteRenderer();
    Scene& GetScene();

  private:
    // Subsystem ownership
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_Input;
    std::unique_ptr<CVirtualFileSystem> m_VFS;
    std::unique_ptr<TextureManager> m_TextureManager;
    std::unique_ptr<SpriteRenderer> m_SpriteRenderer;
    std::unique_ptr<Scene> m_Scene;

    // State
    bool m_Initialized;
    std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime;
    bool m_FirstFrame = true;
};

} // namespace LongXi
