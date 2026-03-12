#pragma once

#include <Windows.h>
#include <cstdint>
#include <functional>
#include <string>

// =============================================================================
// Win32Window — RAII wrapper around Win32 window handle
// Provides clean window creation, display, and destruction.
// =============================================================================

namespace LongXi
{

// Forward declaration — defined in Input/InputSystem.h
enum class MouseButton : uint8_t;

class Win32Window
{
public:
    Win32Window(const std::wstring& title, int width, int height);
    ~Win32Window();

    // Disable copy
    Win32Window(const Win32Window&)            = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool Create(WNDPROC windowProc);
    void Show(int nCmdShow = SW_SHOW);
    void Destroy();

    HWND GetHandle() const
    {
        return m_WindowHandle;
    }

    bool IsValid() const
    {
        return m_WindowHandle != nullptr;
    }

    int GetWidth() const
    {
        return m_Width;
    }

    int GetHeight() const
    {
        return m_Height;
    }

    void SetSize(int width, int height)
    {
        m_Width  = width;
        m_Height = height;
    }

    // Window event callbacks — wired by Application after engine initialization
    std::function<bool(UINT, WPARAM, LPARAM)> OnRawMessage;
    std::function<void(int, int)>             OnResize;
    std::function<void(UINT, bool)>           OnKeyDown;
    std::function<void(UINT)>                 OnKeyUp;
    std::function<void(int, int)>             OnMouseMove;
    std::function<void(MouseButton)>          OnMouseButtonDown;
    std::function<void(MouseButton)>          OnMouseButtonUp;
    std::function<void(int)>                  OnMouseWheel;
    std::function<void()>                     OnFocusLost;

    // Win32 message dispatcher — retrieves Win32Window* via GWLP_USERDATA to route callbacks
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    std::wstring m_Title;
    int          m_Width;
    int          m_Height;
    HWND         m_WindowHandle;
    ATOM         m_ClassAtom;
};

} // namespace LongXi
