#pragma once

#include <vector>

#include "Resource/Resources.h"

namespace LongXi
{

class C3CameParser
{
public:
    static bool Parse(const std::vector<uint8_t>& data, CameraPathResource& outCamera);
};

} // namespace LongXi
