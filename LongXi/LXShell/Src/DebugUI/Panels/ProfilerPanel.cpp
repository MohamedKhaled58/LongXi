#include "ProfilerPanel.h"

#include <imgui.h>

namespace LongXi
{

void ProfilerPanel::Render(const ProfilerPanelViewModel& profiler)
{
    if (!ImGui::Begin("Profiler"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Timing Frame: %llu", static_cast<unsigned long long>(profiler.TimingFrameIndex));
    ImGui::Text("Profile Frame: %llu", static_cast<unsigned long long>(profiler.ProfileFrameIndex));
    ImGui::Text("Delta Time: %.3f ms", profiler.DeltaTimeMs);
    ImGui::Text("Frame Time: %.3f ms", profiler.FrameTimeMs);
    ImGui::Text("Total Time: %.2f s", profiler.TotalTimeSeconds);

    if (!profiler.ProfilingEnabled)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Profiling is disabled for this build configuration.");
        ImGui::End();
        return;
    }

    if (profiler.IsDataStale)
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Profile data is stale due to frame mismatch.");
    }

    ImGui::Separator();
    ImGui::Text("Scopes: %d", static_cast<int>(profiler.Entries.size()));

    if (profiler.Entries.empty())
    {
        ImGui::TextDisabled("No profile entries captured for the last completed frame.");
        ImGui::End();
        return;
    }

    ImGui::Columns(3, "ProfilerColumns");
    ImGui::TextUnformatted("Scope");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Duration (ms)");
    ImGui::NextColumn();
    ImGui::TextUnformatted("Calls");
    ImGui::NextColumn();
    ImGui::Separator();

    for (const ProfilerEntryViewModel& entry : profiler.Entries)
    {
        ImGui::TextUnformatted(entry.ScopeName.c_str());
        ImGui::NextColumn();
        ImGui::Text("%.3f", entry.DurationMs);
        ImGui::NextColumn();
        ImGui::Text("%u", entry.CallCount);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::End();
}

} // namespace LongXi
