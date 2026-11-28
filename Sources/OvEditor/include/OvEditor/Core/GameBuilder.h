/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>

#include <OvCore/Build/GameData.h>
#include <OvTools/Filesystem/IniFile.h>

namespace OvEditor::Core
{
	enum class EBuildType : int
	{
		Debug = 0,
		Release = 1,
		Publish = 2
	};

	/**
	* Returns the name of a build type
	* @param p_buildType
	*/
	std::string GetBuildTypeName(EBuildType p_buildType);

	/**
	* The build type a project starts with: the most optimized one whose game executable is in the builder's folder,
	* Release when there is none. Debug builds are unoptimized and run with synchronous OpenGL debug output, so they
	* are the default only when no other builder is there
	* @param p_builderFolder the editor's Builder folder, one folder per build type
	*/
	EBuildType FindDefaultBuildType(const std::filesystem::path& p_builderFolder);

	/**
	* What a game's build is made of (see Docs/Building.md)
	*/
	struct GameBuildRequest
	{
		OvCore::Build::GameDataSources sources;
		OvCore::Build::GameDataOptions options;
		std::filesystem::path builderFolder; // The editor's Builder folder, one folder per build type
		std::filesystem::path buildFolder; // Where the game is built, which must not exist
		EBuildType buildType = EBuildType::Release;
		std::string executableName = "Game"; // Without its extension
		std::string windowIcon; // An asset path, given to the executable on Windows
	};

	/**
	* The request for a project's build, from its settings: build_type, executable_name, build_packages, project_id,
	* start_scene and window_icon, each with its default when the file does not set it
	* @param p_projectSettings
	* @param p_sources
	* @param p_builderFolder
	* @param p_buildFolder
	*/
	GameBuildRequest MakeGameBuildRequest(
		const OvTools::Filesystem::IniFile& p_projectSettings,
		const OvCore::Build::GameDataSources& p_sources,
		const std::filesystem::path& p_builderFolder,
		const std::filesystem::path& p_buildFolder
	);

	/**
	* Builds a game, without a window: the folder, the game's data (OvCore::Build::WriteGameData), then the game's
	* executable from the builder's folder, renamed. False, with why, when the build folder exists already, the
	* executable of the build type is missing, or a step fails; the build folder is then removed
	* @param p_request
	* @param p_report
	* @param p_error
	*/
	bool BuildGame(const GameBuildRequest& p_request, OvCore::Build::GameDataReport& p_report, std::string& p_error);

	/**
	* The path of a built game's executable
	* @param p_request
	*/
	std::filesystem::path GetGameExecutable(const GameBuildRequest& p_request);
}
