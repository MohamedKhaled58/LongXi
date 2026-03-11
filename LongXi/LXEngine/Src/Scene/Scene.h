#pragma once

#include "Renderer/Renderer.h"
#include "Scene/Camera.h"
#include "Scene/SceneNode.h"

#include <functional>

namespace LongXi
{

class Scene
{
  public:
    Scene();
    ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    bool Initialize(Renderer& renderer);
    void Shutdown();
    bool IsInitialized() const;

    void AddNode(std::unique_ptr<SceneNode> node);
    void RemoveNode(SceneNode* node);

    void Update(float deltaTime);
    void Render(Renderer& renderer);
    void OnResize(int width, int height);

    Camera& GetActiveCamera();
    const Camera& GetActiveCamera() const;
    SceneNode& GetRootNode();
    const SceneNode& GetRootNode() const;

    void VisitNodes(const std::function<void(SceneNode&, int depth)>& visitor);
    void VisitNodes(const std::function<void(const SceneNode&, int depth)>& visitor) const;

  private:
    SceneNode m_Root;
    Camera m_Camera;
    bool m_Initialized = false;
};

} // namespace LongXi
