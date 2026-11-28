/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <span>
#include <utility>

#include "OvRendering/Resources/Loaders/ModelLoader.h"

OvRendering::Resources::Parsers::AssimpParser OvRendering::Resources::Loaders::ModelLoader::__ASSIMP;

OvRendering::Resources::Model* OvRendering::Resources::Loaders::ModelLoader::Create(
	const std::string& p_filepath,
	Parsers::EModelParserFlags p_parserFlags,
	bool p_generateEmbeddedAssets
)
{
	ModelData data;
	if (!Import(p_filepath, p_parserFlags, p_generateEmbeddedAssets, data))
	{
		return nullptr;
	}

	return Create(p_filepath, std::move(data));
}

void OvRendering::Resources::Loaders::ModelLoader::Reload(
	Model& p_model,
	const std::string& p_filePath,
	Parsers::EModelParserFlags p_parserFlags,
	bool p_generateEmbeddedAssets
)
{
	ModelData data;
	if (Import(p_filePath, p_parserFlags, p_generateEmbeddedAssets, data))
	{
		Reload(p_model, std::move(data));
	}
}

OvRendering::Resources::Model* OvRendering::Resources::Loaders::ModelLoader::Create(const std::string& p_path, ModelData&& p_data)
{
	Model* result = new Model(p_path);

	result->m_meshes.reserve(p_data.meshes.size());
	for (const auto& mesh : p_data.meshes)
	{
		// The model owns its meshes
		result->m_meshes.push_back(mesh.IsSkinned() ?
			new Mesh(std::span(mesh.skinnedVertices), std::span(mesh.indices), mesh.materialIndex) :
			new Mesh(std::span(mesh.vertices), std::span(mesh.indices), mesh.materialIndex));
	}

	result->m_materialNames = std::move(p_data.materialNames);
	result->m_skeleton = std::move(p_data.skeleton);
	result->m_animations = std::move(p_data.animations);
	result->m_embeddedMaterials = std::move(p_data.embeddedMaterials);
	result->m_embeddedTextures = std::move(p_data.embeddedTextures);
	result->ComputeBoundingSphere();

	return result;
}

void OvRendering::Resources::Loaders::ModelLoader::Reload(Model& p_model, ModelData&& p_data)
{
	Model* newModel = Create(p_model.path, std::move(p_data));

	p_model.m_meshes = newModel->m_meshes;
	p_model.m_materialNames = newModel->m_materialNames;
	p_model.m_embeddedMaterials = std::move(newModel->m_embeddedMaterials);
	p_model.m_embeddedTextures = std::move(newModel->m_embeddedTextures);
	p_model.m_skeleton = std::move(newModel->m_skeleton);
	p_model.m_animations = std::move(newModel->m_animations);
	p_model.m_boundingSphere = newModel->m_boundingSphere;
	newModel->m_meshes.clear();
	delete newModel;
}

bool OvRendering::Resources::Loaders::ModelLoader::Import(
	const std::string& p_filepath,
	Parsers::EModelParserFlags p_parserFlags,
	bool p_generateEmbeddedAssets,
	ModelData& p_data
)
{
	return __ASSIMP.Import(p_filepath, p_parserFlags, p_generateEmbeddedAssets, p_data);
}

OvRendering::Resources::Model* OvRendering::Resources::Loaders::ModelLoader::Create(
	const std::string& p_path,
	std::vector<Mesh*> p_meshes,
	std::vector<std::string> p_materialNames
)
{
	Model* result = new Model(p_path);
	Replace(*result, std::move(p_meshes), std::move(p_materialNames));
	return result;
}

void OvRendering::Resources::Loaders::ModelLoader::Replace(
	Model& p_model,
	std::vector<Mesh*> p_meshes,
	std::vector<std::string> p_materialNames
)
{
	for (auto mesh : p_model.m_meshes)
	{
		delete mesh;
	}

	p_model.m_meshes = std::move(p_meshes);
	p_model.m_materialNames = std::move(p_materialNames);
	p_model.m_embeddedMaterials.clear();
	p_model.m_embeddedTextures.clear();
	p_model.m_skeleton.reset();
	p_model.m_animations.clear();
	p_model.ComputeBoundingSphere();
}

bool OvRendering::Resources::Loaders::ModelLoader::Destroy(Model*& p_modelInstance)
{
	if (p_modelInstance)
	{
		delete p_modelInstance;
		p_modelInstance = nullptr;

		return true;
	}

	return false;
}
