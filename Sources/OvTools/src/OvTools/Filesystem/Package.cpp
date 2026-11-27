/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <format>
#include <limits>

#include <zlib.h>

#include <OvTools/Filesystem/Package.h>
#include <OvTools/Utils/Json.h>

namespace
{
	using OvTools::Filesystem::EPackageMethod;
	using OvTools::Filesystem::PackageEntry;
	namespace PackageFormat = OvTools::Filesystem::PackageFormat;
	namespace Json = OvTools::Utils::Json;

	constexpr std::array<char, 4> kMagic = { 'L', 'O', 'P', 'K' };
	constexpr int kCompressionLevel = 9;
	constexpr uint64_t kChunk = 1u << 30; // What zlib is handed at once: its sizes are 32-bit on Windows
	constexpr std::string_view kTemporarySuffix = ".~tmp";
	constexpr std::string_view kHashPrefix = "fnv1a64:";

	// Little endian, whatever the machine
	template<typename T>
	void Put(std::vector<char>& p_out, T p_value)
	{
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			p_out.push_back(static_cast<char>(static_cast<uint8_t>(p_value >> (8 * i))));
		}
	}

	template<typename T>
	bool Take(const std::vector<char>& p_in, size_t& p_at, T& p_value)
	{
		if (p_in.size() - p_at < sizeof(T))
		{
			return false;
		}

		p_value = 0;
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			p_value = static_cast<T>(p_value | static_cast<T>(static_cast<T>(static_cast<uint8_t>(p_in[p_at + i])) << (8 * i)));
		}
		p_at += sizeof(T);
		return true;
	}

	uint32_t Crc32(const char* p_data, size_t p_size)
	{
		uLong crc = crc32_z(0L, Z_NULL, 0);
		crc = crc32_z(crc, reinterpret_cast<const Bytef*>(p_data), p_size);
		return static_cast<uint32_t>(crc);
	}

	// FNV-1a, 64 bits: unlike a CRC, wide enough to tell two packages' contents apart
	class ContentHash
	{
	public:
		void Add(const char* p_data, size_t p_size)
		{
			for (size_t i = 0; i < p_size; ++i)
			{
				m_value ^= static_cast<uint8_t>(p_data[i]);
				m_value *= 1099511628211ull;
			}
		}

		// A path, then its size, then its bytes: no two lists of files give the same sequence
		void AddFile(std::string_view p_path, const std::vector<char>& p_content)
		{
			const char separator = 0;
			Add(p_path.data(), p_path.size());
			Add(&separator, 1);

			std::vector<char> size;
			Put<uint64_t>(size, p_content.size());
			Add(size.data(), size.size());
			Add(p_content.data(), p_content.size());
		}

		std::string ToString() const
		{
			return std::format("{}{:016x}", kHashPrefix, m_value);
		}

	private:
		uint64_t m_value = 14695981039346656037ull;
	};

	bool Deflate(const std::vector<char>& p_in, std::vector<char>& p_out)
	{
		z_stream stream{};
		if (deflateInit(&stream, kCompressionLevel) != Z_OK)
		{
			return false;
		}

		p_out.resize(static_cast<size_t>(deflateBound(&stream, static_cast<uLong>(std::min<uint64_t>(p_in.size(), kChunk)))));
		size_t consumed = 0;
		size_t produced = 0;
		int result = Z_OK;

		while (result != Z_STREAM_END)
		{
			const size_t input = std::min<size_t>(p_in.size() - consumed, kChunk);
			stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(p_in.data() + consumed));
			stream.avail_in = static_cast<uInt>(input);

			if (produced == p_out.size())
			{
				p_out.resize(p_out.size() * 2); // Rarely: the first size is what deflate needs at worst
			}
			const size_t room = std::min<size_t>(p_out.size() - produced, kChunk);
			stream.next_out = reinterpret_cast<Bytef*>(p_out.data() + produced);
			stream.avail_out = static_cast<uInt>(room);

			const bool last = consumed + input == p_in.size();
			result = deflate(&stream, last ? Z_FINISH : Z_NO_FLUSH);
			if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			{
				deflateEnd(&stream);
				return false;
			}

			consumed += input - stream.avail_in;
			produced += room - stream.avail_out;
		}

		deflateEnd(&stream);
		p_out.resize(produced);
		return true;
	}

	bool Inflate(const std::vector<char>& p_in, uint64_t p_size, std::vector<char>& p_out)
	{
		z_stream stream{};
		if (inflateInit(&stream) != Z_OK)
		{
			return false;
		}

		p_out.resize(static_cast<size_t>(p_size));
		size_t consumed = 0;
		size_t produced = 0;
		int result = Z_OK;

		while (result != Z_STREAM_END)
		{
			const size_t input = std::min<size_t>(p_in.size() - consumed, kChunk);
			const size_t room = std::min<size_t>(p_out.size() - produced, kChunk);
			stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(p_in.data() + consumed));
			stream.avail_in = static_cast<uInt>(input);
			stream.next_out = reinterpret_cast<Bytef*>(p_out.data() + produced);
			stream.avail_out = static_cast<uInt>(room);

			result = inflate(&stream, Z_NO_FLUSH);
			consumed += input - stream.avail_in;
			produced += room - stream.avail_out;

			// A stream cut short, or longer than its size, is no better than a damaged one
			const bool stuck = result == Z_BUF_ERROR || (result == Z_OK && input == 0 && room == 0);
			if (result == Z_NEED_DICT || result == Z_DATA_ERROR || result == Z_MEM_ERROR || result == Z_STREAM_ERROR || stuck)
			{
				inflateEnd(&stream);
				return false;
			}
		}

		inflateEnd(&stream);
		return produced == p_out.size() && consumed == p_in.size();
	}

	bool ReadWholeFile(const std::filesystem::path& p_file, std::vector<char>& p_content)
	{
		std::ifstream file(p_file, std::ios::binary);
		if (!file)
		{
			return false;
		}

		std::error_code error;
		const auto size = std::filesystem::file_size(p_file, error);
		if (error)
		{
			return false;
		}

		p_content.resize(static_cast<size_t>(size));
		if (size > 0 && !file.read(p_content.data(), static_cast<std::streamsize>(size)))
		{
			return false;
		}
		return true;
	}

	bool IsValidUtf8(std::string_view p_text)
	{
		for (size_t i = 0; i < p_text.size();)
		{
			const auto lead = static_cast<uint8_t>(p_text[i]);
			const size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
			if (length == 0 || i + length > p_text.size())
			{
				return false;
			}

			uint32_t point = length == 1 ? lead : lead & (0xFFu >> (length + 1));
			for (size_t k = 1; k < length; ++k)
			{
				const auto next = static_cast<uint8_t>(p_text[i + k]);
				if ((next >> 6) != 0x2)
				{
					return false;
				}
				point = (point << 6) | (next & 0x3Fu);
			}

			// No over-long form, no surrogate, nothing past U+10FFFF
			static constexpr std::array<uint32_t, 5> kMinimum = { 0, 0, 0x80, 0x800, 0x10000 };
			if (point < kMinimum[length] || (point >= 0xD800 && point <= 0xDFFF) || point > 0x10FFFF)
			{
				return false;
			}
			i += length;
		}
		return true;
	}
}

