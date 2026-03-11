#include "CameraPanel.h"
#include "Scene/Camera.h"
#include "Core/Logging/LogMacros.h"
#include <imgui.h>

namespace LongXi
{

void CameraPanel::Render(const CameraStateViewModel& cameraState, Camera& camera)
{
    if (!ImGui::Begin("Camera"))
    {
        ImGui::End();
        return;
    }

    // Position
    float position[3] = {cameraState.Position[0], cameraState.Position[1], cameraState.Position[2]};

    ImGui::Text("Position");
    if (ImGui::DragFloat3("##Position", position, 0.1f))
    {
        camera.SetPosition({position[0], position[1], position[2]});
        LX_ENGINE_INFO("[CameraPanel] Position changed: ({}, {}, {})", position[0], position[1], position[2]);
    }

    // Rotation
    float rotation[3] = {cameraState.RotationDegrees[0], cameraState.RotationDegrees[1], cameraState.RotationDegrees[2]};

    ImGui::Text("Rotation");
    if (ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f))
    {
        camera.SetRotation({rotation[0], rotation[1], rotation[2]});
        LX_ENGINE_INFO("[CameraPanel] Rotation changed: ({}, {}, {})", rotation[0], rotation[1], rotation[2]);
    }

    ImGui::Separator();

    // Field of View
    float fov = cameraState.FieldOfView;
    if (ImGui::SliderFloat("Field of View", &fov, 1.0f, 179.0f, "%.1f°"))
    {
        camera.SetFOV(fov);
        LX_ENGINE_INFO("[CameraPanel] FOV changed: %.1f°", fov);
    }

    // Near Plane
    float nearPlane = cameraState.NearPlane;
    if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.001f, 100.0f, "%.3f"))
    {
        camera.SetNearFar(nearPlane, cameraState.FarPlane);
        LX_ENGINE_INFO("[CameraPanel] Near plane changed: %.3f", nearPlane);
    }

    // Far Plane
    float farPlane = cameraState.FarPlane;
    if (ImGui::DragFloat("Far Plane", &farPlane, 10.0f, 1.0f, 100000.0f, "%.1f"))
    {
        camera.SetNearFar(cameraState.NearPlane, farPlane);
        LX_ENGINE_INFO("[CameraPanel] Far plane changed: %.1f", farPlane);
    }

    ImGui::Separator();

    // Info section
    ImGui::Text("Camera Info");
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", position[0], position[1], position[2]);
    ImGui::Text("Rotation: (%.1f, %.1f, %.1f)", rotation[0], rotation[1], rotation[2]);
    ImGui::Text("FOV: %.1f°", fov);
    ImGui::Text("Near: %.3f", nearPlane);
    ImGui::Text("Far: %.1f", farPlane);

    ImGui::End();
}

} // namespace LongXi
