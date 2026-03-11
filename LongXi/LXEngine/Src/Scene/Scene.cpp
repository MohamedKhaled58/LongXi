#include "Scene/Scene.h"
#include "Renderer/DX11Renderer.h"
#include "Core/Logging/LogMacros.h"

namespace LongXi
{

// ============================================================================
// Constructor
// ============================================================================

Scene::Scene() {}

// ============================================================================
// Lifecycle
// ============================================================================

bool Scene::Initialize()
{
    m_Initialized = true;
    LX_ENGINE_INFO("[Scene] Initialized");
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
    // Phase 1: no resize-aware nodes yet — no-op
}

} // namespace LongXi
