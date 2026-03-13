#pragma once

#include <cstdint>
#include <vector>

#include "Assets/C3/Resources.h"

namespace LXEngine
{

class C3PtclParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, ParticleResource& outParticle, bool isPtcl3);
};

} // namespace LXEngine
