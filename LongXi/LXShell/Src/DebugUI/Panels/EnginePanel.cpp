#include "EnginePanel.h"
#include "Core/Logging/LogMacros.h"
#include <imgui.h>

namespace LongXi
{

void EnginePanel::Render(const EngineMetricsSnapshot& metrics)
{
    if (!ImGui::Begin("Engine Metrics"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Frame Index: %llu", static_cast<unsigned long long>(metrics.FrameIndex));
    ImGui::Text("Frames Per Second: %.1f", metrics.FramesPerSecond);
    ImGui::Text("Delta Time: %.2f ms", metrics.DeltaTimeMs);
    ImGui::Text("Frame Time: %.2f ms", metrics.FrameTimeMs);
    ImGui::Text("Profile Scopes: %d", metrics.ProfileScopeCount);
    ImGui::Text("Draw Calls: %d", metrics.DrawCallCount);
    ImGui::Text("Map Visible Tiles: %d", metrics.MapVisibleTiles);
    ImGui::Text("Map Visible Objects: %d", metrics.MapVisibleObjects);
    ImGui::Text("Map Animated Tiles: %d", metrics.MapAnimatedTiles);
    ImGui::Separator();
    ImGui::Text("GPU: %s", metrics.GpuDeviceName.c_str());

    ImGui::End();
}

} // namespace LongXi
