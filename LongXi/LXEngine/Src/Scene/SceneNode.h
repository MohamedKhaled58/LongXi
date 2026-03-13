#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/Math/Math.h"
#include "Renderer/Renderer.h"

namespace LXEngine
{

class SceneNode
{
public:
    SceneNode();
    virtual ~SceneNode() = default;

    SceneNode(const SceneNode&)            = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    void AddChild(std::unique_ptr<SceneNode> child);
    void RemoveChild(SceneNode* child);

    void SetPosition(LXCore::Vector3 position);
    void SetRotation(LXCore::Vector3 rotationDegrees);
    void SetScale(LXCore::Vector3 scale);

    const LXCore::Vector3& GetPosition() const;
    const LXCore::Vector3& GetRotation() const;
    const LXCore::Vector3& GetScale() const;

    void               SetName(std::string name);
    const std::string& GetName() const;

    const LXCore::Vector3& GetWorldPosition() const;
    const LXCore::Vector3& GetWorldRotation() const;
    const LXCore::Vector3& GetWorldScale() const;
    SceneNode*             GetParent();
    const SceneNode*       GetParent() const;

    virtual void Update(float deltaTime);
    virtual void Submit(Renderer& renderer);

private:
    void TraverseUpdate(float deltaTime, bool parentWasRecomputed);
    void TraverseRender(Renderer& renderer);
    void ComputeWorldTransform();

    friend class Scene;

private:
    LXCore::Vector3 m_Position = {0.0f, 0.0f, 0.0f};
    LXCore::Vector3 m_Rotation = {0.0f, 0.0f, 0.0f};
    LXCore::Vector3 m_Scale    = {1.0f, 1.0f, 1.0f};

    LXCore::Vector3 m_WorldPosition = {0.0f, 0.0f, 0.0f};
    LXCore::Vector3 m_WorldRotation = {0.0f, 0.0f, 0.0f};
    LXCore::Vector3 m_WorldScale    = {1.0f, 1.0f, 1.0f};

    bool                                    m_TransformDirty = true;
    SceneNode*                              m_Parent         = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_Children;
    std::string                             m_Name = "SceneNode";
};

} // namespace LXEngine
