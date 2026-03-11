#pragma once

// =============================================================================
// Math — Shared LXEngine math types
// Vector2, Vector3 and Color are simple POD structs used across engine subsystems.
// Note: rotation values (Vector3) are stored in degrees throughout the engine.
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

struct Vector3
{
    float x;
    float y;
    float z;
};

// Row-major 4x4 matrix used for camera and render transforms.
struct Matrix4
{
    float m[16];
};

} // namespace LongXi
