#include "Scene/SceneNode.h"
#include "Renderer/DX11Renderer.h"
#include "Core/Logging/LogMacros.h"

#include <algorithm>
#include <utility>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

SceneNode::SceneNode()
    : m_Position({0.0f, 0.0f, 0.0f}), m_Rotation({0.0f, 0.0f, 0.0f}), m_Scale({1.0f, 1.0f, 1.0f}), m_WorldPosition({0.0f, 0.0f, 0.0f}), m_WorldRotation({0.0f, 0.0f, 0.0f}),
      m_WorldScale({1.0f, 1.0f, 1.0f}), m_TransformDirty(true), m_Parent(nullptr)
{
}

// ============================================================================
// Hierarchy
// ============================================================================

void SceneNode::AddChild(std::unique_ptr<SceneNode> child)
{
    if (!child)
    {
        LX_ENGINE_ERROR("[Scene] AddChild: null child");
        return;
    }
    child->m_Parent = this;
    child->m_TransformDirty = true; // ensure world transform computed on first traversal
    m_Children.push_back(std::move(child));
    LX_ENGINE_INFO("[Scene] Node added");
}

void SceneNode::RemoveChild(SceneNode* child)
{
    if (!child)
    {
        LX_ENGINE_ERROR("[Scene] RemoveChild: null pointer");
        return;
    }

    auto it = std::find_if(m_Children.begin(), m_Children.end(), [child](const auto& p) { return p.get() == child; });

    if (it == m_Children.end())
    {
        LX_ENGINE_ERROR("[Scene] RemoveChild: node not found");
        return;
    }

    m_Children.erase(it); // unique_ptr destroyed here — children recursively destroyed
    LX_ENGINE_INFO("[Scene] Node removed");
}

// ============================================================================
// Local Transform Setters (all mark dirty)
// ============================================================================

void SceneNode::SetPosition(Vector3 position)
{
    m_Position = position;
    m_TransformDirty = true;
}

void SceneNode::SetRotation(Vector3 rotationDegrees)
{
    m_Rotation = rotationDegrees;
    m_TransformDirty = true;
}

void SceneNode::SetScale(Vector3 scale)
{
    m_Scale = scale;
    m_TransformDirty = true;
}

// ============================================================================
// Local Transform Getters
// ============================================================================

const Vector3& SceneNode::GetPosition() const
{
    return m_Position;
}

const Vector3& SceneNode::GetRotation() const
{
    return m_Rotation;
}

const Vector3& SceneNode::GetScale() const
{
    return m_Scale;
}

void SceneNode::SetName(std::string name)
{
    if (name.empty())
    {
        m_Name = "SceneNode";
        return;
    }
    m_Name = std::move(name);
}

const std::string& SceneNode::GetName() const
{
    return m_Name;
}

// ============================================================================
// World Transform Getters (read-only — computed during traversal)
// ============================================================================

const Vector3& SceneNode::GetWorldPosition() const
{
    return m_WorldPosition;
}

const Vector3& SceneNode::GetWorldRotation() const
{
    return m_WorldRotation;
}

const Vector3& SceneNode::GetWorldScale() const
{
    return m_WorldScale;
}

SceneNode* SceneNode::GetParent()
{
    return m_Parent;
}

const SceneNode* SceneNode::GetParent() const
{
    return m_Parent;
}

// ============================================================================
// Virtual Frame Callbacks (base no-ops; subclasses override)
// ============================================================================

void SceneNode::Update(float /*deltaTime*/) {}

void SceneNode::Submit(DX11Renderer& /*renderer*/) {}

// ============================================================================
// Private: World Transform Computation
// ============================================================================

void SceneNode::ComputeWorldTransform()
{
    if (m_Parent != nullptr)
    {
        // World position = parent world position + local position
        m_WorldPosition.x = m_Parent->m_WorldPosition.x + m_Position.x;
        m_WorldPosition.y = m_Parent->m_WorldPosition.y + m_Position.y;
        m_WorldPosition.z = m_Parent->m_WorldPosition.z + m_Position.z;

        // World rotation = parent world rotation + local rotation (degrees, additive)
        m_WorldRotation.x = m_Parent->m_WorldRotation.x + m_Rotation.x;
        m_WorldRotation.y = m_Parent->m_WorldRotation.y + m_Rotation.y;
        m_WorldRotation.z = m_Parent->m_WorldRotation.z + m_Rotation.z;

        // World scale = parent world scale × local scale (component-wise multiply)
        m_WorldScale.x = m_Parent->m_WorldScale.x * m_Scale.x;
        m_WorldScale.y = m_Parent->m_WorldScale.y * m_Scale.y;
        m_WorldScale.z = m_Parent->m_WorldScale.z * m_Scale.z;
    }
    else
    {
        // Root-level node: world transform equals local transform
        m_WorldPosition = m_Position;
        m_WorldRotation = m_Rotation;
        m_WorldScale = m_Scale;
    }
}

// ============================================================================
// Private: Traversal (called by Scene)
// ============================================================================

void SceneNode::TraverseUpdate(float deltaTime, bool parentWasRecomputed)
{
    // Recompute world transform if this node or any ancestor is dirty
    bool needsRecompute = m_TransformDirty || parentWasRecomputed;
    if (needsRecompute)
    {
        ComputeWorldTransform(); // parent world transform already fresh (pre-order)
        m_TransformDirty = false;
    }

    Update(deltaTime);

    for (auto& child : m_Children)
    {
        child->TraverseUpdate(deltaTime, needsRecompute);
    }
}

void SceneNode::TraverseRender(DX11Renderer& renderer)
{
    Submit(renderer); // no-op for base class; subclasses issue draw calls here

    for (auto& child : m_Children)
    {
        child->TraverseRender(renderer);
    }
}

} // namespace LongXi
