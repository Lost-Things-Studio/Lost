-- Function to read the version number from VERSION.txt: Lost's own version
local function readVersion()
	local versionFile = io.open("VERSION.txt", "r")
	if versionFile then
		local version = versionFile:read("*l")
		versionFile:close()
		return version
	else
		error("Could not open VERSION.txt")
	end
end

local version = readVersion()

-- The version of Overload that Lost is based on: update it with each sync from Overload
local overloadVersion = "1.9"

workspace "Lost"
	configurations { "Debug", "Release", "Publish" }
	platforms { "x64" }
	startproject "OvEditor"
	defines {
		"LUA_SCRIPTING",
		"LOST_VERSION=\"" .. version .. "\"",
		"OVERLOAD_VERSION=\"" .. overloadVersion .. "\"",
		"SOL_NO_LUA_HPP"
	}

	filter {"configurations:Debug or configurations:Release"}
		defines {
			"TRACY_ENABLE",
			"TRACY_ON_DEMAND",
		}

	-- Memory profiling overrides the global new/delete operators, which adds overhead to every allocation.
	-- Keep it for Debug builds only, so Release builds stay representative of the shipped performance.
	filter {"configurations:Debug"}
		defines {
			"TRACY_MEMORY_ENABLE",
		}
	filter{}

	-- Set toolset based on operating system
	filter {"system:windows"}
		toolset("msc")
		buildoptions { "/FS" } -- Prevent C1041 PDB write conflicts with parallel compilation
	filter {"system:linux"}
		toolset("clang") -- Use Clang on Linux (sol2 doesn't work well with GCC)
	filter {}

	-- Disable "Hot Reload": Doesn't work with Tracy.
	editandcontinue "Off"

outputdir = "%{wks.location}/Bin/"
objoutdir = "%{wks.location}/Bin-Int/"
dependdir = "%{wks.location}/Dependencies/"
builddir = "%{wks.location}/Build/"
resdir = "%{wks.location}/Resources/"

group "Dependencies"
	include "Dependencies/ImGui"
	include "Dependencies/tinyxml2"
	include "Dependencies/tracy"
	include "Dependencies/lua"
	include "Dependencies/freetype"
	include "Dependencies/soloud"
	include "Dependencies/zlib"
	include "Dependencies/assimp"
	include "Dependencies/glfw"
	include "Dependencies/jolt"
	include "Dependencies/recastnavigation"
	include "Dependencies/baregl"
group ""

group "Dependencies/Prebuilt"
	include "Dependencies/stb_image"
	include "Dependencies/sol"
	include "Dependencies/doctest"
group ""

group "Lost SDK"
	include "Sources/OvAudio"
	include "Sources/OvCore"
	include "Sources/OvDebug"
	include "Sources/OvMaths"
	include "Sources/OvPhysics"
	include "Sources/OvRendering"
	include "Sources/OvTools"
	include "Sources/OvUI"
	include "Sources/OvWindowing"
group ""

group "Lost Apps"
	include "Sources/OvEditor"
	include "Sources/OvGame"
group ""

group "Lost Tests"
	include "Sources/OvTests"
group ""

include "Resources"
