#include "Window/Win32Window.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>

namespace LongXi
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

Win32Window::Win32Window(const std::wstring& title, int width, int height) : m_Title(title), m_Width(width), m_Height(height), m_WindowHandle(nullptr), m_ClassAtom(0) {}

Win32Window::~Win32Window()
{
    if (IsValid())
    {
        Destroy();
    }
}

// ============================================================================
// Create
// ============================================================================

bool Win32Window::Create(WNDPROC windowProc)
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = m_Title.c_str();

    m_ClassAtom = RegisterClassEx(&wc);
    if (!m_ClassAtom)
    {
        LX_ENGINE_ERROR("Failed to register window class (error {})", GetLastError());
        return false;
    }

    // Calculate window size so the CLIENT area is exactly m_Width x m_Height
    RECT rc = {0, 0, m_Width, m_Height};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

    m_WindowHandle =
        CreateWindowEx(0, MAKEINTATOM(m_ClassAtom), m_Title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

    if (!m_WindowHandle)
    {
        LX_ENGINE_ERROR("Failed to create window (error {})", GetLastError());
        UnregisterClass(MAKEINTATOM(m_ClassAtom), hInstance);
        m_ClassAtom = 0;
        return false;
    }

    LX_ENGINE_INFO("Win32Window created: \"Long Xi\" ({}x{})", m_Width, m_Height);
    return true;
}

// ============================================================================
// Show
// ============================================================================

void Win32Window::Show(int nCmdShow)
{
    if (IsValid())
    {
        ShowWindow(m_WindowHandle, nCmdShow);
        UpdateWindow(m_WindowHandle);
    }
}

// ============================================================================
// Destroy
// ============================================================================

void Win32Window::Destroy()
{
    if (IsValid())
    {
        DestroyWindow(m_WindowHandle);
        m_WindowHandle = nullptr;
    }

    if (m_ClassAtom)
    {
        UnregisterClass(MAKEINTATOM(m_ClassAtom), GetModuleHandle(nullptr));
        m_ClassAtom = 0;
    }
}

} // namespace LongXi
