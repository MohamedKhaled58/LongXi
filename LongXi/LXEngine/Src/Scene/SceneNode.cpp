#include "Scene/SceneNode.h"

#include <algorithm>
#include <utility>

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

SceneNode::SceneNode() = default;

void SceneNode::AddChild(std::unique_ptr<SceneNode> child)
{
    if (!child)
    {
        LX_ENGINE_ERROR("[Scene] AddChild: null child");
        return;
    }

    child->m_Parent         = this;
    child->m_TransformDirty = true;
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

    const auto it = std::ranges::find_if(m_Children,
                                         [child](const auto& p)
                                         {
                                             return p.get() == child;
                                         });
    if (it == m_Children.end())
    {
        LX_ENGINE_ERROR("[Scene] RemoveChild: node not found");
        return;
    }

    m_Children.erase(it);
    LX_ENGINE_INFO("[Scene] Node removed");
}

void SceneNode::SetPosition(Vector3 position)
{
    m_Position       = position;
    m_TransformDirty = true;
}

void SceneNode::SetRotation(Vector3 rotationDegrees)
{
    m_Rotation       = rotationDegrees;
    m_TransformDirty = true;
}

void SceneNode::SetScale(Vector3 scale)
{
    m_Scale          = scale;
    m_TransformDirty = true;
}

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
    m_Name = name.empty() ? "SceneNode" : std::move(name);
}

const std::string& SceneNode::GetName() const
{
    return m_Name;
}

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

void SceneNode::Update(float /*deltaTime*/) {}

void SceneNode::Submit(Renderer& /*renderer*/) {}

void SceneNode::ComputeWorldTransform()
{
    if (m_Parent)
    {
        m_WorldPosition.x = m_Parent->m_WorldPosition.x + m_Position.x;
        m_WorldPosition.y = m_Parent->m_WorldPosition.y + m_Position.y;
        m_WorldPosition.z = m_Parent->m_WorldPosition.z + m_Position.z;

        m_WorldRotation.x = m_Parent->m_WorldRotation.x + m_Rotation.x;
        m_WorldRotation.y = m_Parent->m_WorldRotation.y + m_Rotation.y;
        m_WorldRotation.z = m_Parent->m_WorldRotation.z + m_Rotation.z;

        m_WorldScale.x = m_Parent->m_WorldScale.x * m_Scale.x;
        m_WorldScale.y = m_Parent->m_WorldScale.y * m_Scale.y;
        m_WorldScale.z = m_Parent->m_WorldScale.z * m_Scale.z;
        return;
    }

    m_WorldPosition = m_Position;
    m_WorldRotation = m_Rotation;
    m_WorldScale    = m_Scale;
}

void SceneNode::TraverseUpdate(float deltaTime, bool parentWasRecomputed)
{
    const bool needsRecompute = m_TransformDirty || parentWasRecomputed;
    if (needsRecompute)
    {
        ComputeWorldTransform();
        m_TransformDirty = false;
    }

    Update(deltaTime);

    for (auto& child : m_Children)
    {
        child->TraverseUpdate(deltaTime, needsRecompute);
    }
}

void SceneNode::TraverseRender(Renderer& renderer)
{
    Submit(renderer);

    for (auto& child : m_Children)
    {
        child->TraverseRender(renderer);
    }
}

} // namespace LongXi
