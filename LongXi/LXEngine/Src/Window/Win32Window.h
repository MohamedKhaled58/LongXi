#pragma once

#include <string>
#include <Windows.h>

// =============================================================================
// Win32Window — RAII wrapper around Win32 window handle
// Provides clean window creation, display, and destruction.
// =============================================================================

namespace LongXi
{

class Win32Window
{
  public:
    Win32Window(const std::wstring& title, int width, int height);
    ~Win32Window();

    // Disable copy
    Win32Window(const Win32Window&) = delete;
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

  private:
    std::wstring m_Title;
    int m_Width;
    int m_Height;
    HWND m_WindowHandle;
    ATOM m_ClassAtom;
};

} // namespace LongXi
