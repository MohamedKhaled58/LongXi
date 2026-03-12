#include "Core/Logging/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

// Static member definitions
std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_EngineLogger;
std::shared_ptr<spdlog::logger> Log::s_ShellLogger;
std::shared_ptr<spdlog::logger> Log::s_MapLogger;

void Log::Initialize()
{
    // Shared console sink with color support
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // Fixed-width module prefixes for aligned console output (FR-007)
    // %-8n pads logger name to 8 chars
    const std::string pattern = "%^[%H:%M:%S.%e] [%-8n] [%-7l] %v%$";

    s_CoreLogger   = std::make_shared<spdlog::logger>("LXCore", console_sink);
    s_EngineLogger = std::make_shared<spdlog::logger>("LXEngine", console_sink);
    s_ShellLogger  = std::make_shared<spdlog::logger>("LXShell", console_sink);
    s_MapLogger    = std::make_shared<spdlog::logger>("LXGameMap", console_sink);

    s_CoreLogger->set_pattern(pattern);
    s_EngineLogger->set_pattern(pattern);
    s_ShellLogger->set_pattern(pattern);
    s_MapLogger->set_pattern(pattern);

    s_CoreLogger->set_level(spdlog::level::trace);
    s_EngineLogger->set_level(spdlog::level::trace);
    s_ShellLogger->set_level(spdlog::level::trace);
    s_MapLogger->set_level(spdlog::level::trace);

    // Register globally
    spdlog::register_logger(s_CoreLogger);
    spdlog::register_logger(s_EngineLogger);
    spdlog::register_logger(s_ShellLogger);
    spdlog::register_logger(s_MapLogger);

    spdlog::set_default_logger(s_EngineLogger);

    LX_CORE_INFO("Logging initialized");
}

void Log::Shutdown()
{
    LX_CORE_INFO("Logging shutting down");

    s_CoreLogger.reset();
    s_EngineLogger.reset();
    s_ShellLogger.reset();
    s_MapLogger.reset();

    spdlog::shutdown();
}

} // namespace LongXi
