#include "Scene/Scene.h"
#include "Renderer/DX11Renderer.h"
#include "Core/Logging/LogMacros.h"

namespace LongXi
{

// ============================================================================
// Constructor
// ============================================================================

Scene::Scene()
{
    m_Root.SetName("Root");
}

// ============================================================================
// Lifecycle
// ============================================================================

bool Scene::Initialize(DX11Renderer& renderer)
{
    if (m_Initialized)
    {
        return true;
    }

    // Default camera state for first-frame rendering.
    m_Camera.SetPosition({0.0f, 0.0f, -10.0f});
    m_Camera.SetRotation({0.0f, 0.0f, 0.0f});
    m_Camera.SetFOV(45.0f);
    m_Camera.SetNearFar(1.0f, 10000.0f);

    // Bootstrap projection from the active backbuffer dimensions.
    const int viewportWidth = renderer.GetViewportWidth();
    const int viewportHeight = renderer.GetViewportHeight();
    m_Camera.UpdateViewMatrix();
    m_Camera.UpdateProjectionMatrix(viewportWidth, viewportHeight);

    m_Initialized = true;
    LX_ENGINE_INFO("[Scene] Initialized");
    LX_ENGINE_INFO("[Camera] Initialized");
    return true;
}

void Scene::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }
    // m_Root destructor clears all children recursively via unique_ptr
    m_Initialized = false;
    LX_ENGINE_INFO("[Scene] Shutdown");
}

bool Scene::IsInitialized() const
{
    return m_Initialized;
}

Camera& Scene::GetActiveCamera()
{
    return m_Camera;
}

const Camera& Scene::GetActiveCamera() const
{
    return m_Camera;
}

SceneNode& Scene::GetRootNode()
{
    return m_Root;
}

const SceneNode& Scene::GetRootNode() const
{
    return m_Root;
}

void Scene::VisitNodes(const std::function<void(SceneNode&, int depth)>& visitor)
{
    if (!visitor)
    {
        return;
    }

    std::function<void(SceneNode&, int)> visitRecursive = [&](SceneNode& node, int depth)
    {
        visitor(node, depth);
        for (auto& child : node.m_Children)
        {
            visitRecursive(*child, depth + 1);
        }
    };

    visitRecursive(m_Root, 0);
}

void Scene::VisitNodes(const std::function<void(const SceneNode&, int depth)>& visitor) const
{
    if (!visitor)
    {
        return;
    }

    std::function<void(const SceneNode&, int)> visitRecursive = [&](const SceneNode& node, int depth)
    {
        visitor(node, depth);
        for (const auto& child : node.m_Children)
        {
            visitRecursive(*child, depth + 1);
        }
    };

    visitRecursive(m_Root, 0);
}

// ============================================================================
// Node Management
// ============================================================================

void Scene::AddNode(std::unique_ptr<SceneNode> node)
{
    if (!node)
    {
        LX_ENGINE_ERROR("[Scene] AddNode: null node");
        return;
    }
    m_Root.AddChild(std::move(node));
}

void Scene::RemoveNode(SceneNode* node)
{
    if (!node)
    {
        LX_ENGINE_ERROR("[Scene] RemoveNode: null pointer");
        return;
    }
    m_Root.RemoveChild(node);
}

// ============================================================================
// Per-frame
// ============================================================================

void Scene::Update(float deltaTime)
{
    // Skip root node itself (invisible container) — traverse children directly
    for (auto& child : m_Root.m_Children)
    {
        child->TraverseUpdate(deltaTime, false);
    }
}

void Scene::Render(DX11Renderer& renderer)
{
    if (!m_Initialized || !renderer.IsInitialized())
    {
        return;
    }

    // Guard ordering: camera matrices must be synchronized and handed off before
    // any node submits render work.
    m_Camera.SyncDirtyMatricesForRender(renderer.GetViewportWidth(), renderer.GetViewportHeight());
    renderer.SetViewProjection(m_Camera.GetViewMatrix(), m_Camera.GetProjectionMatrix());

    // Skip root node itself — traverse children directly
    // Scene issues zero draw calls; all draws happen in SceneNode::Submit()
    for (auto& child : m_Root.m_Children)
    {
        child->TraverseRender(renderer);
    }
}

void Scene::OnResize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    m_Camera.UpdateProjectionMatrix(width, height);
}

} // namespace LongXi
