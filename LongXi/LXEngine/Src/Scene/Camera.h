#pragma once

#include "Math/Math.h"

namespace LongXi
{

class Camera
{
public:
    Camera();

    void    SetPosition(Vector3 position);
    Vector3 GetPosition() const;

    void    SetRotation(Vector3 rotationDegrees);
    Vector3 GetRotation() const;

    void  SetFOV(float degrees);
    float GetFOV() const;

    void  SetNearFar(float nearPlane, float farPlane);
    float GetNearPlane() const;
    float GetFarPlane() const;

    void UpdateViewMatrix();
    void UpdateProjectionMatrix(int viewportWidth, int viewportHeight);

    const Matrix4& GetViewMatrix() const;
    const Matrix4& GetProjectionMatrix() const;

    bool IsViewDirty() const;
    bool IsProjectionDirty() const;
    void SyncDirtyMatricesForRender(int viewportWidth, int viewportHeight);

private:
    Vector3 m_Position        = {0.0f, 0.0f, -10.0f};
    Vector3 m_RotationDegrees = {0.0f, 0.0f, 0.0f};

    float m_FieldOfViewDegrees = 45.0f;
    float m_AspectRatio        = 1.0f;
    float m_NearPlane          = 1.0f;
    float m_FarPlane           = 10000.0f;

    Matrix4 m_ViewMatrix       = {};
    Matrix4 m_ProjectionMatrix = {};

    bool m_ViewDirty       = true;
    bool m_ProjectionDirty = true;
};

} // namespace LongXi
