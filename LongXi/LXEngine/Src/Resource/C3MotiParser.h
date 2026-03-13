#pragma once

#include <vector>

#include "Resource/Resources.h"

namespace LongXi
{

class C3MotiParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, SkeletonResource& outSkeleton, AnimationClipResource& outClip);
};

} // namespace LongXi
