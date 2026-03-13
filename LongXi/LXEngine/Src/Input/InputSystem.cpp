#include "Input/InputSystem.h"

#include <cstring>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

// ============================================================================
// Translation table initialization helpers
// ============================================================================

static void BuildKeyTables(UINT* keyToVK, Key* vkToKey)
{
    // Initialize all entries to unknown/unmapped
    for (size_t i = 0; i < static_cast<size_t>(Key::Count); ++i)
        keyToVK[i] = 0;
    for (int i = 0; i < 256; ++i)
        vkToKey[i] = Key::Unknown;

    // Helper lambda to register a mapping
    auto Map = [&](Key key, UINT vk)
    {
        keyToVK[static_cast<size_t>(key)] = vk;
        vkToKey[vk]                       = key;
    };

    // Letters (VK_A = 0x41 ... VK_Z = 0x5A)
    Map(Key::A, 0x41);
    Map(Key::B, 0x42);
    Map(Key::C, 0x43);
    Map(Key::D, 0x44);
    Map(Key::E, 0x45);
    Map(Key::F, 0x46);
    Map(Key::G, 0x47);
    Map(Key::H, 0x48);
    Map(Key::I, 0x49);
    Map(Key::J, 0x4A);
    Map(Key::K, 0x4B);
    Map(Key::L, 0x4C);
    Map(Key::M, 0x4D);
    Map(Key::N, 0x4E);
    Map(Key::O, 0x4F);
    Map(Key::P, 0x50);
    Map(Key::Q, 0x51);
    Map(Key::R, 0x52);
    Map(Key::S, 0x53);
    Map(Key::T, 0x54);
    Map(Key::U, 0x55);
    Map(Key::V, 0x56);
    Map(Key::W, 0x57);
    Map(Key::X, 0x58);
    Map(Key::Y, 0x59);
    Map(Key::Z, 0x5A);

    // Top-row digits (VK_0 = 0x30 ... VK_9 = 0x39)
    Map(Key::D0, 0x30);
    Map(Key::D1, 0x31);
    Map(Key::D2, 0x32);
    Map(Key::D3, 0x33);
    Map(Key::D4, 0x34);
    Map(Key::D5, 0x35);
    Map(Key::D6, 0x36);
    Map(Key::D7, 0x37);
    Map(Key::D8, 0x38);
    Map(Key::D9, 0x39);

    // Function keys
    Map(Key::F1, VK_F1);
    Map(Key::F2, VK_F2);
    Map(Key::F3, VK_F3);
    Map(Key::F4, VK_F4);
    Map(Key::F5, VK_F5);
    Map(Key::F6, VK_F6);
    Map(Key::F7, VK_F7);
    Map(Key::F8, VK_F8);
    Map(Key::F9, VK_F9);
    Map(Key::F10, VK_F10);
    Map(Key::F11, VK_F11);
    Map(Key::F12, VK_F12);

    // Arrow keys
    Map(Key::Up, VK_UP);
    Map(Key::Down, VK_DOWN);
    Map(Key::Left, VK_LEFT);
    Map(Key::Right, VK_RIGHT);

    // Navigation
    Map(Key::Home, VK_HOME);
    Map(Key::End, VK_END);
    Map(Key::PageUp, VK_PRIOR);
    Map(Key::PageDown, VK_NEXT);
    Map(Key::Insert, VK_INSERT);
    Map(Key::Delete, VK_DELETE);

    // Common keys
    Map(Key::Escape, VK_ESCAPE);
    Map(Key::Enter, VK_RETURN);
    Map(Key::Space, VK_SPACE);
    Map(Key::Tab, VK_TAB);
    Map(Key::Backspace, VK_BACK);

    // Modifier keys
    Map(Key::LShift, VK_LSHIFT);
    Map(Key::RShift, VK_RSHIFT);
    Map(Key::LControl, VK_LCONTROL);
    Map(Key::RControl, VK_RCONTROL);
    Map(Key::LAlt, VK_LMENU);
    Map(Key::RAlt, VK_RMENU);

    // Also map VK_MENU (generic Alt) to LAlt as a fallback
    vkToKey[VK_MENU] = Key::LAlt;

    // Lock keys
    Map(Key::CapsLock, VK_CAPITAL);
    Map(Key::NumLock, VK_NUMLOCK);
    Map(Key::ScrollLock, VK_SCROLL);

    // Numpad
    Map(Key::Numpad0, VK_NUMPAD0);
    Map(Key::Numpad1, VK_NUMPAD1);
    Map(Key::Numpad2, VK_NUMPAD2);
    Map(Key::Numpad3, VK_NUMPAD3);
    Map(Key::Numpad4, VK_NUMPAD4);
    Map(Key::Numpad5, VK_NUMPAD5);
    Map(Key::Numpad6, VK_NUMPAD6);
    Map(Key::Numpad7, VK_NUMPAD7);
    Map(Key::Numpad8, VK_NUMPAD8);
    Map(Key::Numpad9, VK_NUMPAD9);
    Map(Key::NumpadAdd, VK_ADD);
    Map(Key::NumpadSubtract, VK_SUBTRACT);
    Map(Key::NumpadMultiply, VK_MULTIPLY);
    Map(Key::NumpadDivide, VK_DIVIDE);
    Map(Key::NumpadDecimal, VK_DECIMAL);
    // Map(Key::NumpadEnter, VK_RETURN); // Numpad Enter shares VK_RETURN

    // Numpad Enter shares VK_RETURN with main Enter — only set forward mapping
    // to keep vkToKey[VK_RETURN] pointed at Key::Enter
    keyToVK[static_cast<size_t>(Key::NumpadEnter)] = VK_RETURN;

    // Punctuation (OEM keys)
    Map(Key::Semicolon, VK_OEM_1);    // ;:
    Map(Key::Apostrophe, VK_OEM_7);   // '"
    Map(Key::Comma, VK_OEM_COMMA);    // ,<
    Map(Key::Period, VK_OEM_PERIOD);  // .>
    Map(Key::Slash, VK_OEM_2);        // /?
    Map(Key::Backslash, VK_OEM_5);    // \|
    Map(Key::LeftBracket, VK_OEM_4);  // [{
    Map(Key::RightBracket, VK_OEM_6); // ]}
    Map(Key::Minus, VK_OEM_MINUS);    // -_
    Map(Key::Equal, VK_OEM_PLUS);     // =+
    Map(Key::Grave, VK_OEM_3);        // `~
}

