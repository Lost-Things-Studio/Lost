/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <OvTools/Filesystem/Package.h>

namespace OvTools::Filesystem
{
	/**
	* Where the engine reads its assets: the packages mounted on folders, then the disk (see Docs/Packages.md).
	* A path under a folder a package is mounted on is looked for in the package, under its path relative to the
	* folder ("/" between names, case sensitive); a package mounted later is looked in first. A path the packages do
	* not hold, or outside every mounted folder, is read from the disk, as it always was. With nothing mounted, as in
	* the editor, it reads the disk only. Paths are made absolute and normalised first. Its functions may be called
	* from any thread
	*/
	class VirtualFileSystem final
	{
	public:
		VirtualFileSystem() = delete;

		/**
		* Mounts a package on a folder, which need not exist on the disk
		* @param p_folder
		* @param p_package
		*/
		static void Mount(const std::filesystem::path& p_folder, std::unique_ptr<PackageReader> p_package);

		/**
		* Removes the packages mounted on a folder, and says whether there were any
		* @param p_folder
		*/
		static bool Unmount(const std::filesystem::path& p_folder);

		/**
		* Removes every package
		*/
		static void UnmountAll();

		/**
		* How many packages are mounted
		*/
		static size_t GetMountCount();

		/**
		* Reads a file's bytes, from a package or the disk. A file a package holds but cannot read (damaged) is not
		* looked for on the disk
		* @param p_path
		* @param p_content
		* @param p_error
		*/
		static bool Read(const std::filesystem::path& p_path, std::vector<char>& p_content, std::string& p_error);

		/**
		* Reads a file as text
		* @param p_path
		* @param p_text
		* @param p_error
		*/
		static bool ReadText(const std::filesystem::path& p_path, std::string& p_text, std::string& p_error);

		/**
		* Whether a file is there, in a package or on the disk
		* @param p_path
		*/
		static bool Exists(const std::filesystem::path& p_path);

		/**
		* Whether a folder is there: a mounted folder, a folder of a package's paths, or one on the disk
		* @param p_path
		*/
		static bool IsDirectory(const std::filesystem::path& p_path);

		/**
		* The names of the files directly in a folder, from the packages and the disk together, sorted and each once
		* @param p_folder
		*/
		static std::vector<std::string> List(const std::filesystem::path& p_folder);
	};

	/**
	* A package mounted for as long as it lives
	*/
	class ScopedMount final
	{
	public:
		ScopedMount(const std::filesystem::path& p_folder, std::unique_ptr<PackageReader> p_package);
		~ScopedMount();

		ScopedMount(const ScopedMount&) = delete;
		ScopedMount& operator=(const ScopedMount&) = delete;

	private:
		std::filesystem::path m_folder;
	};
}
