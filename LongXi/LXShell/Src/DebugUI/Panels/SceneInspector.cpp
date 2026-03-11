#include "SceneInspector.h"

#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Core/Logging/LogMacros.h"

#include <imgui.h>
#include <string>

namespace LongXi
{

SceneNode* SceneInspector::s_SelectedNode = nullptr;

void SceneInspector::Render(const std::vector<SceneNodeViewModel>& nodes, Scene& scene)
{
    (void)scene;

    if (!ImGui::Begin("Scene Inspector"))
    {
        ImGui::End();
        return;
    }

    // Hierarchy section
    if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (nodes.empty())
        {
            ImGui::TextDisabled("No scene node data available");
        }

        bool selectedNodeExists = false;
        for (const auto& nodeViewModel : nodes)
        {
            if (nodeViewModel.Node == s_SelectedNode)
            {
                selectedNodeExists = true;
            }

            ImGui::PushID(nodeViewModel.Node);
            const std::string indentedLabel(static_cast<size_t>(nodeViewModel.Depth) * 2u, ' ');
            const std::string label = indentedLabel + nodeViewModel.NodeName;

            // Selectable node with selection highlight
            if (ImGui::Selectable(label.c_str(), s_SelectedNode == nodeViewModel.Node))
            {
                s_SelectedNode = nodeViewModel.Node;
                LX_INFO("[SceneInspector] Node selected: {}", nodeViewModel.NodeName);
            }

            ImGui::PopID();
        }

        if (!selectedNodeExists)
        {
            s_SelectedNode = nullptr;
        }
    }

    ImGui::Separator();

    // Transform editor section
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (s_SelectedNode)
        {
            // Display current transform
            ImGui::Text("Selected: %s", s_SelectedNode->GetName().c_str());

            const Vector3 positionValue = s_SelectedNode->GetPosition();
            const Vector3 rotationValue = s_SelectedNode->GetRotation();
            const Vector3 scaleValue = s_SelectedNode->GetScale();

            float position[3] = {positionValue.x, positionValue.y, positionValue.z};
            float rotation[3] = {rotationValue.x, rotationValue.y, rotationValue.z};
            float scale[3] = {scaleValue.x, scaleValue.y, scaleValue.z};

            ImGui::Separator();

            // Position editing
            if (ImGui::DragFloat3("Position", position, 0.1f))
            {
                s_SelectedNode->SetPosition({position[0], position[1], position[2]});
                LX_INFO("[SceneInspector] Position changed: ({}, {}, {})", position[0], position[1], position[2]);
            }

            // Rotation editing
            if (ImGui::DragFloat3("Rotation", rotation, 1.0f))
            {
                s_SelectedNode->SetRotation({rotation[0], rotation[1], rotation[2]});
                LX_INFO("[SceneInspector] Rotation changed: ({}, {}, {})", rotation[0], rotation[1], rotation[2]);
            }

            // Scale editing
            if (ImGui::DragFloat3("Scale", scale, 0.1f))
            {
                s_SelectedNode->SetScale({scale[0], scale[1], scale[2]});
                LX_INFO("[SceneInspector] Scale changed: ({}, {}, {})", scale[0], scale[1], scale[2]);
            }
        }
        else
        {
            ImGui::TextDisabled("No node selected");
        }
    }

    ImGui::End();
}

} // namespace LongXi
