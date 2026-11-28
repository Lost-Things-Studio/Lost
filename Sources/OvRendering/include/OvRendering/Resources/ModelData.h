/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <OvRendering/Animation/SkeletalData.h>
#include <OvRendering/Geometry/Vertex.h>
#include <OvRendering/Resources/EmbeddedAssets.h>

namespace OvRendering::Resources
{
	/**
	* A mesh in memory, as an import or a prepared model gives it: its vertices, plain or skinned, and its triangles
	*/
	struct MeshData
	{
		std::vector<Geometry::Vertex> vertices; // A mesh without skinning
		std::vector<Geometry::SkinnedVertex> skinnedVertices; // A skinned mesh, or one bound whole to a bone
		std::vector<uint32_t> indices; // Three per triangle
		uint32_t materialIndex = 0;

		/**
		* Whether its vertices carry bones and weights
		*/
		bool IsSkinned() const
		{
			return !skinnedVertices.empty();
		}

		/**
		* Its number of vertices, plain or skinned
		*/
		size_t GetVertexCount() const
		{
			return IsSkinned() ? skinnedVertices.size() : vertices.size();
		}
	};

	/**
	* A model in memory, before its meshes are sent to the GPU: what importing its file (Assimp) or reading its prepared
	* version (.lomodel) gives. Building it needs no graphics context
	*/
	struct ModelData
	{
		std::vector<MeshData> meshes;
		std::vector<std::string> materialNames;
		std::optional<Animation::Skeleton> skeleton;
		std::vector<Animation::SkeletalAnimation> animations;
		std::vector<EmbeddedMaterialData> embeddedMaterials;
		std::vector<EmbeddedTextureData> embeddedTextures;
	};
}
