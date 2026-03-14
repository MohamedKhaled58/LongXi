#pragma once

#include <vector>

#include "Animation/AnimationClip.h"
#include "Assets/C3/Resources.h"
#include "Core/Math/Math.h"

namespace LXEngine
{

class AnimationPlayer
{
public:
    void SetClip(const AnimationClip* clip);
    void SetSkeleton(const SkeletonResource* skeleton);
    void SetLooping(bool looping);
    void Reset();
    void SetUseDirectMatrices(bool directMatrices);

    void Advance(float deltaSeconds);
    bool Sample();

    float GetTimeSeconds() const
    {
        return m_TimeSeconds;
    }

    const std::vector<LXCore::Matrix4>& GetModelSpaceTransforms() const
    {
        return m_ModelTransforms;
    }

    const std::vector<LXCore::Matrix4>& GetFinalBoneMatrices() const
    {
        return m_FinalBoneMatrices;
    }

private:
    void EnsureOutputSize();
    void FillIdentity();
    void ResolveModelSpace(uint32_t boneIndex);

private:
    const AnimationClip*    m_Clip              = nullptr;
    const SkeletonResource* m_Skeleton          = nullptr;
    bool                    m_Looping           = true;
    bool                    m_UseDirectMatrices = false;
    float                   m_TimeSeconds       = 0.0f;
    bool                    m_HasLoggedMismatch = false;

    std::vector<LXCore::Matrix4> m_LocalTransforms;
    std::vector<LXCore::Matrix4> m_ModelTransforms;
    std::vector<LXCore::Matrix4> m_InverseBindPose;
    std::vector<LXCore::Matrix4> m_FinalBoneMatrices;
    std::vector<uint8_t>         m_Resolved;
};

} // namespace LXEngine
