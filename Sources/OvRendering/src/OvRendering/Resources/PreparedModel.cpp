/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <bit>
#include <format>
#include <limits>

#include <OvRendering/Resources/PreparedModel.h>
#include <OvTools/Utils/Checksum.h>

namespace
{
	using namespace OvRendering;
	using Resources::EmbeddedMaterialData;
	using Resources::EmbeddedTextureData;
	using Resources::MeshData;
	using Resources::ModelData;

	constexpr std::array<char, 4> kMagic = { 'L', 'O', 'M', 'D' };
	constexpr uint32_t kNoTexture = std::numeric_limits<uint32_t>::max();

	// The smallest size each record can take, to refuse a count its bytes cannot hold before allocating anything
	constexpr size_t kStringSize = 4;
	constexpr size_t kVertexSize = 14 * 4;
	constexpr size_t kSkinnedVertexSize = kVertexSize + 2 * 4 * Animation::kMaxBonesPerVertex;
	constexpr size_t kTextureSize = 1 + kStringSize + kStringSize + 8 + 4 + 4 + 8;
	constexpr size_t kMaterialSize = kStringSize + 4 * 4 + 4 + 4 + 3 * 4 + 4 + 8 * 4;
	constexpr size_t kNodeSize = kStringSize + 4 + 4 + 16 * 4 + 3 * 4 + 4 * 4 + 3 * 4;
	constexpr size_t kBoneSize = kStringSize + 4 + 16 * 4;
	constexpr size_t kMeshSize = 1 + 4 + 4 + 4;
	constexpr size_t kAnimationSize = kStringSize + 4 + 4 + 4;
	constexpr size_t kTrackSize = 4 + 3 * 4;
	constexpr size_t kVectorKeySize = 4 + 3 * 4;
	constexpr size_t kRotationKeySize = 4 + 4 * 4;

	// Little endian, whatever the machine
	class Writer
	{
	public:
		void U8(uint8_t p_value)
		{
			m_bytes.push_back(static_cast<char>(p_value));
		}

		void U32(uint32_t p_value)
		{
			for (size_t i = 0; i < 4; ++i)
			{
				m_bytes.push_back(static_cast<char>(static_cast<uint8_t>(p_value >> (8 * i))));
			}
		}

		void U64(uint64_t p_value)
		{
			for (size_t i = 0; i < 8; ++i)
			{
				m_bytes.push_back(static_cast<char>(static_cast<uint8_t>(p_value >> (8 * i))));
			}
		}

		void I32(int32_t p_value)
		{
			U32(static_cast<uint32_t>(p_value));
		}

		void F32(float p_value)
		{
			U32(std::bit_cast<uint32_t>(p_value));
		}

		void Floats(const float* p_values, size_t p_count)
		{
			for (size_t i = 0; i < p_count; ++i)
			{
				F32(p_values[i]);
			}
		}

		void Count(size_t p_count)
		{
			U32(static_cast<uint32_t>(p_count));
		}

		void String(std::string_view p_text)
		{
			Count(p_text.size());
			m_bytes.insert(m_bytes.end(), p_text.begin(), p_text.end());
		}

		void Bytes(const std::vector<uint8_t>& p_bytes)
		{
			U64(p_bytes.size());
			m_bytes.insert(m_bytes.end(), p_bytes.begin(), p_bytes.end());
		}

		std::vector<char>& GetBytes()
		{
			return m_bytes;
		}

	private:
		std::vector<char> m_bytes;
	};

	// Why a prepared model is refused: thrown by the reader, caught by Read
	struct Refusal
	{
		std::string reason;
	};

	class Reader
	{
	public:
		explicit Reader(std::span<const char> p_bytes) : m_bytes(p_bytes)
		{
		}

		uint8_t U8()
		{
			Need(1);
			return static_cast<uint8_t>(m_bytes[m_at++]);
		}

		uint32_t U32()
		{
			Need(4);
			uint32_t value = 0;
			for (size_t i = 0; i < 4; ++i)
			{
				value |= static_cast<uint32_t>(static_cast<uint8_t>(m_bytes[m_at + i])) << (8 * i);
			}
			m_at += 4;
			return value;
		}

