/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvGame/Utils/PngWriter.h>
#include <OvRendering/Resources/Loaders/ModelLoader.h>
#include <OvRendering/Resources/PreparedModel.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/Package.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>
#include <OvTools/Utils/Checksum.h>

using OvDebug::ELogLevel;
using OvRendering::Resources::EmbeddedTextureData;
using OvRendering::Resources::ModelData;
namespace PreparedModel = OvRendering::Resources::PreparedModel;

namespace
{
	ModelData Import(const std::filesystem::path& p_file)
	{
		const auto settings = OvCore::ResourceManagement::ModelManager::ReadImportSettings(p_file.string());
		ModelData model;
		REQUIRE_MESSAGE(
			OvRendering::Resources::Loaders::ModelLoader::Import(p_file.string(), settings.parserFlags, settings.generateEmbeddedAssets, model),
			p_file.string()
		);
		return model;
	}

	ModelData ReadBack(const std::vector<char>& p_bytes)
	{
		ModelData model;
		std::string error;
		REQUIRE_MESSAGE(PreparedModel::Read(p_bytes, model, error), error);
		return model;
	}

	std::string Refusal(const std::vector<char>& p_bytes)
	{
		ModelData model;
		std::string error;
		CHECK_FALSE(PreparedModel::Read(p_bytes, model, error));
		CHECK(model.meshes.empty());
		return error;
	}

	// Float fields compared bit for bit: a prepared model gives back exactly what the import gave
	bool SameBits(const float* p_left, const float* p_right, size_t p_count)
	{
		return std::memcmp(p_left, p_right, p_count * sizeof(float)) == 0;
	}

	bool SameBits(float p_left, float p_right)
	{
		return std::bit_cast<uint32_t>(p_left) == std::bit_cast<uint32_t>(p_right);
	}

	bool SameBits(const OvMaths::FVector3& p_left, const OvMaths::FVector3& p_right)
	{
		return SameBits(p_left.x, p_right.x) && SameBits(p_left.y, p_right.y) && SameBits(p_left.z, p_right.z);
	}

	bool SameBits(const OvMaths::FQuaternion& p_left, const OvMaths::FQuaternion& p_right)
	{
		return SameBits(p_left.x, p_right.x) && SameBits(p_left.y, p_right.y) && SameBits(p_left.z, p_right.z) && SameBits(p_left.w, p_right.w);
	}

