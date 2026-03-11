#pragma once

#if defined(LX_DEBUG) || defined(LX_DEV)

#include <cstdint>

struct HWND__;
using HWND = HWND__*;

namespace LongXi
{

class Engine;

class ImGuiLayer
{
public:
    bool Initialize(Engine& engine, HWND windowHandle);
    void Shutdown();

    bool IsInitialized() const
    {
        return m_Initialized;
    }

    void BeginFrame();
    void EndFrame();
    void RenderDrawData();
    void OnResize(int width, int height);

    // Returns true if ImGui consumed the input message.
    bool HandleWin32Message(uint32_t msg, uint64_t wParam, int64_t lParam);

private:
    bool m_Initialized = false;
    HWND m_WindowHandle = nullptr;
    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;
};

} // namespace LongXi

#endif // defined(LX_DEBUG) || defined(LX_DEV)
