#pragma once

#include <vector>

#include "Animation/AnimationClip.h"
#include "Math/Math.h"
#include "Resource/Resources.h"

namespace LongXi
{

class AnimationPlayer
{
public:
    void SetClip(const AnimationClip* clip);
    void SetSkeleton(const SkeletonResource* skeleton);
    void SetLooping(bool looping);
    void Reset();

    void Advance(float deltaSeconds);
    bool Sample();

    float GetTimeSeconds() const
    {
        return m_TimeSeconds;
    }

    const std::vector<Matrix4>& GetModelSpaceTransforms() const
    {
        return m_ModelTransforms;
    }

private:
    void EnsureOutputSize();
    void FillIdentity();
    void ResolveModelSpace(uint32_t boneIndex);

private:
    const AnimationClip*    m_Clip              = nullptr;
    const SkeletonResource* m_Skeleton          = nullptr;
    bool                    m_Looping           = true;
    float                   m_TimeSeconds       = 0.0f;
    bool                    m_HasLoggedMismatch = false;

    std::vector<Matrix4> m_LocalTransforms;
    std::vector<Matrix4> m_ModelTransforms;
    std::vector<uint8_t> m_Resolved;
};

} // namespace LongXi
