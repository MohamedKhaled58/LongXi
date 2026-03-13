#pragma once

#include <cstdint>

// =============================================================================
// Handle — Generic handle types for resource management (moved from LXEngine)
// HandleId provides slot/generation tracking for generational indices.
// Handle<T> provides type-safe wrapper for specific resource types.
// =============================================================================

namespace LXCore
{

struct HandleId
{
    uint32_t Slot       = 0;
    uint32_t Generation = 0;

    bool IsValid() const
    {
        return Slot != 0 && Generation != 0;
    }
};

template <typename Tag> struct Handle
{
    HandleId Id;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

} // namespace LXCore
