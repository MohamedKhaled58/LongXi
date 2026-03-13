#pragma once

#include "Core/Math/Math.h"

namespace LXEngine
{
class Camera
{
public:
    Camera();

    void            SetPosition(LXCore::Vector3 position);
    LXCore::Vector3 GetPosition() const;

    void            SetRotation(LXCore::Vector3 rotationDegrees);
    LXCore::Vector3 GetRotation() const;

    void  SetFOV(float degrees);
    float GetFOV() const;

    void  SetNearFar(float nearPlane, float farPlane);
    float GetNearPlane() const;
    float GetFarPlane() const;

    void UpdateViewMatrix();
    void UpdateProjectionMatrix(int viewportWidth, int viewportHeight);

    const LXCore::Matrix4& GetViewMatrix() const;
    const LXCore::Matrix4& GetProjectionMatrix() const;

    bool IsViewDirty() const;
    bool IsProjectionDirty() const;
    void SyncDirtyMatricesForRender(int viewportWidth, int viewportHeight);

private:
    LXCore::Vector3 m_Position        = {0.0f, 0.0f, -10.0f};
    LXCore::Vector3 m_RotationDegrees = {0.0f, 0.0f, 0.0f};

    float m_FieldOfViewDegrees = 45.0f;
    float m_AspectRatio        = 1.0f;
    float m_NearPlane          = 1.0f;
    float m_FarPlane           = 10000.0f;

    LXCore::Matrix4 m_ViewMatrix       = {};
    LXCore::Matrix4 m_ProjectionMatrix = {};

    bool m_ViewDirty       = true;
    bool m_ProjectionDirty = true;
};

} // namespace LXEngine
