/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>


namespace OvTools::Utils
{
	/**
	* Provide some tools to get information about a given path
	*/
	class PathParser
	{
	public:
		enum class EFileType
		{
			UNKNOWN,
			MODEL,
			TEXTURE,
			SHADER,
			SHADER_PART,
			MATERIAL,
			SOUND,
			SCENE,
			PREFAB,
			SCRIPT,
			FONT,
			ANIMATION_GRAPH
		};

		/**
		* Disabled constructor
		*/
		PathParser() = delete;

		/**
		* Returns the windows style version of the given path ('/' replaced by '\')
		* @param p_path
		*/
		static std::string MakeWindowsStyle(const std::string& p_path);

		/**
		* Returns the non-windows style version of the given path ('\' replaced by '/')
		* @param p_path
		*/
		static std::string MakeNonWindowsStyle(const std::string& p_path);

		/**
		* Returns a human-friendly representation of a resource path.
		* Backslashes are normalized to forward slashes.
		* Engine paths (starting with ':') are displayed as {ENGINE}/rest/of/path.
		* @param p_path
		*/
		static std::string GetFriendlyPath(const std::string& p_path);

		/**
		* Returns the containing folder of the file or folder identified by the given path
		* @param p_path
		*/
		static std::string GetContainingFolder(const std::string& p_path);

		/**
		* Returns the name of the file or folder identified by the given path
		* @param p_path
		*/
		static std::string GetElementName(const std::string& p_path);

		/**
		* Returns the extension of the file or folder identified by the given path
		* @param p_path
		*/
		static std::string GetExtension(const std::string& p_path);

		/**
		* Convert the EFileType value to a string
		* @param p_fileType
		*/
		static std::string FileTypeToString(EFileType p_fileType);

		/**
		* Convert a string to an EFileType value (inverse of FileTypeToString)
		* @param p_type
		*/
		static EFileType StringToFileType(const std::string& p_type);

		/**
		* Returns the file type of the file identified by the given path
		* @param p_path
		*/
		static EFileType GetFileType(const std::string& p_path);

		/**
		* Returns the real absolute path of an asset path.
		* Engine paths are prefixed with ':'.
		* @param p_path
		* @param p_enginePath
		* @param p_projectPath
		*/
		static std::filesystem::path GetRealPath(
			const std::filesystem::path& p_path,
			const std::filesystem::path& p_enginePath,
			const std::filesystem::path& p_projectPath
		);

		/**
		* The same path under the other name of its format: ".loscene" for ".ovscene", and the reverse (for projects,
		* scenes, prefabs, materials, shaders, shader parts and animation graphs). An empty string for any other extension
		* @param p_path
		*/
		static std::string GetFormatAlias(const std::string& p_path);

		/**
		* A file to read: the file itself if it exists, or else the same file under the other name of its format, if that
		* one exists. Never for a file to write, which keeps the name it is given
		* @param p_file
		*/
		static std::filesystem::path FindFileToRead(const std::filesystem::path& p_file);

		/**
		* Whether a path names a project file: ".loproject", or Overload's ".ovproject", whatever the case of its
		* extension (Windows names ignore it)
		* @param p_path
		*/
		static bool IsProjectFile(const std::filesystem::path& p_path);

		/**
		* The project files directly in a folder, the Lost ones first, then by name. Empty if the folder cannot be read
		* @param p_folder
		*/
		static std::vector<std::filesystem::path> FindProjectFiles(const std::filesystem::path& p_folder);
	};
}
