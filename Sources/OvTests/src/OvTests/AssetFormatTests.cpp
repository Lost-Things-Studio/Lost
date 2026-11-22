/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <fstream>
#include <sstream>
#include <string>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/Animation/AnimationGraph.h>
#include <OvCore/ECS/Actor.h>
#include <OvCore/Helpers/AssetFormat.h>
#include <OvCore/ResourceManagement/AResourceManager.h>
#include <OvCore/Resources/Loaders/MaterialLoader.h>
#include <OvCore/SceneSystem/PrefabOperations.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Utils/PathParser.h>

namespace AssetFormat = OvCore::Helpers::AssetFormat;
using OvDebug::ELogLevel;
using OvTools::Utils::PathParser;

namespace
{
	std::string ReadText(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		std::stringstream text;
		text << file.rdbuf();
		return text.str();
	}

	/**
	* The version an XML root of this text has, or nothing and the error
	*/
	std::optional<int> XmlVersion(const std::string& p_root, std::string& p_error)
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse((p_root + "</root>").c_str()) == tinyxml2::XML_SUCCESS);
		return AssetFormat::ReadVersion(*document.FirstChildElement("root"), 1, "Test.loscene", p_error);
	}

	/**
	* A resource that is the path of the file it was read from
	*/
	struct FormatTestResource
	{
		std::filesystem::path file;
	};

	class FormatTestManager final : public OvCore::ResourceManagement::AResourceManager<FormatTestResource>
	{
	public:
		int created = 0;

	protected:
		FormatTestResource* CreateResource(const std::filesystem::path& p_path) override
		{
			const auto file = GetRealPath(p_path);
			if (!std::filesystem::exists(file))
			{
				return nullptr;
			}

			++created;
			return new FormatTestResource{ file };
		}

		void DestroyResource(FormatTestResource* p_resource) override { delete p_resource; }
		void ReloadResource(FormatTestResource*, const std::filesystem::path&) override {}
	};

	std::optional<std::string> ParseGraph(const std::string& p_text)
	{
		OvCore::Animation::GraphDefinition definition;
		return OvCore::Animation::GraphDefinition::Parse(p_text, definition);
	}

	constexpr const char* kGraphLayers = R"("layers": [ { "name": "Base", "states": [ { "name": "Idle" } ] } ])";
}

