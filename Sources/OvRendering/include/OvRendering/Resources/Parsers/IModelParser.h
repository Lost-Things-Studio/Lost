/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <string>

#include <OvRendering/Resources/ModelData.h>
#include <OvRendering/Resources/Parsers/EModelParserFlags.h>

namespace OvRendering::Resources::Parsers
{
	/**
	* Interface for any model parser
	*/
	class IModelParser
	{
	public:
		/**
		* Imports a model file into memory, without a graphics context. Returns true on success
		* @param p_fileName
		* @param p_parserFlags
		* @param p_generateEmbeddedAssets
		* @param p_model
		*/
		virtual bool Import
		(
			const std::string& p_fileName,
			EModelParserFlags p_parserFlags,
			bool p_generateEmbeddedAssets,
			ModelData& p_model
		) = 0;
	};
}
