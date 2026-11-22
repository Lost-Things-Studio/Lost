/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace OvCore::Helpers::FormatConversion
{
	/**
	* A line of a Lua script whose strings changed
	*/
	struct LuaChange
	{
		std::filesystem::path file; // Relative to the project's folder
		size_t line = 0; // From 1
		std::string before;
		std::string after;
	};

	/**
	* What a conversion did, its paths relative to the project's folder
	*/
	struct Report
	{
		std::vector<std::pair<std::filesystem::path, std::filesystem::path>> renamed;
		std::vector<std::filesystem::path> rewritten; // Under their names once converted
		std::vector<LuaChange> luaChanges;
		std::vector<std::string> problems; // What was left as it was, and why

		bool HasChanges() const;
	};

	/**
	* Moves a project to the Lost formats (see Docs/AssetFormats.md):
	* - each file under one of Overload's names (".ovscene", ".ovmat"...) in its Assets folder, and its project file,
	*   takes the Lost name of its format, with its metadata (<asset>.meta) if it has some;
	* - every reference to them, and to the engine's resources now under their Lost names, follows: in the project file,
	*   scenes, prefabs, materials, shaders and animation graphs;
	* - in Lua scripts, a string that is exactly such a path changes, and each line changed is reported, since a script
	*   may build a path the conversion cannot see.
	* A file whose Lost name is taken already is left as it is, and so are the references to it. Running it again changes
	* nothing. The files keep their version: they take the latest when the editor saves them
	* @param p_projectFolder (holding the project file and its Assets folder)
	* @param p_engineAssets (the engine's resources, for the references starting with ':')
	*/
	Report ConvertProject(const std::filesystem::path& p_projectFolder, const std::filesystem::path& p_engineAssets);

	/**
	* The report as text: a summary line, then what was renamed, rewritten, changed in scripts, and left as it was
	* @param p_report
	*/
	std::string Describe(const Report& p_report);
}
