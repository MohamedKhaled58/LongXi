#pragma once

#include <vector>

#include "../DebugUI.h"

namespace LongXi
{

class TextureViewer
{
public:
    static void Render(const std::vector<TextureInfoViewModel>& textures);
};

} // namespace LongXi
