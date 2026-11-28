/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OvTools::Filesystem
{
	/**
	* How a file's bytes are kept in a package
	*/
	enum class EPackageMethod : uint8_t
	{
		STORED = 0, // As they are
		DEFLATE = 1 // Compressed, in the zlib format
	};

	/**
	* A file of a package, as its index describes it
	*/
	struct PackageEntry
	{
		std::string path; // Relative, with "/"
		EPackageMethod method = EPackageMethod::STORED;
		uint64_t offset = 0; // Of its bytes, from the start of the package
		uint64_t storedSize = 0; // In the package
		uint64_t size = 0; // Once read
		uint32_t crc = 0; // CRC-32 of the bytes once read
	};

	/**
	* What a package says of itself
	*/
	struct PackageManifest
	{
		uint32_t version = 0; // Of the format
		std::string lostVersion; // Of the engine that wrote it
		std::string projectId; // Of the project it was made from, or empty
		uint64_t fileCount = 0;
		std::string contentHash; // "fnv1a64:" then 16 hexadecimal digits, over every path and its bytes
	};

	/**
	* What a package is written with
	*/
	struct PackageOptions
	{
		std::string projectId;
		std::string lostVersion; // Empty: this engine's version
	};

	/**
	* The rules of the package format (see Docs/Packages.md)
	*/
	namespace PackageFormat
	{
		constexpr uint32_t kVersion = 1;
		constexpr uint64_t kHeaderSize = 32;
		constexpr uint64_t kMaxDeflateRatio = 1032; // What deflate can reach at best, so a larger size is a lie

		/**
		* Whether a package may hold a file under this path: relative, in UTF-8, with "/" between names, and no
		* empty name, ".", "..", "\", ":" or control character
		* @param p_path
		*/
		bool IsValidPath(std::string_view p_path);
	}

	/**
	* Gathers files, then writes them as a package. The same files give the same bytes: paths sorted, a fixed
	* compression level, and no date. Each file is read whole into memory as it is written
	*/
	class PackageBuilder final
	{
	public:
		/**
		* Adds every regular file under the folder, each under its path relative to it
		* @param p_folder
		* @param p_error
		*/
		bool AddFolder(const std::filesystem::path& p_folder, std::string& p_error);

		/**
		* Adds a file of the disk, read when the package is written
		* @param p_packagePath
		* @param p_file
		* @param p_error
		*/
		bool AddFile(std::string_view p_packagePath, const std::filesystem::path& p_file, std::string& p_error);

		/**
		* Adds bytes made in memory
		* @param p_packagePath
		* @param p_data
		* @param p_error
		*/
		bool AddData(std::string_view p_packagePath, std::vector<char> p_data, std::string& p_error);

		/**
		* How many files were added
		*/
		size_t GetFileCount() const;

		/**
		* Writes the package: to a temporary file next to it first, then renamed, so a failure leaves no half package
		* @param p_package
		* @param p_options
		* @param p_error
		*/
		bool Write(const std::filesystem::path& p_package, const PackageOptions& p_options, std::string& p_error) const;

	private:
		struct Source
		{
			std::filesystem::path file;
			std::optional<std::vector<char>> data;
		};

		bool Add(std::string_view p_packagePath, Source p_source, std::string& p_error);

		std::map<std::string, Source> m_sources; // Sorted byte by byte
	};

	/**
	* Reads a package. Its header, manifest and index are checked when it is opened, and each file's size and CRC
	* when it is read. Not to be shared between threads
	*/
	class PackageReader final
	{
	public:
		/**
		* The package, or nothing and why
		* @param p_package
		* @param p_error
		*/
		static std::unique_ptr<PackageReader> Open(const std::filesystem::path& p_package, std::string& p_error);

		const PackageManifest& GetManifest() const;
		const std::vector<PackageEntry>& GetEntries() const;

		/**
		* The file under this path, or nullptr
		* @param p_path
		*/
		const PackageEntry* Find(std::string_view p_path) const;

		/**
		* Reads a file's bytes, checked against its size and CRC
		* @param p_entry
		* @param p_content
		* @param p_error
		*/
		bool Read(const PackageEntry& p_entry, std::vector<char>& p_content, std::string& p_error);

		/**
		* Reads every file, and checks the manifest's content hash
		* @param p_error
		*/
		bool Verify(std::string& p_error);

	private:
		PackageReader() = default;

		std::ifstream m_file;
		PackageManifest m_manifest;
		std::vector<PackageEntry> m_entries;
	};
}