// ============================================================================
// Constructor
// ============================================================================

InputSystem::InputSystem()
    : m_MouseX(0)
    , m_MouseY(0)
    , m_WheelDelta(0)
{
    memset(m_KeyCurrent, 0, sizeof(m_KeyCurrent));
    memset(m_KeyPrevious, 0, sizeof(m_KeyPrevious));
    memset(m_MouseCurrent, 0, sizeof(m_MouseCurrent));
    memset(m_MousePrevious, 0, sizeof(m_MousePrevious));
    memset(m_KeyToVK, 0, sizeof(m_KeyToVK));
    memset(m_VKToKey, 0, sizeof(m_VKToKey));
}

// ============================================================================
// Lifecycle
// ============================================================================

void InputSystem::Initialize()
{
    BuildKeyTables(m_KeyToVK, m_VKToKey);
    LX_ENGINE_INFO("Input system initialized");
}

void InputSystem::Shutdown()
{
    LX_ENGINE_INFO("Input system shutdown");
}

// ============================================================================
// Per-frame boundary
// ============================================================================

void InputSystem::Update()
{
    // Advance frame boundary: current → previous
    memcpy(m_KeyPrevious, m_KeyCurrent, sizeof(m_KeyCurrent));
    memcpy(m_MousePrevious, m_MouseCurrent, sizeof(m_MouseCurrent));

    // Reset per-frame-only fields
    m_WheelDelta = 0;
}

// ============================================================================
// Focus loss — clears all active state to prevent stuck input
// ============================================================================

void InputSystem::OnFocusLost()
{
    memset(m_KeyCurrent, 0, sizeof(m_KeyCurrent));
    memset(m_MouseCurrent, 0, sizeof(m_MouseCurrent));
}

// ============================================================================
// Keyboard message handlers
// ============================================================================

void InputSystem::OnKeyDown(UINT vk, bool isRepeat)
{
    // Ignore auto-repeat messages — only act on the initial key-down
    if (isRepeat)
        return;

    if (vk < 256)
        m_KeyCurrent[vk] = true;
}

void InputSystem::OnKeyUp(UINT vk)
{
    if (vk < 256)
        m_KeyCurrent[vk] = false;
}

// ============================================================================
// Keyboard queries
// ============================================================================

bool InputSystem::IsKeyDown(Key key) const
{
    UINT vk = m_KeyToVK[static_cast<size_t>(key)];
    return vk < 256 && m_KeyCurrent[vk];
}

bool InputSystem::IsKeyPressed(Key key) const
{
    UINT vk = m_KeyToVK[static_cast<size_t>(key)];
    return vk < 256 && m_KeyCurrent[vk] && !m_KeyPrevious[vk];
}

bool InputSystem::IsKeyReleased(Key key) const
{
    UINT vk = m_KeyToVK[static_cast<size_t>(key)];
    return vk < 256 && !m_KeyCurrent[vk] && m_KeyPrevious[vk];
}

std::vector<Key> InputSystem::GetPressedKeys() const
{
    std::vector<Key> pressedKeys;
    pressedKeys.reserve(8);

    for (size_t index = 1; index < static_cast<size_t>(Key::Count); ++index)
    {
        const Key key = static_cast<Key>(index);
        if (IsKeyDown(key))
        {
            pressedKeys.push_back(key);
        }
    }

    return pressedKeys;
}

// ============================================================================
// Mouse message handlers
// ============================================================================

void InputSystem::OnMouseMove(int x, int y)
{
    m_MouseX = x;
    m_MouseY = y;
}

void InputSystem::OnMouseButtonDown(MouseButton button)
{
    m_MouseCurrent[static_cast<size_t>(button)] = true;
}

void InputSystem::OnMouseButtonUp(MouseButton button)
{
    m_MouseCurrent[static_cast<size_t>(button)] = false;
}

void InputSystem::OnMouseWheel(int delta)
{
    m_WheelDelta += delta;
}

// ============================================================================
// Mouse queries
// ============================================================================

bool InputSystem::IsMouseButtonDown(MouseButton button) const
{
    return m_MouseCurrent[static_cast<size_t>(button)];
}

bool InputSystem::IsMouseButtonPressed(MouseButton button) const
{
    size_t idx = static_cast<size_t>(button);
    return m_MouseCurrent[idx] && !m_MousePrevious[idx];
}

bool InputSystem::IsMouseButtonReleased(MouseButton button) const
{
    size_t idx = static_cast<size_t>(button);
    return !m_MouseCurrent[idx] && m_MousePrevious[idx];
}

int InputSystem::GetMouseX() const
{
    return m_MouseX;
}

int InputSystem::GetMouseY() const
{
    return m_MouseY;
}

int InputSystem::GetWheelDelta() const
{
    return m_WheelDelta;
}

} // namespace LXEngine
