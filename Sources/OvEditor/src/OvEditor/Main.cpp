/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <string_view>

#include <OvCore/Helpers/AssetFormat.h>
#include <OvEditor/Core/Application.h>
#include <OvEditor/Core/GameBuilder.h>
#include <OvEditor/Core/ProjectHub.h>
#include <OvEditor/Settings/EditorSettings.h>
#include <OvEditor/Utils/FileSystem.h>
#include <OvEditor/Utils/ProjectManagement.h>

#include <OvRendering/Utils/Defines.h>
#include <OvRendering/Utils/ResourceTracking.h>

#include <OvTools/Profiling/TracyAllocators.h>
#include <OvTools/Utils/String.h>

#include <OvDebug/Logger.h>

#ifdef _WIN32
#undef APIENTRY
#include "Windows.h"
#else
#include <cstdlib>
#endif

FORCE_DEDICATED_GPU

namespace
{
	/**
	* When Overload is launched from a project file, we should consider the executable path as
	* the current working directory
	* @param p_executablePath
	*/
	void UpdateWorkingDirectory(const std::string& p_executablePath)
	{
#ifdef _WIN32
		if (!IsDebuggerPresent())
#endif
		{
			std::filesystem::current_path(OvTools::Utils::PathParser::GetContainingFolder(p_executablePath));
		}
	}

	bool TryRun(const std::filesystem::path& projectPath)
	{
		const auto errorEvent = [](OvWindowing::Context::EDeviceError, std::string errMsg) {
			errMsg = "Overload requires OpenGL 4.5 or newer.\r\n" + errMsg;
#ifdef _WIN32
			MessageBox(0, errMsg.c_str(), "Lost", MB_OK | MB_ICONSTOP);
#else
			// Use zenity for error message on Linux
			std::string command = "zenity --error --title=\"Overload\" --text=\"" + errMsg + "\" 2>/dev/null";
			std::system(command.c_str());
#endif
		};

		std::unique_ptr<OvEditor::Core::Application> app;

		try
		{
			auto listenerId = OvWindowing::Context::Device::ErrorEvent += errorEvent;
			app = std::make_unique<OvEditor::Core::Application>(projectPath);
			OvWindowing::Context::Device::ErrorEvent -= listenerId;
		}
		catch (...) {}

		if (app)
		{
			app->Run();
			return true;
		}

		return false;
	}
}

namespace
{
	/**
	* OvEditor --build <project folder> <build folder> [--type Debug|Release|Publish]: builds the project's game as the
	* Build menu does, without opening a window (see Docs/Building.md), in the project's build type or the one given.
	* 0 when it is built, 1 when it cannot be, 2 for a wrong command line
	* @param argc
	* @param argv
	*/
	int BuildFromCommandLine(int argc, char** argv)
	{
		constexpr std::string_view kUsage = "Usage: OvEditor --build <project folder> <build folder> [--type Debug|Release|Publish]\n";

		std::optional<OvEditor::Core::EBuildType> buildType;
		if (argc == 6 && std::string_view(argv[4]) == "--type")
		{
			using enum OvEditor::Core::EBuildType;
			for (const auto type : { Debug, Release, Publish })
			{
				if (OvEditor::Core::GetBuildTypeName(type) == argv[5])
				{
					buildType = type;
				}
			}
		}

		if (argc != 4 && !buildType)
		{
			std::cerr << kUsage;
			return 2;
		}

		const std::filesystem::path projectFolder = std::filesystem::absolute(argv[2]);
		if (!OvEditor::Utils::ProjectManagement::IsValidProjectDirectory(projectFolder))
		{
			std::cerr << "OvEditor: no project file in " << projectFolder.string() << '\n';
			return 2;
		}

		const auto projectFile = OvEditor::Utils::ProjectManagement::GetProjectFile(projectFolder);
		const OvTools::Filesystem::IniFile projectSettings(projectFile.string());

		// A project newer than this engine is not built, as the editor does not open it
		if (std::string error; !OvCore::Helpers::AssetFormat::ReadVersion(projectSettings, OvCore::Helpers::AssetFormat::kProjectVersion, projectFile.filename().string(), error))
		{
			std::cerr << "OvEditor: " << error << '\n';
			return 1;
		}

		// The engine's resources and the game executables are next to the editor, else in the working folder, where the
		// editor looks for them
		std::filesystem::path editorFolder = std::filesystem::absolute(argv[0]).parent_path();
		if (std::error_code error; !std::filesystem::is_directory(editorFolder / "Data" / "Engine", error))
		{
			editorFolder = std::filesystem::current_path();
		}

		auto request = OvEditor::Core::MakeGameBuildRequest(
			projectSettings,
			{ projectFile, projectFolder / "Assets", editorFolder / "Data" / "Engine" },
			editorFolder / "Builder",
			std::filesystem::absolute(argv[3])
		);
		request.buildType = buildType.value_or(request.buildType);

		std::cout << std::format(
			"Building the game in {} at {}, its assets {}\n",
			OvEditor::Core::GetBuildTypeName(request.buildType),
			request.buildFolder.string(),
			request.options.packages ? "in packages" : "in folders"
		);

		OvCore::Build::GameDataReport report;
		std::string error;
		if (!OvEditor::Core::BuildGame(request, report, error))
		{
			std::cerr << "OvEditor: the game could not be built: " << error << '\n';
			return 1;
		}

		for (const auto& warning : report.warnings)
		{
			std::cerr << "OvEditor: warning: " << warning << '\n';
		}

		constexpr double kMebibyte = 1024.0 * 1024.0;
		std::cout << std::format(
			"Built {} in {:.1f} s: {} files, {} prepared models, {:.1f} MB into {:.1f} MB\n",
			OvEditor::Core::GetGameExecutable(request).string(),
			static_cast<double>(report.duration.count()) / 1000.0,
			report.fileCount,
			report.preparedModelCount,
			static_cast<double>(report.sourceSize) / kMebibyte,
			static_cast<double>(report.writtenSize) / kMebibyte
		);
		return 0;
	}
}

int main(int argc, char** argv)
{
	// A build from the command line opens no window
	if (argc >= 2 && std::string_view(argv[1]) == "--build")
	{
		return BuildFromCommandLine(argc, argv);
	}

	// UpdateWorkingDirectory(argv[0]);

	OvEditor::Settings::EditorSettings::Load();
	OvRendering::Utils::ResourceTracking::TrackResources();

#ifndef WIN32
	if (std::system("command -v zenity >/dev/null 2>&1") != 0) {
		OVLOG_ERROR("Zenity is required to run Overload!");
	}
#endif

	std::optional<std::filesystem::path> projectPath;

	if (argc < 2)
	{
		// No project file given as argument ==> Open the ProjectHub
		OvEditor::Core::ProjectHub hub;

		if (auto result = hub.Run())
		{
			projectPath = result->projectPath;
		}
	}
	else
	{
		// Project file given as argument ==> Open the project
		projectPath = argv[1];
	}

	if (projectPath)
	{
		if (!OvEditor::Utils::ProjectManagement::IsValidProjectDirectory(projectPath.value()))
		{
			// Invalid project path
			return EXIT_FAILURE;
		}

		// Make sure the project is registered in the project registry
		OvEditor::Utils::ProjectManagement::RegisterProject(projectPath.value());

		if (!TryRun(projectPath.value()))
		{
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

#ifdef _WIN32
#ifndef _DEBUG
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
	main(__argc, __argv);
}
#endif
#endif
