#pragma once

#include <cstdint>
#include <vector>

#include "Assets/C3/Resources.h"

namespace LXEngine
{

class C3PhyParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, MeshResource& outMesh, bool isPhy4);
    static void ValidateBoneInfluences(MeshResource& mesh, uint32_t boneCount);
};

} // namespace LXEngine
