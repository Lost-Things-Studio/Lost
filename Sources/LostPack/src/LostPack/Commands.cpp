/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <filesystem>
#include <format>
#include <fstream>

#include <LostPack/Commands.h>
#include <OvTools/Filesystem/Package.h>

namespace
{
	using OvTools::Filesystem::EPackageMethod;
	using OvTools::Filesystem::PackageBuilder;
	using OvTools::Filesystem::PackageReader;

	std::filesystem::path FromUtf8(const std::string& p_path)
	{
		return std::filesystem::path(std::u8string(p_path.begin(), p_path.end()));
	}

	// The package, or nothing once the reason is reported
	std::unique_ptr<PackageReader> Open(const std::string& p_package, std::ostream& p_err)
	{
		std::string error;
		auto reader = PackageReader::Open(p_package, error);
		if (!reader)
		{
			p_err << p_package << ": " << error << '\n';
		}
		return reader;
	}

	// Whether a path is the folder or lies inside it, once both are made absolute
	bool IsWithin(const std::filesystem::path& p_path, const std::filesystem::path& p_folder)
	{
		std::error_code error;
		const auto path = std::filesystem::weakly_canonical(p_path, error);
		const auto folder = std::filesystem::weakly_canonical(p_folder, error);
		const auto relative = path.lexically_relative(folder);
		return !relative.empty() && *relative.begin() != "..";
	}

	int Pack(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err)
	{
		const std::string& folder = p_arguments[1];
		const std::string& package = p_arguments[2];

		OvTools::Filesystem::PackageOptions options;
		for (size_t i = 3; i < p_arguments.size(); i += 2)
		{
			if (p_arguments[i] != "--project-id" || i + 1 >= p_arguments.size())
			{
				p_err << "pack: unknown option " << p_arguments[i] << "\n\n" << LostPack::Usage();
				return LostPack::USAGE;
			}
			options.projectId = p_arguments[i + 1];
		}

		// A package written into what it packs would hold the previous one the next time
		if (IsWithin(package, folder))
		{
			p_err << package << ": a package cannot be written inside the folder it packs\n";
			return LostPack::FAILURE;
		}

		PackageBuilder builder;
		std::string error;
		if (!builder.AddFolder(folder, error) || !builder.Write(package, options, error))
		{
			p_err << error << '\n';
			return LostPack::FAILURE;
		}

		const auto reader = Open(package, p_err);
		if (!reader)
		{
			return LostPack::FAILURE;
		}

		uint64_t size = 0;
		for (const auto& entry : reader->GetEntries())
		{
			size += entry.size;
		}

		std::error_code sizeError;
		p_out << std::format("Packed {} files, {} bytes, into {} ({} bytes), {}\n",
			reader->GetEntries().size(), size, package, std::filesystem::file_size(package, sizeError), reader->GetManifest().contentHash);
		return LostPack::SUCCESS;
	}

	int List(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err)
	{
		const auto reader = Open(p_arguments[1], p_err);
		if (!reader)
		{
			return LostPack::FAILURE;
		}

		const auto& manifest = reader->GetManifest();
		p_out << std::format("Package version {}, written by Lost {}, project \"{}\"\n", manifest.version, manifest.lostVersion, manifest.projectId);
		p_out << std::format("{} files, {}\n", manifest.fileCount, manifest.contentHash);
		p_out << std::format("{:>12} {:>12}  {:<8} {}\n", "size", "stored", "method", "path");

		for (const auto& entry : reader->GetEntries())
		{
			const char* method = entry.method == EPackageMethod::DEFLATE ? "deflate" : "stored";
			p_out << std::format("{:>12} {:>12}  {:<8} {}\n", entry.size, entry.storedSize, method, entry.path);
		}
		return LostPack::SUCCESS;
	}

	int Verify(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err)
	{
		const std::string& package = p_arguments[1];
		const auto reader = Open(package, p_err);
		if (!reader)
		{
			return LostPack::FAILURE;
		}

		std::string error;
		if (!reader->Verify(error))
		{
			p_err << package << ": " << error << '\n';
			return LostPack::FAILURE;
		}

		p_out << std::format("{}: {} files, all whole, {}\n", package, reader->GetEntries().size(), reader->GetManifest().contentHash);
		return LostPack::SUCCESS;
	}

	int Extract(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err)
	{
		const std::string& package = p_arguments[1];
		const std::filesystem::path folder = p_arguments[2];

		// Nothing already there is overwritten
		std::error_code error;
		if (std::filesystem::exists(folder, error) && (!std::filesystem::is_directory(folder, error) || !std::filesystem::is_empty(folder, error)))
		{
			p_err << folder.string() << ": the folder to extract to must be new or empty\n";
			return LostPack::FAILURE;
		}

		const auto reader = Open(package, p_err);
		if (!reader)
		{
			return LostPack::FAILURE;
		}

		std::vector<char> content;
		std::string readError;
		for (const auto& entry : reader->GetEntries())
		{
			if (!reader->Read(entry, content, readError))
			{
				p_err << package << ": " << readError << '\n';
				return LostPack::FAILURE;
			}

			// The package's paths are checked when it is opened: none leads out of the folder
			const auto target = folder / FromUtf8(entry.path);
			std::filesystem::create_directories(target.parent_path(), error);
			std::ofstream file(target, std::ios::binary | std::ios::trunc);
			file.write(content.data(), static_cast<std::streamsize>(content.size()));
			file.close();
			if (!file)
			{
				p_err << target.string() << ": cannot be written\n";
				return LostPack::FAILURE;
			}
		}

		p_out << std::format("Extracted {} files into {}\n", reader->GetEntries().size(), folder.string());
		return LostPack::SUCCESS;
	}
}

std::string LostPack::Usage()
{
	return
		"Usage: LostPack <command> ...\n"
		"  pack <folder> <package> [--project-id <id>]   Packs every file under the folder\n"
		"  list <package>                                Shows the manifest and each file\n"
		"  verify <package>                              Reads every file and checks it\n"
		"  extract <package> <folder>                    Writes the files into a new or empty folder\n"
		"Ends with 0 when done, 1 when the package or a file is at fault, 2 for a wrong command line.\n";
}

int LostPack::Run(const std::vector<std::string>& p_arguments, std::ostream& p_out, std::ostream& p_err)
{
	if (p_arguments.size() == 1 && (p_arguments[0] == "help" || p_arguments[0] == "--help" || p_arguments[0] == "-h"))
	{
		p_out << Usage();
		return SUCCESS;
	}

	const std::string command = p_arguments.empty() ? std::string{} : p_arguments[0];
	const size_t count = p_arguments.size();

	if (command == "pack" && count >= 3)
	{
		return Pack(p_arguments, p_out, p_err);
	}
	if (command == "list" && count == 2)
	{
		return List(p_arguments, p_out, p_err);
	}
	if (command == "verify" && count == 2)
	{
		return Verify(p_arguments, p_out, p_err);
	}
	if (command == "extract" && count == 3)
	{
		return Extract(p_arguments, p_out, p_err);
	}

	p_err << Usage();
	return USAGE;
}
