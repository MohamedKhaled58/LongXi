#pragma once

#include <Windows.h>
#include <cstdint>
#include <vector>

// =============================================================================
// InputSystem — Centralized Win32 keyboard and mouse input foundation
//
// Captures key and mouse button state through the Win32 message path.
// Per-frame Update() advances state transitions (Pressed / Released).
// All state is cleared on focus loss to prevent stuck input.
// =============================================================================

namespace LXEngine
{

// =============================================================================
// Key — Engine-defined keyboard key identifier
// Strongly typed; does not expose Win32 VK codes publicly.
// =============================================================================
enum class Key : uint8_t
{
    Unknown = 0,

    // Letters
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    // Top-row digits
    D0,
    D1,
    D2,
    D3,
    D4,
    D5,
    D6,
    D7,
    D8,
    D9,

    // Function keys
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    // Arrow keys
    Up,
    Down,
    Left,
    Right,

    // Navigation
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,

    // Common keys
    Escape,
    Enter,
    Space,
    Tab,
    Backspace,

    // Modifier keys
    LShift,
    RShift,
    LControl,
    RControl,
    LAlt,
    RAlt,

    // Lock keys
    CapsLock,
    NumLock,
    ScrollLock,

    // Numpad
    Numpad0,
    Numpad1,
    Numpad2,
    Numpad3,
    Numpad4,
    Numpad5,
    Numpad6,
    Numpad7,
    Numpad8,
    Numpad9,
    NumpadAdd,
    NumpadSubtract,
    NumpadMultiply,
    NumpadDivide,
    NumpadDecimal,
    NumpadEnter,

    // Punctuation
    Semicolon,
    Apostrophe,
    Comma,
    Period,
    Slash,
    Backslash,
    LeftBracket,
    RightBracket,
    Minus,
    Equal,
    Grave,

    Count // Sentinel — total key count; used as array size
};

// =============================================================================
// MouseButton — Standard 3-button mouse identifier
// =============================================================================
enum class MouseButton : uint8_t
{
    Left   = 0,
    Right  = 1,
    Middle = 2,
    Count  = 3
};

// =============================================================================
// InputSystem
// =============================================================================
class InputSystem
{
public:
    InputSystem();
    ~InputSystem() = default;

    // Disable copy
    InputSystem(const InputSystem&)            = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    // Lifecycle
    void Initialize();
    void Shutdown();

    // Per-frame boundary — call once per frame BEFORE querying input state
    void Update();

    // Win32 message handlers (called from Application::WindowProc)
    void OnKeyDown(UINT vk, bool isRepeat);
    void OnKeyUp(UINT vk);
    void OnMouseMove(int x, int y);
    void OnMouseButtonDown(MouseButton button);
    void OnMouseButtonUp(MouseButton button);
    void OnMouseWheel(int delta);
    void OnFocusLost();

    // Keyboard queries
    bool             IsKeyDown(Key key) const;
    bool             IsKeyPressed(Key key) const;
    bool             IsKeyReleased(Key key) const;
    std::vector<Key> GetPressedKeys() const;

    // Mouse button queries
    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;

    // Mouse position and wheel
    int GetMouseX() const;
    int GetMouseY() const;
    int GetWheelDelta() const;

private:
    // Keyboard state — indexed by Win32 VK code (0–255)
    bool m_KeyCurrent[256];
    bool m_KeyPrevious[256];

    // Mouse button state — indexed by MouseButton cast to size_t
    bool m_MouseCurrent[static_cast<size_t>(MouseButton::Count)];
    bool m_MousePrevious[static_cast<size_t>(MouseButton::Count)];

    // Mouse position (client-space pixels)
    int m_MouseX;
    int m_MouseY;

    // Wheel delta accumulated this frame (raw Win32 units; ±120 per notch)
    int m_WheelDelta;

    // Translation tables: Key enum ↔ Win32 VK code
    UINT m_KeyToVK[static_cast<size_t>(Key::Count)];
    Key  m_VKToKey[256];
};

} // namespace LXEngine
