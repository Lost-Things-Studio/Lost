/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cctype>
#include <charconv>

#include <OvGame/Core/LaunchOptions.h>

namespace
{
	/**
	* Whether a path names a project file, whatever the case of its extension (Windows names ignore it)
	*/
	bool IsProjectFile(const std::filesystem::path& p_path)
	{
		std::string extension = p_path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](char p_character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(p_character))); });
		return extension == ".ovproject";
	}

	/**
	* The project file a --project argument names: the file itself, or the one .ovproject of a folder
	*/
	std::optional<std::filesystem::path> FindProjectFile(const std::filesystem::path& p_path, std::string& p_error)
	{
		std::error_code error;

		if (std::filesystem::is_regular_file(p_path, error))
		{
			if (IsProjectFile(p_path))
				return p_path;

			p_error = "--project: " + p_path.string() + " is not a .ovproject file";
			return std::nullopt;
		}

		if (!std::filesystem::is_directory(p_path, error))
		{
			p_error = "--project: no file or folder " + p_path.string();
			return std::nullopt;
		}

		std::vector<std::filesystem::path> found;
		for (std::filesystem::directory_iterator entry(p_path, error), end; !error && entry != end; entry.increment(error))
		{
			std::error_code entryError;
			if (entry->is_regular_file(entryError) && IsProjectFile(entry->path()))
				found.push_back(entry->path());
		}

		if (error)
		{
			p_error = "--project: " + p_path.string() + " cannot be read (" + error.message() + ")";
			return std::nullopt;
		}

		if (found.size() != 1)
		{
			p_error = "--project: " + p_path.string() + (found.empty() ? " holds no .ovproject file" : " holds several .ovproject files: name one");
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
				std::error_code error;
				options.engineAssets = std::filesystem::absolute(*value);
				if (!std::filesystem::is_directory(*options.engineAssets, error))
				{
					result.error = "--engine: no folder " + *value;
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

std::string OvGame::Core::LaunchOptions::GetUsage()
{
	return
		"Usage: OvGame [options]\n"
		"Without options, runs the built game: its project in Data/User, the engine in Data/Engine.\n"
		"An argument that does not start with -- is left aside.\n"
		"  --project <folder or file>  run this project (a .ovproject file, or the folder holding it), its assets in\n"
		"                              the Assets folder next to it\n"
		"  --engine <folder>           the engine's resources (a Lost checkout's Resources/Engine)\n"
		"  --scene <path>              start on this scene, relative to the project's assets\n"
		"  --frames <n>                quit after n frames\n"
		"  --screenshot <file.png>     write the last frame to a PNG image (with --frames)\n"
		"  --hidden-window             do not show the window (never full screen)\n"
		"  --audio <null|auto>         null: mix the sounds and discard them, even with an audio device; auto (the\n"
		"                              default): the system's device, else the null driver\n"
		"  --fail-on-error             end with exit code 2 if an error was logged\n"
		"  --dump-lua-api              print the engine's Lua API, one name per line, and quit\n"
		"Exit codes: 0 on success, 1 for bad options or a scene that cannot be loaded, 2 for logged errors\n"
		"(with --fail-on-error), 3 when the screenshot was not written.\n";
}
