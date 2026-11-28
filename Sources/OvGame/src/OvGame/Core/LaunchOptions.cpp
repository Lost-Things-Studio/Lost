/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <charconv>
#include <format>

#include <OvGame/Core/LaunchOptions.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>
#include <OvTools/Utils/PathParser.h>

namespace
{
	/**
	* The project file a --project argument names: the file itself, or the one project file of a folder
	*/
	std::optional<std::filesystem::path> FindProjectFile(const std::filesystem::path& p_path, std::string& p_error)
	{
		std::error_code error;

		if (std::filesystem::is_regular_file(p_path, error))
		{
			if (OvTools::Utils::PathParser::IsProjectFile(p_path))
				return p_path;

			p_error = "--project: " + p_path.string() + " is not a project file (.loproject or .ovproject)";
			return std::nullopt;
		}

		if (!std::filesystem::is_directory(p_path, error))
		{
			p_error = "--project: no file or folder " + p_path.string();
			return std::nullopt;
		}

		// Read once more for its error, which the listing of project files leaves out
		std::filesystem::directory_iterator readable(p_path, error);
		if (error)
		{
			p_error = "--project: " + p_path.string() + " cannot be read (" + error.message() + ")";
			return std::nullopt;
		}

		const auto found = OvTools::Utils::PathParser::FindProjectFiles(p_path);
		if (found.size() != 1)
		{
			p_error = "--project: " + p_path.string() + (found.empty() ? " holds no project file (.loproject or .ovproject)" : " holds several project files: name one");
			return std::nullopt;
		}

		return found.front();
	}
}

OvGame::Core::LaunchOptions::ParseResult OvGame::Core::LaunchOptions::Parse(const std::vector<std::string>& p_arguments)
{
	ParseResult result;
	LaunchOptions options;

	for (size_t index = 0; index < p_arguments.size(); ++index)
	{
		const std::string& argument = p_arguments[index];

		const auto next = [&]() -> std::optional<std::string>
		{
			if (index + 1 >= p_arguments.size() || p_arguments[index + 1].starts_with("--"))
			{
				result.error = argument + " needs a value";
				return std::nullopt;
			}

			return p_arguments[++index];
		};

		if (argument == "--help" || argument == "-h")
		{
			result.help = true;
			return result;
		}
		else if (argument == "--hidden-window")
		{
			options.hiddenWindow = true;
		}
		else if (argument == "--fail-on-error")
		{
			options.failOnError = true;
		}
		else if (argument == "--dump-lua-api")
		{
			options.dumpLuaApi = true;
		}
		else if (argument == "--audio")
		{
			const auto value = next();
			if (!value)
				return result;

			if (*value != "null" && *value != "auto")
			{
				result.error = "--audio: " + *value + " is not a driver (null, or auto for the system's device)";
				return result;
			}

			options.nullAudio = *value == "null";
		}
		else if (argument == "--project" || argument == "--engine" || argument == "--scene" || argument == "--frames" || argument == "--screenshot")
		{
			const auto value = next();
			if (!value)
				return result;

			if (argument == "--project")
			{
				options.projectFile = FindProjectFile(std::filesystem::absolute(*value), result.error);
				if (!options.projectFile)
					return result;
			}
			else if (argument == "--engine")
			{
				// A folder, or only its package next to it
				std::error_code error;
				options.engineAssets = std::filesystem::absolute(*value).lexically_normal();
				if (!options.engineAssets->has_filename())
				{
					options.engineAssets = options.engineAssets->parent_path(); // "Engine/" is "Engine"
				}
				auto package = *options.engineAssets;
				package += ".lopak";
				if (!std::filesystem::is_directory(*options.engineAssets, error) && !std::filesystem::is_regular_file(package, error))
				{
					result.error = "--engine: no folder or package " + *value;
					return result;
				}
			}
			else if (argument == "--scene")
			{
				options.scene = *value;
			}
			else if (argument == "--frames")
			{
				uint32_t frames = 0;
				const auto [end, error] = std::from_chars(value->data(), value->data() + value->size(), frames);
				if (error != std::errc{} || end != value->data() + value->size() || frames == 0)
				{
					result.error = "--frames: " + *value + " is not a number of frames above 0";
					return result;
				}
				options.frames = frames;
			}
			else
			{
				options.screenshot = std::filesystem::absolute(*value);
			}
		}
		else if (argument.starts_with("--"))
		{
			result.error = "unknown option " + argument + " (--help lists them)";
			return result;
		}
		else
		{
			result.ignored.push_back(argument);
		}
	}

	if (options.screenshot && !options.frames)
	{
		result.error = "--screenshot needs --frames: the image is the last frame";
		return result;
	}

	result.options = options;
	return result;
}

std::filesystem::path OvGame::Core::LaunchOptions::GetProjectSettingsFile() const
{
	return projectFile.value_or(std::filesystem::current_path() / "Data" / "User" / "Game.ini");
}

std::filesystem::path OvGame::Core::LaunchOptions::GetProjectAssetsFolder() const
{
	return projectFile ? projectFile->parent_path() / "Assets" : std::filesystem::current_path() / "Data" / "User" / "Assets";
}

std::filesystem::path OvGame::Core::LaunchOptions::GetEngineAssetsFolder() const
{
	return engineAssets.value_or(std::filesystem::current_path() / "Data" / "Engine");
}

bool OvGame::Core::MountAssetPackage(const std::filesystem::path& p_folder, std::string& p_message)
{
	auto folder = std::filesystem::absolute(p_folder).lexically_normal();
	if (!folder.has_filename())
	{
		folder = folder.parent_path();
	}

	auto package = folder;
	package += ".lopak";

	p_message.clear();
	std::error_code error;
	if (!std::filesystem::is_regular_file(package, error))
	{
		return true;
	}

	std::string reason;
	auto reader = OvTools::Filesystem::PackageReader::Open(package, reason);
	if (!reader)
	{
		p_message = package.string() + ": " + reason;
		return false;
	}

	p_message = std::format("Mounted {} on {} ({} files, {})", package.string(), folder.string(), reader->GetEntries().size(), reader->GetManifest().contentHash);
	OvTools::Filesystem::VirtualFileSystem::Mount(folder, std::move(reader));
	return true;
}

std::string OvGame::Core::LaunchOptions::GetUsage()
{
	return
		"Usage: OvGame [options]\n"
		"Without options, runs the built game: its project in Data/User, the engine in Data/Engine.\n"
		"An argument that does not start with -- is left aside.\n"
		"An asset folder's package next to it, its name followed by .lopak (Data/User/Assets.lopak), is read before it.\n"
		"  --project <folder or file>  run this project (a .loproject or .ovproject file, or the folder holding it), its assets in\n"
		"                              the Assets folder next to it\n"
		"  --engine <folder>           the engine's resources (a Lost checkout's Resources/Engine, or Engine.lopak alone)\n"
		"  --scene <path>              start on this scene, relative to the project's assets\n"
		"  --frames <n>                quit after n frames\n"
		"  --screenshot <file.png>     write the last frame to a PNG image (with --frames)\n"
		"  --hidden-window             do not show the window (never full screen)\n"
		"  --audio <null|auto>         null: mix the sounds and discard them, even with an audio device; auto (the\n"
		"                              default): the system's device, else the null driver\n"
		"  --fail-on-error             end with exit code 2 if an error was logged\n"
		"  --dump-lua-api              print the engine's Lua API, one name per line, and quit\n"
		"Exit codes: 0 on success, 1 for bad options, or a project, package or scene that cannot be read, 2 for logged\n"
		"errors (with --fail-on-error), 3 when the screenshot was not written.\n";
}
