#pragma once

#include "../DebugUI.h"

namespace LXShell
{

class EnginePanel
{
public:
    static void Render(const EngineMetricsSnapshot& metrics);
};

} // namespace LXShell
