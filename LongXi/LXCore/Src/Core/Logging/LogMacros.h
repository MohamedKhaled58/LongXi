#pragma once

#include "Core/Logging/Log.h"

// =============================================================================
// LongXi Logging Macros — All modules
//
// All macros are defined here to avoid include path collisions.
// Macro prefixes: LX_CORE_*, LX_ENGINE_*, LX_* (shell)
//
// Fixed-width module prefixes: [LXCore  ] [LXEngine] [LXShell ]
// =============================================================================

// --- LXCore Macros ---
#define LX_CORE_TRACE(...) ::LongXi::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LX_CORE_INFO(...) ::LongXi::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LX_CORE_WARN(...) ::LongXi::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LX_CORE_ERROR(...) ::LongXi::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LX_CORE_CRITICAL(...) ::LongXi::Log::GetCoreLogger()->critical(__VA_ARGS__)

// --- LXEngine Macros ---
#define LX_ENGINE_TRACE(...) ::LongXi::Log::GetEngineLogger()->trace(__VA_ARGS__)
#define LX_ENGINE_INFO(...) ::LongXi::Log::GetEngineLogger()->info(__VA_ARGS__)
#define LX_ENGINE_WARN(...) ::LongXi::Log::GetEngineLogger()->warn(__VA_ARGS__)
#define LX_ENGINE_ERROR(...) ::LongXi::Log::GetEngineLogger()->error(__VA_ARGS__)
#define LX_ENGINE_CRITICAL(...) ::LongXi::Log::GetEngineLogger()->critical(__VA_ARGS__)

// --- LXShell Macros ---
#define LX_TRACE(...) ::LongXi::Log::GetShellLogger()->trace(__VA_ARGS__)
#define LX_INFO(...) ::LongXi::Log::GetShellLogger()->info(__VA_ARGS__)
#define LX_WARN(...) ::LongXi::Log::GetShellLogger()->warn(__VA_ARGS__)
#define LX_ERROR(...) ::LongXi::Log::GetShellLogger()->error(__VA_ARGS__)
#define LX_CRITICAL(...) ::LongXi::Log::GetShellLogger()->critical(__VA_ARGS__)

// --- LXGameMap Macros ---
#define LX_MAP_TRACE(...) ::LongXi::Log::GetShellLogger()->trace(__VA_ARGS__)
#define LX_MAP_INFO(...) ::LongXi::Log::GetShellLogger()->info(__VA_ARGS__)
#define LX_MAP_WARN(...) ::LongXi::Log::GetShellLogger()->warn(__VA_ARGS__)
#define LX_MAP_ERROR(...) ::LongXi::Log::GetShellLogger()->error(__VA_ARGS__)
#define LX_MAP_CRITICAL(...) ::LongXi::Log::GetShellLogger()->critical(__VA_ARGS__)
