/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace OvCore::Build
{
	/**
	* Where a game's data comes from
	*/
	struct GameDataSources
	{
		std::filesystem::path projectFile; // The project file (.loproject, .ovproject), the game's Data/User/Game.ini
		std::filesystem::path projectAssets; // The project's Assets folder
		std::filesystem::path engineAssets; // The engine's resources (the editor's Data/Engine)
	};

	/**
	* How a game's data is written
	*/
	struct GameDataOptions
	{
		bool packages = true; // Packages with prepared models, or folders as the sources are
		std::string projectId; // In the packages' manifests
		std::string startScene; // The scene the game starts on, relative to the project's assets, checked first
	};

	/**
	* What was written
	*/
	struct GameDataReport
	{
		size_t fileCount = 0; // Files in the packages, or copied
		size_t preparedModelCount = 0;
		uint64_t sourceSize = 0; // In bytes: the files read
		uint64_t writtenSize = 0; // In bytes: the packages, or the copies
		std::vector<std::string> warnings; // A model kept as its file, a file left aside
		std::chrono::milliseconds duration{};
	};

	/**
	* Writes a game's data in a build's folder, which must exist (see Docs/Building.md): Data/User/Game.ini, then the
	* project's assets and the engine's resources, as two packages (Data/User/Assets.lopak, Data/Engine.lopak) or as two
	* folders (Data/User/Assets, Data/Engine).
	* In the packages, each model is prepared (<model>.lomodel) and its file left out, the engine's files that only the
	* editor uses are left out, and the same sources give the same bytes. Needs no graphics context.
	* False, with why, when the start scene is not in the assets (before anything is written), or when a file cannot be
	* read, packed (a name a package refuses) or written
	* @param p_sources
	* @param p_buildFolder
	* @param p_options
	* @param p_report
	* @param p_error
	*/
	bool WriteGameData(
		const GameDataSources& p_sources,
		const std::filesystem::path& p_buildFolder,
		const GameDataOptions& p_options,
		GameDataReport& p_report,
		std::string& p_error
	);

	/**
	* Whether a file of the engine's resources is used by the editor only, and left out of a game's package: the Lua
	* definitions of the language server (Lua/), the documentation of the UI library (Libraries/UI/Docs/)
	* @param p_path relative to the engine's resources, with "/"
	*/
	bool IsEditorOnlyEngineFile(std::string_view p_path);
}
