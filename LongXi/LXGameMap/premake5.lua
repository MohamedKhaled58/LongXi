project "LXGameMap"
    kind     "StaticLib"
    language "C++"
    location "."

    files
    {
        "Src/**.h",
        "Src/**.cpp",
    }

    includedirs
    {
        ".",
        "Src",
        "%{IncludeDir.LXCore}",
        "%{IncludeDir.LXCoreSrc}",
        "%{IncludeDir.LXEngine}",
        "%{IncludeDir.LXEngineSrc}",
    }