bool OvTools::Filesystem::PackageFormat::IsValidPath(std::string_view p_path)
{
	if (p_path.empty() || p_path.size() > std::numeric_limits<uint16_t>::max() || !IsValidUtf8(p_path))
	{
		return false;
	}

	for (const char character : p_path)
	{
		if (static_cast<uint8_t>(character) < 0x20 || character == 0x7F || character == '\\' || character == ':')
		{
			return false;
		}
	}

	size_t start = 0;
	while (start <= p_path.size())
	{
		const size_t end = std::min(p_path.find('/', start), p_path.size());
		const std::string_view name = p_path.substr(start, end - start);
		if (name.empty() || name == "." || name == "..")
		{
			return false;
		}
		start = end + 1;
	}
	return true;
}

bool OvTools::Filesystem::PackageBuilder::AddFolder(const std::filesystem::path& p_folder, std::string& p_error)
{
	std::error_code error;
	if (!std::filesystem::is_directory(p_folder, error))
	{
		p_error = std::format("{} is not a folder", p_folder.string());
		return false;
	}

	for (std::filesystem::recursive_directory_iterator entry(p_folder, error), end; !error && entry != end; entry.increment(error))
	{
		// Folders are walked; links, which may lead out of the folder, and other kinds of files are left out
		if (entry->is_symlink(error) || !entry->is_regular_file(error))
		{
			continue;
		}

		const std::u8string relative = entry->path().lexically_relative(p_folder).generic_u8string();
		const std::string path(relative.begin(), relative.end());
		if (!Add(path, Source{ entry->path(), std::nullopt }, p_error))
		{
			return false;
		}
	}

	if (error)
	{
		p_error = std::format("{} cannot be read through: {}", p_folder.string(), error.message());
		return false;
	}
	return true;
}