		uint64_t U64()
		{
			Need(8);
			uint64_t value = 0;
			for (size_t i = 0; i < 8; ++i)
			{
				value |= static_cast<uint64_t>(static_cast<uint8_t>(m_bytes[m_at + i])) << (8 * i);
			}
			m_at += 8;
			return value;
		}

		int32_t I32()
		{
			return static_cast<int32_t>(U32());
		}

		float F32()
		{
			return std::bit_cast<float>(U32());
		}

		void Floats(float* p_values, size_t p_count)
		{
			for (size_t i = 0; i < p_count; ++i)
			{
				p_values[i] = F32();
			}
		}

		// A count of records of at least this size each, which the bytes left must be able to hold
		uint32_t Count(size_t p_recordSize, std::string_view p_what)
		{
			const uint32_t count = U32();
			if (static_cast<uint64_t>(count) * p_recordSize > Remaining())
			{
				throw Refusal{ std::format("it counts {} {}, more than its bytes can hold", count, p_what) };
			}
			return count;
		}

		std::string String()
		{
			const uint32_t size = Count(1, "bytes of text");
			std::string text(m_bytes.data() + m_at, size);
			m_at += size;
			return text;
		}

		std::vector<uint8_t> Bytes()
		{
			const uint64_t size = U64();
			if (size > Remaining())
			{
				throw Refusal{ std::format("it counts {} bytes of a texture, more than its bytes can hold", size) };
			}

			std::vector<uint8_t> bytes(static_cast<size_t>(size));
			for (auto& byte : bytes)
			{
				byte = static_cast<uint8_t>(m_bytes[m_at++]);
			}
			return bytes;
		}

		size_t Remaining() const
		{
			return m_bytes.size() - m_at;
		}

	private:
		void Need(size_t p_size)
		{
			if (Remaining() < p_size)
			{
				throw Refusal{ "it is cut short" };
			}
		}

	private:
		std::span<const char> m_bytes;
		size_t m_at = 0;
	};

	void WriteVertex(Writer& p_out, const Geometry::Vertex& p_vertex)
	{
		p_out.Floats(p_vertex.position, 3);
		p_out.Floats(p_vertex.texCoords, 2);
		p_out.Floats(p_vertex.normals, 3);
		p_out.Floats(p_vertex.tangent, 3);
		p_out.Floats(p_vertex.bitangent, 3);
	}

	void ReadVertex(Reader& p_in, Geometry::Vertex& p_vertex)
	{
		p_in.Floats(p_vertex.position, 3);
		p_in.Floats(p_vertex.texCoords, 2);
		p_in.Floats(p_vertex.normals, 3);
		p_in.Floats(p_vertex.tangent, 3);
		p_in.Floats(p_vertex.bitangent, 3);
	}

	void WriteTexture(Writer& p_out, std::optional<uint32_t> p_texture)
	{
		p_out.U32(p_texture.value_or(kNoTexture));
	}

	std::optional<uint32_t> ReadTexture(Reader& p_in, size_t p_textureCount, size_t p_material)
	{
		const uint32_t texture = p_in.U32();
		if (texture == kNoTexture)
		{
			return std::nullopt;
		}

		if (texture >= p_textureCount)
		{
			throw Refusal{ std::format("embedded material {} names texture {} of {}", p_material, texture, p_textureCount) };
		}
		return texture;
	}

