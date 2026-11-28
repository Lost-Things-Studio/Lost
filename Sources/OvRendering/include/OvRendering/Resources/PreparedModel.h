/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <OvRendering/Resources/ModelData.h>

/**
* A prepared model (.lomodel): a model as its import gives it, written by a game's build so that the game loads it
* without importing its file again (see Docs/AssetFormats.md). It is named after its model, "Rock.fbx.lomodel" for
* "Rock.fbx", and holds everything the import gives: meshes, materials' names, skeleton, animations, embedded assets.
* The same model gives the same bytes
*/
namespace OvRendering::Resources::PreparedModel
{
	/**
	* The version of the format this engine writes, and the newest it reads
	*/
	constexpr uint32_t kVersion = 1;

	/**
	* The size of the header: the mark, the version, a reserved word, the CRC-32 of the body and its size
	*/
	constexpr size_t kHeaderSize = 24;

	/**
	* What follows a model's file name in the name of its prepared version
	*/
	constexpr std::string_view kExtension = ".lomodel";

	/**
	* The path of a model's prepared version
	* @param p_modelPath
	*/
	std::string GetPath(std::string_view p_modelPath);

	/**
	* Writes a model in memory as a prepared model
	* @param p_model
	*/
	std::vector<char> Write(const ModelData& p_model);

	/**
	* Reads a prepared model. False, with why, when it is not one, of a newer version, damaged, or names what it does not
	* hold (a vertex, a node, a bone, a texture); the model is then left empty
	* @param p_bytes
	* @param p_model
	* @param p_error
	*/
	bool Read(std::span<const char> p_bytes, ModelData& p_model, std::string& p_error);
}
