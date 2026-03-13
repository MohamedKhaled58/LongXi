#pragma once

#include <cstdint>
#include <vector>

#include "Resource/Resources.h"

namespace LongXi
{

class C3PhyParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, MeshResource& outMesh, bool isPhy4);
    static void ValidateBoneInfluences(MeshResource& mesh, uint32_t boneCount);
};

} // namespace LongXi
