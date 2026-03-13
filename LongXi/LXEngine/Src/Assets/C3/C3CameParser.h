#pragma once

#include <vector>

#include "Assets/C3/Resources.h"

namespace LXEngine
{

class C3CameParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, CameraPathResource& outCamera);
};

} // namespace LXEngine