	// Every field of two models, the indexes by name included
	void CheckSame(const ModelData& p_left, const ModelData& p_right)
	{
		CHECK(p_left.materialNames == p_right.materialNames);

		REQUIRE(p_left.meshes.size() == p_right.meshes.size());
		for (size_t i = 0; i < p_left.meshes.size(); ++i)
		{
			const auto& left = p_left.meshes[i];
			const auto& right = p_right.meshes[i];
			CHECK(left.materialIndex == right.materialIndex);
			CHECK(left.indices == right.indices);
			REQUIRE(left.vertices.size() == right.vertices.size());
			REQUIRE(left.skinnedVertices.size() == right.skinnedVertices.size());
			CHECK(std::memcmp(left.vertices.data(), right.vertices.data(), left.vertices.size() * sizeof(left.vertices[0])) == 0);
			CHECK(std::memcmp(left.skinnedVertices.data(), right.skinnedVertices.data(), left.skinnedVertices.size() * sizeof(left.skinnedVertices[0])) == 0);
		}

		REQUIRE(p_left.skeleton.has_value() == p_right.skeleton.has_value());
		if (p_left.skeleton)
		{
			const auto& left = *p_left.skeleton;
			const auto& right = *p_right.skeleton;
			REQUIRE(left.nodes.size() == right.nodes.size());
			for (size_t i = 0; i < left.nodes.size(); ++i)
			{
				CHECK(left.nodes[i].name == right.nodes[i].name);
				CHECK(left.nodes[i].parentIndex == right.nodes[i].parentIndex);
				CHECK(left.nodes[i].boneIndex == right.nodes[i].boneIndex);
				CHECK(SameBits(left.nodes[i].localBindTransform.data, right.nodes[i].localBindTransform.data, 16));
				CHECK(SameBits(left.nodes[i].bindPosition, right.nodes[i].bindPosition));
				CHECK(SameBits(left.nodes[i].bindRotation, right.nodes[i].bindRotation));
				CHECK(SameBits(left.nodes[i].bindScale, right.nodes[i].bindScale));
			}

			REQUIRE(left.bones.size() == right.bones.size());
			for (size_t i = 0; i < left.bones.size(); ++i)
			{
				CHECK(left.bones[i].name == right.bones[i].name);
				CHECK(left.bones[i].nodeIndex == right.bones[i].nodeIndex);
				CHECK(SameBits(left.bones[i].offsetMatrix.data, right.bones[i].offsetMatrix.data, 16));
			}

			CHECK(left.nodeByName == right.nodeByName);
			CHECK(left.boneByName == right.boneByName);
		}

		REQUIRE(p_left.animations.size() == p_right.animations.size());
		for (size_t i = 0; i < p_left.animations.size(); ++i)
		{
			const auto& left = p_left.animations[i];
			const auto& right = p_right.animations[i];
			CHECK(left.name == right.name);
			CHECK(SameBits(left.duration, right.duration));
			CHECK(SameBits(left.ticksPerSecond, right.ticksPerSecond));
			CHECK(left.trackByNodeIndex == right.trackByNodeIndex);

			REQUIRE(left.tracks.size() == right.tracks.size());
			for (size_t j = 0; j < left.tracks.size(); ++j)
			{
				const auto& leftTrack = left.tracks[j];
				const auto& rightTrack = right.tracks[j];
				CHECK(leftTrack.nodeIndex == rightTrack.nodeIndex);

				REQUIRE(leftTrack.positionKeys.size() == rightTrack.positionKeys.size());
				for (size_t k = 0; k < leftTrack.positionKeys.size(); ++k)
				{
					CHECK(SameBits(leftTrack.positionKeys[k].time, rightTrack.positionKeys[k].time));
					CHECK(SameBits(leftTrack.positionKeys[k].value, rightTrack.positionKeys[k].value));
				}

				REQUIRE(leftTrack.rotationKeys.size() == rightTrack.rotationKeys.size());
				for (size_t k = 0; k < leftTrack.rotationKeys.size(); ++k)
				{
					CHECK(SameBits(leftTrack.rotationKeys[k].time, rightTrack.rotationKeys[k].time));
					CHECK(SameBits(leftTrack.rotationKeys[k].value, rightTrack.rotationKeys[k].value));
				}

				REQUIRE(leftTrack.scaleKeys.size() == rightTrack.scaleKeys.size());
				for (size_t k = 0; k < leftTrack.scaleKeys.size(); ++k)
				{
					CHECK(SameBits(leftTrack.scaleKeys[k].time, rightTrack.scaleKeys[k].time));
					CHECK(SameBits(leftTrack.scaleKeys[k].value, rightTrack.scaleKeys[k].value));
				}
			}
		}

		REQUIRE(p_left.embeddedMaterials.size() == p_right.embeddedMaterials.size());
		for (size_t i = 0; i < p_left.embeddedMaterials.size(); ++i)
		{
			const auto& left = p_left.embeddedMaterials[i];
			const auto& right = p_right.embeddedMaterials[i];
			CHECK(left.name == right.name);
			CHECK((SameBits(left.albedo.x, right.albedo.x) && SameBits(left.albedo.y, right.albedo.y) && SameBits(left.albedo.z, right.albedo.z) && SameBits(left.albedo.w, right.albedo.w)));
			CHECK(SameBits(left.metallic, right.metallic));
			CHECK(SameBits(left.roughness, right.roughness));
			CHECK(SameBits(left.emissiveColor, right.emissiveColor));
			CHECK(SameBits(left.emissiveIntensity, right.emissiveIntensity));
			CHECK(left.albedoTexture == right.albedoTexture);
			CHECK(left.metallicTexture == right.metallicTexture);
			CHECK(left.roughnessTexture == right.roughnessTexture);
			CHECK(left.ambientOcclusionTexture == right.ambientOcclusionTexture);
			CHECK(left.normalTexture == right.normalTexture);
			CHECK(left.heightTexture == right.heightTexture);
			CHECK(left.emissiveTexture == right.emissiveTexture);
			CHECK(left.opacityTexture == right.opacityTexture);
		}

		REQUIRE(p_left.embeddedTextures.size() == p_right.embeddedTextures.size());
		for (size_t i = 0; i < p_left.embeddedTextures.size(); ++i)
		{
			const auto& left = p_left.embeddedTextures[i];
			const auto& right = p_right.embeddedTextures[i];
			CHECK(left.sourceType == right.sourceType);
			CHECK(left.sourcePath == right.sourcePath);
			CHECK(left.extension == right.extension);
			CHECK(left.compressedData == right.compressedData);
			CHECK(left.rawRGBAData == right.rawRGBAData);
			CHECK(left.width == right.width);
			CHECK(left.height == right.height);
		}
	}

