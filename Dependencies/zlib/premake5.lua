-- zlib 1.2.13, as assimp ships it: one copy, for assimp's importers and for the engine's packages (.lopak)
project "zlib"
	kind "StaticLib"
	language "C"
	targetdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	objdir (objoutdir .. "%{cfg.buildcfg}/%{prj.name}")
	warnings "Off"

	files {
		"*.h",
		"*.c",
		"premake5.lua"
	}

	includedirs {
		"./"
	}

	filter { "system:linux" }
		defines { "HAVE_UNISTD_H" }

	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"

	filter { "configurations:Release or configurations:Publish" }
		defines { "NDEBUG" }
		optimize "On"
