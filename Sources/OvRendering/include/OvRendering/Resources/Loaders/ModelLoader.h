/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <string>
#include <vector>

#include "OvRendering/Resources/Model.h"
#include "OvRendering/Resources/ModelData.h"
#include "OvRendering/Resources/Parsers/AssimpParser.h"

namespace OvRendering::Resources::Loaders
{
	/**
	* Handle the Model creation and destruction
	*/
	class ModelLoader
	{
	public:
		/**
		* Disabled constructor
		*/
		ModelLoader() = delete;

		/**
		* Create a model
		* @param p_filepath
		* @param p_parserFlags
		* @param p_generateEmbeddedAssets
		*/
		static Model* Create(
			const std::string& p_filepath,
			Parsers::EModelParserFlags p_parserFlags = Parsers::EModelParserFlags::NONE,
			bool p_generateEmbeddedAssets = true
		);

		/**
		* Reload a model from file
		* @param p_model
		* @param p_filePath
		* @param p_parserFlags
		* @param p_generateEmbeddedAssets
		*/
		static void Reload(
			Model& p_model,
			const std::string& p_filePath,
			Parsers::EModelParserFlags p_parserFlags = Parsers::EModelParserFlags::NONE,
			bool p_generateEmbeddedAssets = true
		);

		/**
		* Create a model from a model in memory (an import, a prepared model): its meshes are sent to the GPU here
		* @param p_path
		* @param p_data
		*/
		static Model* Create(const std::string& p_path, ModelData&& p_data);

		/**
		* Reload a model from a model in memory, as Reload does from its file
		* @param p_model
		* @param p_data
		*/
		static void Reload(Model& p_model, ModelData&& p_data);

		/**
		* Import a model file into memory through Assimp, without a graphics context. Returns true on success
		* @param p_filepath
		* @param p_parserFlags
		* @param p_generateEmbeddedAssets
		* @param p_data
		*/
		static bool Import(
			const std::string& p_filepath,
			Parsers::EModelParserFlags p_parserFlags,
			bool p_generateEmbeddedAssets,
			ModelData& p_data
		);

		/**
		* Create a model made of the given meshes, which it then owns (no file is read)
		* @param p_path
		* @param p_meshes
		* @param p_materialNames
		*/
		static Model* Create(const std::string& p_path, std::vector<Mesh*> p_meshes, std::vector<std::string> p_materialNames);

		/**
		* Replace the meshes of a model by the given ones, which it then owns. The previous meshes are destroyed, and so
		* is what came from a file (skeleton, animations, embedded assets)
		* @param p_model
		* @param p_meshes
		* @param p_materialNames
		*/
		static void Replace(Model& p_model, std::vector<Mesh*> p_meshes, std::vector<std::string> p_materialNames);

		/**
		* Disabled constructor
		* @param p_modelInstance
		*/
		static bool Destroy(Model*& p_modelInstance);

	private:
		static Parsers::AssimpParser __ASSIMP;
	};
}
