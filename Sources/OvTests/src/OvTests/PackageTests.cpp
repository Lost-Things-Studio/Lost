/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <array>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include <LostPack/Commands.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/Package.h>

using namespace OvTools::Filesystem;

namespace
{
	std::string ReadBytes(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		std::stringstream bytes;
		bytes << file.rdbuf();
		return bytes.str();
	}

	void WriteBytes(const std::filesystem::path& p_path, const std::string& p_bytes)
	{
		std::ofstream file(p_path, std::ios::binary | std::ios::trunc);
		file.write(p_bytes.data(), static_cast<std::streamsize>(p_bytes.size()));
	}

	std::string Text(const std::vector<char>& p_bytes)
	{
		return std::string(p_bytes.begin(), p_bytes.end());
	}

	std::vector<char> Bytes(std::string_view p_text)
	{
		return std::vector<char>(p_text.begin(), p_text.end());
	}

	std::string RandomBytes(size_t p_size, uint32_t p_seed)
	{
		std::mt19937 random(p_seed);
		std::string bytes(p_size, '\0');
		for (auto& byte : bytes)
		{
			byte = static_cast<char>(random() & 0xFF);
		}
		return bytes;
	}

	uint32_t Crc32(const std::string& p_bytes)
	{
		uint32_t crc = 0xFFFFFFFFu;
		for (const char byte : p_bytes)
		{
			crc ^= static_cast<uint8_t>(byte);
			for (int bit = 0; bit < 8; ++bit)
			{
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
		return ~crc;
	}

	uint64_t ReadNumber(const std::string& p_bytes, size_t p_at, size_t p_size)
	{
		uint64_t value = 0;
		for (size_t i = 0; i < p_size; ++i)
		{
			value |= static_cast<uint64_t>(static_cast<uint8_t>(p_bytes[p_at + i])) << (8 * i);
		}
		return value;
	}

	void WriteNumber(std::string& p_bytes, size_t p_at, size_t p_size, uint64_t p_value)
	{
		for (size_t i = 0; i < p_size; ++i)
		{
			p_bytes[p_at + i] = static_cast<char>(static_cast<uint8_t>(p_value >> (8 * i)));
		}
	}

	// Changes a package's index, and gives it the CRC that matches, so that what follows the CRC is what is tested
	void RewriteIndex(const std::filesystem::path& p_package, const std::function<void(std::string&)>& p_change)
	{
		std::string bytes = ReadBytes(p_package);
		const auto offset = static_cast<size_t>(ReadNumber(bytes, 8, 8));
		std::string index = bytes.substr(offset);
		p_change(index);
		bytes = bytes.substr(0, offset) + index;
		WriteNumber(bytes, 16, 8, index.size());
		WriteNumber(bytes, 24, 4, Crc32(index));
		WriteBytes(p_package, bytes);
	}

	// Where the first entry of an index starts, past its manifest and count
	size_t FirstEntry(const std::string& p_index)
	{
		return 4 + static_cast<size_t>(ReadNumber(p_index, 0, 4)) + 4;
	}

	std::filesystem::path Pack(const OvTests::TempFolder& p_folder, const std::vector<std::pair<std::string, std::string>>& p_files, const std::string& p_name = "Game.lopak")
	{
		PackageBuilder builder;
		std::string error;
		for (const auto& [path, content] : p_files)
		{
			REQUIRE_MESSAGE(builder.AddData(path, Bytes(content), error), error);
		}

		const auto package = p_folder.GetPath() / p_name;
		REQUIRE_MESSAGE(builder.Write(package, { "Game", {} }, error), error);
		return package;
	}

	std::string OpenError(const std::filesystem::path& p_package)
	{
		std::string error;
		CHECK_FALSE(PackageReader::Open(p_package, error));
		return error;
	}

	int RunLostPack(const std::vector<std::string>& p_arguments, std::string& p_out, std::string& p_err)
	{
		std::ostringstream out;
		std::ostringstream err;
		const int code = LostPack::Run(p_arguments, out, err);
		p_out = out.str();
		p_err = err.str();
		return code;
	}
}

TEST_SUITE("Packages")
{
	TEST_CASE("a folder packed and read back, each file as it was, with the manifest")
	{
		OvTests::TempFolder folder;
		const std::string rock = RandomBytes(300, 7) + std::string(1000, 'x');
		folder.WriteFile("Assets/Scenes/Level.loscene", "<root version=\"1\"><scene/></root>");
		folder.WriteFile("Assets/Textures/Rock.png", rock);
		folder.WriteFile("Assets/Empty.txt", "");
		folder.WriteFile(std::filesystem::path(u8"Assets/\u00c9lan/\u00df.txt"), "non-ASCII");

		// A link may lead out of the folder: it is left out
		std::error_code linkError;
		std::filesystem::create_symlink(folder.WriteFile("Outside.txt", "outside"), folder.GetPath() / "Assets/Link.txt", linkError);

		PackageBuilder builder;
		std::string error;
		REQUIRE_MESSAGE(builder.AddFolder(folder.GetPath() / "Assets", error), error);
		CHECK(builder.GetFileCount() == 4);
		REQUIRE_MESSAGE(builder.Write(folder.GetPath() / "Game.lopak", { "Game", {} }, error), error);
		CHECK_FALSE(std::filesystem::exists(folder.GetPath() / "Game.lopak.~tmp"));

		auto reader = PackageReader::Open(folder.GetPath() / "Game.lopak", error);
		REQUIRE_MESSAGE(reader, error);

		// Sorted byte by byte, so the UTF-8 name comes last
		const auto& entries = reader->GetEntries();
		REQUIRE(entries.size() == 4);
		CHECK(entries[0].path == "Empty.txt");
		CHECK(entries[1].path == "Scenes/Level.loscene");
		CHECK(entries[2].path == "Textures/Rock.png");
		CHECK(entries[3].path == "\xc3\x89lan/\xc3\x9f.txt");

		std::vector<char> content;
		REQUIRE(reader->Find("Textures/Rock.png"));
		REQUIRE_MESSAGE(reader->Read(*reader->Find("Textures/Rock.png"), content, error), error);
		CHECK(Text(content) == rock);
		REQUIRE_MESSAGE(reader->Read(*reader->Find("Scenes/Level.loscene"), content, error), error);
		CHECK(Text(content) == "<root version=\"1\"><scene/></root>");
		REQUIRE_MESSAGE(reader->Read(entries[0], content, error), error);
		CHECK(content.empty());
		REQUIRE_MESSAGE(reader->Read(entries[3], content, error), error);
		CHECK(Text(content) == "non-ASCII");
		CHECK_FALSE(reader->Find("Textures"));
		CHECK_FALSE(reader->Find("textures/rock.png"));

		const auto& manifest = reader->GetManifest();
		CHECK(manifest.version == PackageFormat::kVersion);
		CHECK(manifest.lostVersion == LOST_VERSION);
		CHECK(manifest.projectId == "Game");
		CHECK(manifest.fileCount == 4);
		CHECK(manifest.contentHash.starts_with("fnv1a64:"));
		CHECK(manifest.contentHash.size() == 24);
		CHECK_MESSAGE(reader->Verify(error), error);

		// The format as Docs/Packages.md sets it out
		const std::string bytes = ReadBytes(folder.GetPath() / "Game.lopak");
		CHECK(bytes.substr(0, 4) == "LOPK");
		CHECK(ReadNumber(bytes, 4, 4) == 1);
		CHECK(ReadNumber(bytes, 8, 8) + ReadNumber(bytes, 16, 8) == bytes.size());
		CHECK(ReadNumber(bytes, 28, 4) == 0);
		CHECK(entries[0].offset == PackageFormat::kHeaderSize);
	}

	TEST_CASE("a file is compressed when it gains by it, and kept as it is when it does not")
	{
		OvTests::TempFolder folder;
		std::string text;
		for (int i = 0; i < 1000; ++i)
		{
			text += "local value = Vector3.new(1, 2, 3)\n";
		}
		const std::string noise = RandomBytes(4096, 42);
		const auto package = Pack(folder, { { "Scripts/Long.lua", text }, { "Sounds/Noise.wav", noise }, { "Empty.txt", "" } });

		std::string error;
		auto reader = PackageReader::Open(package, error);
		REQUIRE_MESSAGE(reader, error);

		const auto* script = reader->Find("Scripts/Long.lua");
		const auto* sound = reader->Find("Sounds/Noise.wav");
		const auto* empty = reader->Find("Empty.txt");
		REQUIRE((script && sound && empty));
		CHECK(script->method == EPackageMethod::DEFLATE);
		CHECK(script->storedSize < script->size / 10);
		CHECK(script->size == text.size());
		CHECK(sound->method == EPackageMethod::STORED);
		CHECK(sound->storedSize == noise.size());
		CHECK(empty->method == EPackageMethod::STORED);
		CHECK(empty->storedSize == 0);

		std::vector<char> content;
		REQUIRE_MESSAGE(reader->Read(*script, content, error), error);
		CHECK(Text(content) == text);
		REQUIRE_MESSAGE(reader->Read(*sound, content, error), error);
		CHECK(Text(content) == noise);
	}

	TEST_CASE("the same files give the same bytes, and the content hash follows the content only")
	{
		OvTests::TempFolder folder;
		const std::vector<std::pair<std::string, std::string>> files = {
			{ "b/2.txt", "two" }, { "a/1.txt", "one" }, { "c.txt", RandomBytes(500, 1) }
		};

		// The same files, made on the disk in two orders
		for (const auto& [path, content] : files)
		{
			folder.WriteFile(std::filesystem::path("First") / path, content);
		}
		for (auto file = files.rbegin(); file != files.rend(); ++file)
		{
			folder.WriteFile(std::filesystem::path("Second") / file->first, file->second);
		}

		std::string error;
		for (const char* name : { "First", "Second" })
		{
			PackageBuilder builder;
			REQUIRE_MESSAGE(builder.AddFolder(folder.GetPath() / name, error), error);
			REQUIRE_MESSAGE(builder.Write(folder.GetPath() / (std::string(name) + ".lopak"), { "Game", "0.1.0" }, error), error);
		}
		const std::string first = ReadBytes(folder.GetPath() / "First.lopak");
		CHECK(first == ReadBytes(folder.GetPath() / "Second.lopak"));

		// Written again over itself, the same bytes
		const std::string again = ReadBytes(Pack(folder, files, "Again.lopak"));
		CHECK(again == ReadBytes(Pack(folder, files, "Again.lopak")));
		CHECK(again == first);

		const auto hashOf = [&](const std::filesystem::path& p_package)
		{
			auto reader = PackageReader::Open(p_package, error);
			REQUIRE_MESSAGE(reader, error);
			return reader->GetManifest().contentHash;
		};

		// Another project: other bytes, the same content
		PackageBuilder other;
		for (const auto& [path, content] : files)
		{
			REQUIRE(other.AddData(path, Bytes(content), error));
		}
		REQUIRE(other.Write(folder.GetPath() / "Other.lopak", { "Other", "0.1.0" }, error));
		CHECK(ReadBytes(folder.GetPath() / "Other.lopak") != first);
		CHECK(hashOf(folder.GetPath() / "Other.lopak") == hashOf(folder.GetPath() / "First.lopak"));

		// One byte more: another content
		auto changed = files;
		changed[0].second += "!";
		CHECK(hashOf(Pack(folder, changed, "Changed.lopak")) != hashOf(folder.GetPath() / "First.lopak"));

		// The same bytes under another name: another content
		auto renamed = files;
		renamed[0].first = "b/3.txt";
		CHECK(hashOf(Pack(folder, renamed, "Renamed.lopak")) != hashOf(folder.GetPath() / "First.lopak"));
	}

	TEST_CASE("a damaged file is found when it is read, and by a verification")
	{
		OvTests::TempFolder folder;
		std::string text;
		for (int i = 0; i < 200; ++i)
		{
			text += "<actor><name>Rock</name></actor>\n";
		}
		const auto package = Pack(folder, { { "Kept.txt", "kept" }, { "Noise.bin", RandomBytes(256, 3) }, { "Scene.loscene", text } });

		std::string error;
		PackageEntry noise;
		PackageEntry scene;
		{
			auto reader = PackageReader::Open(package, error);
			REQUIRE_MESSAGE(reader, error);
			noise = *reader->Find("Noise.bin");
			scene = *reader->Find("Scene.loscene");
		}
		REQUIRE(noise.method == EPackageMethod::STORED);
		REQUIRE(scene.method == EPackageMethod::DEFLATE);

		std::string bytes = ReadBytes(package);
		bytes[static_cast<size_t>(noise.offset + 10)] ^= 0x01;
		bytes[static_cast<size_t>(scene.offset + scene.storedSize / 2)] ^= 0x10;
		WriteBytes(package, bytes);

		auto reader = PackageReader::Open(package, error);
		REQUIRE_MESSAGE(reader, error); // The index is whole

		std::vector<char> content;
		CHECK(reader->Read(*reader->Find("Kept.txt"), content, error));
		CHECK_FALSE(reader->Read(*reader->Find("Noise.bin"), content, error));
		CHECK(error == "Noise.bin is damaged: its CRC does not match");
		CHECK_FALSE(reader->Read(*reader->Find("Scene.loscene"), content, error));
		CHECK(error.starts_with("Scene.loscene is damaged"));
		CHECK_FALSE(reader->Verify(error));
		CHECK(error.starts_with("Noise.bin is damaged"));
	}

	TEST_CASE("a header or an index that is not right is refused when the package is opened")
	{
		OvTests::TempFolder folder;
		const std::vector<std::pair<std::string, std::string>> files = { { "a.txt", "a" }, { "b.txt", std::string(100, 'b') } };

		SUBCASE("not a package")
		{
			const auto package = Pack(folder, files);
			std::string bytes = ReadBytes(package);
			bytes[0] = 'Z';
			WriteBytes(package, bytes);
			CHECK(OpenError(package) == "it is not a package: it does not start with LOPK");

			WriteBytes(package, "LOPK");
			CHECK(OpenError(package) == "it is too short to be a package");
		}

		SUBCASE("a newer version, or flags")
		{
			const auto package = Pack(folder, files);
			std::string bytes = ReadBytes(package);
			WriteNumber(bytes, 4, 4, 2);
			WriteBytes(package, bytes);
			CHECK(OpenError(package) == "it is version 2 of the format, newer than this engine reads (version 1 at most)");

			WriteNumber(bytes, 4, 4, 1);
			WriteNumber(bytes, 28, 4, 1);
			WriteBytes(package, bytes);
			CHECK(OpenError(package) == "it has flags this engine does not know");
		}

		SUBCASE("cut short, or longer")
		{
			const auto package = Pack(folder, files);
			const std::string bytes = ReadBytes(package);
			WriteBytes(package, bytes.substr(0, bytes.size() - 1));
			CHECK(OpenError(package) == "it is cut short, or its index is not where its header says");
			WriteBytes(package, bytes + "x");
			CHECK(OpenError(package) == "it is cut short, or its index is not where its header says");
		}

		SUBCASE("a damaged index")
		{
			const auto package = Pack(folder, files);
			std::string bytes = ReadBytes(package);
			bytes[bytes.size() - 3] ^= 0x01;
			WriteBytes(package, bytes);
			CHECK(OpenError(package) == "its index is damaged: its CRC does not match");
		}

		SUBCASE("an index whose CRC is right, but which lies")
		{
			const auto package = Pack(folder, files);
			const std::string original = ReadBytes(package);

			// A method this engine does not know, as an encryption would be
			RewriteIndex(package, [](std::string& p_index) { p_index[FirstEntry(p_index) + 2 + 5] = 2; });
			CHECK(OpenError(package) == "it holds a.txt by method 2, which is not one this engine reads");

			// A stored file that says it is larger than its bytes
			WriteBytes(package, original);
			RewriteIndex(package, [](std::string& p_index) { WriteNumber(p_index, FirstEntry(p_index) + 2 + 5 + 1 + 16, 8, 2); });
			CHECK(OpenError(package) == "its index is damaged: a.txt is not where or what it says");

			// A file that is not where the previous one ends
			WriteBytes(package, original);
			RewriteIndex(package, [](std::string& p_index) { WriteNumber(p_index, FirstEntry(p_index) + 2 + 5 + 1, 8, 33); });
			CHECK(OpenError(package) == "its index is damaged: a.txt is not where or what it says");

			// A path that would lead out of the folder it is extracted to
			WriteBytes(package, original);
			RewriteIndex(package, [](std::string& p_index) { p_index.replace(FirstEntry(p_index) + 2, 5, "../ab"); });
			CHECK(OpenError(package) == "its index is damaged: \"../ab\" is not a path in its place");

			// A manifest member this engine does not read
			WriteBytes(package, original);
			RewriteIndex(package, [](std::string& p_index)
			{
				const size_t size = static_cast<size_t>(ReadNumber(p_index, 0, 4));
				std::string manifest = p_index.substr(4, size);
				manifest.replace(manifest.find("\"projectId\""), 11, "\"encryption\"");
				p_index = p_index.substr(0, 4) + manifest + p_index.substr(4 + size);
				WriteNumber(p_index, 0, 4, manifest.size());
			});
			CHECK(OpenError(package) == "its manifest has a \"encryption\" this engine does not read");

			// A count of files that is not the manifest's
			WriteBytes(package, original);
			RewriteIndex(package, [](std::string& p_index) { WriteNumber(p_index, FirstEntry(p_index) - 4, 4, 1); });
			CHECK(OpenError(package) == "its index is damaged: its count of files is not the manifest's");
		}
	}

	TEST_CASE("a package holds relative paths in UTF-8 with \"/\", each once, and nothing else")
	{
		for (const char* path : { "a", "a/b.c", ".hidden", "Textures/Rock 2.png", "\xc3\x89lan/\xc3\x9f.txt" })
		{
			CHECK_MESSAGE(PackageFormat::IsValidPath(path), path);
		}

		for (const char* path : { "", "/a", "a/", "a//b", "./a", "a/./b", "a/../b", "..", "a\\b", "C:/a", "a\x01" "b", "a\x7f", "\xc3", "\xc0\xaf", "\xed\xa0\x80" })
		{
			CHECK_FALSE_MESSAGE(PackageFormat::IsValidPath(path), path);
		}

		PackageBuilder builder;
		std::string error;
		CHECK_FALSE(builder.AddData("../Secret.txt", {}, error));
		CHECK(error == "\"../Secret.txt\" cannot be a path in a package");
		CHECK(builder.AddData("Kept.txt", Bytes("one"), error));
		CHECK_FALSE(builder.AddData("Kept.txt", Bytes("two"), error));
		CHECK(error == "Kept.txt is in the package already");
		CHECK(builder.GetFileCount() == 1);

		OvTests::TempFolder folder;
		CHECK_FALSE(builder.AddFolder(folder.GetPath() / "Missing", error));
		CHECK(error.ends_with("is not a folder"));
	}

	TEST_CASE("a package is written whole or not at all")
	{
		OvTests::TempFolder folder;
		PackageBuilder builder;
		std::string error;
		REQUIRE(builder.AddData("a.txt", Bytes("a"), error));
		REQUIRE(builder.AddFile("b.txt", folder.GetPath() / "Missing.txt", error));

		// A file that cannot be read: no package, and no temporary file left
		CHECK_FALSE(builder.Write(folder.GetPath() / "Game.lopak", {}, error));
		CHECK(error.ends_with("Missing.txt cannot be read"));
		CHECK_FALSE(std::filesystem::exists(folder.GetPath() / "Game.lopak"));
		CHECK_FALSE(std::filesystem::exists(folder.GetPath() / "Game.lopak.~tmp"));

		// A package written again replaces the previous one
		const auto package = Pack(folder, { { "a.txt", "a" } });
		Pack(folder, { { "b.txt", "b" } });
		auto reader = PackageReader::Open(package, error);
		REQUIRE_MESSAGE(reader, error);
		REQUIRE(reader->GetEntries().size() == 1);
		CHECK(reader->GetEntries()[0].path == "b.txt");
	}

	TEST_CASE("LostPack packs, lists, verifies and extracts, and says why it cannot")
	{
		OvTests::TempFolder folder;
		folder.WriteFile("Assets/Scenes/Level.loscene", "<root/>");
		folder.WriteFile("Assets/Textures/Rock.png", RandomBytes(64, 5));
		const std::string assets = (folder.GetPath() / "Assets").string();
		const std::string package = (folder.GetPath() / "Game.lopak").string();
		std::string out, err;

		REQUIRE_MESSAGE(RunLostPack({ "pack", assets, package, "--project-id", "Game" }, out, err) == LostPack::SUCCESS, err);
		CHECK(out.starts_with("Packed 2 files, "));

		REQUIRE(RunLostPack({ "list", package }, out, err) == LostPack::SUCCESS);
		CHECK(out.find("Package version 1, written by Lost " LOST_VERSION ", project \"Game\"\n") == 0);
		CHECK(out.find("2 files, fnv1a64:") != std::string::npos);
		CHECK(out.find("stored   Scenes/Level.loscene\n") != std::string::npos);
		CHECK(out.find("Textures/Rock.png\n") != std::string::npos);

		REQUIRE(RunLostPack({ "verify", package }, out, err) == LostPack::SUCCESS);
		CHECK(out.find(": 2 files, all whole, fnv1a64:") != std::string::npos);

		const auto extracted = folder.GetPath() / "Extracted";
		REQUIRE_MESSAGE(RunLostPack({ "extract", package, extracted.string() }, out, err) == LostPack::SUCCESS, err);
		CHECK(ReadBytes(extracted / "Scenes/Level.loscene") == "<root/>");
		CHECK(ReadBytes(extracted / "Textures/Rock.png") == RandomBytes(64, 5));

		// What it refuses
		CHECK(RunLostPack({ "extract", package, extracted.string() }, out, err) == LostPack::FAILURE);
		CHECK(err.ends_with(": the folder to extract to must be new or empty\n"));
		CHECK(RunLostPack({ "pack", assets, (folder.GetPath() / "Assets/Game.lopak").string() }, out, err) == LostPack::FAILURE);
		CHECK(err.ends_with(": a package cannot be written inside the folder it packs\n"));
		CHECK(RunLostPack({ "pack", (folder.GetPath() / "Missing").string(), package }, out, err) == LostPack::FAILURE);
		CHECK(err.ends_with("is not a folder\n"));

		std::string bytes = ReadBytes(package);
		bytes[bytes.size() - 2] ^= 0x01;
		WriteBytes(package, bytes);
		CHECK(RunLostPack({ "verify", package }, out, err) == LostPack::FAILURE);
		CHECK(err == package + ": its index is damaged: its CRC does not match\n");
		CHECK(RunLostPack({ "list", package }, out, err) == LostPack::FAILURE);

		// What it does not understand
		for (const std::vector<std::string>& arguments : std::vector<std::vector<std::string>>{
			{}, { "unpack", package }, { "pack", assets }, { "list" }, { "pack", assets, package, "--level", "9" }, { "pack", assets, package, "--project-id" } })
		{
			CHECK(RunLostPack(arguments, out, err) == LostPack::USAGE);
			CHECK(err.find("Usage: LostPack") != std::string::npos);
		}

		CHECK(RunLostPack({ "--help" }, out, err) == LostPack::SUCCESS);
		CHECK(out == LostPack::Usage());
	}
}