	void WriteBody(Writer& p_out, const ModelData& p_model)
	{
		p_out.Count(p_model.materialNames.size());
		for (const auto& name : p_model.materialNames)
		{
			p_out.String(name);
		}

		// The embedded textures before the materials that name them, the skeleton before the meshes and the animations
		p_out.Count(p_model.embeddedTextures.size());
		for (const auto& texture : p_model.embeddedTextures)
		{
			p_out.U8(static_cast<uint8_t>(texture.sourceType));
			p_out.String(texture.sourcePath);
			p_out.String(texture.extension);
			p_out.Bytes(texture.compressedData);
			p_out.U32(texture.width);
			p_out.U32(texture.height);
			p_out.Bytes(texture.rawRGBAData);
		}

		p_out.Count(p_model.embeddedMaterials.size());
		for (const auto& material : p_model.embeddedMaterials)
		{
			p_out.String(material.name);
			p_out.F32(material.albedo.x);
			p_out.F32(material.albedo.y);
			p_out.F32(material.albedo.z);
			p_out.F32(material.albedo.w);
			p_out.F32(material.metallic);
			p_out.F32(material.roughness);
			p_out.F32(material.emissiveColor.x);
			p_out.F32(material.emissiveColor.y);
			p_out.F32(material.emissiveColor.z);
			p_out.F32(material.emissiveIntensity);
			for (const auto& texture : {
				material.albedoTexture, material.metallicTexture, material.roughnessTexture, material.ambientOcclusionTexture,
				material.normalTexture, material.heightTexture, material.emissiveTexture, material.opacityTexture })
			{
				WriteTexture(p_out, texture);
			}
		}

		p_out.U8(p_model.skeleton ? 1 : 0);
		if (p_model.skeleton)
		{
			p_out.Count(p_model.skeleton->nodes.size());
			for (const auto& node : p_model.skeleton->nodes)
			{
				p_out.String(node.name);
				p_out.I32(node.parentIndex);
				p_out.I32(node.boneIndex);
				p_out.Floats(node.localBindTransform.data, 16);
				p_out.F32(node.bindPosition.x);
				p_out.F32(node.bindPosition.y);
				p_out.F32(node.bindPosition.z);
				p_out.F32(node.bindRotation.x);
				p_out.F32(node.bindRotation.y);
				p_out.F32(node.bindRotation.z);
				p_out.F32(node.bindRotation.w);
				p_out.F32(node.bindScale.x);
				p_out.F32(node.bindScale.y);
				p_out.F32(node.bindScale.z);
			}

			p_out.Count(p_model.skeleton->bones.size());
			for (const auto& bone : p_model.skeleton->bones)
			{
				p_out.String(bone.name);
				p_out.U32(bone.nodeIndex);
				p_out.Floats(bone.offsetMatrix.data, 16);
			}
		}

		p_out.Count(p_model.meshes.size());
		for (const auto& mesh : p_model.meshes)
		{
			p_out.U8(mesh.IsSkinned() ? 1 : 0);
			p_out.U32(mesh.materialIndex);
			p_out.Count(mesh.GetVertexCount());
			p_out.Count(mesh.indices.size());

			if (mesh.IsSkinned())
			{
				for (const auto& vertex : mesh.skinnedVertices)
				{
					WriteVertex(p_out, vertex);
					for (const uint32_t bone : vertex.boneIDs)
					{
						p_out.U32(bone);
					}
					p_out.Floats(vertex.boneWeights, Animation::kMaxBonesPerVertex);
				}
			}
			else
			{
				for (const auto& vertex : mesh.vertices)
				{
					WriteVertex(p_out, vertex);
				}
			}

			for (const uint32_t index : mesh.indices)
			{
				p_out.U32(index);
			}
		}

		p_out.Count(p_model.animations.size());
		for (const auto& animation : p_model.animations)
		{
			p_out.String(animation.name);
			p_out.F32(animation.duration);
			p_out.F32(animation.ticksPerSecond);
			p_out.Count(animation.tracks.size());
			for (const auto& track : animation.tracks)
			{
				p_out.U32(track.nodeIndex);

				p_out.Count(track.positionKeys.size());
				for (const auto& key : track.positionKeys)
				{
					p_out.F32(key.time);
					p_out.F32(key.value.x);
					p_out.F32(key.value.y);
					p_out.F32(key.value.z);
				}

				p_out.Count(track.rotationKeys.size());
				for (const auto& key : track.rotationKeys)
				{
					p_out.F32(key.time);
					p_out.F32(key.value.x);
					p_out.F32(key.value.y);
					p_out.F32(key.value.z);
					p_out.F32(key.value.w);
				}

				p_out.Count(track.scaleKeys.size());
				for (const auto& key : track.scaleKeys)
				{
					p_out.F32(key.time);
					p_out.F32(key.value.x);
					p_out.F32(key.value.y);
					p_out.F32(key.value.z);
				}
			}
		}
	}