bool OvTools::Filesystem::PackageBuilder::AddFile(std::string_view p_packagePath, const std::filesystem::path& p_file, std::string& p_error)
{
	return Add(p_packagePath, Source{ p_file, std::nullopt }, p_error);
}

bool OvTools::Filesystem::PackageBuilder::AddData(std::string_view p_packagePath, std::vector<char> p_data, std::string& p_error)
{
	return Add(p_packagePath, Source{ {}, std::move(p_data) }, p_error);
}

size_t OvTools::Filesystem::PackageBuilder::GetFileCount() const
{
	return m_sources.size();
}

bool OvTools::Filesystem::PackageBuilder::Add(std::string_view p_packagePath, Source p_source, std::string& p_error)
{
	if (!PackageFormat::IsValidPath(p_packagePath))
	{
		p_error = std::format("\"{}\" cannot be a path in a package", p_packagePath);
		return false;
	}

	if (!m_sources.emplace(std::string(p_packagePath), std::move(p_source)).second)
	{
		p_error = std::format("{} is in the package already", p_packagePath);
		return false;
	}
	return true;
}

bool OvTools::Filesystem::PackageBuilder::Write(const std::filesystem::path& p_package, const PackageOptions& p_options, std::string& p_error) const
{
	std::filesystem::path temporary = p_package;
	temporary += std::string(kTemporarySuffix);

	std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		p_error = std::format("{} cannot be written", temporary.string());
		return false;
	}

	const auto fail = [&](std::string p_message)
	{
		file.close();
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored);
		p_error = std::move(p_message);
		return false;
	};

	// The header comes last, once the index is known
	const std::vector<char> blank(PackageFormat::kHeaderSize, 0);
	file.write(blank.data(), static_cast<std::streamsize>(blank.size()));

	std::vector<PackageEntry> entries;
	ContentHash hash;
	uint64_t offset = PackageFormat::kHeaderSize;

	for (const auto& [path, source] : m_sources)
	{
		std::vector<char> content;
		if (source.data)
		{
			content = *source.data;
		}
		else if (!ReadWholeFile(source.file, content))
		{
			return fail(std::format("{} cannot be read", source.file.string()));
		}

		PackageEntry entry;
		entry.path = path;
		entry.offset = offset;
		entry.size = content.size();
		entry.crc = Crc32(content.data(), content.size());
		hash.AddFile(path, content);

		// Compressed only when it gains something
		std::vector<char> compressed;
		if (!Deflate(content, compressed))
		{
			return fail(std::format("{} cannot be compressed", path));
		}

		const std::vector<char>& stored = compressed.size() < content.size() ? compressed : content;
		entry.method = &stored == &compressed ? EPackageMethod::DEFLATE : EPackageMethod::STORED;
		entry.storedSize = stored.size();
		file.write(stored.data(), static_cast<std::streamsize>(stored.size()));

		offset += entry.storedSize;
		entries.push_back(std::move(entry));
	}

	const Json::Value manifest = Json::Object{
		{ "version", static_cast<double>(PackageFormat::kVersion) },
		{ "lostVersion", p_options.lostVersion.empty() ? std::string(LOST_VERSION) : p_options.lostVersion },
		{ "projectId", p_options.projectId },
		{ "fileCount", static_cast<double>(entries.size()) },
		{ "contentHash", hash.ToString() }
	};
	const std::string manifestText = Json::Write(manifest);

	std::vector<char> index;
	Put<uint32_t>(index, static_cast<uint32_t>(manifestText.size()));
	index.insert(index.end(), manifestText.begin(), manifestText.end());
	Put<uint32_t>(index, static_cast<uint32_t>(entries.size()));
	for (const auto& entry : entries)
	{
		Put<uint16_t>(index, static_cast<uint16_t>(entry.path.size()));
		index.insert(index.end(), entry.path.begin(), entry.path.end());
		Put<uint8_t>(index, static_cast<uint8_t>(entry.method));
		Put<uint64_t>(index, entry.offset);
		Put<uint64_t>(index, entry.storedSize);
		Put<uint64_t>(index, entry.size);
		Put<uint32_t>(index, entry.crc);
	}
	file.write(index.data(), static_cast<std::streamsize>(index.size()));

	std::vector<char> header(kMagic.begin(), kMagic.end());
	Put<uint32_t>(header, PackageFormat::kVersion);
	Put<uint64_t>(header, offset);
	Put<uint64_t>(header, index.size());
	Put<uint32_t>(header, Crc32(index.data(), index.size()));
	Put<uint32_t>(header, 0); // Reserved
	file.seekp(0);
	file.write(header.data(), static_cast<std::streamsize>(header.size()));

	file.close();
	if (!file)
	{
		return fail(std::format("{} cannot be written", temporary.string()));
	}

	std::error_code error;
	std::filesystem::rename(temporary, p_package, error);
	if (error)
	{
		return fail(std::format("{} cannot be written: {}", p_package.string(), error.message()));
	}
	return true;
}

