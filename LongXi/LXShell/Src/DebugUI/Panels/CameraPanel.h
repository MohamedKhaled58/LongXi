#pragma once

#include "../DebugUI.h"

namespace LXEngine
{
class Camera;
}

namespace LXShell
{

using LXEngine::Camera;

class CameraPanel
{
public:
    static void Render(const CameraStateViewModel& cameraState, Camera& camera);
};

} // namespace LXShell
