#pragma once

#include <vector>

#include "../DebugUI.h"

namespace LXShell
{

class TextureViewer
{
public:
    static void Render(const std::vector<TextureInfoViewModel>& textures);
};

} // namespace LXShell
