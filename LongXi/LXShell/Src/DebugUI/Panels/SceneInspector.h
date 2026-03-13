#pragma once

#include <vector>

#include "../DebugUI.h"

namespace LXShell
{

class Scene;

class SceneInspector
{
public:
    static void       Render(const std::vector<SceneNodeViewModel>& nodes, class Scene& scene);
    static SceneNode* s_SelectedNode;
};

} // namespace LXShell
