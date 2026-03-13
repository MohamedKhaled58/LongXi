#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/Math/Math.h"

namespace LXEngine
{

struct BoneInfluence
{
    std::array<uint32_t, 4> indices{0, 0, 0, 0};
    std::array<float, 4>    weights{0.0f, 0.0f, 0.0f, 0.0f};
};

struct Vertex
{
    LXCore::Vector3 position{0.0f, 0.0f, 0.0f};
    LXCore::Vector3 normal{0.0f, 0.0f, 0.0f};
    LXCore::Vector2 uv{0.0f, 0.0f};
    BoneInfluence   skinning{};
    bool            hasNormal = false;
};

struct MorphTargetSet
{
    std::vector<LXCore::Vector3> positions;
};

struct MeshSubset
{
    uint32_t startIndex    = 0;
    uint32_t indexCount    = 0;
    bool     isTransparent = false;
};

struct MeshResource
{
    std::vector<Vertex>         vertices;
    std::vector<uint32_t>       indices;
    std::vector<MorphTargetSet> morphTargets;
    std::string                 name;
    std::string                 textureName;
    std::vector<MeshSubset>     subsets;
};

struct SkeletonResource
{
    uint32_t                     boneCount = 0;
    std::vector<int32_t>         parentIndices;
    std::vector<LXCore::Matrix4> bindPose;
};

struct AnimationClipResource
{
    uint32_t                     frameCount      = 0;
    uint32_t                     boneCount       = 0;
    float                        durationSeconds = 0.0f;
    std::vector<uint32_t>        keyframePositions;
    std::vector<LXCore::Matrix4> boneTransforms;
    uint32_t                     morphCount = 0;
    std::vector<float>           morphWeights;

    const LXCore::Matrix4* GetTransform(uint32_t frameIndex, uint32_t boneIndex) const
    {
        if (boneCount == 0)
        {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(frameIndex) * boneCount + boneIndex;
        if (index >= boneTransforms.size())
        {
            return nullptr;
        }
        return &boneTransforms[index];
    }
};

struct ParticleFrame
{
    uint32_t                     count = 0;
    std::vector<uint16_t>        indices;
    std::vector<LXCore::Vector3> positions;
    std::vector<float>           ages;
    std::vector<float>           sizes;
    LXCore::Matrix4              matrix{};
};

struct ParticleResource
{
    std::string                name;
    std::string                textureName;
    uint32_t                   rowCount     = 1;
    uint32_t                   maxParticles = 0;
    uint32_t                   frameCount   = 0;
    std::vector<ParticleFrame> frames;

    bool    isPtcl3       = false;
    uint8_t behavior1     = 0;
    uint8_t behavior2     = 0;
    float   scaleX        = 1.0f;
    float   scaleY        = 1.0f;
    float   scaleZ        = 1.0f;
    float   rotationSpeed = 0.0f;
    float   maxAlpha      = 1.0f;
    float   minAlpha      = 0.0f;
    float   totalLifetime = 1.0f;
    float   fadeStartAge  = 0.1f;

    std::vector<uint8_t> rawData;
};

struct CameraKeyframe
{
    float           time = 0.0f;
    LXCore::Vector3 position{0.0f, 0.0f, 0.0f};
    LXCore::Vector3 target{0.0f, 0.0f, 0.0f};
    float           fov = 0.0f;
};

struct CameraPathResource
{
    std::string                 name;
    float                       fov = 0.0f;
    std::vector<CameraKeyframe> keyframes;
    std::vector<uint8_t>        rawData;
};

} // namespace LXEngine
