/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvRendering/Resources/Parsers/IModelParser.h>

namespace OvRendering::Resources::Parsers
{
	/**
	* Imports models through Assimp: their meshes, materials, skeleton, animations and embedded assets
	*/
	class AssimpParser : public IModelParser
	{
	public:
		/**
		* Imports a model file into memory, without a graphics context. Returns true on success
		* @param p_fileName
		* @param p_parserFlags
		* @param p_generateEmbeddedAssets
		* @param p_model
		*/
		bool Import
		(
			const std::string& p_fileName,
			EModelParserFlags p_parserFlags,
			bool p_generateEmbeddedAssets,
			ModelData& p_model
		) override;
	};
}
