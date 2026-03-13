#include "C3AssetViewer.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <string>

#include "Core/Logging/LogMacros.h"
#include "Engine/Engine.h"
#include "Resource/C3RuntimeLoader.h"
#include "Resource/C3RuntimeTypes.h"
#include "Resource/C3Types.h"

namespace LongXi
{

namespace
{

struct ViewerState
{
    char         inputBuffer[256] = {0};
    bool         hasResult        = false;
    std::string  lastError;
    C3LoadResult result;
};

bool IsNumeric(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }
    return std::all_of(value.begin(),
                       value.end(),
                       [](unsigned char c)
                       {
                           return std::isdigit(c) != 0;
                       });
}

} // namespace

void C3AssetViewer::Render(Engine& engine)
{
    static ViewerState state;

    if (!ImGui::Begin("C3 Asset Viewer"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Load C3 asset by virtual path or numeric file ID.");
    ImGui::InputText("Path / File ID", state.inputBuffer, sizeof(state.inputBuffer));
    ImGui::SameLine();

    if (ImGui::Button("Load"))
    {
        std::string input = state.inputBuffer;
        state.lastError.clear();
        state.result    = C3LoadResult{};
        state.hasResult = false;

        if (input.empty())
        {
            state.lastError = "Provide a path or numeric file ID.";
        }
        else
        {
            C3LoadRequest request;
            if (IsNumeric(input))
            {
                request.fileId = static_cast<uint32_t>(std::stoul(input));
            }
            else
            {
                request.virtualPath = input;
            }

            C3RuntimeLoader loader;
            if (loader.LoadFromVfs(engine.GetVFS(), request, state.result))
            {
                state.hasResult = true;
            }
            else
            {
                state.lastError = state.result.error.empty() ? "Load failed" : state.result.error;
            }
        }
    }

    if (!state.lastError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", state.lastError.c_str());
    }

    if (!state.hasResult)
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text("Chunks");
    ImGui::BulletText("Meshes: %zu", state.result.meshes.size());
    ImGui::BulletText("Skeletons: %zu", state.result.skeletons.size());
    ImGui::BulletText("Animations: %zu", state.result.animations.size());
    ImGui::BulletText("Particles: %zu", state.result.particles.size());
    ImGui::BulletText("Cameras: %zu", state.result.cameras.size());
    ImGui::BulletText("Unknown: %zu", state.result.unknownChunks.size());

    if (!state.result.unknownChunks.empty() && ImGui::TreeNode("Unknown Chunks"))
    {
        for (const auto& chunk : state.result.unknownChunks)
        {
            const std::string type = FourCCToString(chunk.info.type);
            ImGui::BulletText("%s (%u bytes)", type.c_str(), chunk.info.size);
        }
        ImGui::TreePop();
    }

    if (!state.result.meshes.empty() && ImGui::TreeNode("Meshes"))
    {
        for (size_t i = 0; i < state.result.meshes.size(); ++i)
        {
            const auto& mesh = state.result.meshes[i];
            ImGui::BulletText("Mesh %zu: vertices=%zu indices=%zu morphTargets=%zu",
                              i,
                              mesh.vertices.size(),
                              mesh.indices.size(),
                              mesh.morphTargets.size());
        }
        ImGui::TreePop();
    }

    if (!state.result.skeletons.empty() && ImGui::TreeNode("Skeletons"))
    {
        for (size_t i = 0; i < state.result.skeletons.size(); ++i)
        {
            const auto& skeleton = state.result.skeletons[i];
            ImGui::BulletText("Skeleton %zu: bones=%u", i, skeleton.boneCount);
            if (ImGui::TreeNode(("Hierarchy##" + std::to_string(i)).c_str()))
            {
                for (size_t b = 0; b < skeleton.parentIndices.size(); ++b)
                {
                    ImGui::BulletText("Bone %zu -> Parent %d", b, skeleton.parentIndices[b]);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (!state.result.animations.empty() && ImGui::TreeNode("Animations"))
    {
        for (size_t i = 0; i < state.result.animations.size(); ++i)
        {
            const auto& clip = state.result.animations[i];
            ImGui::BulletText("Clip %zu: frames=%u bones=%u duration=%.2fs", i, clip.frameCount, clip.boneCount, clip.durationSeconds);
        }
        ImGui::TreePop();
    }

    if (!state.result.warnings.empty() && ImGui::TreeNode("Warnings"))
    {
        for (const auto& warning : state.result.warnings)
        {
            ImGui::BulletText("%s", warning.c_str());
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

} // namespace LongXi