	OvMaths::FVector3 ReadVector3(Reader& p_in)
	{
		const float x = p_in.F32();
		const float y = p_in.F32();
		const float z = p_in.F32();
		return { x, y, z };
	}

	OvMaths::FQuaternion ReadQuaternion(Reader& p_in)
	{
		const float x = p_in.F32();
		const float y = p_in.F32();
		const float z = p_in.F32();
		const float w = p_in.F32();
		return { x, y, z, w };
	}

	void ReadBody(Reader& p_in, ModelData& p_model)
	{
		const uint32_t materialCount = p_in.Count(kStringSize, "material names");
		p_model.materialNames.reserve(materialCount);
		for (uint32_t i = 0; i < materialCount; ++i)
		{
			p_model.materialNames.push_back(p_in.String());
		}

		const uint32_t textureCount = p_in.Count(kTextureSize, "embedded textures");
		p_model.embeddedTextures.resize(textureCount);
		for (uint32_t i = 0; i < textureCount; ++i)
		{
			auto& texture = p_model.embeddedTextures[i];
			const uint8_t sourceType = p_in.U8();
			if (sourceType > static_cast<uint8_t>(EmbeddedTextureData::ESourceType::EMBEDDED_RAW_RGBA8))
			{
				throw Refusal{ std::format("embedded texture {} has a source of type {}, which this engine does not know", i, sourceType) };
			}

			texture.sourceType = static_cast<EmbeddedTextureData::ESourceType>(sourceType);
			texture.sourcePath = p_in.String();
			texture.extension = p_in.String();
			texture.compressedData = p_in.Bytes();
			texture.width = p_in.U32();
			texture.height = p_in.U32();
			texture.rawRGBAData = p_in.Bytes();

			if (!texture.rawRGBAData.empty() &&
				texture.rawRGBAData.size() != static_cast<uint64_t>(texture.width) * texture.height * 4)
			{
				throw Refusal{ std::format("embedded texture {} holds {} bytes of texels for {}x{}", i, texture.rawRGBAData.size(), texture.width, texture.height) };
			}
		}

		const uint32_t embeddedMaterialCount = p_in.Count(kMaterialSize, "embedded materials");
		p_model.embeddedMaterials.resize(embeddedMaterialCount);
		for (uint32_t i = 0; i < embeddedMaterialCount; ++i)
		{
			auto& material = p_model.embeddedMaterials[i];
			material.name = p_in.String();
			material.albedo.x = p_in.F32();
			material.albedo.y = p_in.F32();
			material.albedo.z = p_in.F32();
			material.albedo.w = p_in.F32();
			material.metallic = p_in.F32();
			material.roughness = p_in.F32();
			material.emissiveColor = ReadVector3(p_in);
			material.emissiveIntensity = p_in.F32();
			for (auto* texture : {
				&material.albedoTexture, &material.metallicTexture, &material.roughnessTexture, &material.ambientOcclusionTexture,
				&material.normalTexture, &material.heightTexture, &material.emissiveTexture, &material.opacityTexture })
			{
				*texture = ReadTexture(p_in, textureCount, i);
			}
		}

		size_t nodeCount = 0;
		size_t boneCount = 0;
		if (p_in.U8() != 0)
		{
			auto& skeleton = p_model.skeleton.emplace();

			nodeCount = p_in.Count(kNodeSize, "skeleton nodes");
			skeleton.nodes.resize(nodeCount);
			for (size_t i = 0; i < nodeCount; ++i)
			{
				auto& node = skeleton.nodes[i];
				node.name = p_in.String();
				node.parentIndex = p_in.I32();
				node.boneIndex = p_in.I32();
				p_in.Floats(node.localBindTransform.data, 16);
				node.bindPosition = ReadVector3(p_in);
				node.bindRotation = ReadQuaternion(p_in);
				node.bindScale = ReadVector3(p_in);

				// A parent comes before its children, so that no node is its own ancestor
				if (node.parentIndex < -1 || node.parentIndex >= static_cast<int64_t>(i))
				{
					throw Refusal{ std::format("node {} has node {} as its parent", i, node.parentIndex) };
				}
			}

			boneCount = p_in.Count(kBoneSize, "bones");
			skeleton.bones.resize(boneCount);
			for (size_t i = 0; i < boneCount; ++i)
			{
				auto& bone = skeleton.bones[i];
				bone.name = p_in.String();
				bone.nodeIndex = p_in.U32();
				p_in.Floats(bone.offsetMatrix.data, 16);

				if (bone.nodeIndex >= nodeCount)
				{
					throw Refusal{ std::format("bone {} is on node {} of {}", i, bone.nodeIndex, nodeCount) };
				}
			}

			for (size_t i = 0; i < nodeCount; ++i)
			{
				const int32_t bone = skeleton.nodes[i].boneIndex;
				if (bone < -1 || bone >= static_cast<int64_t>(boneCount))
				{
					throw Refusal{ std::format("node {} has bone {} of {}", i, bone, boneCount) };
				}
			}

			// The indexes by name are not written: made again in the order of the nodes and bones, the first name kept, as
			// the import makes them
			for (size_t i = 0; i < nodeCount; ++i)
			{
				skeleton.nodeByName.emplace(skeleton.nodes[i].name, static_cast<uint32_t>(i));
			}
			for (size_t i = 0; i < boneCount; ++i)
			{
				skeleton.boneByName.emplace(skeleton.bones[i].name, static_cast<uint32_t>(i));
			}
		}

		const uint32_t meshCount = p_in.Count(kMeshSize, "meshes");
		p_model.meshes.resize(meshCount);
		for (uint32_t i = 0; i < meshCount; ++i)
		{
			auto& mesh = p_model.meshes[i];
			const bool skinned = p_in.U8() != 0;
			mesh.materialIndex = p_in.U32();
			const uint32_t vertexCount = p_in.Count(skinned ? kSkinnedVertexSize : kVertexSize, "vertices");
			const uint32_t indexCount = p_in.U32();

			if (vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0)
			{
				throw Refusal{ std::format("mesh {} has {} vertices and {} indices, not whole triangles", i, vertexCount, indexCount) };
			}

			if (skinned)
			{
				mesh.skinnedVertices.resize(vertexCount);
				for (auto& vertex : mesh.skinnedVertices)
				{
					ReadVertex(p_in, vertex);
					for (uint32_t& bone : vertex.boneIDs)
					{
						bone = p_in.U32();
					}
					p_in.Floats(vertex.boneWeights, Animation::kMaxBonesPerVertex);

					for (uint8_t j = 0; j < Animation::kMaxBonesPerVertex; ++j)
					{
						if (vertex.boneWeights[j] != 0.0f && vertex.boneIDs[j] >= boneCount)
						{
							throw Refusal{ std::format("mesh {} weighs a vertex on bone {} of {}", i, vertex.boneIDs[j], boneCount) };
						}
					}
				}
			}
			else
			{
				mesh.vertices.resize(vertexCount);
				for (auto& vertex : mesh.vertices)
				{
					ReadVertex(p_in, vertex);
				}
			}

			if (static_cast<uint64_t>(indexCount) * 4 > p_in.Remaining())
			{
				throw Refusal{ std::format("it counts {} indices, more than its bytes can hold", indexCount) };
			}

			mesh.indices.resize(indexCount);
			for (auto& index : mesh.indices)
			{
				index = p_in.U32();
				if (index >= vertexCount)
				{
					throw Refusal{ std::format("mesh {} names vertex {} of {}", i, index, vertexCount) };
				}
			}
		}

		const uint32_t animationCount = p_in.Count(kAnimationSize, "animations");
		p_model.animations.resize(animationCount);
		for (uint32_t i = 0; i < animationCount; ++i)
		{
			auto& animation = p_model.animations[i];
			animation.name = p_in.String();
			animation.duration = p_in.F32();
			animation.ticksPerSecond = p_in.F32();

			const uint32_t trackCount = p_in.Count(kTrackSize, "animation tracks");
			animation.tracks.resize(trackCount);
			for (uint32_t j = 0; j < trackCount; ++j)
			{
				auto& track = animation.tracks[j];
				track.nodeIndex = p_in.U32();
				if (track.nodeIndex >= nodeCount)
				{
					throw Refusal{ std::format("animation {} moves node {} of {}", i, track.nodeIndex, nodeCount) };
				}

				track.positionKeys.resize(p_in.Count(kVectorKeySize, "position keys"));
				for (auto& key : track.positionKeys)
				{
					key.time = p_in.F32();
					key.value = ReadVector3(p_in);
				}

				track.rotationKeys.resize(p_in.Count(kRotationKeySize, "rotation keys"));
				for (auto& key : track.rotationKeys)
				{
					key.time = p_in.F32();
					key.value = ReadQuaternion(p_in);
				}

				track.scaleKeys.resize(p_in.Count(kVectorKeySize, "scale keys"));
				for (auto& key : track.scaleKeys)
				{
					key.time = p_in.F32();
					key.value = ReadVector3(p_in);
				}

				animation.trackByNodeIndex.emplace(track.nodeIndex, j);
			}
		}

		if (p_in.Remaining() != 0)
		{
			throw Refusal{ std::format("its body holds {} bytes past its model", p_in.Remaining()) };
		}
	}
}

