/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>

namespace tinyxml2 { class XMLElement; }
namespace OvTools::Filesystem { class IniFile; }

namespace OvCore::Helpers::AssetFormat
{
	/**
	* The version each format is written in (see Docs/AssetFormats.md). A file without a version is version 0
	*/
	constexpr int kProjectVersion = 1;
	constexpr int kSceneVersion = 1;
	constexpr int kPrefabVersion = 1;
	constexpr int kMaterialVersion = 1;
	constexpr int kAnimationGraphVersion = 1;
	constexpr int kMetadataVersion = 1;

	/**
	* The key of a version in an INI file (a project, an asset's metadata)
	*/
	constexpr const char* kIniVersionKey = "format_version";

	/**
	* Writes the version on an XML file's root, as its attribute "version"
	* @param p_root
	* @param p_version
	*/
	void WriteVersion(tinyxml2::XMLElement& p_root, int p_version);

	/**
	* The version of an XML file, from its root's attribute "version": 0 without one
	* @param p_root
	* @param p_latest (the newest version this engine reads)
	* @param p_file (named in the error)
	* @param p_error (why the file cannot be read)
	* @return nothing if the version is not a whole number of 0 or more, or newer than p_latest
	*/
	std::optional<int> ReadVersion(const tinyxml2::XMLElement& p_root, int p_latest, const std::string& p_file, std::string& p_error);

	/**
	* The version of an INI file, from its key "format_version": 0 without one
	* @param p_file
	* @param p_latest
	* @param p_name (named in the error)
	* @param p_error
	* @return nothing if the version is not a whole number of 0 or more, or newer than p_latest
	*/
	std::optional<int> ReadVersion(const OvTools::Filesystem::IniFile& p_file, int p_latest, const std::string& p_name, std::string& p_error);

	/**
	* Reads the version of an asset's metadata (<asset>.meta). One that cannot be read (newer than this engine, or not a
	* version) is emptied, with a warning: the asset is then read with the default settings
	* @param p_file
	* @param p_name (named in the warning)
	* @return false if the file was emptied
	*/
	bool CheckMetadata(OvTools::Filesystem::IniFile& p_file, const std::string& p_name);

	/**
	* Writes the version of an asset's metadata, before the file is written
	* @param p_file
	*/
	void WriteMetadataVersion(OvTools::Filesystem::IniFile& p_file);

	/**
	* Checks a version read from a file of any kind
	* @param p_version
	* @param p_latest
	* @param p_file
	* @param p_error
	* @return false if it is below 0 or newer than p_latest
	*/
	bool CheckVersion(int p_version, int p_latest, const std::string& p_file, std::string& p_error);
}
