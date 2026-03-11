#include "Window/Win32Window.h"
#include "Input/InputSystem.h"
#include "Core/Logging/LogMacros.h"

#include <windows.h>
#include <windowsx.h>

namespace LongXi
{

static UINT NormalizeVirtualKey(UINT vk, LPARAM lParam)
{
    if (vk == VK_SHIFT)
    {
        UINT scanCode = (static_cast<UINT>(lParam) >> 16) & 0xFFu;
        UINT mapped = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        if (mapped == VK_LSHIFT || mapped == VK_RSHIFT)
            return mapped;

        // Fallback if scan code mapping is unavailable.
        return VK_LSHIFT;
    }

    if (vk == VK_CONTROL)
        return (lParam & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;

    if (vk == VK_MENU)
        return (lParam & 0x01000000) ? VK_RMENU : VK_LMENU;

    return vk;
}

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

    // Store Win32Window* in GWLP_USERDATA so WindowProc can retrieve callbacks
    SetWindowLongPtr(m_WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

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

// ============================================================================
// WindowProc — Centralized Win32 message handler
// Retrieves Win32Window* from GWLP_USERDATA and routes messages to callbacks.
// ============================================================================

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // Let optional raw handler consume input before InputSystem routing callbacks.
    if (window && window->OnRawMessage)
    {
        if (window->OnRawMessage(msg, wParam, lParam))
        {
            return 0;
        }
    }

    switch (msg)
    {
    case WM_CLOSE:
        LX_ENGINE_INFO("[Window] WM_CLOSE received - destroying window");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        LX_ENGINE_INFO("[Window] WM_DESTROY received - posting quit message");
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (window)
        {
            int width = static_cast<int>(LOWORD(lParam));
            int height = static_cast<int>(HIWORD(lParam));
            window->SetSize(width, height);
            // Guard against zero-area (minimized window)
            if (width > 0 && height > 0 && window->OnResize)
            {
                LX_ENGINE_INFO("[Window] WM_SIZE received: {}x{}", width, height);
                window->OnResize(width, height);
            }
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (window && window->OnKeyDown)
        {
            UINT vk = NormalizeVirtualKey(static_cast<UINT>(wParam), lParam);
            bool isRepeat = (lParam & 0x40000000) != 0;
            window->OnKeyDown(vk, isRepeat);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (window && window->OnKeyUp)
        {
            UINT vk = NormalizeVirtualKey(static_cast<UINT>(wParam), lParam);
            window->OnKeyUp(vk);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (window && window->OnMouseMove)
        {
            window->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (window && window->OnMouseButtonDown)
        {
            SetCapture(hwnd);
            window->OnMouseButtonDown(MouseButton::Left);
        }
        return 0;

    case WM_LBUTTONUP:
        if (window && window->OnMouseButtonUp)
        {
            window->OnMouseButtonUp(MouseButton::Left);
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (window && window->OnMouseButtonDown)
        {
            SetCapture(hwnd);
            window->OnMouseButtonDown(MouseButton::Right);
        }
        return 0;

    case WM_RBUTTONUP:
        if (window && window->OnMouseButtonUp)
        {
            window->OnMouseButtonUp(MouseButton::Right);
            ReleaseCapture();
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (window && window->OnMouseButtonDown)
        {
            SetCapture(hwnd);
            window->OnMouseButtonDown(MouseButton::Middle);
        }
        return 0;

    case WM_MBUTTONUP:
        if (window && window->OnMouseButtonUp)
        {
            window->OnMouseButtonUp(MouseButton::Middle);
            ReleaseCapture();
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (window && window->OnMouseWheel)
        {
            window->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;

    case WM_KILLFOCUS:
        if (window && window->OnFocusLost)
        {
            window->OnFocusLost();
        }
        ReleaseCapture();
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

} // namespace LongXi
