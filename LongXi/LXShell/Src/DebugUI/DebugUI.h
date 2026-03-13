#pragma once

#if defined(LX_DEBUG) || defined(LX_DEV)

#include <cstdint>
#include <string>
#include <vector>

namespace LXEngine
{
class Engine;
class Camera;
class SceneNode;
} // namespace LXEngine

namespace LXShell
{

using LXEngine::Camera;
using LXEngine::Engine;
using LXEngine::SceneNode;

struct EngineMetricsSnapshot
{
    uint64_t    FrameIndex        = 0;
    float       FramesPerSecond   = 0.0f;
    float       DeltaTimeMs       = 0.0f;
    float       FrameTimeMs       = 0.0f;
    int         ProfileScopeCount = 0;
    int         DrawCallCount     = 0;
    int         MapVisibleTiles   = 0;
    int         MapVisibleObjects = 0;
    int         MapAnimatedTiles  = 0;
    std::string GpuDeviceName     = "Unknown";
};

struct SceneNodeViewModel
{
    SceneNode*  Node = nullptr;
    std::string NodeName;
    int         Depth       = 0;
    float       Position[3] = {0.0f, 0.0f, 0.0f};
    float       Rotation[3] = {0.0f, 0.0f, 0.0f};
    float       Scale[3]    = {1.0f, 1.0f, 1.0f};
};

struct TextureInfoViewModel
{
    std::string TextureName;
    int         Width       = 0;
    int         Height      = 0;
    size_t      MemoryBytes = 0;
};

struct CameraStateViewModel
{
    float Position[3]        = {0.0f, 0.0f, 0.0f};
    float RotationDegrees[3] = {0.0f, 0.0f, 0.0f};
    float FieldOfView        = 45.0f;
    float NearPlane          = 1.0f;
    float FarPlane           = 10000.0f;
};

struct InputStateViewModel
{
    int                      MouseX            = 0;
    int                      MouseY            = 0;
    int                      WheelDelta        = 0;
    bool                     LeftButtonDown    = false;
    bool                     RightButtonDown   = false;
    bool                     MiddleButtonDown  = false;
    bool                     ConsumedByDebugUI = false;
    std::vector<std::string> PressedKeys;
};

struct ProfilerEntryViewModel
{
    std::string ScopeName;
    float       DurationMs = 0.0f;
    uint32_t    CallCount  = 0;
};

struct ProfilerPanelViewModel
{
    bool                                ProfilingEnabled  = false;
    bool                                IsDataStale       = false;
    uint64_t                            TimingFrameIndex  = 0;
    uint64_t                            ProfileFrameIndex = 0;
    float                               DeltaTimeMs       = 0.0f;
    float                               FrameTimeMs       = 0.0f;
    float                               TotalTimeSeconds  = 0.0f;
    std::vector<ProfilerEntryViewModel> Entries;
};

class DebugUI
{
public:
    void UpdateViewModels(Engine& engine);
    void RenderPanels(Engine& engine);
    void SetLastInputConsumedByDebugUI(bool consumed);
    void SetProfilerPanelVisible(bool visible);
    void ToggleProfilerPanel();

private:
    EngineMetricsSnapshot             m_EngineMetrics;
    std::vector<SceneNodeViewModel>   m_SceneNodes;
    std::vector<TextureInfoViewModel> m_Textures;
    CameraStateViewModel              m_CameraState;
    InputStateViewModel               m_InputState;
    ProfilerPanelViewModel            m_ProfilerPanel;

    bool m_ShowEnginePanel            = true;
    bool m_ShowSceneInspector         = true;
    bool m_ShowTextureViewer          = true;
    bool m_ShowCameraPanel            = true;
    bool m_ShowInputMonitor           = true;
    bool m_ShowProfilerPanel          = true;
    bool m_ShowC3AssetViewer          = true;
    bool m_WasC3AssetViewerVisible    = false;
    bool m_LastInputConsumedByDebugUI = false;
};

} // namespace LXShell

#endif // defined(LX_DEBUG) || defined(LX_DEV)
