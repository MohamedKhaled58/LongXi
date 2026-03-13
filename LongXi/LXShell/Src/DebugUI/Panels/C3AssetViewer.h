#pragma once

#include "../DebugUI.h"

class LXEngine::Engine;

namespace LXShell
{
class C3AssetViewer
{
public:
    static void Render(Engine& engine);
    static void ReleaseResources(Engine& engine);
};

} // namespace LXShell
