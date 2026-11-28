/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <format>

#include "OvCore/ResourceManagement/ModelManager.h"

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/AssetFormat.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/TextureManager.h>
#include <OvDebug/Logger.h>
#include <OvRendering/Resources/PreparedModel.h>
#include <OvRendering/Resources/Parsers/EmbeddedAssetPath.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>
#include <OvTools/Utils/PathParser.h>

namespace
{
	using OvCore::ResourceManagement::ModelImportSettings;

	ModelImportSettings GetAssetMetadata(const std::string& p_path)
	{
		auto metaFile = OvTools::Filesystem::IniFile(p_path + ".meta");
		OvCore::Helpers::AssetFormat::CheckMetadata(metaFile, p_path + ".meta");
		ModelImportSettings modelMetadata;

		if (metaFile.GetOrDefault("CALC_TANGENT_SPACE",			true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::CALC_TANGENT_SPACE;
		if (metaFile.GetOrDefault("JOIN_IDENTICAL_VERTICES",		true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::JOIN_IDENTICAL_VERTICES;
		if (metaFile.GetOrDefault("MAKE_LEFT_HANDED",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::MAKE_LEFT_HANDED;
		if (metaFile.GetOrDefault("TRIANGULATE",					true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::TRIANGULATE;
		if (metaFile.GetOrDefault("REMOVE_COMPONENT",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::REMOVE_COMPONENT;
		if (metaFile.GetOrDefault("GEN_NORMALS",					false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::GEN_NORMALS;
		if (metaFile.GetOrDefault("GEN_SMOOTH_NORMALS",			true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::GEN_SMOOTH_NORMALS;
		if (metaFile.GetOrDefault("SPLIT_LARGE_MESHES",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::SPLIT_LARGE_MESHES;
		if (metaFile.GetOrDefault("PRE_TRANSFORM_VERTICES",		false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::PRE_TRANSFORM_VERTICES;
		if (metaFile.GetOrDefault("LIMIT_BONE_WEIGHTS",			true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::LIMIT_BONE_WEIGHTS;
		if (metaFile.GetOrDefault("VALIDATE_DATA_STRUCTURE",		false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::VALIDATE_DATA_STRUCTURE;
		if (metaFile.GetOrDefault("IMPROVE_CACHE_LOCALITY",		true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::IMPROVE_CACHE_LOCALITY;
		if (metaFile.GetOrDefault("REMOVE_REDUNDANT_MATERIALS",	false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::REMOVE_REDUNDANT_MATERIALS;
		if (metaFile.GetOrDefault("FIX_INFACING_NORMALS",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FIX_INFACING_NORMALS;
		if (metaFile.GetOrDefault("SORT_BY_PTYPE",					false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::SORT_BY_PTYPE;
		if (metaFile.GetOrDefault("FIND_DEGENERATES",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FIND_DEGENERATES;
		if (metaFile.GetOrDefault("FIND_INVALID_DATA",				true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FIND_INVALID_DATA;
		if (metaFile.GetOrDefault("GEN_UV_COORDS",					true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::GEN_UV_COORDS;
		if (metaFile.GetOrDefault("TRANSFORM_UV_COORDS",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::TRANSFORM_UV_COORDS;
		if (metaFile.GetOrDefault("FIND_INSTANCES",				true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FIND_INSTANCES;
		if (metaFile.GetOrDefault("OPTIMIZE_MESHES",				true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::OPTIMIZE_MESHES;
		if (metaFile.GetOrDefault("OPTIMIZE_GRAPH",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::OPTIMIZE_GRAPH;
		if (metaFile.GetOrDefault("FLIP_UVS",						false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FLIP_UVS;
		if (metaFile.GetOrDefault("FLIP_WINDING_ORDER",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FLIP_WINDING_ORDER;
		if (metaFile.GetOrDefault("SPLIT_BY_BONE_COUNT",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::SPLIT_BY_BONE_COUNT;
		if (metaFile.GetOrDefault("DEBONE",						false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::DEBONE;
		if (metaFile.GetOrDefault("GLOBAL_SCALE",					true))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::GLOBAL_SCALE;
		if (metaFile.GetOrDefault("EMBED_TEXTURES",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::EMBED_TEXTURES;
		if (metaFile.GetOrDefault("FORCE_GEN_NORMALS",				false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::FORCE_GEN_NORMALS;
		if (metaFile.GetOrDefault("DROP_NORMALS",					false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::DROP_NORMALS;
		if (metaFile.GetOrDefault("GEN_BOUNDING_BOXES",			false))	modelMetadata.parserFlags |= OvRendering::Resources::Parsers::EModelParserFlags::GEN_BOUNDING_BOXES;

		modelMetadata.generateEmbeddedAssets = metaFile.GetOrDefault("GENERATE_EMBEDDED_ASSETS", true);

		return modelMetadata;
	}

	template<typename TResourceManager, typename TIndexParser>
	void ReloadEmbeddedResourcesForModelByType(
		TResourceManager& p_resourceManager,
		const std::string& p_modelPath,
		TIndexParser p_parseIndex
	)
	{
		const std::string normalizedModelPath = OvTools::Utils::PathParser::MakeNonWindowsStyle(p_modelPath);

		for (auto& [resourcePath, resource] : p_resourceManager.GetResources())
		{
			(void)resource;
			const auto embeddedAssetPath = OvRendering::Resources::Parsers::ParseEmbeddedAssetPath(resourcePath.string());
			if (
				!embeddedAssetPath ||
				OvTools::Utils::PathParser::MakeNonWindowsStyle(embeddedAssetPath->modelPath) != normalizedModelPath
			)
			{
				continue;
			}

			if (!p_parseIndex(embeddedAssetPath->assetName))
			{
				continue;
			}

			p_resourceManager.AResourceManager::ReloadResource(resourcePath);
		}
	}

	void ReloadEmbeddedModelResources(const std::string& p_modelPath)
	{
		ReloadEmbeddedResourcesForModelByType(
			OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::MaterialManager>(),
			p_modelPath,
			[](const std::string& p_assetName) { return OvRendering::Resources::Parsers::ParseEmbeddedMaterialIndex(p_assetName).has_value(); }
		);

		ReloadEmbeddedResourcesForModelByType(
			OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::TextureManager>(),
			p_modelPath,
			[](const std::string& p_assetName) { return OvRendering::Resources::Parsers::ParseEmbeddedTextureIndex(p_assetName).has_value(); }
		);
	}
}

OvCore::ResourceManagement::ModelImportSettings OvCore::ResourceManagement::ModelManager::ReadImportSettings(const std::string& p_realPath)
{
	return GetAssetMetadata(p_realPath);
}

bool OvCore::ResourceManagement::ModelManager::LoadModelData(const std::string& p_realPath, OvRendering::Resources::ModelData& p_data)
{
	namespace PreparedModel = OvRendering::Resources::PreparedModel;
	using OvTools::Filesystem::VirtualFileSystem;

	// A built game's model, prepared by its build: no import
	if (const std::string preparedPath = PreparedModel::GetPath(p_realPath); VirtualFileSystem::Exists(preparedPath))
	{
		std::vector<char> bytes;
		std::string error;
		if (VirtualFileSystem::Read(preparedPath, bytes, error) && PreparedModel::Read(bytes, p_data, error))
		{
			return true;
		}

		OVLOG_ERROR(std::format("Cannot read the prepared model \"{}\": {}. The model's file is imported instead", preparedPath, error));
	}

	const auto settings = GetAssetMetadata(p_realPath);
	return OvRendering::Resources::Loaders::ModelLoader::Import(p_realPath, settings.parserFlags, settings.generateEmbeddedAssets, p_data);
}

OvRendering::Resources::Model* OvCore::ResourceManagement::ModelManager::CreateResource(const std::filesystem::path& p_path)
{
	std::string realPath = GetRealPath(p_path).string();
	OvRendering::Resources::ModelData data;
	if (!LoadModelData(realPath, data))
	{
		return nullptr;
	}

	auto model = OvRendering::Resources::Loaders::ModelLoader::Create(realPath, std::move(data));
	const_cast<std::string&>(model->path) = p_path.string(); // Force the resource path to fit the given path
	return model;
}

void OvCore::ResourceManagement::ModelManager::DestroyResource(OvRendering::Resources::Model* p_resource)
{
	m_geometryModels.erase(p_resource);
	OvRendering::Resources::Loaders::ModelLoader::Destroy(p_resource);
}

void OvCore::ResourceManagement::ModelManager::ReloadResource(OvRendering::Resources::Model* p_resource, const std::filesystem::path& p_path)
{
	// A model made from geometry has no file to be reloaded from
	if (m_geometryModels.contains(p_resource))
	{
		return;
	}

	std::string realPath = GetRealPath(p_path).string();
	OvRendering::Resources::ModelData data;
	if (LoadModelData(realPath, data))
	{
		OvRendering::Resources::Loaders::ModelLoader::Reload(*p_resource, std::move(data));
	}

	ReloadEmbeddedModelResources(p_path.string());
}

OvRendering::Resources::Model* OvCore::ResourceManagement::ModelManager::CreateFromGeometry(
	const std::filesystem::path& p_path,
	std::vector<OvRendering::Resources::Mesh*> p_meshes,
	std::vector<std::string> p_materialNames
)
{
	if (auto existing = GetResource(p_path, false))
	{
		if (!m_geometryModels.contains(existing))
		{
			return nullptr;
		}

		// The model keeps its address, but not its content: what caches its meshes (shadows) learns it through the revision
		OvRendering::Resources::Loaders::ModelLoader::Replace(*existing, std::move(p_meshes), std::move(p_materialNames));
		MarkResourcesChanged();
		return existing;
	}

	auto model = OvRendering::Resources::Loaders::ModelLoader::Create(p_path.string(), std::move(p_meshes), std::move(p_materialNames));
	m_geometryModels.insert(model);
	return RegisterResource(p_path, model);
}
