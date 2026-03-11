#include "ImGuiLayer.h"

#if defined(LX_DEBUG) || defined(LX_DEV)

#include <Core/Logging/LogMacros.h>
#include <Engine/Engine.h>

#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <windows.h>

namespace LongXi
{

bool ImGuiLayer::Initialize(Engine& engine, HWND windowHandle)
{
    if (m_Initialized)
    {
        LX_ENGINE_WARN("[ImGuiLayer] Already initialized");
        return true;
    }

    if (!windowHandle)
    {
        LX_ENGINE_ERROR("[ImGuiLayer] Invalid window handle");
        return false;
    }

    ID3D11Device* device = static_cast<ID3D11Device*>(engine.GetRendererDeviceHandle());
    ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(engine.GetRendererContextHandle());
    if (!device || !context)
    {
        LX_ENGINE_ERROR("[ImGuiLayer] Missing renderer device/context");
        return false;
    }

    m_WindowHandle = windowHandle;
    m_NativeDevice = device;
    m_NativeContext = context;
    m_ViewportWidth = engine.GetRendererViewportWidth();
    m_ViewportHeight = engine.GetRendererViewportHeight();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(windowHandle))
    {
        LX_ENGINE_ERROR("[ImGuiLayer] ImGui_ImplWin32_Init failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(m_NativeDevice), static_cast<ID3D11DeviceContext*>(m_NativeContext)))
    {
        LX_ENGINE_ERROR("[ImGuiLayer] ImGui_ImplDX11_Init failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_Initialized = true;
    LX_ENGINE_INFO("[ImGuiLayer] Initialized");
    return true;
}

void ImGuiLayer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_Initialized = false;
    m_NativeDevice = nullptr;
    m_NativeContext = nullptr;
    LX_ENGINE_INFO("[ImGuiLayer] Shutdown complete");
}

void ImGuiLayer::BeginFrame()
{
    if (!m_Initialized)
    {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame()
{
    if (!m_Initialized)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::OnResize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

bool ImGuiLayer::HandleWin32Message(uint32_t msg, uint64_t wParam, int64_t lParam)
{
    if (!m_Initialized)
    {
        return false;
    }

    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
    return ImGui_ImplWin32_WndProcHandler(m_WindowHandle, static_cast<UINT>(msg), static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam)) != 0;
}

} // namespace LongXi

#endif // defined(LX_DEBUG) || defined(LX_DEV)
