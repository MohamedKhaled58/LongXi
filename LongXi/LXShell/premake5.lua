project "LXShell"
	kind         "WindowedApp"
	language     "C++"
	location     "."

	files
	{
		"Src/**.h",
		"Src/**.cpp"
	}

	includedirs
	{
		".",
		"Src",
		"%{IncludeDir.LXCore}",
		"%{IncludeDir.LXCoreSrc}",
		"%{IncludeDir.LXEngine}",
		"%{IncludeDir.LXEngineSrc}",
		"%{IncludeDir.LXGameMap}",
		"%{IncludeDir.LXGameMapSrc}"
	}

	links
	{
		"LXEngine",
		"LXGameMap",
		"LXCore"
	}

	filter { "system:Windows" }
		links
		{
			"user32",
			"gdi32",
			"d3d11",
			"d3dcompiler"
		}
	filter {}

	-- DebugUI is development-only.
	filter { "configurations:Release or Dist" }
		removefiles
		{
			"Src/DebugUI/**.h",
			"Src/DebugUI/**.cpp",
			"Src/ImGui/**.h",
			"Src/ImGui/**.cpp"
		}
	filter {}

	filter { "configurations:Debug" }
		includedirs
		{
			"%{IncludeDir.ImGui}",
			"%{IncludeDir.ImGuiBackends}"
		}

		files
		{
			"../../Vendor/imgui/imgui.cpp",
			"../../Vendor/imgui/imgui_draw.cpp",
			"../../Vendor/imgui/imgui_tables.cpp",
			"../../Vendor/imgui/imgui_widgets.cpp",
			"../../Vendor/imgui/imgui_demo.cpp",
			"../../Vendor/imgui/backends/imgui_impl_win32.cpp",
			"../../Vendor/imgui/backends/imgui_impl_dx11.cpp"
		}
	filter {}