	std::string ToBase64(const std::vector<char>& p_bytes)
	{
		constexpr std::string_view kDigits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string text;
		for (size_t i = 0; i < p_bytes.size(); i += 3)
		{
			uint32_t group = static_cast<uint32_t>(static_cast<uint8_t>(p_bytes[i])) << 16;
			if (i + 1 < p_bytes.size()) group |= static_cast<uint32_t>(static_cast<uint8_t>(p_bytes[i + 1])) << 8;
			if (i + 2 < p_bytes.size()) group |= static_cast<uint32_t>(static_cast<uint8_t>(p_bytes[i + 2]));

			text += kDigits[(group >> 18) & 63];
			text += kDigits[(group >> 12) & 63];
			text += i + 1 < p_bytes.size() ? kDigits[(group >> 6) & 63] : '=';
			text += i + 2 < p_bytes.size() ? kDigits[group & 63] : '=';
		}
		return text;
	}

	std::vector<char> ReadDiskBytes(const std::filesystem::path& p_file)
	{
		std::ifstream file(p_file, std::ios::binary);
		return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}

	void PutFloats(std::vector<char>& p_out, std::initializer_list<float> p_values)
	{
		for (const float value : p_values)
		{
			const uint32_t bits = std::bit_cast<uint32_t>(value);
			for (int i = 0; i < 4; ++i)
			{
				p_out.push_back(static_cast<char>(static_cast<uint8_t>(bits >> (8 * i))));
			}
		}
	}

	// A triangle with a texture of its own: a glTF whose buffer and image are inside it (data URIs)
	std::filesystem::path WriteEmbeddedGltf(const OvTests::TempFolder& p_folder)
	{
		const auto image = p_folder.GetPath() / "Painted.png";
		const std::vector<uint8_t> texels = { 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255 };
		REQUIRE(OvGame::Utils::WritePng(image, 2, 2, texels));

		std::vector<char> buffer;
		PutFloats(buffer, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f }); // Positions
		PutFloats(buffer, { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f }); // Texture coordinates