std::unique_ptr<OvTools::Filesystem::PackageReader> OvTools::Filesystem::PackageReader::Open(const std::filesystem::path& p_package, std::string& p_error)
{
	std::unique_ptr<PackageReader> reader(new PackageReader());
	reader->m_file.open(p_package, std::ios::binary);

	std::error_code error;
	const uint64_t fileSize = std::filesystem::file_size(p_package, error);
	if (!reader->m_file || error)
	{
		p_error = "it cannot be read";
		return nullptr;
	}

	std::vector<char> header(PackageFormat::kHeaderSize);
	if (fileSize < PackageFormat::kHeaderSize || !reader->m_file.read(header.data(), static_cast<std::streamsize>(header.size())))
	{
		p_error = "it is too short to be a package";
		return nullptr;
	}

	if (!std::equal(kMagic.begin(), kMagic.end(), header.begin()))
	{
		p_error = "it is not a package: it does not start with LOPK";
		return nullptr;
	}

	size_t at = kMagic.size();
	uint32_t version = 0, indexCrc = 0, reserved = 0;
	uint64_t indexOffset = 0, indexSize = 0;
	Take(header, at, version);
	Take(header, at, indexOffset);
	Take(header, at, indexSize);
	Take(header, at, indexCrc);
	Take(header, at, reserved);

	if (version == 0 || version > PackageFormat::kVersion)
	{
		p_error = version == 0 ?
			"it has no version" :
			std::format("it is version {} of the format, newer than this engine reads (version {} at most)", version, PackageFormat::kVersion);
		return nullptr;
	}

	if (reserved != 0)
	{
		p_error = "it has flags this engine does not know";
		return nullptr;
	}

	// The index ends the file, after the data
	if (indexOffset < PackageFormat::kHeaderSize || indexOffset > fileSize || indexSize != fileSize - indexOffset)
	{
		p_error = "it is cut short, or its index is not where its header says";
		return nullptr;
	}

	std::vector<char> index(static_cast<size_t>(indexSize));
	reader->m_file.seekg(static_cast<std::streamoff>(indexOffset));
	if (!reader->m_file.read(index.data(), static_cast<std::streamsize>(index.size())))
	{
		p_error = "it cannot be read";
		return nullptr;
	}

	if (Crc32(index.data(), index.size()) != indexCrc)
	{
		p_error = "its index is damaged: its CRC does not match";
		return nullptr;
	}

	at = 0;
	uint32_t manifestSize = 0;
	if (!Take(index, at, manifestSize) || index.size() - at < manifestSize)
	{
		p_error = "its index is damaged: its manifest is cut short";
		return nullptr;
	}

	std::string jsonError;
	const auto manifestValue = Json::Parse(std::string_view(index.data() + at, manifestSize), jsonError);
	at += manifestSize;
	const Json::Object* manifest = manifestValue ? manifestValue->AsObject() : nullptr;
	if (!manifest)
	{
		p_error = manifestValue ? "its manifest is not an object" : std::format("its manifest cannot be read: {}", jsonError);
		return nullptr;
	}

	for (const auto& member : *manifest)
	{
		const auto* number = member.value.AsNumber();
		const auto* text = member.value.AsString();

		if (member.name == "version" && number && *number == static_cast<double>(version))
		{
			reader->m_manifest.version = version;
		}
		else if (member.name == "lostVersion" && text)
		{
			reader->m_manifest.lostVersion = *text;
		}
		else if (member.name == "projectId" && text)
		{
			reader->m_manifest.projectId = *text;
		}
		else if (member.name == "fileCount" && number && *number >= 0 && *number <= 9007199254740992.0 && *number == static_cast<double>(static_cast<uint64_t>(*number)))
		{
			reader->m_manifest.fileCount = static_cast<uint64_t>(*number);
		}
		else if (member.name == "contentHash" && text && text->size() == kHashPrefix.size() + 16 && text->starts_with(kHashPrefix))
		{
			reader->m_manifest.contentHash = *text;
		}
		else
		{
			p_error = std::format("its manifest has a \"{}\" this engine does not read", member.name);
			return nullptr;
		}
	}

	if (reader->m_manifest.version == 0 || reader->m_manifest.contentHash.empty())
	{
		p_error = "its manifest has no version or no content hash";
		return nullptr;
	}

	uint32_t count = 0;
	if (!Take(index, at, count) || count != reader->m_manifest.fileCount)
	{
		p_error = "its index is damaged: its count of files is not the manifest's";
		return nullptr;
	}

	// The files lie one after the other, in the index's order, from the header to the index
	uint64_t expectedOffset = PackageFormat::kHeaderSize;
	for (uint32_t i = 0; i < count; ++i)
	{
		PackageEntry entry;
		uint16_t pathSize = 0;
		uint8_t method = 0;
		if (!Take(index, at, pathSize) || index.size() - at < pathSize)
		{
			p_error = "its index is damaged: it is cut short";
			return nullptr;
		}
		entry.path.assign(index.data() + at, pathSize);
		at += pathSize;

		if (!Take(index, at, method) || !Take(index, at, entry.offset) || !Take(index, at, entry.storedSize) ||
			!Take(index, at, entry.size) || !Take(index, at, entry.crc))
		{
			p_error = "its index is damaged: it is cut short";
			return nullptr;
		}

		if (!PackageFormat::IsValidPath(entry.path) || (!reader->m_entries.empty() && reader->m_entries.back().path >= entry.path))
		{
			p_error = std::format("its index is damaged: \"{}\" is not a path in its place", entry.path);
			return nullptr;
		}

		if (method > static_cast<uint8_t>(EPackageMethod::DEFLATE))
		{
			p_error = std::format("it holds {} by method {}, which is not one this engine reads", entry.path, method);
			return nullptr;
		}
		entry.method = static_cast<EPackageMethod>(method);

		const bool inPlace = entry.offset == expectedOffset && entry.storedSize <= indexOffset - entry.offset;
		const bool sizeHolds = entry.method == EPackageMethod::STORED ?
			entry.size == entry.storedSize :
			entry.size / PackageFormat::kMaxDeflateRatio <= entry.storedSize;
		if (!inPlace || !sizeHolds)
		{
			p_error = std::format("its index is damaged: {} is not where or what it says", entry.path);
			return nullptr;
		}

		expectedOffset = entry.offset + entry.storedSize;
		reader->m_entries.push_back(std::move(entry));
	}

	if (at != index.size() || expectedOffset != indexOffset)
	{
		p_error = "its index is damaged: it does not account for the whole package";
		return nullptr;
	}

	return reader;
}

