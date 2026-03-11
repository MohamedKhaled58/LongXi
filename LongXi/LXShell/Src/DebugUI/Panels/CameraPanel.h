#pragma once

#include "../DebugUI.h"

namespace LongXi
{

class Camera;

class CameraPanel
{
  public:
    static void Render(const CameraStateViewModel& cameraState, Camera& camera);
};

} // namespace LongXi
