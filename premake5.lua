-- =============================================================================
-- LongXi — Root Workspace
-- Generator: Win-Generate Project.bat (uses vs2026 action).
-- =============================================================================

workspace "LongXi"
	platforms      { "x64" }
	systemversion  "latest"
	cppdialect     "C++23"
	startproject "LXShell"

IncludeDir = {}
IncludeDir["Spdlog"]        = path.getabsolute("Vendor/Spdlog/include")
IncludeDir["ImGui"]         = path.getabsolute("Vendor/imgui")
IncludeDir["ImGuiBackends"] = path.getabsolute("Vendor/imgui/backends")
IncludeDir["LXCore"]        = path.getabsolute("LongXi/LXCore")
IncludeDir["LXCoreSrc"]     = path.getabsolute("LongXi/LXCore/Src")
IncludeDir["LXEngine"]      = path.getabsolute("LongXi/LXEngine")
IncludeDir["LXEngineSrc"]   = path.getabsolute("LongXi/LXEngine/Src")
IncludeDir["LXGameMap"]     = path.getabsolute("LongXi/LXGameMap")
IncludeDir["LXGameMapSrc"]  = path.getabsolute("LongXi/LXGameMap/Src")

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	buildoptions   { "/utf-8", "/Gm-" }

	includedirs
	{
		"%{IncludeDir.Spdlog}"
	}

	targetdir "%{wks.location}/Build/%{cfg.buildcfg}/%{prj.group}"
	objdir    "%{wks.location}/Build/Obj/%{cfg.buildcfg}/%{prj.group}/%{prj.name}"

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
		optimize "On"
		multiprocessorcompile "On"

	filter { "system:Windows" }
		defines "LX_PLATFORM_WINDOWS"

	filter {}

	-- =========================================================================
	-- Vendor
	-- =========================================================================
	group "Vendor"
		-- Dear ImGui sources are compiled directly by LXShell in development builds.
	group ""

	-- =========================================================================
	-- Modules/Core  — zero or minimal deps; used by everyone
	-- =========================================================================
group "Core"
		include "LongXi/LXCore"
		-- LXEngine contains renderer backend modules under Src/Renderer/Backend/DX11 (including resource system files).
		include "LongXi/LXEngine"
		include "LongXi/LXGameMap"
	group ""

	group "Executables"
		include "LongXi/LXShell"
	group ""
