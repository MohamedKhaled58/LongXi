#pragma once

#include <cstdint>

// =============================================================================
// Input Types — Shared input enumerations (moved from LXEngine)
// Key and MouseButton enums are used across engine subsystems.
// =============================================================================

namespace LXCore
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

} // namespace LXCore
