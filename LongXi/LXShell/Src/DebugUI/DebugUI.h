#pragma once

#if defined(LX_DEBUG) || defined(LX_DEV)

#include <chrono>
#include <string>
#include <vector>

namespace LongXi
{

class Engine;
class Camera;
class SceneNode;

struct EngineMetricsSnapshot
{
    float FramesPerSecond = 0.0f;
    float FrameTimeMs = 0.0f;
    int DrawCallCount = 0;
    std::string GpuDeviceName = "Unknown";
};

struct SceneNodeViewModel
{
    SceneNode* Node = nullptr;
    std::string NodeName;
    int Depth = 0;
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Rotation[3] = {0.0f, 0.0f, 0.0f};
    float Scale[3] = {1.0f, 1.0f, 1.0f};
};

struct TextureInfoViewModel
{
    std::string TextureName;
    int Width = 0;
    int Height = 0;
    size_t MemoryBytes = 0;
};

struct CameraStateViewModel
{
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float RotationDegrees[3] = {0.0f, 0.0f, 0.0f};
    float FieldOfView = 45.0f;
    float NearPlane = 1.0f;
    float FarPlane = 10000.0f;
};

struct InputStateViewModel
{
    int MouseX = 0;
    int MouseY = 0;
    int WheelDelta = 0;
    bool LeftButtonDown = false;
    bool RightButtonDown = false;
    bool MiddleButtonDown = false;
    bool ConsumedByDebugUI = false;
    std::vector<std::string> PressedKeys;
};

class DebugUI
{
  public:
    void UpdateViewModels(Engine& engine);
    void RenderPanels(Engine& engine);
    void SetLastInputConsumedByDebugUI(bool consumed);

  private:
    EngineMetricsSnapshot m_EngineMetrics;
    std::vector<SceneNodeViewModel> m_SceneNodes;
    std::vector<TextureInfoViewModel> m_Textures;
    CameraStateViewModel m_CameraState;
    InputStateViewModel m_InputState;

    bool m_ShowEnginePanel = true;
    bool m_ShowSceneInspector = true;
    bool m_ShowTextureViewer = true;
    bool m_ShowCameraPanel = true;
    bool m_ShowInputMonitor = true;
    bool m_LastInputConsumedByDebugUI = false;
    std::chrono::steady_clock::time_point m_LastMetricsSampleTime = {};
};

} // namespace LongXi

#endif // defined(LX_DEBUG) || defined(LX_DEV)
