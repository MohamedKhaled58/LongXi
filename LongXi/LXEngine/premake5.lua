project "LXEngine"
    kind         "StaticLib"
    language     "C++"
    location     "."

	files
	{
		"Src/**.h",
		"Src/**.cpp",
	}

	includedirs
	{
		".",
		"Src",
		"../LXCore",
		"../LXCore/Src"
	}

	links
	{
		"LXCore",
		"d3d11",
		"dxgi",
		"dxguid"
	}
