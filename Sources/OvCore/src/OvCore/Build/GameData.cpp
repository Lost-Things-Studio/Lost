/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cctype>
#include <format>

#include <OvCore/Build/GameData.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvRendering/Resources/Loaders/ModelLoader.h>
#include <OvRendering/Resources/PreparedModel.h>
#include <OvTools/Filesystem/Package.h>
#include <OvTools/Utils/PathParser.h>

namespace
{
	namespace fs = std::filesystem;
	namespace PreparedModel = OvRendering::Resources::PreparedModel;
	using OvCore::Build::GameDataReport;
	using OvTools::Utils::PathParser;

	std::string GenericPath(const fs::path& p_path)
	{
		const std::u8string text = p_path.generic_u8string();
		return { text.begin(), text.end() };
	}

	std::string ToLower(std::string p_text)
	{
		std::transform(p_text.begin(), p_text.end(), p_text.begin(), [](unsigned char p_char) { return static_cast<char>(std::tolower(p_char)); });
		return p_text;
	}

	// The regular files of a folder, in a fixed order. Links, which may lead out of it, are left out, as packages leave
	// them out
	bool ListFiles(const fs::path& p_folder, std::vector<fs::path>& p_files, std::string& p_error)
	{
		std::error_code error;
		for (fs::recursive_directory_iterator it(p_folder, error), end; !error && it != end; it.increment(error))
		{
			std::error_code typeError;
			if (it->is_symlink(typeError))
			{
				continue;
			}

			if (it->is_regular_file(typeError))
			{
				p_files.push_back(it->path());
			}
		}

		if (error)
		{
			p_error = std::format("{} cannot be read through: {}", p_folder.string(), error.message());
			return false;
		}

		std::sort(p_files.begin(), p_files.end());
		return true;
	}

	uint64_t FileSize(const fs::path& p_file)
	{
		std::error_code error;
		const auto size = fs::file_size(p_file, error);
		return error ? 0 : static_cast<uint64_t>(size);
	}

	// A model of a game's package, prepared: its import, as the game would make it, written once. False when it cannot
	// be imported
	bool PrepareModel(const fs::path& p_file, std::vector<char>& p_prepared)
	{
		const std::string realPath = p_file.string();
		const auto settings = OvCore::ResourceManagement::ModelManager::ReadImportSettings(realPath);

		OvRendering::Resources::ModelData model;
		if (!OvRendering::Resources::Loaders::ModelLoader::Import(realPath, settings.parserFlags, settings.generateEmbeddedAssets, model))
		{
			return false;
		}

		p_prepared = PreparedModel::Write(model);
		return true;
	}

	bool PackFolder(
		const fs::path& p_folder,
		const fs::path& p_package,
		const std::string& p_projectId,
		bool p_isEngine,
		GameDataReport& p_report,
		std::string& p_error
	)
	{
		std::vector<fs::path> files;
		if (!ListFiles(p_folder, files, p_error))
		{
			return false;
		}

		OvTools::Filesystem::PackageBuilder builder;
		std::string error;

		for (const auto& file : files)
		{
			const std::string path = GenericPath(file.lexically_relative(p_folder));
			if (p_isEngine && OvCore::Build::IsEditorOnlyEngineFile(path))
			{
				continue;
			}

			// What a build makes is never an asset of its own: a package, a prepared model
			if (const std::string name = ToLower(path); name.ends_with(".lopak") || name.ends_with(PreparedModel::kExtension))
			{
				p_report.warnings.push_back(std::format("{} is left out of the package: packages and prepared models are made by the build", path));
				continue;
			}

			p_report.sourceSize += FileSize(file);

			if (PathParser::GetFileType(path) == PathParser::EFileType::MODEL)
			{
				if (std::vector<char> prepared; PrepareModel(file, prepared))
				{
					if (!builder.AddData(PreparedModel::GetPath(path), std::move(prepared), error))
					{
						p_error = error;
						return false;
					}

					++p_report.preparedModelCount;
					continue;
				}

				p_report.warnings.push_back(std::format("{} cannot be imported: its file is kept in the package, for the game to import it as before", path));
			}

			if (!builder.AddFile(path, file, error))
			{
				p_error = error;
				return false;
			}
		}

		OvTools::Filesystem::PackageOptions options;
		options.projectId = p_projectId;
		if (!builder.Write(p_package, options, error))
		{
			p_error = error;
			return false;
		}

		p_report.fileCount += builder.GetFileCount();
		p_report.writtenSize += FileSize(p_package);
		return true;
	}

	bool CopyFolder(const fs::path& p_from, const fs::path& p_to, GameDataReport& p_report, std::string& p_error)
	{
		std::error_code error;
		fs::copy(p_from, p_to, fs::copy_options::recursive, error);
		if (error)
		{
			p_error = std::format("{} cannot be copied to {}: {}", p_from.string(), p_to.string(), error.message());
			return false;
		}

		std::vector<fs::path> files;
		if (!ListFiles(p_to, files, p_error))
		{
			return false;
		}

		p_report.fileCount += files.size();
		for (const auto& file : files)
		{
			const uint64_t size = FileSize(file);
			p_report.sourceSize += size;
			p_report.writtenSize += size;
		}
		return true;
	}
}

bool OvCore::Build::WriteGameData(
	const GameDataSources& p_sources,
	const std::filesystem::path& p_buildFolder,
	const GameDataOptions& p_options,
	GameDataReport& p_report,
	std::string& p_error
)
{
	const auto start = std::chrono::steady_clock::now();
	p_report = {};

	// Under either of its names, as the game finds it
	if (!p_options.startScene.empty())
	{
		const fs::path scene = PathParser::FindFileToRead(p_sources.projectAssets / PathParser::MakeNonWindowsStyle(p_options.startScene));
		if (std::error_code error; !fs::is_regular_file(scene, error))
		{
			p_error = std::format("the start scene \"{}\" is not in the project's assets", p_options.startScene);
			return false;
		}
	}

	const fs::path data = p_buildFolder / "Data";
	const fs::path user = data / "User";

	std::error_code error;
	fs::create_directories(user, error);
	if (error)
	{
		p_error = std::format("{} cannot be made: {}", user.string(), error.message());
		return false;
	}

	fs::copy_file(p_sources.projectFile, user / "Game.ini", fs::copy_options::overwrite_existing, error);
	if (error)
	{
		p_error = std::format("{} cannot be copied to {}: {}", p_sources.projectFile.string(), (user / "Game.ini").string(), error.message());
		return false;
	}

	const bool written = p_options.packages ?
		PackFolder(p_sources.projectAssets, user / "Assets.lopak", p_options.projectId, false, p_report, p_error) &&
		PackFolder(p_sources.engineAssets, data / "Engine.lopak", {}, true, p_report, p_error) :
		CopyFolder(p_sources.projectAssets, user / "Assets", p_report, p_error) &&
		CopyFolder(p_sources.engineAssets, data / "Engine", p_report, p_error);

	p_report.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	return written;
}

bool OvCore::Build::IsEditorOnlyEngineFile(std::string_view p_path)
{
	return p_path.starts_with("Lua/") || p_path.starts_with("Libraries/UI/Docs/");
}
