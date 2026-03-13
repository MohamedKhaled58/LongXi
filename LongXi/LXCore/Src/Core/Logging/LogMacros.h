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
#define LX_CORE_TRACE(...) ::LXCore::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LX_CORE_INFO(...) ::LXCore::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LX_CORE_WARN(...) ::LXCore::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LX_CORE_ERROR(...) ::LXCore::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LX_CORE_CRITICAL(...) ::LXCore::Log::GetCoreLogger()->critical(__VA_ARGS__)

// --- LXEngine Macros ---
#define LX_ENGINE_TRACE(...) ::LXCore::Log::GetEngineLogger()->trace(__VA_ARGS__)
#define LX_ENGINE_INFO(...) ::LXCore::Log::GetEngineLogger()->info(__VA_ARGS__)
#define LX_ENGINE_WARN(...) ::LXCore::Log::GetEngineLogger()->warn(__VA_ARGS__)
#define LX_ENGINE_ERROR(...) ::LXCore::Log::GetEngineLogger()->error(__VA_ARGS__)
#define LX_ENGINE_CRITICAL(...) ::LXCore::Log::GetEngineLogger()->critical(__VA_ARGS__)

// --- LXShell Macros ---
#define LX_TRACE(...) ::LXCore::Log::GetShellLogger()->trace(__VA_ARGS__)
#define LX_INFO(...) ::LXCore::Log::GetShellLogger()->info(__VA_ARGS__)
#define LX_WARN(...) ::LXCore::Log::GetShellLogger()->warn(__VA_ARGS__)
#define LX_ERROR(...) ::LXCore::Log::GetShellLogger()->error(__VA_ARGS__)
#define LX_CRITICAL(...) ::LXCore::Log::GetShellLogger()->critical(__VA_ARGS__)

// --- LXGameMap Macros ---
#define LX_MAP_TRACE(...) ::LXCore::Log::GetShellLogger()->trace(__VA_ARGS__)
#define LX_MAP_INFO(...) ::LXCore::Log::GetShellLogger()->info(__VA_ARGS__)
#define LX_MAP_WARN(...) ::LXCore::Log::GetShellLogger()->warn(__VA_ARGS__)
#define LX_MAP_ERROR(...) ::LXCore::Log::GetShellLogger()->error(__VA_ARGS__)
#define LX_MAP_CRITICAL(...) ::LXCore::Log::GetShellLogger()->critical(__VA_ARGS__)
