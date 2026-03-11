#pragma once

#include "Scene/SceneNode.h"

// =============================================================================
// Scene — Engine subsystem that owns the world object hierarchy
//
// Scene contains a root SceneNode that acts as the invisible parent of all
// top-level game objects. Each frame:
//   - Scene::Update() performs a dirty-flag pre-order depth-first traversal,
//     recomputing world transforms for dirty nodes before calling Update().
//   - Scene::Render() performs a pre-order depth-first traversal calling
//     Submit() on every node. Scene itself issues no draw calls.
//
// Initialized as Engine subsystem step 6 (after SpriteRenderer).
// =============================================================================

namespace LongXi
{

class DX11Renderer;

class Scene
{
  public:
    Scene();
    ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    // =========================================================================
    // Node management (attaches/detaches from scene root)
    // =========================================================================

    void AddNode(std::unique_ptr<SceneNode> node);
    void RemoveNode(SceneNode* node);

    // =========================================================================
    // Per-frame (called by Engine)
    // =========================================================================

    void Update(float deltaTime);
    void Render(DX11Renderer& renderer);
    void OnResize(int width, int height);

  private:
    SceneNode m_Root;
    bool m_Initialized = false;
};

} // namespace LongXi
