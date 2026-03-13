#pragma once

#include <vector>

#include "../DebugUI.h"

namespace LXEngine
{
class Scene;
}

namespace LXShell
{

using LXEngine::Scene;

class SceneInspector
{
public:
    static void       Render(const std::vector<SceneNodeViewModel>& nodes, class Scene& scene);
    static SceneNode* s_SelectedNode;
};

} // namespace LXShell
