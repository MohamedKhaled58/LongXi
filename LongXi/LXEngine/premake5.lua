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
		"Src",
		"../LXCore/Src"
	}
	
		links
	{
		"LXCore"
	}
