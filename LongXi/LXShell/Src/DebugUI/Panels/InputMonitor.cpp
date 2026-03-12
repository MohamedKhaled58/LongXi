#include "InputMonitor.h"

#include <imgui.h>
#include <string>

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

void InputMonitor::Render(const InputStateViewModel& inputState)
{
    if (!ImGui::Begin("Input Monitor"))
    {
        ImGui::End();
        return;
    }

    // Mouse Position section
    if (ImGui::CollapsingHeader("Mouse Position", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("X: %d", inputState.MouseX);
        ImGui::Text("Y: %d", inputState.MouseY);
        ImGui::SameLine();
        ImGui::TextDisabled("(screen coordinates)");
    }

    ImGui::Separator();

    // Mouse Buttons section
    if (ImGui::CollapsingHeader("Mouse Buttons", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Left:   %s", inputState.LeftButtonDown ? "Down" : "Up");
        ImGui::Text("Right:  %s", inputState.RightButtonDown ? "Down" : "Up");
        ImGui::Text("Middle: %s", inputState.MiddleButtonDown ? "Down" : "Up");
    }

    ImGui::Separator();

    // Keyboard section
    if (ImGui::CollapsingHeader("Pressed Keys", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (inputState.PressedKeys.empty())
        {
            ImGui::TextDisabled("No keys pressed");
        }
        else
        {
            for (const std::string& keyName : inputState.PressedKeys)
            {
                ImGui::BulletText("%s", keyName.c_str());
            }
        }
    }

    ImGui::Separator();

    // Mouse Wheel section
    if (ImGui::CollapsingHeader("Mouse Wheel", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Delta: %d", inputState.WheelDelta);
        ImGui::SameLine();
        ImGui::TextDisabled("(WHEEL_DELTA = 120)");
    }

    ImGui::Separator();

    // Input Routing section
    if (ImGui::CollapsingHeader("Input Routing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Consumed by DebugUI:");
        ImGui::SameLine();

        if (inputState.ConsumedByDebugUI)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Yes");
            ImGui::SameLine();
            ImGui::TextDisabled("(event did NOT reach engine)");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No");
            ImGui::SameLine();
            ImGui::TextDisabled("(event forwarded to InputSystem)");
        }
    }

    ImGui::End();
}

} // namespace LongXi
