#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

namespace
{

constexpr float kMinFovDegrees = 1.0f;
constexpr float kMaxFovDegrees = 179.0f;
constexpr float kMinNearPlane  = 0.01f;
constexpr float kMinDepthRange = 0.01f;
constexpr float kEpsilon       = 0.000001f;

Matrix4 MakeIdentity()
{
    Matrix4 matrix = {};
    matrix.m[0]    = 1.0f;
    matrix.m[5]    = 1.0f;
    matrix.m[10]   = 1.0f;
    matrix.m[15]   = 1.0f;
    return matrix;
}

float DegreesToRadians(float degrees)
{
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

float NormalizeDegrees(float degrees)
{
    float normalized = std::fmod(degrees, 360.0f);
    if (normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    return normalized;
}

float Dot(const Vector3& lhs, const Vector3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vector3 Normalize(const Vector3& value)
{
    const float lengthSq = Dot(value, value);
    if (lengthSq <= kEpsilon)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

} // namespace

Camera::Camera()
{
    m_ViewMatrix       = MakeIdentity();
    m_ProjectionMatrix = MakeIdentity();
}

void Camera::SetPosition(Vector3 position)
{
    m_Position  = position;
    m_ViewDirty = true;
}

Vector3 Camera::GetPosition() const
{
    return m_Position;
}

void Camera::SetRotation(Vector3 rotationDegrees)
{
    rotationDegrees.x = NormalizeDegrees(rotationDegrees.x);
    rotationDegrees.y = NormalizeDegrees(rotationDegrees.y);
    rotationDegrees.z = NormalizeDegrees(rotationDegrees.z);

    m_RotationDegrees = rotationDegrees;
    m_ViewDirty       = true;
}

Vector3 Camera::GetRotation() const
{
    return m_RotationDegrees;
}

void Camera::SetFOV(float degrees)
{
    const float clampedDegrees = std::clamp(degrees, kMinFovDegrees, kMaxFovDegrees);
    if (clampedDegrees != degrees)
    {
        LX_ENGINE_WARN("[Camera] Invalid FOV {} clamped to {}", degrees, clampedDegrees);
    }

    m_FieldOfViewDegrees = clampedDegrees;
    m_ProjectionDirty    = true;
}

float Camera::GetFOV() const
{
    return m_FieldOfViewDegrees;
}

void Camera::SetNearFar(float nearPlane, float farPlane)
{
    float clampedNear = std::max(nearPlane, kMinNearPlane);
    float clampedFar  = std::max(farPlane, clampedNear + kMinDepthRange);

    if (clampedNear != nearPlane || clampedFar != farPlane)
    {
        LX_ENGINE_WARN("[Camera] Invalid near/far ({}, {}) clamped to ({}, {})", nearPlane, farPlane, clampedNear, clampedFar);
    }

    m_NearPlane       = clampedNear;
    m_FarPlane        = clampedFar;
    m_ProjectionDirty = true;
}

float Camera::GetNearPlane() const
{
    return m_NearPlane;
}

float Camera::GetFarPlane() const
{
    return m_FarPlane;
}

void Camera::UpdateViewMatrix()
{
    const float yawRadians   = DegreesToRadians(m_RotationDegrees.y);
    const float pitchRadians = DegreesToRadians(m_RotationDegrees.x);
    const float rollRadians  = DegreesToRadians(m_RotationDegrees.z);

    Vector3 forward = {
        std::cos(pitchRadians) * std::sin(yawRadians),
        std::sin(pitchRadians),
        std::cos(pitchRadians) * std::cos(yawRadians),
    };
    forward = Normalize(forward);

    Vector3 right = Normalize(Cross({0.0f, 1.0f, 0.0f}, forward));
    if (Dot(right, right) <= kEpsilon)
    {
        right = {1.0f, 0.0f, 0.0f};
    }

    Vector3 up = Normalize(Cross(forward, right));

    if (std::fabs(rollRadians) > kEpsilon)
    {
        const float   cosRoll  = std::cos(rollRadians);
        const float   sinRoll  = std::sin(rollRadians);
        const Vector3 oldRight = right;

        right = Normalize({
            oldRight.x * cosRoll + up.x * sinRoll,
            oldRight.y * cosRoll + up.y * sinRoll,
            oldRight.z * cosRoll + up.z * sinRoll,
        });
        up    = Normalize({
            up.x * cosRoll - oldRight.x * sinRoll,
            up.y * cosRoll - oldRight.y * sinRoll,
            up.z * cosRoll - oldRight.z * sinRoll,
        });
    }

    m_ViewMatrix      = {};
    m_ViewMatrix.m[0] = right.x;
    m_ViewMatrix.m[1] = up.x;
    m_ViewMatrix.m[2] = forward.x;
    m_ViewMatrix.m[3] = 0.0f;

    m_ViewMatrix.m[4] = right.y;
    m_ViewMatrix.m[5] = up.y;
    m_ViewMatrix.m[6] = forward.y;
    m_ViewMatrix.m[7] = 0.0f;

    m_ViewMatrix.m[8]  = right.z;
    m_ViewMatrix.m[9]  = up.z;
    m_ViewMatrix.m[10] = forward.z;
    m_ViewMatrix.m[11] = 0.0f;

    m_ViewMatrix.m[12] = -Dot(m_Position, right);
    m_ViewMatrix.m[13] = -Dot(m_Position, up);
    m_ViewMatrix.m[14] = -Dot(m_Position, forward);
    m_ViewMatrix.m[15] = 1.0f;

    m_ViewDirty = false;
    LX_ENGINE_INFO("[Camera] View matrix updated");
}

void Camera::UpdateProjectionMatrix(int viewportWidth, int viewportHeight)
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }

    m_AspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    const float fovRadians = DegreesToRadians(m_FieldOfViewDegrees);
    const float yScale     = 1.0f / std::tan(fovRadians * 0.5f);
    const float xScale     = yScale / m_AspectRatio;
    const float depthRange = m_FarPlane - m_NearPlane;

    m_ProjectionMatrix       = {};
    m_ProjectionMatrix.m[0]  = xScale;
    m_ProjectionMatrix.m[5]  = yScale;
    m_ProjectionMatrix.m[10] = m_FarPlane / depthRange;
    m_ProjectionMatrix.m[11] = 1.0f;
    m_ProjectionMatrix.m[14] = (-m_NearPlane * m_FarPlane) / depthRange;

    m_ProjectionDirty = false;
    LX_ENGINE_INFO("[Camera] Projection updated");
}

const Matrix4& Camera::GetViewMatrix() const
{
    return m_ViewMatrix;
}

const Matrix4& Camera::GetProjectionMatrix() const
{
    return m_ProjectionMatrix;
}

bool Camera::IsViewDirty() const
{
    return m_ViewDirty;
}

bool Camera::IsProjectionDirty() const
{
    return m_ProjectionDirty;
}

void Camera::SyncDirtyMatricesForRender(int viewportWidth, int viewportHeight)
{
    if (m_ViewDirty)
    {
        UpdateViewMatrix();
    }

    if (m_ProjectionDirty)
    {
        UpdateProjectionMatrix(viewportWidth, viewportHeight);
    }
}

} // namespace LXEngine
