#include "DebugUI.h"

#if defined(LX_DEBUG) || defined(LX_DEV)

#include "Panels/CameraPanel.h"
#include "Panels/EnginePanel.h"
#include "Panels/InputMonitor.h"
#include "Panels/ProfilerPanel.h"
#include "Panels/SceneInspector.h"
#include "Panels/TextureViewer.h"

#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>
#include <Input/InputSystem.h>
#include <Scene/Camera.h>
#include <Scene/Scene.h>
#include <Texture/Texture.h>
#include <Texture/TextureFormat.h>
#include <Texture/TextureManager.h>

#include <algorithm>

namespace LongXi
{

namespace
{

size_t EstimateTextureMemoryBytes(const Texture& texture)
{
    const size_t width = static_cast<size_t>(texture.GetWidth());
    const size_t height = static_cast<size_t>(texture.GetHeight());

    switch (texture.GetFormat())
    {
        case TextureFormat::RGBA8:
            return width * height * 4u;
        case TextureFormat::DXT1:
        {
            const size_t blocksWide = std::max<size_t>(1, (width + 3u) / 4u);
            const size_t blocksHigh = std::max<size_t>(1, (height + 3u) / 4u);
            return blocksWide * blocksHigh * 8u;
        }
        case TextureFormat::DXT3:
        case TextureFormat::DXT5:
        {
            const size_t blocksWide = std::max<size_t>(1, (width + 3u) / 4u);
            const size_t blocksHigh = std::max<size_t>(1, (height + 3u) / 4u);
            return blocksWide * blocksHigh * 16u;
        }
        default:
            return 0;
    }
}

std::string KeyToString(Key key)
{
    const uint8_t raw = static_cast<uint8_t>(key);
    const uint8_t a = static_cast<uint8_t>(Key::A);
    const uint8_t z = static_cast<uint8_t>(Key::Z);
    if (raw >= a && raw <= z)
    {
        const char c = static_cast<char>('A' + (raw - a));
        return std::string(1, c);
    }

    const uint8_t d0 = static_cast<uint8_t>(Key::D0);
    const uint8_t d9 = static_cast<uint8_t>(Key::D9);
    if (raw >= d0 && raw <= d9)
    {
        const char c = static_cast<char>('0' + (raw - d0));
        return std::string(1, c);
    }

    const uint8_t f1 = static_cast<uint8_t>(Key::F1);
    const uint8_t f12 = static_cast<uint8_t>(Key::F12);
    if (raw >= f1 && raw <= f12)
    {
        const int functionIndex = static_cast<int>(raw - f1 + 1);
        return "F" + std::to_string(functionIndex);
    }

    switch (key)
    {
        case Key::Up:
            return "Up";
        case Key::Down:
            return "Down";
        case Key::Left:
            return "Left";
        case Key::Right:
            return "Right";
        case Key::Escape:
            return "Escape";
        case Key::Enter:
            return "Enter";
        case Key::Space:
            return "Space";
        case Key::Tab:
            return "Tab";
        case Key::Backspace:
            return "Backspace";
        case Key::LShift:
            return "LShift";
        case Key::RShift:
            return "RShift";
        case Key::LControl:
            return "LCtrl";
        case Key::RControl:
            return "RCtrl";
        case Key::LAlt:
            return "LAlt";
        case Key::RAlt:
            return "RAlt";
        default:
            return "Other";
    }
}

} // namespace

void DebugUI::UpdateViewModels(Engine& engine)
{
    const TimingSnapshot& timingSnapshot = engine.GetTimingSnapshot();
    m_EngineMetrics.FrameIndex = timingSnapshot.FrameIndex;
    m_EngineMetrics.DeltaTimeMs = static_cast<float>(timingSnapshot.DeltaTimeSeconds * 1000.0);
    m_EngineMetrics.FrameTimeMs = static_cast<float>(timingSnapshot.FrameTimeSeconds * 1000.0);
    m_EngineMetrics.FramesPerSecond = timingSnapshot.DeltaTimeSeconds > 0.0 ? static_cast<float>(1.0 / timingSnapshot.DeltaTimeSeconds) : 0.0f;
    m_EngineMetrics.DrawCallCount = 0;
    m_EngineMetrics.GpuDeviceName = "Renderer API (DX11 backend)";

    m_ProfilerPanel.ProfilingEnabled = engine.IsProfilingEnabled();
    m_ProfilerPanel.TimingFrameIndex = timingSnapshot.FrameIndex;
    m_ProfilerPanel.DeltaTimeMs = static_cast<float>(timingSnapshot.DeltaTimeSeconds * 1000.0);
    m_ProfilerPanel.FrameTimeMs = static_cast<float>(timingSnapshot.FrameTimeSeconds * 1000.0);
    m_ProfilerPanel.TotalTimeSeconds = static_cast<float>(timingSnapshot.TotalTimeSeconds);

    m_SceneNodes.clear();
    Scene& scene = engine.GetScene();
    scene.VisitNodes(
        [&](SceneNode& node, int depth)
        {
            SceneNodeViewModel vm;
            vm.Node = &node;
            vm.NodeName = node.GetName();
            vm.Depth = depth;
            vm.Position[0] = node.GetPosition().x;
            vm.Position[1] = node.GetPosition().y;
            vm.Position[2] = node.GetPosition().z;
            vm.Rotation[0] = node.GetRotation().x;
            vm.Rotation[1] = node.GetRotation().y;
            vm.Rotation[2] = node.GetRotation().z;
            vm.Scale[0] = node.GetScale().x;
            vm.Scale[1] = node.GetScale().y;
            vm.Scale[2] = node.GetScale().z;
            m_SceneNodes.push_back(std::move(vm));
        });

    m_Textures.clear();
    engine.GetTextureManager().ForEachLoadedTexture(
        [&](const std::string& path, const Texture& texture)
        {
            TextureInfoViewModel vm;
            vm.TextureName = path;
            vm.Width = static_cast<int>(texture.GetWidth());
            vm.Height = static_cast<int>(texture.GetHeight());
            vm.MemoryBytes = EstimateTextureMemoryBytes(texture);
            m_Textures.push_back(std::move(vm));
        });
    std::sort(m_Textures.begin(),
              m_Textures.end(),
              [](const TextureInfoViewModel& lhs, const TextureInfoViewModel& rhs)
              {
                  return lhs.TextureName < rhs.TextureName;
              });

    Camera& camera = scene.GetActiveCamera();
    m_CameraState.Position[0] = camera.GetPosition().x;
    m_CameraState.Position[1] = camera.GetPosition().y;
    m_CameraState.Position[2] = camera.GetPosition().z;
    m_CameraState.RotationDegrees[0] = camera.GetRotation().x;
    m_CameraState.RotationDegrees[1] = camera.GetRotation().y;
    m_CameraState.RotationDegrees[2] = camera.GetRotation().z;
    m_CameraState.FieldOfView = camera.GetFOV();
    m_CameraState.NearPlane = camera.GetNearPlane();
    m_CameraState.FarPlane = camera.GetFarPlane();

    InputSystem& input = engine.GetInput();
    m_InputState.MouseX = input.GetMouseX();
    m_InputState.MouseY = input.GetMouseY();
    m_InputState.WheelDelta = input.GetWheelDelta();
    m_InputState.LeftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    m_InputState.RightButtonDown = input.IsMouseButtonDown(MouseButton::Right);
    m_InputState.MiddleButtonDown = input.IsMouseButtonDown(MouseButton::Middle);
    m_InputState.ConsumedByDebugUI = m_LastInputConsumedByDebugUI;
    m_InputState.PressedKeys.clear();
    const auto pressedKeys = input.GetPressedKeys();
    m_InputState.PressedKeys.reserve(pressedKeys.size());
    for (Key key : pressedKeys)
    {
        m_InputState.PressedKeys.push_back(KeyToString(key));
    }

    const FrameProfileSnapshot& profileSnapshot = engine.GetLastFrameProfileSnapshot();
    const bool frameMismatch = profileSnapshot.FrameIndex > timingSnapshot.FrameIndex && profileSnapshot.FrameIndex != 0;
    if (frameMismatch)
    {
        LX_WARN("[DebugUI] Profile snapshot frame mismatch (profile={} timing={})", profileSnapshot.FrameIndex, timingSnapshot.FrameIndex);
        m_ProfilerPanel.IsDataStale = true;
        m_ProfilerPanel.ProfileFrameIndex = 0;
        m_ProfilerPanel.Entries.clear();
    }
    else
    {
        m_ProfilerPanel.IsDataStale = false;
        m_ProfilerPanel.ProfileFrameIndex = profileSnapshot.FrameIndex;
        m_ProfilerPanel.Entries.clear();
        m_ProfilerPanel.Entries.reserve(profileSnapshot.Entries.size());
        for (const FrameProfileEntry& entry : profileSnapshot.Entries)
        {
            ProfilerEntryViewModel vm;
            vm.ScopeName = entry.ScopeName;
            vm.DurationMs = static_cast<float>(entry.TotalDurationMicroseconds / 1000.0);
            vm.CallCount = entry.CallCount;
            m_ProfilerPanel.Entries.push_back(std::move(vm));
        }
    }

    m_EngineMetrics.ProfileScopeCount = static_cast<int>(m_ProfilerPanel.Entries.size());
}

void DebugUI::RenderPanels(Engine& engine)
{
    static bool loggedEnginePanelOpen = false;
    static bool loggedSceneInspectorOpen = false;
    static bool loggedTextureViewerOpen = false;
    static bool loggedInputMonitorOpen = false;
    static bool loggedProfilerPanelOpen = false;

    if (m_ShowEnginePanel)
    {
        if (!loggedEnginePanelOpen)
        {
            LX_INFO("[DebugUI] Engine panel opened");
            loggedEnginePanelOpen = true;
        }
        EnginePanel::Render(m_EngineMetrics);
    }

    if (m_ShowSceneInspector)
    {
        if (!loggedSceneInspectorOpen)
        {
            LX_INFO("[DebugUI] Scene inspector opened");
            loggedSceneInspectorOpen = true;
        }
        SceneInspector::Render(m_SceneNodes, engine.GetScene());
    }

    if (m_ShowTextureViewer)
    {
        if (!loggedTextureViewerOpen)
        {
            LX_INFO("[DebugUI] Texture viewer opened");
            loggedTextureViewerOpen = true;
        }
        TextureViewer::Render(m_Textures);
    }

    if (m_ShowCameraPanel)
    {
        CameraPanel::Render(m_CameraState, engine.GetScene().GetActiveCamera());
    }

    if (m_ShowInputMonitor)
    {
        if (!loggedInputMonitorOpen)
        {
            LX_INFO("[DebugUI] Input monitor opened");
            loggedInputMonitorOpen = true;
        }
        InputMonitor::Render(m_InputState);
    }

    if (m_ShowProfilerPanel)
    {
        if (!loggedProfilerPanelOpen)
        {
            LX_INFO("[DebugUI] Profiler panel opened");
            loggedProfilerPanelOpen = true;
        }
        ProfilerPanel::Render(m_ProfilerPanel);
    }
}

void DebugUI::SetLastInputConsumedByDebugUI(bool consumed)
{
    m_LastInputConsumedByDebugUI = consumed;
}

void DebugUI::SetProfilerPanelVisible(bool visible)
{
    m_ShowProfilerPanel = visible;
}

void DebugUI::ToggleProfilerPanel()
{
    m_ShowProfilerPanel = !m_ShowProfilerPanel;
}

} // namespace LongXi

#endif // defined(LX_DEBUG) || defined(LX_DEV)
