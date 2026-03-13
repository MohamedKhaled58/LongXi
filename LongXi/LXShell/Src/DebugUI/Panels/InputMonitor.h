#pragma once

#include "../DebugUI.h"

namespace LXShell
{

class InputMonitor
{
public:
    static void Render(const InputStateViewModel& inputState);
};

} // namespace LXShell
