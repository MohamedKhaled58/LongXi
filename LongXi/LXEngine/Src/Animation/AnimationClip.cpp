#include "Animation/AnimationClip.h"

#include <algorithm>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

bool AnimationClip::Initialize(const AnimationClipResource& resource)
{
    m_FrameCount        = resource.frameCount;
    m_BoneCount         = resource.boneCount;
    m_DurationSeconds   = resource.durationSeconds;
    m_BoneTransforms    = resource.boneTransforms;
    m_KeyframePositions = resource.keyframePositions;

    if (m_BoneCount == 0 || m_BoneTransforms.empty())
    {
        LX_ENGINE_WARN("[AnimationClip] Invalid resource (boneCount={}, transforms={})", m_BoneCount, m_BoneTransforms.size());
        return false;
    }

    const size_t computedKeyCount = m_BoneTransforms.size() / m_BoneCount;
    if (computedKeyCount == 0)
    {
        LX_ENGINE_WARN("[AnimationClip] No keyframe data available");
        m_BoneTransforms.clear();
        m_KeyframePositions.clear();
        return false;
    }
    if (m_BoneTransforms.size() != computedKeyCount * m_BoneCount)
    {
        const size_t trimmedSize = computedKeyCount * m_BoneCount;
        if (trimmedSize != m_BoneTransforms.size())
        {
            m_BoneTransforms.resize(trimmedSize);
            LX_ENGINE_WARN("[AnimationClip] Trimmed bone transform data to {} entries", trimmedSize);
        }
    }

    if (m_KeyframePositions.empty())
    {
        m_KeyframePositions.resize(computedKeyCount);
        for (size_t i = 0; i < computedKeyCount; ++i)
        {
            m_KeyframePositions[i] = static_cast<uint32_t>(i);
        }
    }
    else if (m_KeyframePositions.size() != computedKeyCount)
    {
        const size_t trimmedKeys = std::min(m_KeyframePositions.size(), computedKeyCount);
        if (trimmedKeys != m_KeyframePositions.size())
        {
            m_KeyframePositions.resize(trimmedKeys);
        }
        if (trimmedKeys != computedKeyCount)
        {
            m_BoneTransforms.resize(trimmedKeys * m_BoneCount);
        }
        LX_ENGINE_WARN("[AnimationClip] Keyframe count mismatch, clamped to {} keys", trimmedKeys);
    }

    if (m_FrameCount == 0 && !m_KeyframePositions.empty())
    {
        m_FrameCount = static_cast<uint32_t>(m_KeyframePositions.back() + 1u);
    }

    if (m_DurationSeconds <= 0.0f)
    {
        m_DurationSeconds = static_cast<float>(m_FrameCount);
    }

    return IsValid();
}

const LXCore::Matrix4* AnimationClip::GetTransform(uint32_t keyIndex, uint32_t boneIndex) const
{
    if (m_BoneCount == 0 || keyIndex >= m_KeyframePositions.size() || boneIndex >= m_BoneCount)
    {
        return nullptr;
    }

    const size_t index = static_cast<size_t>(keyIndex) * m_BoneCount + boneIndex;
    if (index >= m_BoneTransforms.size())
    {
        return nullptr;
    }

    return &m_BoneTransforms[index];
}

} // namespace LXEngine
