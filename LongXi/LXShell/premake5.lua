project "LXShell"
    kind         "WindowedApp"
    language     "C++"
    location     "."

	files
	{
		"Src/**.h",
		"Src/**.cpp",
	}

	includedirs
	{
		"Src",
		"../LXCore",
		"../LXCore/Src",
		"../LXEngine",
		"../LXEngine/Src"
	}

	links
	{
		"LXEngine",
		"LXCore"
	}

	filter { "system:Windows" }
		links
		{
			"user32",
			"gdi32"
		}
	filter {}