		const std::string gltf = R"({
			"asset": { "version": "2.0" },
			"scene": 0,
			"scenes": [ { "nodes": [ 0 ] } ],
			"nodes": [ { "mesh": 0, "name": "Triangle" } ],
			"meshes": [ { "primitives": [ { "attributes": { "POSITION": 0, "TEXCOORD_0": 1 }, "material": 0 } ] } ],
			"materials": [ { "name": "Painted", "pbrMetallicRoughness": { "baseColorTexture": { "index": 0 }, "metallicFactor": 0.25, "roughnessFactor": 0.75 } } ],
			"textures": [ { "source": 0 } ],
			"images": [ { "uri": "data:image/png;base64,)" + ToBase64(ReadDiskBytes(image)) + R"(" } ],
			"buffers": [ { "byteLength": 60, "uri": "data:application/octet-stream;base64,)" + ToBase64(buffer) + R"(" } ],
			"bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 36 }, { "buffer": 0, "byteOffset": 36, "byteLength": 24 } ],
			"accessors": [
				{ "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [ 0, 0, 0 ], "max": [ 1, 1, 0 ] },
				{ "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2" }
			]
		})";

		return p_folder.WriteFile("Embedded.gltf", gltf);
	}

	// A triangle whose material names a texture next to it: an OBJ and its .mtl
	std::filesystem::path WriteObjWithMaterial(const OvTests::TempFolder& p_folder, const std::string& p_name = "Box")
	{
		p_folder.WriteFile(p_name + ".mtl", "newmtl Wood\nKd 0.5 0.25 1\nmap_Kd Textures/Wood.png\n");
		return p_folder.WriteFile(p_name + ".obj",
			"mtllib " + p_name + ".mtl\n"
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
			"vt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\n"
			"vn 0 0 1\n"
			"usemtl Wood\n"
			"f 1/1/1 2/2/1 3/3/1\nf 2/2/1 4/4/1 3/3/1\n"
		);
	}

	// The CRC of a prepared model, made right again after its body is changed on purpose
	void FixCrc(std::vector<char>& p_bytes)
	{
		const uint32_t crc = OvTools::Utils::Crc32(p_bytes.data() + PreparedModel::kHeaderSize, p_bytes.size() - PreparedModel::kHeaderSize);
		for (int i = 0; i < 4; ++i)
		{
			p_bytes[12 + i] = static_cast<char>(static_cast<uint8_t>(crc >> (8 * i)));
		}
	}

	void PutU32(std::vector<char>& p_bytes, size_t p_at, uint32_t p_value)
	{
		for (int i = 0; i < 4; ++i)
		{
			p_bytes[p_at + i] = static_cast<char>(static_cast<uint8_t>(p_value >> (8 * i)));
		}
	}

	// A model of one triangle, written by hand, for the refusals of what it names
	ModelData MakeTriangle()
	{
		ModelData model;
		auto& mesh = model.meshes.emplace_back();
		mesh.vertices.resize(3);
		mesh.indices = { 0, 1, 2 };
		model.materialNames = { "Default" };
		return model;
	}
}

