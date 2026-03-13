#pragma once

#include <cstdint>

namespace LXEngine
{

class AnimationPlayer;
class RuntimeMesh;

namespace LXHeroSlot
{
constexpr uint32_t Armet   = 0;
constexpr uint32_t Misc    = 1;
constexpr uint32_t RWeapon = 2;
constexpr uint32_t LWeapon = 3;
constexpr uint32_t Mount   = 4;
constexpr uint32_t LShield = 5;
constexpr uint32_t RShield = 6;
constexpr uint32_t Count   = 7;
} // namespace LXHeroSlot

struct HeroEquipmentSlot
{
    const char*  name = nullptr;
    RuntimeMesh* mesh = nullptr;
};

class LXHero
{
public:
    float   worldX    = 0.0f;
    float   worldY    = 0.0f;
    float   height    = 0.0f;
    uint8_t direction = 0;
    float   scale     = 0.75f;

    RuntimeMesh* bodyMesh = nullptr;
    RuntimeMesh* headMesh = nullptr;

    HeroEquipmentSlot slots[LXHeroSlot::Count];

    AnimationPlayer* animationPlayer = nullptr;
};

} // namespace LXEngine
