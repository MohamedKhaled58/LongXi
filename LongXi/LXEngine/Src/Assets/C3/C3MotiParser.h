#pragma once

#include <vector>

#include "Assets/C3/Resources.h"

namespace LXEngine
{

class C3MotiParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, SkeletonResource& outSkeleton, AnimationClipResource& outClip);
};

} // namespace LXEngine
