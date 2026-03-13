#pragma once

#include <memory>
#include <spdlog/spdlog.h>

// =============================================================================
// Log — Central logging system for all LongXi modules
// Owns Spdlog initialization, logger creation, and shutdown.
// Each module gets a named logger with fixed-width prefixes for aligned output.
// =============================================================================

namespace LXCore
{

class Log
{
public:
    static void Initialize();
    static void Shutdown();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger()
    {
        return s_CoreLogger;
    }

    static std::shared_ptr<spdlog::logger>& GetEngineLogger()
    {
        return s_EngineLogger;
    }

    static std::shared_ptr<spdlog::logger>& GetShellLogger()
    {
        return s_ShellLogger;
    }

    static std::shared_ptr<spdlog::logger>& GetMapLogger()
    {
        return s_MapLogger;
    }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_EngineLogger;
    static std::shared_ptr<spdlog::logger> s_ShellLogger;
    static std::shared_ptr<spdlog::logger> s_MapLogger;
};

} // namespace LXCore
