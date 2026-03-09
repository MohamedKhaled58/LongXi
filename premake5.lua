-- =============================================================================
-- LongXi — Root Workspace
-- Generator: Win-Generate Project.bat (uses vs2026 action).
-- =============================================================================

workspace "LongXi"
    platforms      { "x64" }
    systemversion  "latest"
    cppdialect     "C++23"
	startproject "LXShell"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

    buildoptions   { "/utf-8", "/Gm-" }

    includedirs { "Vendor/Spdlog/include" }

    targetdir "Build/%{cfg.buildcfg}/%{prj.group}"
    objdir    "Build/Obj/%{cfg.buildcfg}/%{prj.group}/%{prj.name}"

	filter "configurations:Debug"
		defines "LX_DEBUG"
		runtime "Debug"
		symbols "on"
        multiprocessorcompile "On"

	filter "configurations:Release"
		defines "LX_RELEASE"
		runtime "Release"
		optimize "on"
        multiprocessorcompile "On"

	filter "configurations:Dist"
		defines "LX_DIST"
		runtime "Release"
		optimize "on"
        multiprocessorcompile "On"

    filter { "system:Windows" }
        defines "LX_PLATFORM_WINDOWS"

    filter {}

    -- =========================================================================
    -- Vendor
    -- =========================================================================
    group "Vendor"

    group ""

    -- =========================================================================
    -- Modules/Core  — zero or minimal deps; used by everyone
    -- =========================================================================
    group "Core"
        include "LongXi/LXCore"
        include "LongXi/LXEngine"
    group ""

    group "Executables"
        include "LongXi/LXShell"
    group ""