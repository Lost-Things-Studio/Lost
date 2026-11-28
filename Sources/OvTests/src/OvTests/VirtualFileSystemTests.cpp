/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Input/GameInput.h>
#include <OvCore/Localization/Localization.h>
#include <OvCore/SceneSystem/PrefabOperations.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/Scripting/ScriptEngine.h>
#include <OvCore/Terrain/Heightmap.h>
#include <OvGame/Core/LaunchOptions.h>
#include <OvGame/Utils/PngWriter.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/Package.h>
#include <OvTools/Filesystem/SaveStorage.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>
#include <OvTools/Time/Clock.h>
#include <OvTools/Utils/PathParser.h>

using OvDebug::ELogLevel;
using OvTools::Filesystem::PackageBuilder;
using OvTools::Filesystem::PackageReader;
using OvTools::Filesystem::ScopedMount;
using OvTools::Filesystem::VirtualFileSystem;

namespace
{
	using Files = std::vector<std::pair<std::string, std::string>>;

	// A package of these files, opened
	std::unique_ptr<PackageReader> Pack(const OvTests::TempFolder& p_folder, const Files& p_files, const std::string& p_name = "Assets.lopak")
	{
		PackageBuilder builder;
		std::string error;
		for (const auto& [path, content] : p_files)
		{
			REQUIRE_MESSAGE(builder.AddData(path, std::vector<char>(content.begin(), content.end()), error), error);
		}

		const auto package = p_folder.GetPath() / p_name;
		REQUIRE_MESSAGE(builder.Write(package, {}, error), error);
		auto reader = PackageReader::Open(package, error);
		REQUIRE_MESSAGE(reader, error);
		return reader;
	}

	std::string ReadText(const std::filesystem::path& p_path)
	{
		std::string text;
		std::string error;
		CHECK_MESSAGE(VirtualFileSystem::ReadText(p_path, text, error), error);
		return text;
	}

	std::string ReadDiskFile(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		std::stringstream bytes;
		bytes << file.rdbuf();
		return bytes.str();
	}

	// A folder no package and no file of the disk is in yet, under the temporary folder
	std::filesystem::path Assets(const OvTests::TempFolder& p_folder)
	{
		return p_folder.GetPath() / "Game" / "Assets";
	}

	// The working folder, changed for as long as it lives. A path relative to it holds on any drive, which a path made
	// relative to another folder does not on Windows: between two drives, std::filesystem::relative gives nothing
	class WorkingFolder final
	{
	public:
		explicit WorkingFolder(const std::filesystem::path& p_folder) :
			m_previous(std::filesystem::current_path())
		{
			std::filesystem::current_path(p_folder);
		}

		~WorkingFolder()
		{
			std::error_code error;
			std::filesystem::current_path(m_previous, error);
		}

		WorkingFolder(const WorkingFolder&) = delete;
		WorkingFolder& operator=(const WorkingFolder&) = delete;

	private:
		std::filesystem::path m_previous;
	};
}

