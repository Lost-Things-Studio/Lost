project "LostPack"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	targetdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	objdir (objoutdir .. "%{cfg.buildcfg}/%{prj.name}")
	debugdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	fatalwarnings { "All" }

	files {
		"**.h",
		"**.cpp",
		"**.lua"
	}

	includedirs {
		-- Lost SDK
		"%{wks.location}/Sources/OvTools/include",

		-- Current project
		"include"
	}

	links {
		"OvTools",
		"zlib" -- Used by OvTools, so after it
	}

	filter { "system:windows" }
		characterset ("MBCS")

	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"

	filter { "configurations:Release or configurations:Publish" }
		defines { "NDEBUG" }
		optimize "Speed"
