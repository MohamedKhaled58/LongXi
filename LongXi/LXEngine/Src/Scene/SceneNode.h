#pragma once

#include "Math/Math.h"
#include "Renderer/Renderer.h"

#include <memory>
#include <string>
#include <vector>

namespace LongXi
{

class SceneNode
{
public:
    SceneNode();
    virtual ~SceneNode() = default;

    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    void AddChild(std::unique_ptr<SceneNode> child);
    void RemoveChild(SceneNode* child);

    void SetPosition(Vector3 position);
    void SetRotation(Vector3 rotationDegrees);
    void SetScale(Vector3 scale);

    const Vector3& GetPosition() const;
    const Vector3& GetRotation() const;
    const Vector3& GetScale() const;

    void SetName(std::string name);
    const std::string& GetName() const;

    const Vector3& GetWorldPosition() const;
    const Vector3& GetWorldRotation() const;
    const Vector3& GetWorldScale() const;
    SceneNode* GetParent();
    const SceneNode* GetParent() const;

    virtual void Update(float deltaTime);
    virtual void Submit(Renderer& renderer);

private:
    void TraverseUpdate(float deltaTime, bool parentWasRecomputed);
    void TraverseRender(Renderer& renderer);
    void ComputeWorldTransform();

    friend class Scene;

private:
    Vector3 m_Position = {0.0f, 0.0f, 0.0f};
    Vector3 m_Rotation = {0.0f, 0.0f, 0.0f};
    Vector3 m_Scale = {1.0f, 1.0f, 1.0f};

    Vector3 m_WorldPosition = {0.0f, 0.0f, 0.0f};
    Vector3 m_WorldRotation = {0.0f, 0.0f, 0.0f};
    Vector3 m_WorldScale = {1.0f, 1.0f, 1.0f};

    bool m_TransformDirty = true;
    SceneNode* m_Parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_Children;
    std::string m_Name = "SceneNode";
};

} // namespace LongXi
