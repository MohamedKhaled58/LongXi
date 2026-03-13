#pragma once

#include <cstdint>
#include <vector>

#include "Resource/Resources.h"

namespace LongXi
{

class C3PtclParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, ParticleResource& outParticle, bool isPtcl3);
};

} // namespace LongXi