TEST_SUITE("Prepared models")
{
	TEST_CASE("each model is written as a prepared model and read back whole, field by field")
	{
		const auto engine = OvTests::FindEngineResources();
		REQUIRE_FALSE(engine.empty());
		const auto checks = engine.parent_path().parent_path() / "Tests" / "GameChecks" / "Assets" / "Models";

		OvTests::TempFolder folder;
		folder.WriteFile("Textures/Wood.png", "not read by the import");

		struct Case
		{
			std::filesystem::path file;
			bool skeleton;
			bool animations;
		};

		const std::vector<Case> cases = {
			{ engine / "Models" / "Cube.fbx", false, false },
			{ engine / "Models" / "Mannequin.fbx", true, true },
			{ checks / "Rigs" / "Chain.gltf", true, true },
			{ checks / "Rigs" / "Post.gltf", true, false },
			{ checks / "Lod" / "Sphere0.obj", false, false },
			{ WriteObjWithMaterial(folder), false, false },
			{ WriteEmbeddedGltf(folder), false, false }
		};

		for (const auto& [file, skeleton, animations] : cases)
		{
			CAPTURE(file.string());
			const ModelData imported = Import(file);
			REQUIRE_FALSE(imported.meshes.empty());
			CHECK(imported.skeleton.has_value() == skeleton);
			CHECK(imported.animations.empty() != animations);

			const auto bytes = PreparedModel::Write(imported);
			CHECK(std::string(bytes.data(), 4) == "LOMD");

			const ModelData read = ReadBack(bytes);
			CheckSame(imported, read);
			CHECK(PreparedModel::Write(read) == bytes);
		}
	}

	TEST_CASE("embedded and external textures and materials are kept in a prepared model")
	{
		OvTests::TempFolder folder;

		SUBCASE("a texture inside the model, encoded, named by its material")
		{
			const ModelData model = ReadBack(PreparedModel::Write(Import(WriteEmbeddedGltf(folder))));
			const auto painted = std::find_if(model.embeddedMaterials.begin(), model.embeddedMaterials.end(), [](const auto& p_material) { return p_material.name == "Painted"; });
			REQUIRE(painted != model.embeddedMaterials.end());
			CHECK(painted->metallic == doctest::Approx(0.25f));
			CHECK(painted->roughness == doctest::Approx(0.75f));
			REQUIRE(painted->albedoTexture.has_value());

			const auto& texture = model.embeddedTextures.at(*painted->albedoTexture);
			CHECK(texture.sourceType == EmbeddedTextureData::ESourceType::EMBEDDED_COMPRESSED);
			const auto png = ReadDiskBytes(folder.GetPath() / "Painted.png");
			CHECK(texture.compressedData == std::vector<uint8_t>(png.begin(), png.end()));
		}

		SUBCASE("a texture next to the model, named as the model names it")
		{
			const ModelData model = ReadBack(PreparedModel::Write(Import(WriteObjWithMaterial(folder))));
			CHECK(std::find(model.materialNames.begin(), model.materialNames.end(), "Wood") != model.materialNames.end());
			const auto wood = std::find_if(model.embeddedMaterials.begin(), model.embeddedMaterials.end(), [](const auto& p_material) { return p_material.name == "Wood"; });
			REQUIRE(wood != model.embeddedMaterials.end());
			REQUIRE(wood->albedoTexture.has_value());

			// A path of the model's, resolved when it is loaded: never one of the machine that built the game
			const auto& texture = model.embeddedTextures.at(*wood->albedoTexture);
			CHECK(texture.sourceType == EmbeddedTextureData::ESourceType::EXTERNAL_FILE);
			CHECK(texture.sourcePath == "Textures/Wood.png");
		}
	}

	TEST_CASE("the same model gives the same bytes")
	{
		const auto engine = OvTests::FindEngineResources();
		const auto first = PreparedModel::Write(Import(engine / "Models" / "Mannequin.fbx"));
		const auto second = PreparedModel::Write(Import(engine / "Models" / "Mannequin.fbx"));
		CHECK(first == second);
		CHECK(PreparedModel::GetPath("Models/Rock.fbx") == "Models/Rock.fbx.lomodel");
	}

	TEST_CASE("a prepared model that is not whole, newer or damaged is refused")
	{
		const auto bytes = PreparedModel::Write(MakeTriangle());
		REQUIRE(ReadBack(bytes).meshes.size() == 1);

		auto changed = bytes;
		changed[0] = 'X';
		CHECK(Refusal(changed).find("does not start with LOMD") != std::string::npos);

		changed = bytes;
		PutU32(changed, 4, 2);
		CHECK(Refusal(changed) == "it is version 2 of the format, newer than this engine reads (version 1 at most)");

		changed = bytes;
		PutU32(changed, 8, 1);
		CHECK(Refusal(changed).find("reserved bits") != std::string::npos);

		changed = bytes;
		changed.pop_back();
		CHECK(Refusal(changed) == "it is cut short");

		changed = std::vector<char>(bytes.begin(), bytes.begin() + 10);
		CHECK(Refusal(changed) == "it is cut short");

		changed = bytes;
		changed.push_back('\0');
		CHECK(Refusal(changed) == "it is longer than its header says");

		changed = bytes;
		changed.back() ^= 1;
		CHECK(Refusal(changed) == "it is damaged: its CRC does not match");

		// A count its bytes cannot hold is refused before anything is allocated
		changed = bytes;
		PutU32(changed, PreparedModel::kHeaderSize, 0xFFFFFFF0u);
		FixCrc(changed);
		CHECK(Refusal(changed) == "it counts 4294967280 material names, more than its bytes can hold");
	}

	TEST_CASE("a prepared model that names what it does not hold is refused")
	{
		auto model = MakeTriangle();
		model.meshes[0].indices = { 0, 1, 3 };
		CHECK(Refusal(PreparedModel::Write(model)) == "mesh 0 names vertex 3 of 3");

		model = MakeTriangle();
		model.meshes[0].indices = { 0, 1 };
		CHECK(Refusal(PreparedModel::Write(model)) == "mesh 0 has 3 vertices and 2 indices, not whole triangles");

		model = MakeTriangle();
		model.meshes[0].skinnedVertices.resize(3);
		model.meshes[0].vertices.clear();
		model.meshes[0].skinnedVertices[1].boneWeights[0] = 1.0f;
		CHECK(Refusal(PreparedModel::Write(model)) == "mesh 0 weighs a vertex on bone 0 of 0");

		model = MakeTriangle();
		auto& skeleton = model.skeleton.emplace();
		skeleton.nodes.resize(2);
		skeleton.nodes[0].parentIndex = 1;
		CHECK(Refusal(PreparedModel::Write(model)) == "node 0 has node 1 as its parent");

		skeleton.nodes[0].parentIndex = -1;
		skeleton.nodes[1].parentIndex = 0;
		skeleton.bones.push_back({ .name = "Root", .nodeIndex = 2 });
		CHECK(Refusal(PreparedModel::Write(model)) == "bone 0 is on node 2 of 2");

		skeleton.bones[0].nodeIndex = 0;
		skeleton.nodes[1].boneIndex = 1;
		CHECK(Refusal(PreparedModel::Write(model)) == "node 1 has bone 1 of 1");

		skeleton.nodes[1].boneIndex = -1;
		auto& animation = model.animations.emplace_back();
		animation.tracks.push_back({ .nodeIndex = 5 });
		CHECK(Refusal(PreparedModel::Write(model)) == "animation 0 moves node 5 of 2");

		model = MakeTriangle();
		model.embeddedMaterials.push_back({ .name = "Painted", .albedoTexture = 0 });
		CHECK(Refusal(PreparedModel::Write(model)) == "embedded material 0 names texture 0 of 0");

		model = MakeTriangle();
		model.embeddedTextures.push_back({ .sourceType = EmbeddedTextureData::ESourceType::EMBEDDED_RAW_RGBA8, .rawRGBAData = { 1, 2, 3 }, .width = 1, .height = 1 });
		CHECK(Refusal(PreparedModel::Write(model)) == "embedded texture 0 holds 3 bytes of texels for 1x1");
	}

	TEST_CASE("a model is loaded from its prepared version when there is one, in a package or on the disk")
	{
		using OvCore::ResourceManagement::ModelManager;
		OvTests::TempFolder folder;
		const auto model = WriteObjWithMaterial(folder);
		const ModelData imported = Import(model);

		// What is prepared differs from the file, to tell which one is read
		const auto triangle = MakeTriangle();
		const auto prepared = PreparedModel::Write(triangle);

		ModelData loaded;
		REQUIRE(ModelManager::LoadModelData(model.string(), loaded));
		CheckSame(imported, loaded);

		SUBCASE("on the disk, next to the model, read instead of it")
		{
			folder.WriteFile("Box.obj.lomodel", std::string(prepared.begin(), prepared.end()));
			REQUIRE(ModelManager::LoadModelData(model.string(), loaded));
			CheckSame(triangle, loaded);
		}

		SUBCASE("damaged: an error, and the model's file imported instead")
		{
			auto damaged = prepared;
			damaged.back() ^= 1;
			folder.WriteFile("Box.obj.lomodel", std::string(damaged.begin(), damaged.end()));

			OvTests::LogCapture logs;
			REQUIRE(ModelManager::LoadModelData(model.string(), loaded));
			CHECK(logs.Contains(ELogLevel::LOG_ERROR, "Box.obj.lomodel\": it is damaged: its CRC does not match"));
			CheckSame(imported, loaded);
		}

		SUBCASE("in a mounted package, without the model's file")
		{
			OvTools::Filesystem::PackageBuilder builder;
			std::string error;
			REQUIRE(builder.AddData("Models/Box.obj.lomodel", prepared, error));
			REQUIRE(builder.Write(folder.GetPath() / "Assets.lopak", {}, error));

			const auto assets = folder.GetPath() / "Game" / "Assets";
			OvTools::Filesystem::ScopedMount mount(assets, OvTools::Filesystem::PackageReader::Open(folder.GetPath() / "Assets.lopak", error));
			REQUIRE(ModelManager::LoadModelData((assets / "Models" / "Box.obj").string(), loaded));
			CheckSame(triangle, loaded);
		}
	}
}
