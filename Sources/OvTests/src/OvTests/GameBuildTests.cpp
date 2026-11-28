/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/Build/GameData.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvEditor/Core/GameBuilder.h>
#include <OvGame/Core/LaunchOptions.h>
#include <OvRendering/Resources/Loaders/ModelLoader.h>
#include <OvRendering/Resources/PreparedModel.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/Package.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>

using OvCore::Build::GameDataOptions;
using OvCore::Build::GameDataReport;
using OvCore::Build::WriteGameData;
using OvEditor::Core::EBuildType;
using OvTools::Filesystem::PackageReader;
using OvTools::Filesystem::VirtualFileSystem;
namespace PreparedModel = OvRendering::Resources::PreparedModel;

namespace
{
	constexpr std::string_view kExecutableExtension =
#if defined(_WIN32)
		".exe";
#else
		"";
#endif

	constexpr std::string_view kProjectFile = "start_scene=Scenes/Start.loscene\nproject_id=tests.build\n";

	std::string ReadDiskFile(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		std::stringstream bytes;
		bytes << file.rdbuf();
		return bytes.str();
	}

	std::string Box()
	{
		return
			"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
			"vt 0 0\nvt 1 0\nvt 0 1\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1\n";
	}

	/**
	* A project and an engine, small: a scene, a model with its metadata, a file no import reads, what a build makes
	* (left out of the packages), and the engine's files that only the editor uses
	*/
	struct TestProject
	{
		OvTests::TempFolder folder;
		OvCore::Build::GameDataSources sources;

		TestProject()
		{
			folder.WriteFile("Project/Game.loproject", kProjectFile);
			folder.WriteFile("Project/Assets/Scenes/Start.loscene", "<root/>");
			folder.WriteFile("Project/Assets/Models/Box.obj", Box());
			folder.WriteFile("Project/Assets/Models/Box.obj.meta", "FLIP_UVS=true\n");
			folder.WriteFile("Project/Assets/Models/Broken.fbx", "not a model");
			folder.WriteFile("Project/Assets/Notes.txt", "kept");
			folder.WriteFile("Project/Assets/Stray.lopak", "a package is not an asset");
			folder.WriteFile("Project/Assets/Models/Old.obj.lomodel", "nor is a prepared model");

			folder.WriteFile("Engine/Shaders/Unlit.lofx", "#shader vertex");
			folder.WriteFile("Engine/Models/Cube.obj", Box());
			folder.WriteFile("Engine/Lua/Api.lua", "---@meta");
			folder.WriteFile("Engine/Libraries/UI/Init.lua", "return {}");
			folder.WriteFile("Engine/Libraries/UI/Docs/Guide.md", "# Guide");

			sources = {
				folder.GetPath() / "Project" / "Game.loproject",
				folder.GetPath() / "Project" / "Assets",
				folder.GetPath() / "Engine"
			};
		}

		GameDataOptions Options(bool p_packages = true) const
		{
			GameDataOptions options;
			options.packages = p_packages;
			options.projectId = "tests.build";
			options.startScene = "Scenes/Start.loscene";
			return options;
		}
	};

	std::unique_ptr<PackageReader> OpenPackage(const std::filesystem::path& p_package)
	{
		std::string error;
		auto reader = PackageReader::Open(p_package, error);
		REQUIRE_MESSAGE(reader, error);
		return reader;
	}

	std::vector<std::string> Paths(PackageReader& p_package)
	{
		std::vector<std::string> paths;
		for (const auto& entry : p_package.GetEntries())
		{
			paths.push_back(entry.path);
		}
		return paths;
	}

	std::vector<char> ReadEntry(PackageReader& p_package, std::string_view p_path)
	{
		const auto* entry = p_package.Find(p_path);
		REQUIRE_MESSAGE(entry, p_path);

		std::vector<char> bytes;
		std::string error;
		REQUIRE_MESSAGE(p_package.Read(*entry, bytes, error), error);
		return bytes;
	}

	std::vector<char> Prepare(const std::filesystem::path& p_model, OvRendering::Resources::Parsers::EModelParserFlags p_flags, bool p_generateEmbeddedAssets)
	{
		OvRendering::Resources::ModelData model;
		REQUIRE(OvRendering::Resources::Loaders::ModelLoader::Import(p_model.string(), p_flags, p_generateEmbeddedAssets, model));
		return PreparedModel::Write(model);
	}

