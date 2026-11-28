/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>

#include <OvTools/Filesystem/VirtualFileSystem.h>

namespace
{
	using OvTools::Filesystem::PackageReader;

	struct Mount
	{
		std::filesystem::path folder;
		std::unique_ptr<PackageReader> package;
	};

	// Mounted in order: the last one is looked in first
	std::vector<Mount>& Mounts()
	{
		static std::vector<Mount> mounts;
		return mounts;
	}

	// A PackageReader is not to be shared between threads, and the list may change under a reader
	std::mutex& Lock()
	{
		static std::mutex lock;
		return lock;
	}

	std::filesystem::path Normalize(const std::filesystem::path& p_path)
	{
		std::error_code error;
		std::filesystem::path path = std::filesystem::absolute(p_path, error);
		if (error)
		{
			path = p_path;
		}

		path = path.lexically_normal();

		// "folder/" and "folder" are the same folder
		if (!path.has_filename() && path.has_parent_path() && path != path.root_path())
		{
			path = path.parent_path();
		}
		return path;
	}

	// The path inside a mounted folder, as a package holds it ("" for the folder itself), or nothing when outside
	std::optional<std::string> PathInside(const std::filesystem::path& p_path, const std::filesystem::path& p_folder)
	{
		const std::filesystem::path relative = p_path.lexically_relative(p_folder);
		if (relative.empty() || *relative.begin() == "..")
		{
			return std::nullopt;
		}

		if (relative == ".")
		{
			return std::string{};
		}

		const std::u8string text = relative.generic_u8string();
		return std::string(text.begin(), text.end());
	}
}

void OvTools::Filesystem::VirtualFileSystem::Mount(const std::filesystem::path& p_folder, std::unique_ptr<PackageReader> p_package)
{
	std::scoped_lock guard(Lock());
	Mounts().push_back({ Normalize(p_folder), std::move(p_package) });
}

bool OvTools::Filesystem::VirtualFileSystem::Unmount(const std::filesystem::path& p_folder)
{
	std::scoped_lock guard(Lock());
	const auto folder = Normalize(p_folder);
	return std::erase_if(Mounts(), [&](const ::Mount& p_mount) { return p_mount.folder == folder; }) > 0;
}

void OvTools::Filesystem::VirtualFileSystem::UnmountAll()
{
	std::scoped_lock guard(Lock());
	Mounts().clear();
}

size_t OvTools::Filesystem::VirtualFileSystem::GetMountCount()
{
	std::scoped_lock guard(Lock());
	return Mounts().size();
}

bool OvTools::Filesystem::VirtualFileSystem::Read(const std::filesystem::path& p_path, std::vector<char>& p_content, std::string& p_error)
{
	const auto path = Normalize(p_path);

	{
		std::scoped_lock guard(Lock());
		for (auto mount = Mounts().rbegin(); mount != Mounts().rend(); ++mount)
		{
			const auto inside = PathInside(path, mount->folder);
			const auto* entry = inside ? mount->package->Find(*inside) : nullptr;
			if (entry)
			{
				std::string error;
				if (!mount->package->Read(*entry, p_content, error))
				{
					p_error = std::format("{}: {}", path.string(), error);
					return false;
				}
				return true;
			}
		}
	}

	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		p_error = std::format("{}: not found", path.string());
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	const auto size = std::filesystem::file_size(path, error);
	if (!file || error)
	{
		p_error = std::format("{}: cannot be read", path.string());
		return false;
	}

	p_content.resize(static_cast<size_t>(size));
	if (size > 0 && !file.read(p_content.data(), static_cast<std::streamsize>(size)))
	{
		p_error = std::format("{}: cannot be read", path.string());
		return false;
	}
	return true;
}

bool OvTools::Filesystem::VirtualFileSystem::ReadText(const std::filesystem::path& p_path, std::string& p_text, std::string& p_error)
{
	std::vector<char> content;
	if (!Read(p_path, content, p_error))
	{
		return false;
	}

	p_text.assign(content.begin(), content.end());
	return true;
}

bool OvTools::Filesystem::VirtualFileSystem::Exists(const std::filesystem::path& p_path)
{
	const auto path = Normalize(p_path);

	{
		std::scoped_lock guard(Lock());
		for (const auto& mount : Mounts())
		{
			const auto inside = PathInside(path, mount.folder);
			if (inside && mount.package->Find(*inside))
			{
				return true;
			}
		}
	}

	std::error_code error;
	return std::filesystem::is_regular_file(path, error);
}

bool OvTools::Filesystem::VirtualFileSystem::IsDirectory(const std::filesystem::path& p_path)
{
	const auto path = Normalize(p_path);

	{
		std::scoped_lock guard(Lock());
		for (const auto& mount : Mounts())
		{
			const auto inside = PathInside(path, mount.folder);
			if (!inside)
			{
				continue;
			}

			if (inside->empty())
			{
				return true;
			}

			const std::string prefix = *inside + "/";
			const auto& entries = mount.package->GetEntries();
			const auto first = std::lower_bound(entries.begin(), entries.end(), prefix, [](const PackageEntry& p_entry, const std::string& p_value)
			{
				return p_entry.path < p_value;
			});
			if (first != entries.end() && first->path.starts_with(prefix))
			{
				return true;
			}
		}
	}

	std::error_code error;
	return std::filesystem::is_directory(path, error);
}

std::vector<std::string> OvTools::Filesystem::VirtualFileSystem::List(const std::filesystem::path& p_folder)
{
	const auto folder = Normalize(p_folder);
	std::set<std::string> names;

	{
		std::scoped_lock guard(Lock());
		for (const auto& mount : Mounts())
		{
			const auto inside = PathInside(folder, mount.folder);
			if (!inside)
			{
				continue;
			}

			const std::string prefix = inside->empty() ? std::string{} : *inside + "/";
			for (const auto& entry : mount.package->GetEntries())
			{
				if (entry.path.starts_with(prefix) && entry.path.find('/', prefix.size()) == std::string::npos)
				{
					names.insert(entry.path.substr(prefix.size()));
				}
			}
		}
	}

	std::error_code error;
	for (std::filesystem::directory_iterator entry(folder, error), end; !error && entry != end; entry.increment(error))
	{
		if (entry->is_regular_file(error))
		{
			const std::u8string name = entry->path().filename().u8string();
			names.emplace(name.begin(), name.end());
		}
	}

	return { names.begin(), names.end() };
}

OvTools::Filesystem::ScopedMount::ScopedMount(const std::filesystem::path& p_folder, std::unique_ptr<PackageReader> p_package) :
	m_folder(p_folder)
{
	VirtualFileSystem::Mount(m_folder, std::move(p_package));
}

OvTools::Filesystem::ScopedMount::~ScopedMount()
{
	VirtualFileSystem::Unmount(m_folder);
}
