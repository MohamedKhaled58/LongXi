#include "Animation/AnimationPlayer.h"

#include <algorithm>
#include <cmath>

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

namespace
{

Matrix4 MakeIdentity()
{
    Matrix4 matrix = {};
    matrix.m[0]    = 1.0f;
    matrix.m[5]    = 1.0f;
    matrix.m[10]   = 1.0f;
    matrix.m[15]   = 1.0f;
    return matrix;
}

Matrix4 Multiply(const Matrix4& lhs, const Matrix4& rhs)
{
    Matrix4 result = {};
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

Matrix4 LerpMatrix(const Matrix4& a, const Matrix4& b, float t)
{
    Matrix4     result  = {};
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    for (int i = 0; i < 16; ++i)
    {
        result.m[i] = a.m[i] + (b.m[i] - a.m[i]) * clamped;
    }
    return result;
}

} // namespace

void AnimationPlayer::SetClip(const AnimationClip* clip)
{
    m_Clip              = clip;
    m_TimeSeconds       = 0.0f;
    m_HasLoggedMismatch = false;
    EnsureOutputSize();
}

void AnimationPlayer::SetSkeleton(const SkeletonResource* skeleton)
{
    m_Skeleton          = skeleton;
    m_TimeSeconds       = 0.0f;
    m_HasLoggedMismatch = false;
    EnsureOutputSize();
}

void AnimationPlayer::SetLooping(bool looping)
{
    m_Looping = looping;
}

void AnimationPlayer::Reset()
{
    m_TimeSeconds = 0.0f;
    FillIdentity();
}

void AnimationPlayer::Advance(float deltaSeconds)
{
    if (!m_Clip || !m_Skeleton || !m_Clip->IsValid() || m_Skeleton->boneCount == 0)
    {
        FillIdentity();
        return;
    }

    m_TimeSeconds += deltaSeconds;
    Sample();
}

bool AnimationPlayer::Sample()
{
    if (!m_Clip || !m_Skeleton || !m_Clip->IsValid() || m_Skeleton->boneCount == 0)
    {
        FillIdentity();
        return false;
    }

    if (m_Clip->GetBoneCount() != m_Skeleton->boneCount)
    {
        if (!m_HasLoggedMismatch)
        {
            LX_ENGINE_WARN("[AnimationPlayer] Bone count mismatch (clip={}, skeleton={})", m_Clip->GetBoneCount(), m_Skeleton->boneCount);
            m_HasLoggedMismatch = true;
        }
        FillIdentity();
        return false;
    }

    const float duration = m_Clip->GetDurationSeconds();
    if (duration <= 0.0f)
    {
        LX_ENGINE_WARN("[AnimationPlayer] Invalid clip duration");
        FillIdentity();
        return false;
    }

    float time = m_TimeSeconds;
    if (m_Looping)
    {
        time = std::fmod(time, duration);
        if (time < 0.0f)
        {
            time += duration;
        }
    }
    else
    {
        time = std::clamp(time, 0.0f, duration);
    }

    const uint32_t frameCount = std::max<uint32_t>(1, m_Clip->GetFrameCount());
    const float    framePos   = (frameCount > 1) ? (time / duration) * static_cast<float>(frameCount - 1) : 0.0f;

    const std::vector<uint32_t>& keyPositions = m_Clip->GetKeyframePositions();
    const size_t                 keyCount     = keyPositions.size();
    size_t                       lowerKey     = 0;
    size_t                       upperKey     = 0;
    float                        blend        = 0.0f;

    if (keyCount > 1)
    {
        size_t idx = 0;
        while (idx < keyCount && static_cast<float>(keyPositions[idx]) < framePos)
        {
            ++idx;
        }

        if (idx == 0)
        {
            lowerKey = 0;
            upperKey = 0;
        }
        else if (idx >= keyCount)
        {
            lowerKey = keyCount - 1;
            upperKey = keyCount - 1;
        }
        else
        {
            lowerKey = idx - 1;
            upperKey = idx;
        }

        const float lowerFrame = static_cast<float>(keyPositions[lowerKey]);
        const float upperFrame = static_cast<float>(keyPositions[upperKey]);
        const float span       = upperFrame - lowerFrame;
        if (span > 0.0f)
        {
            blend = (framePos - lowerFrame) / span;
        }
    }

    EnsureOutputSize();

    for (uint32_t boneIndex = 0; boneIndex < m_Skeleton->boneCount; ++boneIndex)
    {
        const Matrix4* a             = m_Clip->GetTransform(static_cast<uint32_t>(lowerKey), boneIndex);
        const Matrix4* b             = m_Clip->GetTransform(static_cast<uint32_t>(upperKey), boneIndex);
        const Matrix4  localA        = a ? *a : MakeIdentity();
        const Matrix4  localB        = b ? *b : MakeIdentity();
        m_LocalTransforms[boneIndex] = LerpMatrix(localA, localB, blend);
    }

    std::fill(m_Resolved.begin(), m_Resolved.end(), 0);
    for (uint32_t boneIndex = 0; boneIndex < m_Skeleton->boneCount; ++boneIndex)
    {
        ResolveModelSpace(boneIndex);
    }

    return true;
}

void AnimationPlayer::EnsureOutputSize()
{
    const uint32_t boneCount = m_Skeleton ? m_Skeleton->boneCount : 0;
    if (boneCount == 0)
    {
        m_LocalTransforms.clear();
        m_ModelTransforms.clear();
        m_Resolved.clear();
        return;
    }

    m_LocalTransforms.assign(boneCount, MakeIdentity());
    m_ModelTransforms.assign(boneCount, MakeIdentity());
    m_Resolved.assign(boneCount, 0);
}

void AnimationPlayer::FillIdentity()
{
    const uint32_t boneCount = m_Skeleton ? m_Skeleton->boneCount : 0;
    if (boneCount == 0)
    {
        m_ModelTransforms.clear();
        return;
    }

    m_ModelTransforms.assign(boneCount, MakeIdentity());
}

void AnimationPlayer::ResolveModelSpace(uint32_t boneIndex)
{
    if (boneIndex >= m_Resolved.size() || m_Resolved[boneIndex])
    {
        return;
    }

    const int32_t parentIndex = (m_Skeleton && boneIndex < m_Skeleton->parentIndices.size()) ? m_Skeleton->parentIndices[boneIndex] : -1;

    if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < m_ModelTransforms.size())
    {
        ResolveModelSpace(static_cast<uint32_t>(parentIndex));
        m_ModelTransforms[boneIndex] = Multiply(m_ModelTransforms[parentIndex], m_LocalTransforms[boneIndex]);
    }
    else
    {
        m_ModelTransforms[boneIndex] = m_LocalTransforms[boneIndex];
    }

    m_Resolved[boneIndex] = 1;
}

} // namespace LongXi
