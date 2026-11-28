/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <format>
#include <span>

#include <OvDebug/Assertion.h>
#include <OvEditor/Core/GameBuilder.h>
#include <OvRendering/Data/Image.h>
#include <OvTools/Utils/PathParser.h>
#include <OvTools/Utils/SystemCalls.h>

namespace
{
	namespace fs = std::filesystem;
	using OvEditor::Core::EBuildType;
	using OvEditor::Core::GameBuildRequest;

	constexpr std::string_view kExecutableExtension =
#if defined(_WIN32)
		".exe";
#else
		"";
#endif

	fs::path GetBuilderExecutable(const fs::path& p_builderFolder, EBuildType p_buildType)
	{
		return p_builderFolder / OvEditor::Core::GetBuildTypeName(p_buildType) / ("OvGame" + std::string(kExecutableExtension));
	}

#if defined(_WIN32)
	// The project's window icon, given to the executable: a warning when it cannot be, the executable keeping its own
	void ApplyExecutableIcon(const GameBuildRequest& p_request, const fs::path& p_executable, std::vector<std::string>& p_warnings)
	{
		if (p_request.windowIcon.empty())
		{
			return;
		}

		const fs::path icon = OvTools::Utils::PathParser::GetRealPath(p_request.windowIcon, p_request.sources.engineAssets, p_request.sources.projectAssets);

		if (!fs::exists(icon))
		{
			p_warnings.push_back(std::format("Window icon \"{}\" was not found. Keeping default executable icon.", icon.string()));
		}
		else if (OvRendering::Data::Image image{ icon }; !image)
		{
			p_warnings.push_back(std::format("Failed to load window icon \"{}\". Keeping default executable icon.", icon.string()));
		}
		else if (image.isHDR)
		{
			p_warnings.push_back(std::format("Window icon \"{}\" is HDR and cannot be used for executable icons. Keeping default executable icon.", icon.string()));
		}
		else
		{
			const size_t byteCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u;
			const auto bytes = std::span<const uint8_t>{ static_cast<const uint8_t*>(image.data), byteCount };
			if (!OvTools::Utils::SystemCalls::SetExecutableIcon(p_executable, bytes, static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)))
			{
				p_warnings.push_back(std::format("Failed to apply executable icon from \"{}\". Keeping default executable icon.", icon.string()));
			}
		}
	}
#endif
}

std::string OvEditor::Core::GetBuildTypeName(EBuildType p_buildType)
{
	switch (p_buildType)
	{
		case EBuildType::Debug: return "Debug";
		case EBuildType::Release: return "Release";
		case EBuildType::Publish: return "Publish";
	}

	OVASSERT(false, "Unknown build type used.");
	return "";
}

OvEditor::Core::EBuildType OvEditor::Core::FindDefaultBuildType(const std::filesystem::path& p_builderFolder)
{
	for (const auto buildType : { EBuildType::Publish, EBuildType::Release, EBuildType::Debug })
	{
		if (std::error_code error; fs::exists(GetBuilderExecutable(p_builderFolder, buildType), error))
		{
			return buildType;
		}
	}

	return EBuildType::Release;
}

OvEditor::Core::GameBuildRequest OvEditor::Core::MakeGameBuildRequest(
	const OvTools::Filesystem::IniFile& p_projectSettings,
	const OvCore::Build::GameDataSources& p_sources,
	const std::filesystem::path& p_builderFolder,
	const std::filesystem::path& p_buildFolder
)
{
	GameBuildRequest request;
	request.sources = p_sources;
	request.builderFolder = p_builderFolder;
	request.buildFolder = p_buildFolder;

	const EBuildType defaultType = FindDefaultBuildType(p_builderFolder);
	const int buildType = p_projectSettings.GetOrDefault<int>("build_type", static_cast<int>(defaultType));
	request.buildType = buildType >= 0 && buildType <= static_cast<int>(EBuildType::Publish) ? static_cast<EBuildType>(buildType) : defaultType;

	if (const auto name = p_projectSettings.GetOrDefault<std::string>("executable_name", {}); !name.empty())
	{
		request.executableName = name;
	}

	request.windowIcon = p_projectSettings.GetOrDefault<std::string>("window_icon", {});
	request.options.packages = p_projectSettings.GetOrDefault<bool>("build_packages", true);
	request.options.projectId = p_projectSettings.GetOrDefault<std::string>("project_id", {});
	request.options.startScene = p_projectSettings.GetOrDefault<std::string>("start_scene", {});
	return request;
}

std::filesystem::path OvEditor::Core::GetGameExecutable(const GameBuildRequest& p_request)
{
	return p_request.buildFolder / (p_request.executableName + std::string(kExecutableExtension));
}

bool OvEditor::Core::BuildGame(const GameBuildRequest& p_request, OvCore::Build::GameDataReport& p_report, std::string& p_error)
{
	std::error_code error;
	if (fs::exists(p_request.buildFolder, error))
	{
		p_error = std::format("{} exists already: a game is built in a folder of its own", p_request.buildFolder.string());
		return false;
	}

	// Checked first: without it, there is no game to build the data for
	const fs::path builderExecutable = GetBuilderExecutable(p_request.builderFolder, p_request.buildType);
	if (!fs::is_regular_file(builderExecutable, error))
	{
		p_error = std::format(
			"there is no game executable for the {} build type at {}: build OvGame and OvEditor in that configuration first",
			GetBuildTypeName(p_request.buildType),
			builderExecutable.string()
		);
		return false;
	}

	fs::create_directories(p_request.buildFolder, error);
	if (error)
	{
		p_error = std::format("{} cannot be made: {}", p_request.buildFolder.string(), error.message());
		return false;
	}

	// A build that fails leaves nothing behind
	const auto fail = [&](std::string p_reason)
	{
		std::error_code removeError;
		fs::remove_all(p_request.buildFolder, removeError);
		p_error = std::move(p_reason);
		return false;
	};

	std::string dataError;
	if (!OvCore::Build::WriteGameData(p_request.sources, p_request.buildFolder, p_request.options, p_report, dataError))
	{
		return fail(dataError);
	}

	// The executable, and on Windows the libraries next to it
	fs::copy(builderExecutable.parent_path(), p_request.buildFolder, error);
	if (error)
	{
		return fail(std::format("the game executable cannot be copied from {}: {}", builderExecutable.parent_path().string(), error.message()));
	}

	const fs::path executable = GetGameExecutable(p_request);
	if (executable.filename() != builderExecutable.filename())
	{
		fs::rename(p_request.buildFolder / builderExecutable.filename(), executable, error);
		if (error)
		{
			return fail(std::format("the game executable cannot be renamed {}: {}", executable.filename().string(), error.message()));
		}
	}

#if defined(_WIN32)
	ApplyExecutableIcon(p_request, executable, p_report.warnings);
#endif

	return true;
}