std::string OvRendering::Resources::PreparedModel::GetPath(std::string_view p_modelPath)
{
	return std::string(p_modelPath) + std::string(kExtension);
}

std::vector<char> OvRendering::Resources::PreparedModel::Write(const ModelData& p_model)
{
	Writer body;
	WriteBody(body, p_model);
	const auto& bodyBytes = body.GetBytes();

	Writer out;
	auto& bytes = out.GetBytes();
	bytes.reserve(kHeaderSize + bodyBytes.size());
	bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
	out.U32(kVersion);
	out.U32(0); // Reserved
	out.U32(OvTools::Utils::Crc32(bodyBytes.data(), bodyBytes.size()));
	out.U64(bodyBytes.size());
	bytes.insert(bytes.end(), bodyBytes.begin(), bodyBytes.end());
	return std::move(bytes);
}

bool OvRendering::Resources::PreparedModel::Read(std::span<const char> p_bytes, ModelData& p_model, std::string& p_error)
{
	p_model = ModelData{};

	try
	{
		if (p_bytes.size() < kMagic.size() || !std::equal(kMagic.begin(), kMagic.end(), p_bytes.begin()))
		{
			throw Refusal{ "it is not a prepared model: it does not start with LOMD" };
		}

		Reader header(p_bytes.first(std::min(p_bytes.size(), kHeaderSize)));
		header.U32(); // The mark
		const uint32_t version = header.U32();
		const uint32_t reserved = header.U32();
		const uint32_t crc = header.U32();
		const uint64_t bodySize = header.U64();

		if (version == 0 || version > kVersion)
		{
			throw Refusal{ std::format("it is version {} of the format, newer than this engine reads (version {} at most)", version, kVersion) };
		}

		if (reserved != 0)
		{
			throw Refusal{ std::format("it sets reserved bits ({:#x}), which this engine does not know", reserved) };
		}

		const uint64_t actualSize = p_bytes.size() - kHeaderSize;
		if (bodySize != actualSize)
		{
			throw Refusal{ bodySize > actualSize ? "it is cut short" : "it is longer than its header says" };
		}

		const auto body = p_bytes.subspan(kHeaderSize);
		if (OvTools::Utils::Crc32(body.data(), body.size()) != crc)
		{
			throw Refusal{ "it is damaged: its CRC does not match" };
		}

		Reader in(body);
		ReadBody(in, p_model);
		return true;
	}
	catch (const Refusal& p_refusal)
	{
		p_model = ModelData{};
		p_error = p_refusal.reason;
		return false;
	}
}