TEST_SUITE("Asset formats")
{
	TEST_CASE("each format is one file type under its Lost name and under Overload's")
	{
		using EFileType = PathParser::EFileType;

		const std::pair<const char*, EFileType> names[] = {
			{ "scene", EFileType::SCENE },
			{ "prefab", EFileType::PREFAB },
			{ "mat", EFileType::MATERIAL },
			{ "fx", EFileType::SHADER },
			{ "fxh", EFileType::SHADER_PART },
			{ "anim", EFileType::ANIMATION_GRAPH },
		};

		for (const auto& [name, type] : names)
		{
			CAPTURE(name);
			CHECK(PathParser::GetFileType(std::string("Assets/A.lo") + name) == type);
			CHECK(PathParser::GetFileType(std::string("Assets/A.ov") + name) == type);
			CHECK(PathParser::GetFileType(std::string("Assets/A.LO") + name) == type);
		}

		CHECK(PathParser::GetFileType("Models/Rock.gltf") == EFileType::MODEL);
	}

	TEST_CASE("a format's other name is found, and only a format's")
	{
		CHECK(PathParser::GetFormatAlias("Scenes/Level.ovscene") == "Scenes/Level.loscene");
		CHECK(PathParser::GetFormatAlias("Scenes/Level.loscene") == "Scenes/Level.ovscene");
		CHECK(PathParser::GetFormatAlias(":Shaders/Standard.ovfx") == ":Shaders/Standard.lofx");
		CHECK(PathParser::GetFormatAlias("Shaders/Common.lofxh") == "Shaders/Common.ovfxh");
		CHECK(PathParser::GetFormatAlias("Game.OVPROJECT") == "Game.loproject");
		CHECK(PathParser::GetFormatAlias("Graphs/Hero.loanim") == "Graphs/Hero.ovanim");

		CHECK(PathParser::GetFormatAlias("Models/Rock.fbx").empty());
		CHECK(PathParser::GetFormatAlias("Scripts/Player.lua").empty());
		CHECK(PathParser::GetFormatAlias("Rock.png.meta").empty());
		CHECK(PathParser::GetFormatAlias("Folder").empty());
	}

	TEST_CASE("a file to read is the one asked for, or else the one under its format's other name")
	{
		OvTests::TempFolder folder;
		const auto lost = folder.WriteFile("Scenes/Level.loscene", "<root/>");
		const auto both = folder.WriteFile("Scenes/Both.ovscene", "<root/>");
		folder.WriteFile("Scenes/Both.loscene", "<root/>");

		CHECK(PathParser::FindFileToRead(lost) == lost);
		CHECK(PathParser::FindFileToRead(folder.GetPath() / "Scenes/Level.ovscene") == lost);
		CHECK(PathParser::FindFileToRead(both) == both);

		// Neither: the name asked for, for the reader to say it is missing
		const auto missing = folder.GetPath() / "Scenes/Missing.ovscene";
		CHECK(PathParser::FindFileToRead(missing) == missing);
		const auto model = folder.GetPath() / "Models/Rock.fbx";
		CHECK(PathParser::FindFileToRead(model) == model);
	}

	TEST_CASE("a project file is a .loproject or a .ovproject, the Lost one first")
	{
		CHECK(PathParser::IsProjectFile("Game.loproject"));
		CHECK(PathParser::IsProjectFile("Game.ovproject"));
		CHECK(PathParser::IsProjectFile("Game.LOPROJECT"));
		CHECK_FALSE(PathParser::IsProjectFile("Game.ini"));
		CHECK_FALSE(PathParser::IsProjectFile("loproject"));

		OvTests::TempFolder folder;
		folder.WriteFile("B.ovproject", "");
		folder.WriteFile("A.ovproject", "");
		folder.WriteFile("Z.loproject", "");
		folder.WriteFile("readme.txt", "");
		folder.WriteFile("Sub/C.loproject", "");

		const auto found = PathParser::FindProjectFiles(folder.GetPath());
		REQUIRE(found.size() == 3);
		CHECK(found[0].filename() == "Z.loproject");
		CHECK(found[1].filename() == "A.ovproject");
		CHECK(found[2].filename() == "B.ovproject");

		CHECK(PathParser::FindProjectFiles(folder.GetPath() / "Missing").empty());
	}

	TEST_CASE("an XML file's version is its root's attribute, 0 without one, and a newer one is refused")
	{
		std::string error;
		CHECK(XmlVersion("<root>", error) == 0);
		CHECK(XmlVersion(R"(<root version="0">)", error) == 0);
		CHECK(XmlVersion(R"(<root version="1">)", error) == 1);
		CHECK(error.empty());

		CHECK_FALSE(XmlVersion(R"(<root version="2">)", error));
		CHECK(error == "Test.loscene is version 2 of its format, newer than this engine reads (version 1 at most)");

		CHECK_FALSE(XmlVersion(R"(<root version="one">)", error));
		CHECK(error.find("not a whole number") != std::string::npos);
		CHECK_FALSE(XmlVersion(R"(<root version="-1">)", error));
		CHECK(error.find("not a version") != std::string::npos);

		tinyxml2::XMLDocument document;
		auto* root = document.NewElement("root");
		document.InsertFirstChild(root);
		AssetFormat::WriteVersion(*root, 3);
		CHECK(root->IntAttribute("version") == 3);
	}

	TEST_CASE("an INI file's version is its key format_version, and its keys are written by name")
	{
		OvTests::TempFolder folder;
		std::string error;

		{
			OvTools::Filesystem::IniFile ini(folder.WriteFile("Old.ovproject", "executable_name=Game\n").string());
			CHECK(AssetFormat::ReadVersion(ini, AssetFormat::kProjectVersion, "Old.ovproject", error) == 0);
		}
		{
			OvTools::Filesystem::IniFile ini(folder.WriteFile("New.loproject", "format_version=1\n").string());
			CHECK(AssetFormat::ReadVersion(ini, AssetFormat::kProjectVersion, "New.loproject", error) == 1);
		}
		{
			OvTools::Filesystem::IniFile ini(folder.WriteFile("Newer.loproject", "format_version=2\n").string());
			CHECK_FALSE(AssetFormat::ReadVersion(ini, AssetFormat::kProjectVersion, "Newer.loproject", error));
			CHECK(error.find("Newer.loproject is version 2") == 0);
		}
		{
			OvTools::Filesystem::IniFile ini(folder.WriteFile("Bad.loproject", "format_version=1x\n").string());
			CHECK_FALSE(AssetFormat::ReadVersion(ini, AssetFormat::kProjectVersion, "Bad.loproject", error));
			CHECK(error.find("not a whole number") != std::string::npos);
		}

		const auto path = folder.WriteFile("Sorted.loproject", "");
		{
			OvTools::Filesystem::IniFile ini(path.string());
			ini.Add<int>("y_resolution", 720);
			ini.Add<std::string>("executable_name", "Game");
			ini.Add<int>("format_version", 1);
			ini.Add<int>("x_resolution", 1280);
			ini.Rewrite();
		}
		CHECK(ReadText(path) == "executable_name=Game\nformat_version=1\nx_resolution=1280\ny_resolution=720\n");
	}

	TEST_CASE("an asset's metadata of a newer version is left aside, the asset read with the default settings")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;

		OvTools::Filesystem::IniFile older(folder.WriteFile("Rock.png.meta", "MIN_FILTER=9728\n").string());
		CHECK(AssetFormat::CheckMetadata(older, "Rock.png.meta"));
		CHECK(older.Get<int>("MIN_FILTER") == 9728);

		OvTools::Filesystem::IniFile newer(folder.WriteFile("Tree.png.meta", "format_version=2\nMIN_FILTER=9728\n").string());
		CHECK_FALSE(AssetFormat::CheckMetadata(newer, "Tree.png.meta"));
		CHECK_FALSE(newer.IsKeyExisting("MIN_FILTER"));
		CHECK(logs.Contains(ELogLevel::LOG_WARNING, "Tree.png.meta is version 2"));

		AssetFormat::WriteMetadataVersion(older);
		AssetFormat::WriteMetadataVersion(older);
		CHECK(older.Get<int>("format_version") == AssetFormat::kMetadataVersion);
	}

	TEST_CASE("a scene of a newer version is refused whole, an older one read")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;
		OvCore::SceneSystem::SceneManager manager;
		manager.SetErrorDialogsEnabled(false);

		const char* actors = "<scene><actors><actor><name>Kept</name><active>true</active><id>1</id><guid>11</guid><parent>0</parent><components/><behaviours/></actor></actors></scene></root>";
		folder.WriteFile("Overload.ovscene", std::string("<root>") + actors);
		folder.WriteFile("Lost.loscene", std::string(R"(<root version="1">)") + actors);
		folder.WriteFile("Newer.loscene", std::string(R"(<root version="2">)") + actors);

		REQUIRE(manager.LoadScene((folder.GetPath() / "Overload.ovscene").string(), true));
		CHECK(manager.GetCurrentScene()->FindActorByName("Kept"));

		REQUIRE(manager.LoadScene((folder.GetPath() / "Lost.loscene").string(), true));
		CHECK(manager.GetCurrentScene()->FindActorByName("Kept"));

		// Asked for by Overload's name, it is found by its own
		REQUIRE(manager.LoadScene((folder.GetPath() / "Lost.ovscene").string(), true));
		CHECK(manager.GetCurrentSceneSourcePath() == (folder.GetPath() / "Lost.loscene").string());

		CHECK_FALSE(manager.LoadScene((folder.GetPath() / "Newer.loscene").string(), true));
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "Newer.loscene is version 2 of its format"));
	}

	TEST_CASE("a prefab is saved in the latest version, and a newer one is not instantiated")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;
		OvCore::SceneSystem::SceneManager manager;
		auto& scene = *manager.GetCurrentScene();

		auto& root = scene.CreateActor("Crate");
		const auto saved = folder.GetPath() / "Crate.loprefab";
		REQUIRE(OvCore::SceneSystem::PrefabOperations::SaveToFile(root, saved));

		std::string text = ReadText(saved);
		CHECK(text.find(R"(<root version="1">)") != std::string::npos);

		const auto create = [&scene]() -> OvCore::ECS::Actor& { return scene.CreateActor(); };
		CHECK(OvCore::SceneSystem::PrefabOperations::InstantiateFromFile(saved, create));

		// Asked for by Overload's name, it is found by its own
		CHECK(OvCore::SceneSystem::PrefabOperations::InstantiateFromFile(folder.GetPath() / "Crate.ovprefab", create));

		text.replace(text.find(R"(version="1")"), 11, R"(version="9")");
		const auto newer = folder.WriteFile("Newer.loprefab", text);
		CHECK_FALSE(OvCore::SceneSystem::PrefabOperations::InstantiateFromFile(newer, create));
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "is version 9 of its format"));
	}

	TEST_CASE("a material is saved in the latest version, and a newer one is not read")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;

		OvCore::Resources::Material material;
		const auto saved = folder.GetPath() / "Rock.lomat";
		OvCore::Resources::Loaders::MaterialLoader::Save(material, saved.string());
		CHECK(ReadText(saved).find(R"(<root version="1">)") != std::string::npos);

		for (const char* root : { "<root>", R"(<root version="1">)" })
		{
			const auto file = folder.WriteFile("Read.lomat", std::string(root) + "<settings><blendable>true</blendable></settings></root>");
			auto* read = OvCore::Resources::Loaders::MaterialLoader::Create(file.string());
			REQUIRE(read);
			CHECK(read->IsBlendable());
			OvCore::Resources::Loaders::MaterialLoader::Destroy(read);
		}

		const auto newer = folder.WriteFile("Newer.lomat", R"(<root version="2"><settings/></root>)");
		auto* refused = OvCore::Resources::Loaders::MaterialLoader::Create(newer.string());
		CHECK_FALSE(refused);
		CHECK(logs.Contains(ELogLevel::LOG_ERROR, "Newer.lomat is version 2 of its format"));
	}

	TEST_CASE("an animation graph has a version and an editor member, and a newer one is refused")
	{
		CHECK_FALSE(ParseGraph(std::string("{ ") + kGraphLayers + " }"));
		CHECK_FALSE(ParseGraph(std::string(R"({ "version": 1, )") + kGraphLayers + " }"));
		CHECK_FALSE(ParseGraph(std::string(R"({ "version": 1, "editor": { "states": { "Idle": [ 0, 0 ] } }, )") + kGraphLayers + " }"));

		const auto newer = ParseGraph(std::string(R"({ "version": 2, )") + kGraphLayers + " }");
		REQUIRE(newer);
		CHECK(*newer == "version: 2 is newer than this engine reads (version 1 at most)");

		for (const char* version : { "1.5", "-1", "\"1\"" })
		{
			CAPTURE(version);
			const auto error = ParseGraph(std::string(R"({ "version": )") + version + ", " + kGraphLayers + " }");
			REQUIRE(error);
			CHECK(*error == "version: must be a whole number of 0 or more");
		}

		const auto unknown = ParseGraph(std::string(R"({ "versoin": 1, )") + kGraphLayers + " }");
		REQUIRE(unknown);
		CHECK(unknown->find("versoin: unknown member") == 0);
	}

	TEST_CASE("a resource asked for under its format's other name is the one of the file that exists")
	{
		OvTests::TempFolder folder;
		FormatTestManager::ProvideAssetPaths(folder.GetPath() / "Assets", folder.GetPath() / "Engine");
		folder.WriteFile("Assets/Materials/Rock.lomat", "");
		folder.WriteFile("Engine/Shaders/Standard.ovfx", "");

		FormatTestManager manager;

		auto* byOldName = manager.GetResource("Materials/Rock.ovmat");
		REQUIRE(byOldName);
		CHECK(byOldName->file.filename() == "Rock.lomat");
		CHECK(manager.IsResourceRegistered("Materials/Rock.lomat"));
		CHECK_FALSE(manager.IsResourceRegistered("Materials/Rock.ovmat"));

		// Either name gives the same resource, loaded once
		CHECK(manager.GetResource("Materials/Rock.lomat") == byOldName);
		CHECK(manager.GetResource("Materials/Rock.ovmat") == byOldName);
		CHECK(manager.created == 1);

		// An engine asset keeps the name it has on disk, whichever name asks for it
		auto* shader = manager.GetResource(":Shaders/Standard.lofx");
		REQUIRE(shader);
		CHECK(shader->file.filename() == "Standard.ovfx");
		CHECK(manager.IsResourceRegistered(":Shaders/Standard.ovfx"));
		CHECK(manager.GetResource(":Shaders/Standard.ovfx") == shader);

		// Nothing under either name: nothing
		CHECK_FALSE(manager.GetResource("Materials/Missing.lomat"));

		manager.UnloadResources();
	}
}
