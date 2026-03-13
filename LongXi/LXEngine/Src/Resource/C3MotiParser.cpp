#include "Resource/C3MotiParser.h"

#include <cstring>

#include "Resource/C3BinaryReader.h"
#include "Resource/C3Log.h"
#include "Resource/C3Types.h"

namespace LongXi
{

namespace
{
#pragma pack(push, 1)

struct ZKeyBoneData
{
    float rotation[4];
    float translation[3];
};

#pragma pack(pop)
static_assert(sizeof(ZKeyBoneData) == 28, "ZKeyBoneData must be 28 bytes");

Matrix4 IdentityMatrix()
{
    Matrix4 m{};
    m.m[0]  = 1.0f;
    m.m[5]  = 1.0f;
    m.m[10] = 1.0f;
    m.m[15] = 1.0f;
    return m;
}

Matrix4 QuaternionTranslationToMatrix(const float q[4], const float t[3])
{
    const float x = q[0];
    const float y = q[1];
    const float z = q[2];
    const float w = q[3];

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    Matrix4 m{};
    m.m[0] = 1.0f - 2.0f * (yy + zz);
    m.m[1] = 2.0f * (xy + wz);
    m.m[2] = 2.0f * (xz - wy);
    m.m[3] = 0.0f;

    m.m[4] = 2.0f * (xy - wz);
    m.m[5] = 1.0f - 2.0f * (xx + zz);
    m.m[6] = 2.0f * (yz + wx);
    m.m[7] = 0.0f;

    m.m[8]  = 2.0f * (xz + wy);
    m.m[9]  = 2.0f * (yz - wx);
    m.m[10] = 1.0f - 2.0f * (xx + yy);
    m.m[11] = 0.0f;

    m.m[12] = t[0];
    m.m[13] = t[1];
    m.m[14] = t[2];
    m.m[15] = 1.0f;

    return m;
}

} // namespace

bool C3MotiParser::Parse(const std::vector<uint8_t>& data, SkeletonResource& outSkeleton, AnimationClipResource& outClip)
{
    outSkeleton = SkeletonResource{};
    outClip     = AnimationClipResource{};

    C3BinaryReader reader(data);

    uint32_t boneCount  = 0;
    uint32_t frameCount = 0;
    if (!reader.Read(boneCount) || !reader.Read(frameCount))
    {
        LX_C3_WARN("MOTI: Failed to read bone/frame count");
        return false;
    }

    outSkeleton.boneCount = boneCount;
    outSkeleton.parentIndices.assign(boneCount, -1);
    outSkeleton.bindPose.assign(boneCount, IdentityMatrix());

    char marker[4] = {0, 0, 0, 0};
    if (!reader.ReadBytes(4, reinterpret_cast<uint8_t*>(marker)))
    {
        LX_C3_WARN("MOTI: Failed to read keyframe marker");
        return false;
    }

    uint32_t keyCount = 0;
    if (!reader.Read(keyCount))
    {
        LX_C3_WARN("MOTI: Failed to read keyframe count");
        return false;
    }

    outClip.frameCount      = frameCount;
    outClip.boneCount       = boneCount;
    outClip.durationSeconds = static_cast<float>(frameCount);
    outClip.keyframePositions.reserve(keyCount);
    outClip.boneTransforms.reserve(static_cast<size_t>(keyCount) * boneCount);

    const FourCC markerId = MakeFourCC(marker[0], marker[1], marker[2], marker[3]);

    if (markerId == C3ChunkKKey)
    {
        for (uint32_t k = 0; k < keyCount; ++k)
        {
            uint32_t framePos = 0;
            if (!reader.Read(framePos))
            {
                LX_C3_WARN("MOTI: Failed to read KKEY frame position");
                return false;
            }
            outClip.keyframePositions.push_back(framePos);

            for (uint32_t b = 0; b < boneCount; ++b)
            {
                Matrix4 matrix{};
                if (!reader.Read(matrix))
                {
                    LX_C3_WARN("MOTI: Failed to read KKEY matrix (frame={}, bone={})", framePos, b);
                    return false;
                }
                outClip.boneTransforms.push_back(matrix);
            }
        }
    }
    else if (markerId == C3ChunkZKey)
    {
        for (uint32_t k = 0; k < keyCount; ++k)
        {
            uint16_t framePos = 0;
            if (!reader.Read(framePos))
            {
                LX_C3_WARN("MOTI: Failed to read ZKEY frame position");
                return false;
            }
            outClip.keyframePositions.push_back(framePos);

            for (uint32_t b = 0; b < boneCount; ++b)
            {
                ZKeyBoneData zdata{};
                if (!reader.Read(zdata))
                {
                    LX_C3_WARN("MOTI: Failed to read ZKEY bone data (frame={}, bone={})", framePos, b);
                    return false;
                }
                Matrix4 matrix = QuaternionTranslationToMatrix(zdata.rotation, zdata.translation);
                outClip.boneTransforms.push_back(matrix);
            }
        }
    }
    else
    {
        LX_C3_WARN("MOTI: Unknown keyframe marker '{}{}{}{}'", marker[0], marker[1], marker[2], marker[3]);
        return false;
    }

    if (boneCount > 0 && !outClip.boneTransforms.empty())
    {
        for (uint32_t b = 0; b < boneCount; ++b)
        {
            outSkeleton.bindPose[b] = outClip.boneTransforms[b];
        }
    }

    uint32_t morphCount = 0;
    if (!reader.Read(morphCount))
    {
        LX_C3_WARN("MOTI: Failed to read morph count");
        return false;
    }

    outClip.morphCount = morphCount;
    if (morphCount > 0)
    {
        const size_t morphTotal = static_cast<size_t>(morphCount) * frameCount;
        outClip.morphWeights.resize(morphTotal);
        if (!reader.ReadBytes(morphTotal * sizeof(float), reinterpret_cast<uint8_t*>(outClip.morphWeights.data())))
        {
            LX_C3_WARN("MOTI: Failed to read morph weights");
            return false;
        }
    }

    return true;
}

} // namespace LongXi
