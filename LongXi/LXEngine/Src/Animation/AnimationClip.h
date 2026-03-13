#pragma once

#include <cstdint>
#include <vector>

#include "Assets/C3/Resources.h"
#include "Core/Math/Math.h"

namespace LXEngine
{


class AnimationClip
{
public:
    bool Initialize(const AnimationClipResource& resource);

    bool IsValid() const
    {
        return m_BoneCount > 0 && !m_KeyframePositions.empty() && !m_BoneTransforms.empty();
    }

    uint32_t GetFrameCount() const
    {
        return m_FrameCount;
    }

    uint32_t GetBoneCount() const
    {
        return m_BoneCount;
    }

    uint32_t GetKeyframeCount() const
    {
        return static_cast<uint32_t>(m_KeyframePositions.size());
    }

    float GetDurationSeconds() const
    {
        return m_DurationSeconds;
    }

    const std::vector<uint32_t>& GetKeyframePositions() const
    {
        return m_KeyframePositions;
    }

    const LXCore::Matrix4* GetTransform(uint32_t keyIndex, uint32_t boneIndex) const;

private:
    uint32_t                     m_FrameCount      = 0;
    uint32_t                     m_BoneCount       = 0;
    float                        m_DurationSeconds = 0.0f;
    std::vector<uint32_t>        m_KeyframePositions;
    std::vector<LXCore::Matrix4> m_BoneTransforms;
};

} // namespace LXEngine
