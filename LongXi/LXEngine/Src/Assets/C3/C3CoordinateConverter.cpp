#include "Assets/C3/C3CoordinateConverter.h"

#include <cmath>

namespace LXEngine
{

static LXCore::Matrix4 Multiply(const LXCore::Matrix4& lhs, const LXCore::Matrix4& rhs)
{
    LXCore::Matrix4 result = {};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += lhs.m[row * 4 + k] * rhs.m[k * 4 + col];
            }
            result.m[row * 4 + col] = sum;
        }
    }
    return result;
}

static LXCore::Matrix4 Transpose(const LXCore::Matrix4& matrix)
{
    LXCore::Matrix4 result = {};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            result.m[row * 4 + col] = matrix.m[col * 4 + row];
        }
    }
    return result;
}

static LXCore::Matrix4 MakeAxisFix()
{
    // C3 format: -Z is UP, X is right, Y is forward
    // Engine format: Y is UP, X is right, -Z is forward
    // Conversion: Rotate -90° around X axis (270°)
    // (x, y, z) -> (x, -z, y) which is: X=X, Y=-Z, Z=Y
    LXCore::Matrix4 matrix = {};
    matrix.m[0]  = 1.0f;   // X
    matrix.m[6]  = 1.0f;   // Y component from Z
    matrix.m[9]  = -1.0f;  // Z component from -Y
    matrix.m[15] = 1.0f;
    return matrix;
}

static const LXCore::Matrix4 kAxisFix    = MakeAxisFix();
static const LXCore::Matrix4 kAxisFixInv = Transpose(kAxisFix);

static LXCore::Vector3 ConvertVector(const LXCore::Vector3& value)
{
    // C3: -Z is UP -> Engine: Y is UP
    // (x, y, z) -> (x, -z, y)
    return {value.x, -value.z, value.y};
}

static LXCore::Vector3 Normalize(const LXCore::Vector3& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSq <= 0.000001f)
    {
        return {0.0f, 0.0f, 0.0f};
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

static LXCore::Matrix4 ConvertMatrix(const LXCore::Matrix4& matrix)
{
    return Multiply(Multiply(kAxisFixInv, matrix), kAxisFix);
}

void C3CoordinateConverter::ConvertToEngineSpace(C3LoadResult& result)
{
    for (auto& mesh : result.meshes)
    {
        for (auto& vertex : mesh.vertices)
        {
            vertex.position = ConvertVector(vertex.position);
            if (vertex.hasNormal)
            {
                vertex.normal = Normalize(ConvertVector(vertex.normal));
            }
        }

        for (auto& morph : mesh.morphTargets)
        {
            for (auto& position : morph.positions)
            {
                position = ConvertVector(position);
            }
        }

        mesh.initMatrix = ConvertMatrix(mesh.initMatrix);
    }

    for (auto& skeleton : result.skeletons)
    {
        for (auto& bindPose : skeleton.bindPose)
        {
            bindPose = ConvertMatrix(bindPose);
        }
    }

    for (auto& clip : result.animations)
    {
        for (auto& transform : clip.boneTransforms)
        {
            transform = ConvertMatrix(transform);
        }
    }

    for (auto& particle : result.particles)
    {
        for (auto& frame : particle.frames)
        {
            for (auto& position : frame.positions)
            {
                position = ConvertVector(position);
            }
            frame.matrix = ConvertMatrix(frame.matrix);
        }
    }

    for (auto& camera : result.cameras)
    {
        for (auto& keyframe : camera.keyframes)
        {
            keyframe.position = ConvertVector(keyframe.position);
            keyframe.target   = ConvertVector(keyframe.target);
        }
    }
}

} // namespace LXEngine
