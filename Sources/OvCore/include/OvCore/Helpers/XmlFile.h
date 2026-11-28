/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <tinyxml2.h>

#include <OvTools/Filesystem/VirtualFileSystem.h>

namespace OvCore::Helpers
{
	/**
	* Reads an XML file from a mounted package or the disk. A file found nowhere is left to tinyxml2, so that the
	* document holds the same error as LoadFile gives
	* @param p_document
	* @param p_path
	*/
	inline tinyxml2::XMLError LoadXmlFile(tinyxml2::XMLDocument& p_document, const std::filesystem::path& p_path)
	{
		std::vector<char> bytes;
		std::string error;
		if (!OvTools::Filesystem::VirtualFileSystem::Read(p_path, bytes, error))
		{
			return p_document.LoadFile(p_path.string().c_str());
		}

		return p_document.Parse(bytes.data(), bytes.size());
	}
}
