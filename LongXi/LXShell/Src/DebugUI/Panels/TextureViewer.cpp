#include "TextureViewer.h"

#include <imgui.h>

#include "Core/Logging/LogMacros.h"

namespace LXShell
{

void TextureViewer::Render(const std::vector<TextureInfoViewModel>& textures)
{
    if (!ImGui::Begin("Texture Viewer"))
    {
        ImGui::End();
        return;
    }

    // Texture count
    ImGui::Text("Loaded Textures: %zu", textures.size());
    ImGui::Separator();

    // Header
    ImGui::Columns(4, "TextureColumns");
    ImGui::Separator();
    ImGui::Text("Name");
    ImGui::NextColumn();
    ImGui::Text("Width");
    ImGui::NextColumn();
    ImGui::Text("Height");
    ImGui::NextColumn();
    ImGui::Text("Memory");
    ImGui::NextColumn();
    ImGui::Separator();

    // Texture rows
    for (const auto& tex : textures)
    {
        ImGui::Text("%s", tex.TextureName.empty() ? "Unknown" : tex.TextureName.c_str());
        ImGui::NextColumn();
        ImGui::Text("%d", tex.Width);
        ImGui::NextColumn();
        ImGui::Text("%d", tex.Height);
        ImGui::NextColumn();

        // Format memory size (bytes to KB/MB)
        char   memoryStr[64];
        double memMB = static_cast<double>(tex.MemoryBytes) / (1024.0 * 1024.0);
        if (memMB >= 1.0)
        {
            snprintf(memoryStr, sizeof(memoryStr), "%.2f MB", memMB);
        }
        else
        {
            double memKB = static_cast<double>(tex.MemoryBytes) / 1024.0;
            snprintf(memoryStr, sizeof(memoryStr), "%.2f KB", memKB);
        }
        ImGui::Text("%s", memoryStr);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();

    // Footer - total memory
    size_t totalMemory = 0;
    for (const auto& tex : textures)
    {
        totalMemory += tex.MemoryBytes;
    }
    double totalMB = static_cast<double>(totalMemory) / (1024.0 * 1024.0);
    ImGui::Text("Total Memory: %.2f MB", totalMB);

    ImGui::End();
}

} // namespace LXShell
