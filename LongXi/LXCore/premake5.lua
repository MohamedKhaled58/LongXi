project "LXCore"
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
	}