const OvTools::Filesystem::PackageManifest& OvTools::Filesystem::PackageReader::GetManifest() const
{
	return m_manifest;
}

const std::vector<OvTools::Filesystem::PackageEntry>& OvTools::Filesystem::PackageReader::GetEntries() const
{
	return m_entries;
}

const OvTools::Filesystem::PackageEntry* OvTools::Filesystem::PackageReader::Find(std::string_view p_path) const
{
	const auto found = std::lower_bound(m_entries.begin(), m_entries.end(), p_path, [](const PackageEntry& p_entry, std::string_view p_value)
	{
		return std::string_view(p_entry.path) < p_value;
	});

	return found != m_entries.end() && found->path == p_path ? &*found : nullptr;
}

bool OvTools::Filesystem::PackageReader::Read(const PackageEntry& p_entry, std::vector<char>& p_content, std::string& p_error)
{
	std::vector<char> stored(static_cast<size_t>(p_entry.storedSize));
	m_file.clear();
	m_file.seekg(static_cast<std::streamoff>(p_entry.offset));
	if (!m_file.read(stored.data(), static_cast<std::streamsize>(stored.size())))
	{
		p_error = std::format("{} cannot be read", p_entry.path);
		return false;
	}

	if (p_entry.method == EPackageMethod::DEFLATE)
	{
		if (!Inflate(stored, p_entry.size, p_content))
		{
			p_error = std::format("{} is damaged: it cannot be decompressed to its size", p_entry.path);
			return false;
		}
	}
	else
	{
		p_content = std::move(stored);
	}

	if (Crc32(p_content.data(), p_content.size()) != p_entry.crc)
	{
		p_error = std::format("{} is damaged: its CRC does not match", p_entry.path);
		return false;
	}
	return true;
}

bool OvTools::Filesystem::PackageReader::Verify(std::string& p_error)
{
	ContentHash hash;
	std::vector<char> content;

	for (const auto& entry : m_entries)
	{
		if (!Read(entry, content, p_error))
		{
			return false;
		}
		hash.AddFile(entry.path, content);
	}

	if (hash.ToString() != m_manifest.contentHash)
	{
		p_error = std::format("it does not hold what its manifest says: its content hash is {}, not {}", hash.ToString(), m_manifest.contentHash);
		return false;
	}
	return true;
}