TEST_SUITE("Virtual filesystem")
{
	TEST_CASE("a mounted package is read before the disk, which is read for anything else")
	{
		OvTests::TempFolder folder;
		const auto assets = Assets(folder);
		folder.WriteFile("Game/Assets/Both.txt", "disk");
		folder.WriteFile("Game/Assets/OnDisk.txt", "only on the disk");
		folder.WriteFile("Elsewhere.txt", "outside");

		CHECK(VirtualFileSystem::GetMountCount() == 0);
		CHECK(ReadText(assets / "Both.txt") == "disk");

		ScopedMount mount(assets, Pack(folder, { { "Both.txt", "package" }, { "Sub/Packed.txt", "packed" } }));
		CHECK(VirtualFileSystem::GetMountCount() == 1);

		CHECK(ReadText(assets / "Both.txt") == "package");
		CHECK(ReadText(assets / "Sub/Packed.txt") == "packed");
		CHECK(ReadText(assets / "OnDisk.txt") == "only on the disk");
		CHECK(ReadText(folder.GetPath() / "Elsewhere.txt") == "outside");

		// Normalised: a "..", a separator of Windows, a path relative to the working folder
		CHECK(ReadText(assets / "Sub" / ".." / "Both.txt") == "package");
		{
			// Given back before the temporary folder is removed, which Windows refuses for a working folder
			WorkingFolder working(folder.GetPath());
			CHECK(ReadText(std::filesystem::path("Game") / "Assets" / "Sub" / "Packed.txt") == "packed");
		}

		std::vector<char> bytes;
		std::string error;
		CHECK_FALSE(VirtualFileSystem::Read(assets / "Missing.txt", bytes, error));
		CHECK(error.ends_with("Missing.txt: not found"));
		CHECK_FALSE(VirtualFileSystem::Read(assets / "Sub", bytes, error));
	}

	TEST_CASE("files and folders are found in packages and on the disk, and listed together")
	{
		OvTests::TempFolder folder;
		const auto assets = Assets(folder);
		folder.WriteFile("Game/Assets/Translations/de.json", "{}");
		folder.WriteFile("Game/Assets/Translations/en.json", "{}");

		ScopedMount mount(assets, Pack(folder, {
			{ "Translations/en.json", "{}" }, { "Translations/fr.json", "{}" }, { "Translations/Old/ru.json", "{}" }, { "Root.txt", "" }
		}));

		CHECK(VirtualFileSystem::Exists(assets / "Translations/fr.json"));
		CHECK(VirtualFileSystem::Exists(assets / "Translations/de.json"));
		CHECK(VirtualFileSystem::Exists(assets / "Root.txt"));
		CHECK_FALSE(VirtualFileSystem::Exists(assets / "Translations"));
		CHECK_FALSE(VirtualFileSystem::Exists(assets / "translations/fr.json")); // Case counts in a package

		CHECK(VirtualFileSystem::IsDirectory(assets));
		CHECK(VirtualFileSystem::IsDirectory(assets / "Translations"));
		CHECK(VirtualFileSystem::IsDirectory(assets / "Translations/Old"));
		CHECK_FALSE(VirtualFileSystem::IsDirectory(assets / "Trans"));
		CHECK_FALSE(VirtualFileSystem::IsDirectory(assets / "Root.txt"));

		// Each name once, sorted; a sub-folder is not a file
		CHECK(VirtualFileSystem::List(assets / "Translations") == std::vector<std::string>{ "de.json", "en.json", "fr.json" });
		CHECK(VirtualFileSystem::List(assets) == std::vector<std::string>{ "Root.txt" });
		CHECK(VirtualFileSystem::List(assets / "Nowhere").empty());
	}

	TEST_CASE("the package mounted last is looked in first, and a package unmounted is no longer read")
	{
		OvTests::TempFolder folder;
		const auto assets = Assets(folder);

		VirtualFileSystem::Mount(assets, Pack(folder, { { "a.txt", "first" }, { "b.txt", "first" } }, "First.lopak"));
		VirtualFileSystem::Mount(assets / "", Pack(folder, { { "a.txt", "second" } }, "Second.lopak"));
		CHECK(VirtualFileSystem::GetMountCount() == 2);
		CHECK(ReadText(assets / "a.txt") == "second");
		CHECK(ReadText(assets / "b.txt") == "first");

		CHECK(VirtualFileSystem::Unmount(assets));
		CHECK_FALSE(VirtualFileSystem::Unmount(assets));
		CHECK(VirtualFileSystem::GetMountCount() == 0);
		CHECK_FALSE(VirtualFileSystem::Exists(assets / "a.txt"));

		VirtualFileSystem::Mount(assets, Pack(folder, { { "a.txt", "third" } }, "Third.lopak"));
		VirtualFileSystem::UnmountAll();
		CHECK(VirtualFileSystem::GetMountCount() == 0);
	}

	TEST_CASE("a file a package holds but cannot read is not looked for on the disk")
	{
		OvTests::TempFolder folder;
		const auto assets = Assets(folder);
		folder.WriteFile("Game/Assets/Noise.bin", "the disk's copy");

		// Bytes deflate cannot shrink: the package keeps them as they are
		std::string noise(64, '\0');
		uint32_t state = 12345;
		for (auto& byte : noise)
		{
			state = state * 1664525u + 1013904223u;
			byte = static_cast<char>(state >> 24);
		}
		Pack(folder, { { "Noise.bin", noise } });

		// The package's bytes changed after it was written: its index is whole, the file is not
		std::string bytes = ReadDiskFile(folder.GetPath() / "Assets.lopak");
		bytes[40] ^= 0x01;
		{
			std::ofstream file(folder.GetPath() / "Assets.lopak", std::ios::binary | std::ios::trunc);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}

		std::string error;
		auto reader = PackageReader::Open(folder.GetPath() / "Assets.lopak", error);
		REQUIRE_MESSAGE(reader, error);
		ScopedMount mount(assets, std::move(reader));

		std::vector<char> content;
		CHECK_FALSE(VirtualFileSystem::Read(assets / "Noise.bin", content, error));
		CHECK(error.ends_with("Noise.bin: Noise.bin is damaged: its CRC does not match"));
	}

	TEST_CASE("scenes, prefabs, metadata and the other name of a format are read from a package")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;
		const auto assets = Assets(folder);
		const std::string actor = "<actor><name>Kept</name><active>true</active><id>1</id><guid>11</guid><parent>0</parent><components/><behaviours/></actor>";

		ScopedMount mount(assets, Pack(folder, {
			{ "Scenes/Level.loscene", R"(<root version="1"><scene><actors>)" + actor + "</actors></scene></root>" },
			{ "Prefabs/Crate.loprefab", R"(<root version="1"><prefab><actors>)" + actor + "</actors></prefab></root>" },
			{ "Textures/Rock.png.meta", "format_version=1\nMIN_FILTER=9728\r\nMAG_FILTER=9729\n" }
		}));

		OvCore::SceneSystem::SceneManager manager(assets.string());
		manager.SetErrorDialogsEnabled(false);
		REQUIRE(manager.LoadScene("Scenes/Level.loscene"));
		CHECK(manager.GetCurrentScene()->FindActorByName("Kept"));

		// Asked for by Overload's name, it is found in the package by its own
		REQUIRE(manager.LoadScene("Scenes/Level.ovscene"));
		CHECK(std::filesystem::path(manager.GetCurrentSceneSourcePath()) == assets / "Scenes/Level.loscene");
		CHECK(OvTools::Utils::PathParser::FindFileToRead(assets / "Prefabs/Crate.ovprefab") == assets / "Prefabs/Crate.loprefab");

		auto& scene = *manager.GetCurrentScene();
		const auto create = [&scene]() -> OvCore::ECS::Actor& { return scene.CreateActor(); };
		CHECK(OvCore::SceneSystem::PrefabOperations::InstantiateFromFile(assets / "Prefabs/Crate.loprefab", create));

		// Read as bytes: a line of Windows keeps no "\r" in its value
		OvTools::Filesystem::IniFile metadata((assets / "Textures/Rock.png.meta").string());
		CHECK(metadata.Get<int>("MIN_FILTER") == 9728);
		CHECK(metadata.Get<int>("MAG_FILTER") == 9729);

		CHECK_FALSE(manager.LoadScene("Scenes/Missing.loscene"));
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "Failed to load scene"));
	}

	TEST_CASE("scripts, input actions, translations and height maps are read from a package")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;
		const auto assets = Assets(folder);

		// A square grey image of 3 by 3 samples
		const std::vector<uint8_t> grey = { 0, 0, 0, 128, 128, 128, 255, 255, 255, 0, 0, 0, 128, 128, 128, 255, 255, 255, 0, 0, 0, 128, 128, 128, 255, 255, 255 };
		const auto png = folder.GetPath() / "Height.png";
		REQUIRE(OvGame::Utils::WritePng(png, 3, 3, grey));

		ScopedMount mount(assets, Pack(folder, {
			{ "Scripts/Packed.lua", "return { answer = 42 }" },
			{ "Scripts/Broken.lua", "error('boom')" },
			{ "Settings/Actions.json", R"({ "actions": [ { "name": "Jump", "inputs": ["SPACE"] } ] })" },
			{ "Translations/en.json", R"({ "name": "English", "strings": { "menu.play": "Play" } })" },
			{ "Translations/fr.json", R"({ "name": "Français", "strings": { "menu.play": "Jouer" } })" },
			{ "Terrain/Height.png", ReadDiskFile(png) }
		}));

		// require, from the package, and an error that names the script's path
		OvTools::Time::Clock clock;
		OvCore::Scripting::ScriptEngine engine{ assets, OvTests::FindEngineResources() };
		OvCore::Global::ServiceLocator::Provide(engine);
		OvCore::Global::ServiceLocator::Provide(clock);

		const sol::object loaded = engine.GetScript("Scripts/Packed.lua");
		REQUIRE(loaded.get_type() == sol::type::table);
		CHECK(loaded.as<sol::table>()["answer"].get<int>() == 42);

		CHECK_FALSE(engine.GetScript("Scripts/Broken.lua").valid());
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "Scripts/Broken.lua:1: boom"));
		CHECK_FALSE(engine.GetScript("Scripts/Missing.lua").valid());
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "'Scripts/Missing.lua' not found"));

		// The project's input actions and translations
		const auto projectFile = folder.WriteFile("Game/Game.loproject", "input_actions=Settings/Actions.json\ntranslations=Translations\n");
		OvTools::Filesystem::IniFile project(projectFile.string());
		OvTools::Filesystem::SaveStorage storage(folder.GetPath() / "Saves");

		OvCore::Input::InputActions actions;
		OvCore::Input::LoadProjectActions(project, assets, storage, actions);
		CHECK(actions.GetBindings("Jump").has_value());

		OvCore::Localization::Localization localization;
		OvCore::Localization::LoadProjectTranslations(project, assets, storage, localization);
		const auto languages = localization.GetLanguages();
		REQUIRE(languages.size() == 2);
		CHECK(localization.Get("menu.play") == "Play");
		CHECK(localization.SetLanguage("fr"));
		CHECK(localization.Get("menu.play") == "Jouer");

		OvCore::Terrain::HeightmapImage image;
		const auto heightError = OvCore::Terrain::LoadHeightmap(assets / "Terrain/Height.png", image);
		CHECK_MESSAGE(!heightError, heightError.value_or(""));
		CHECK(image.size == 3);
		CHECK(image.values.size() == 9);
	}

	TEST_CASE("a game mounts the package next to an asset folder, and refuses a damaged one")
	{
		OvTests::TempFolder folder;
		const auto assets = Assets(folder);
		std::string message;

		// No package: nothing to mount, and nothing wrong
		CHECK(OvGame::Core::MountAssetPackage(assets, message));
		CHECK(message.empty());
		CHECK(VirtualFileSystem::GetMountCount() == 0);

		PackageBuilder builder;
		std::string error;
		REQUIRE(builder.AddData("Scenes/Start.loscene", { 'x' }, error));
		std::filesystem::create_directories(folder.GetPath() / "Game");
		REQUIRE_MESSAGE(builder.Write(folder.GetPath() / "Game" / "Assets.lopak", {}, error), error);

		CHECK(OvGame::Core::MountAssetPackage(assets / "", message));
		CHECK(message.starts_with("Mounted "));
		CHECK(message.find("Assets.lopak on ") != std::string::npos);
		CHECK(message.find("(1 files, fnv1a64:") != std::string::npos);
		CHECK(VirtualFileSystem::Exists(assets / "Scenes/Start.loscene"));
		VirtualFileSystem::UnmountAll();

		// A package of a newer version, or a damaged one, stops the game with why
		std::string bytes = ReadDiskFile(folder.GetPath() / "Game" / "Assets.lopak");
		bytes[4] = 2;
		{
			std::ofstream file(folder.GetPath() / "Game" / "Assets.lopak", std::ios::binary | std::ios::trunc);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		CHECK_FALSE(OvGame::Core::MountAssetPackage(assets, message));
		CHECK(message.ends_with("Assets.lopak: it is version 2 of the format, newer than this engine reads (version 1 at most)"));
		CHECK(VirtualFileSystem::GetMountCount() == 0);

		// The engine's resources may be only their package
		const auto engine = folder.GetPath() / "Engine";
		folder.WriteFile("Engine.lopak", "");
		const auto result = OvGame::Core::LaunchOptions::Parse({ "--engine", engine.string() });
		REQUIRE_MESSAGE(result.options, result.error);
		CHECK(result.options->GetEngineAssetsFolder() == engine);
	}
}
