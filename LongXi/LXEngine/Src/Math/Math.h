#pragma once

// =============================================================================
// Math — Shared LXEngine math types
// Vector2 and Color are simple POD structs used across engine subsystems.
// =============================================================================

namespace LongXi
{

struct Vector2
{
    float x;
    float y;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

} // namespace LongXi
