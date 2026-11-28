/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <OvCore/Helpers/AssetFormat.h>
#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvDebug/Logger.h>
#include <OvGame/Core/Application.h>
#include <OvGame/Core/LaunchOptions.h>
#include <OvRendering/Utils/Defines.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Profiling/TracyAllocators.h>

FORCE_DEDICATED_GPU

namespace
{
	int RunGame(int p_argc, char** p_argv)
	{
		std::vector<std::string> arguments;
		for (int index = 1; index < p_argc; ++index)
			arguments.emplace_back(p_argv[index]);

		const auto result = OvGame::Core::LaunchOptions::Parse(arguments);

		for (const std::string& ignored : result.ignored)
			std::cerr << "OvGame: " << ignored << " is no option, it is left aside\n";

		if (result.help)
		{
			std::cout << OvGame::Core::LaunchOptions::GetUsage();
			return EXIT_SUCCESS;
		}

		if (!result.options)
		{
			std::cerr << "OvGame: " << result.error << "\n\n" << OvGame::Core::LaunchOptions::GetUsage();
			return EXIT_FAILURE;
		}

		if (result.options->dumpLuaApi)
		{
			for (const auto& name : OvCore::Scripting::LuaApi::Describe())
				std::cout << name << '\n';
			return EXIT_SUCCESS;
		}

		// A project newer than this engine is not run: its settings may mean what this engine does not know
		{
			const auto projectFile = result.options->GetProjectSettingsFile();
			std::string error;
			if (!OvCore::Helpers::AssetFormat::ReadVersion(OvTools::Filesystem::IniFile(projectFile.string()), OvCore::Helpers::AssetFormat::kProjectVersion, projectFile.filename().string(), error))
			{
				std::cerr << "OvGame: " << error << '\n';
				return EXIT_FAILURE;
			}
		}

		// The packages next to the asset folders, mounted before anything is read from them. A package that cannot be
		// read is not run around: the game would miss its files one by one
		for (const auto& folder : { result.options->GetProjectAssetsFolder(), result.options->GetEngineAssetsFolder() })
		{
			std::string message;
			if (!OvGame::Core::MountAssetPackage(folder, message))
			{
				std::cerr << "OvGame: " << message << '\n';
				return EXIT_FAILURE;
			}

			if (!message.empty())
			{
				OVLOG_INFO(message);
			}
		}

		// Errors are counted over the whole run, from the start scene's loading to the game's destruction
		std::atomic<uint32_t> errorCount = 0;
		const auto logListener = OvDebug::Logger::LogEvent += [&errorCount](const OvDebug::LogData& p_log)
		{
			if (p_log.logLevel == OvDebug::ELogLevel::LOG_ERROR)
				++errorCount;
		};

		int exitCode = EXIT_SUCCESS;
		{
			OvGame::Core::Application app(*result.options);
			exitCode = app.Run();
		}

		OvDebug::Logger::LogEvent -= logListener;

		if (exitCode == EXIT_SUCCESS && result.options->failOnError && errorCount > 0)
			exitCode = 2;

		return exitCode;
	}
}

#ifdef _DEBUG
int main(int argc, char** argv)
{
	return RunGame(argc, argv);
}
#else
	#ifdef _WIN32
		#undef APIENTRY
		#include "Windows.h"
		INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
		{
			// A windowed build has no console: started from a terminal with options, it writes to that terminal
			if (__argc > 1 && AttachConsole(ATTACH_PARENT_PROCESS))
			{
				FILE* stream = nullptr;
				freopen_s(&stream, "CONOUT$", "w", stdout);
				freopen_s(&stream, "CONOUT$", "w", stderr);
			}

			return RunGame(__argc, __argv);
		}
	#else
		int main(int argc, char** argv)
		{
			return RunGame(argc, argv);
		}
	#endif
#endif
