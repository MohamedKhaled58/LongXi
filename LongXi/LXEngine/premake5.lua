project "LXEngine"
    kind         "StaticLib"
    language     "C++"
    location     "."

	files
	{
		"Src/**.h",
		"Src/**.cpp",
	}

	-- DebugUI lives in LXShell by design (Engine stays UI-framework agnostic).
	removefiles
	{
		"Src/DebugUI/**.h",
		"Src/DebugUI/**.cpp"
	}

	includedirs
	{
		".",
		"Src",
		"%{IncludeDir.LXCore}",
		"%{IncludeDir.LXCoreSrc}"
		-- FR-002: LXEngine MUST NOT depend on LXGameMap (downward dependency forbidden)
		-- No LXGameMap or LXGameMapSrc include paths here
	}

	links
	{
		"LXCore",
		"d3d11",
		"dxgi",
		"dxguid",
		"d3dcompiler"
	}
