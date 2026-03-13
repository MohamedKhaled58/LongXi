#pragma once

#include "Core/Logging/LogMacros.h"

// =============================================================================
// C3 parsing log helpers
// =============================================================================

#define LX_C3_INFO(...) LX_ENGINE_INFO("[C3] " __VA_ARGS__)
#define LX_C3_WARN(...) LX_ENGINE_WARN("[C3] " __VA_ARGS__)
#define LX_C3_ERROR(...) LX_ENGINE_ERROR("[C3] " __VA_ARGS__)