	bool Mentions(const std::vector<std::string>& p_warnings, std::string_view p_text)
	{
		return std::any_of(p_warnings.begin(), p_warnings.end(), [&](const std::string& p_warning) { return p_warning.find(p_text) != std::string::npos; });
	}

	// The packages a test mounts are unmounted whatever happens to it
	struct UnmountAtEnd
	{
		~UnmountAtEnd()
		{
			VirtualFileSystem::UnmountAll();
		}
	};

	// A builder's folder with the game executables of some build types, and a library next to each
	std::filesystem::path MakeBuilder(const OvTests::TempFolder& p_folder, std::initializer_list<std::string_view> p_buildTypes)
	{
		for (const auto buildType : p_buildTypes)
		{
			p_folder.WriteFile(std::filesystem::path("Builder") / buildType / ("OvGame" + std::string(kExecutableExtension)), "game");
			p_folder.WriteFile(std::filesystem::path("Builder") / buildType / "Helper.dll", "library");
		}
		return p_folder.GetPath() / "Builder";
	}
}

TEST_SUITE("Game build")
{
	TEST_CASE("a game's data is written as packages, with prepared models and without what only the editor uses")
	{
		TestProject project;
		const auto build = project.folder.GetPath() / "Build";

		GameDataReport report;
		std::string error;
		REQUIRE_MESSAGE(WriteGameData(project.sources, build, project.Options(), report, error), error);

		CHECK(ReadDiskFile(build / "Data" / "User" / "Game.ini") == kProjectFile);
		CHECK_FALSE(std::filesystem::exists(build / "Data" / "User" / "Assets"));
		CHECK_FALSE(std::filesystem::exists(build / "Data" / "Engine"));

		// A model's file is replaced by its prepared version; its metadata, and what no import reads, stay
		auto assets = OpenPackage(build / "Data" / "User" / "Assets.lopak");
		CHECK(Paths(*assets) == std::vector<std::string>{ "Models/Box.obj.lomodel", "Models/Box.obj.meta", "Models/Broken.fbx", "Notes.txt", "Scenes/Start.loscene" });
		CHECK(assets->GetManifest().projectId == "tests.build");

		auto engine = OpenPackage(build / "Data" / "Engine.lopak");
		CHECK(Paths(*engine) == std::vector<std::string>{ "Libraries/UI/Init.lua", "Models/Cube.obj.lomodel", "Shaders/Unlit.lofx" });
		CHECK(engine->GetManifest().projectId.empty());

		CHECK(report.preparedModelCount == 2);
		CHECK(report.fileCount == 8);
		CHECK(report.writtenSize == std::filesystem::file_size(build / "Data" / "User" / "Assets.lopak") + std::filesystem::file_size(build / "Data" / "Engine.lopak"));
		CHECK(report.warnings.size() == 3);
		CHECK(Mentions(report.warnings, "Models/Broken.fbx cannot be imported"));
		CHECK(Mentions(report.warnings, "Stray.lopak is left out"));
		CHECK(Mentions(report.warnings, "Models/Old.obj.lomodel is left out"));

		// Prepared with the model's own settings (its .meta flips the texture coordinates)
		const auto box = project.sources.projectAssets / "Models" / "Box.obj";
		const auto settings = OvCore::ResourceManagement::ModelManager::ReadImportSettings(box.string());
		CHECK(ReadEntry(*assets, "Models/Box.obj.lomodel") == Prepare(box, settings.parserFlags, settings.generateEmbeddedAssets));
		CHECK(ReadEntry(*assets, "Models/Box.obj.lomodel") != Prepare(box, settings.parserFlags & ~OvRendering::Resources::Parsers::EModelParserFlags::FLIP_UVS, settings.generateEmbeddedAssets));
	}

	TEST_CASE("the same project gives the same packages")
	{
		TestProject project;
		GameDataReport report;
		std::string error;
		REQUIRE_MESSAGE(WriteGameData(project.sources, project.folder.GetPath() / "First", project.Options(), report, error), error);
		REQUIRE_MESSAGE(WriteGameData(project.sources, project.folder.GetPath() / "Second", project.Options(), report, error), error);

		for (const auto& package : { std::filesystem::path("Data/User/Assets.lopak"), std::filesystem::path("Data/Engine.lopak") })
		{
			CAPTURE(package.string());
			CHECK(ReadDiskFile(project.folder.GetPath() / "First" / package) == ReadDiskFile(project.folder.GetPath() / "Second" / package));
		}
	}

	TEST_CASE("a built game reads its data from its packages, its models from their prepared versions")
	{
		TestProject project;
		const auto build = project.folder.GetPath() / "Build";
		GameDataReport report;
		std::string error;
		REQUIRE_MESSAGE(WriteGameData(project.sources, build, project.Options(), report, error), error);

		UnmountAtEnd unmount;
		const auto assets = build / "Data" / "User" / "Assets";
		const auto engine = build / "Data" / "Engine";
		std::string message;
		REQUIRE(OvGame::Core::MountAssetPackage(assets, message));
		REQUIRE(OvGame::Core::MountAssetPackage(engine, message));

		std::string text;
		REQUIRE(VirtualFileSystem::ReadText(assets / "Scenes" / "Start.loscene", text, error));
		CHECK(text == "<root/>");
		CHECK(VirtualFileSystem::Exists(engine / "Shaders" / "Unlit.lofx"));
		CHECK_FALSE(VirtualFileSystem::Exists(engine / "Lua" / "Api.lua"));

		// No model's file is there: only its prepared version
		CHECK_FALSE(VirtualFileSystem::Exists(assets / "Models" / "Box.obj"));
		OvRendering::Resources::ModelData model;
		REQUIRE(OvCore::ResourceManagement::ModelManager::LoadModelData((assets / "Models" / "Box.obj").string(), model));
		REQUIRE(model.meshes.size() == 1);
		CHECK(model.meshes[0].indices.size() == 3);

		REQUIRE(OvCore::ResourceManagement::ModelManager::LoadModelData((engine / "Models" / "Cube.obj").string(), model));
		CHECK(model.meshes.size() == 1);
	}

	TEST_CASE("a game's data is written as folders, as the sources are, when packages are off")
	{
		TestProject project;
		const auto build = project.folder.GetPath() / "Build";
		GameDataReport report;
		std::string error;
		REQUIRE_MESSAGE(WriteGameData(project.sources, build, project.Options(false), report, error), error);

		CHECK(ReadDiskFile(build / "Data" / "User" / "Game.ini") == kProjectFile);
		CHECK(ReadDiskFile(build / "Data" / "User" / "Assets" / "Models" / "Box.obj") == Box());
		CHECK(std::filesystem::exists(build / "Data" / "User" / "Assets" / "Stray.lopak"));
		CHECK(std::filesystem::exists(build / "Data" / "Engine" / "Lua" / "Api.lua"));
		CHECK_FALSE(std::filesystem::exists(build / "Data" / "User" / "Assets.lopak"));
		CHECK_FALSE(std::filesystem::exists(build / "Data" / "Engine.lopak"));
		CHECK(report.fileCount == 12);
		CHECK(report.preparedModelCount == 0);
		CHECK(report.warnings.empty());
	}

	TEST_CASE("a game's data is refused when its start scene is missing or a file cannot be packed")
	{
		TestProject project;
		const auto build = project.folder.GetPath() / "Build";
		GameDataReport report;
		std::string error;

		auto options = project.Options();
		options.startScene = "Scenes/Missing.loscene";
		CHECK_FALSE(WriteGameData(project.sources, build, options, report, error));
		CHECK(error == "the start scene \"Scenes/Missing.loscene\" is not in the project's assets");
		CHECK_FALSE(std::filesystem::exists(build / "Data"));

		// Found under the other name of its format, as the game finds it
		options.startScene = "Scenes/Start.ovscene";
		CHECK_MESSAGE(WriteGameData(project.sources, project.folder.GetPath() / "Other", options, report, error), error);

#if !defined(_WIN32)
		// A name a package cannot hold stops the build, and names the file
		project.folder.WriteFile("Project/Assets/Bad:Name.txt", "");
		CHECK_FALSE(WriteGameData(project.sources, project.folder.GetPath() / "Third", project.Options(), report, error));
		CHECK(error.find("Bad:Name.txt") != std::string::npos);
#endif
	}

	TEST_CASE("a project's build takes its settings, or their defaults")
	{
		OvTests::TempFolder folder;
		const auto builder = MakeBuilder(folder, { "Debug", "Release" });
		CHECK(OvEditor::Core::FindDefaultBuildType(builder) == EBuildType::Release);
		CHECK(OvEditor::Core::FindDefaultBuildType(MakeBuilder(folder, { "Publish" })) == EBuildType::Publish);
		CHECK(OvEditor::Core::FindDefaultBuildType(folder.GetPath() / "Nowhere") == EBuildType::Release);

		const auto file = folder.WriteFile("Game.loproject",
			"build_type=0\nexecutable_name=Adventure\nbuild_packages=false\nproject_id=tests.build\n"
			"start_scene=Scenes/Start.loscene\nwindow_icon=Textures/Icon.png\n");
		const OvTools::Filesystem::IniFile settings(file.string());
		const auto request = OvEditor::Core::MakeGameBuildRequest(settings, {}, builder, folder.GetPath() / "Build");
		CHECK(request.buildType == EBuildType::Debug);
		CHECK(request.executableName == "Adventure");
		CHECK_FALSE(request.options.packages);
		CHECK(request.options.projectId == "tests.build");
		CHECK(request.options.startScene == "Scenes/Start.loscene");
		CHECK(request.windowIcon == "Textures/Icon.png");
		CHECK(OvEditor::Core::GetGameExecutable(request) == folder.GetPath() / "Build" / ("Adventure" + std::string(kExecutableExtension)));

		const OvTools::Filesystem::IniFile empty(folder.WriteFile("Empty.loproject", "").string());
		const auto defaults = OvEditor::Core::MakeGameBuildRequest(empty, {}, folder.GetPath() / "Nowhere", folder.GetPath() / "Build");
		CHECK(defaults.buildType == EBuildType::Release);
		CHECK(defaults.executableName == "Game");
		CHECK(defaults.options.packages);
		CHECK(defaults.options.projectId.empty());
		CHECK(defaults.options.startScene.empty());
	}

	TEST_CASE("a game is built in a folder of its own, its executable renamed, or not at all")
	{
		TestProject project;
		const auto builder = MakeBuilder(project.folder, { "Release" });
		const OvTools::Filesystem::IniFile settings(project.folder.WriteFile("Settings.loproject",
			"build_type=1\nexecutable_name=Adventure\nstart_scene=Scenes/Start.loscene\nproject_id=tests.build\n").string());

		auto request = OvEditor::Core::MakeGameBuildRequest(settings, project.sources, builder, project.folder.GetPath() / "Game");
		GameDataReport report;
		std::string error;
		REQUIRE_MESSAGE(OvEditor::Core::BuildGame(request, report, error), error);

		const auto executable = project.folder.GetPath() / "Game" / ("Adventure" + std::string(kExecutableExtension));
		CHECK(ReadDiskFile(executable) == "game");
		CHECK_FALSE(std::filesystem::exists(project.folder.GetPath() / "Game" / ("OvGame" + std::string(kExecutableExtension))));
		CHECK(ReadDiskFile(project.folder.GetPath() / "Game" / "Helper.dll") == "library");
		CHECK(std::filesystem::exists(project.folder.GetPath() / "Game" / "Data" / "User" / "Assets.lopak"));
		CHECK(report.preparedModelCount == 2);

		// Never over a folder that is there
		CHECK_FALSE(OvEditor::Core::BuildGame(request, report, error));
		CHECK(error.find("exists already") != std::string::npos);

		// Without the executable of its build type, nothing is written
		request.buildFolder = project.folder.GetPath() / "Published";
		request.buildType = EBuildType::Publish;
		CHECK_FALSE(OvEditor::Core::BuildGame(request, report, error));
		CHECK(error.find("no game executable for the Publish build type") != std::string::npos);
		CHECK_FALSE(std::filesystem::exists(request.buildFolder));

		// A build that fails leaves nothing behind
		request.buildType = EBuildType::Release;
		request.options.startScene = "Scenes/Missing.loscene";
		CHECK_FALSE(OvEditor::Core::BuildGame(request, report, error));
		CHECK(error.find("Scenes/Missing.loscene") != std::string::npos);
		CHECK_FALSE(std::filesystem::exists(request.buildFolder));
	}
}
