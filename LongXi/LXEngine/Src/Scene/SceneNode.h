#pragma once

#include "Math/Math.h"

#include <memory>
#include <vector>

// =============================================================================
// SceneNode — Base class for all world objects in the Scene hierarchy
//
// Stores a local transform (position, rotation in degrees, scale) and a
// computed world transform. World transforms are recomputed lazily using a
// dirty-flag: setting any local transform component marks the node dirty.
// During Scene::Update() traversal, dirty nodes (or children of dirty parents)
// recompute their world transform before Update() is called.
//
// Parent pointer is immutable after AddChild() — re-parenting is not supported
// in Phase 1.
// =============================================================================

namespace LongXi
{

class DX11Renderer;

class SceneNode
{
  public:
    SceneNode();
    virtual ~SceneNode() = default;

    // Non-copyable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // =========================================================================
    // Hierarchy
    // =========================================================================

    // Take ownership of child and attach it to this node.
    // Sets child's parent pointer (immutable after this call).
    // Marks child transform dirty for first-frame world transform computation.
    void AddChild(std::unique_ptr<SceneNode> child);

    // Find and destroy a child by raw pointer. Logs error if null or not found.
    void RemoveChild(SceneNode* child);

    // =========================================================================
    // Local Transform (rotation values are in degrees)
    // =========================================================================

    void SetPosition(Vector3 position);
    void SetRotation(Vector3 rotationDegrees);
    void SetScale(Vector3 scale);

    const Vector3& GetPosition() const;
    const Vector3& GetRotation() const; // degrees
    const Vector3& GetScale() const;

    // =========================================================================
    // World Transform (computed during traversal — read-only)
    // =========================================================================

    const Vector3& GetWorldPosition() const;
    const Vector3& GetWorldRotation() const; // degrees
    const Vector3& GetWorldScale() const;

    // =========================================================================
    // Overridable frame callbacks
    // =========================================================================

    // Called once per frame during Scene::Update() traversal.
    // deltaTime is elapsed seconds since the last frame (0.0f on first frame).
    virtual void Update(float deltaTime);

    // Called once per frame during Scene::Render() traversal.
    // Subclasses issue draw calls here. Base implementation is a no-op.
    // Scene NEVER calls renderer methods directly — all draws happen in Submit().
    virtual void Submit(DX11Renderer& renderer);

  private:
    // Traversal methods — called by Scene only
    void TraverseUpdate(float deltaTime, bool parentWasRecomputed);
    void TraverseRender(DX11Renderer& renderer);
    void ComputeWorldTransform();

    friend class Scene;

  private:
    // Local transform (rotation in degrees)
    Vector3 m_Position = {0.0f, 0.0f, 0.0f};
    Vector3 m_Rotation = {0.0f, 0.0f, 0.0f};
    Vector3 m_Scale = {1.0f, 1.0f, 1.0f};

    // Computed world transform (dirty-flag updated during traversal)
    Vector3 m_WorldPosition = {0.0f, 0.0f, 0.0f};
    Vector3 m_WorldRotation = {0.0f, 0.0f, 0.0f};
    Vector3 m_WorldScale = {1.0f, 1.0f, 1.0f};

    // Dirty flag — true at construction; set by any Set*() call or AddChild()
    bool m_TransformDirty = true;

    // Non-owning parent pointer — set by parent's AddChild(), immutable after
    SceneNode* m_Parent = nullptr;

    // Owned child nodes
    std::vector<std::unique_ptr<SceneNode>> m_Children;
};

} // namespace LongXi
