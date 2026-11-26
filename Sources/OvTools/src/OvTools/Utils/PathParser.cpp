/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include <OvTools/Utils/PathParser.h>

std::string OvTools::Utils::PathParser::MakeWindowsStyle(const std::string & p_path)
{
	std::string result;
	result.resize(p_path.size());

	for (size_t i = 0; i < p_path.size(); ++i)
		result[i] = p_path[i] == '/' ? '\\' : p_path[i];

	return result;
}

std::string OvTools::Utils::PathParser::MakeNonWindowsStyle(const std::string & p_path)
{
	std::string result;
	result.resize(p_path.size());

	for (size_t i = 0; i < p_path.size(); ++i)
		result[i] = p_path[i] == '\\' ? '/' : p_path[i];

	return result;
}

std::string OvTools::Utils::PathParser::GetFriendlyPath(const std::string& p_path)
{
	const std::string normalized = MakeNonWindowsStyle(p_path);

	if (!normalized.empty() && normalized[0] == ':')
		return "{ENGINE}/" + normalized.substr(1);

	return normalized;
}

std::string OvTools::Utils::PathParser::GetContainingFolder(const std::string & p_path)
{
	std::string result;

	bool extraction = false;

	for (auto it = p_path.rbegin(); it != p_path.rend(); ++it)
	{
		if (extraction)
			result += *it;

		if (!extraction && it != p_path.rbegin() && (*it == '\\' || *it == '/'))
			extraction = true;
	}

	std::reverse(result.begin(), result.end());

	if (!result.empty() && result.back() != '\\')
		result += '\\';

	return result;
}

std::string OvTools::Utils::PathParser::GetElementName(const std::string & p_path)
{
	std::string result;

	std::string path = p_path;
	if (!path.empty() && path.back() == '\\')
		path.pop_back();

	for (auto it = path.rbegin(); it != path.rend() && *it != '\\' && *it != '/'; ++it)
		result += *it;

	std::reverse(result.begin(), result.end());

	return result;
}

std::string OvTools::Utils::PathParser::GetExtension(const std::string & p_path)
{
	std::string result;

	for (auto it = p_path.rbegin(); it != p_path.rend() && *it != '.'; ++it)
		result += *it;

	std::reverse(result.begin(), result.end());

	return result;
}

std::string OvTools::Utils::PathParser::FileTypeToString(EFileType p_fileType)
{
	switch (p_fileType)
	{
	case OvTools::Utils::PathParser::EFileType::MODEL:		return "Model";
	case OvTools::Utils::PathParser::EFileType::TEXTURE:	return "Texture";
	case OvTools::Utils::PathParser::EFileType::SHADER:		return "Shader";
	case OvTools::Utils::PathParser::EFileType::SHADER_PART:return "Shader_Part";
	case OvTools::Utils::PathParser::EFileType::MATERIAL:	return "Material";
	case OvTools::Utils::PathParser::EFileType::SOUND:		return "Sound";
	case OvTools::Utils::PathParser::EFileType::SCENE:		return "Scene";
	case OvTools::Utils::PathParser::EFileType::PREFAB:	return "Prefab";
	case OvTools::Utils::PathParser::EFileType::SCRIPT:		return "Script";
	case OvTools::Utils::PathParser::EFileType::FONT:		return "Font";
	case OvTools::Utils::PathParser::EFileType::ANIMATION_GRAPH:	return "Animation_Graph";
	default:												return "Unknown";
	}
}

OvTools::Utils::PathParser::EFileType OvTools::Utils::PathParser::StringToFileType(const std::string& p_type)
{
	if (p_type == "Model")       return EFileType::MODEL;
	if (p_type == "Texture")     return EFileType::TEXTURE;
	if (p_type == "Shader")      return EFileType::SHADER;
	if (p_type == "Shader_Part") return EFileType::SHADER_PART;
	if (p_type == "Material")    return EFileType::MATERIAL;
	if (p_type == "Sound")       return EFileType::SOUND;
	if (p_type == "Scene")       return EFileType::SCENE;
	if (p_type == "Prefab")      return EFileType::PREFAB;
	if (p_type == "Script")      return EFileType::SCRIPT;
	if (p_type == "Font")        return EFileType::FONT;
	if (p_type == "Animation_Graph") return EFileType::ANIMATION_GRAPH;
	return EFileType::UNKNOWN;
}

OvTools::Utils::PathParser::EFileType OvTools::Utils::PathParser::GetFileType(const std::string & p_path)
{
	std::string ext = GetExtension(p_path);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == "fbx" || ext == "obj" || ext == "glb" || ext == "gltf" || ext == "vrm") return EFileType::MODEL;
	else if (ext == "png" || ext == "jpeg" || ext == "jpg" || ext == "tga" || ext == "hdr") return EFileType::TEXTURE;
	else if (ext == "lofx" || ext == "ovfx") return EFileType::SHADER;
	else if (ext == "lofxh" || ext == "ovfxh") return EFileType::SHADER_PART;
	else if (ext == "lomat" || ext == "ovmat") return EFileType::MATERIAL;
	else if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac") return EFileType::SOUND;
	else if (ext == "loscene" || ext == "ovscene") return EFileType::SCENE;
	else if (ext == "loprefab" || ext == "ovprefab") return EFileType::PREFAB;
	else if (ext == "lua" || ext == "ovscript") return EFileType::SCRIPT;
	else if (ext == "ttf" || ext == "otf") return EFileType::FONT;
	else if (ext == "loanim" || ext == "ovanim") return EFileType::ANIMATION_GRAPH;

	return EFileType::UNKNOWN;
}

std::filesystem::path OvTools::Utils::PathParser::GetRealPath(
	const std::filesystem::path& p_path,
	const std::filesystem::path& p_enginePath,
	const std::filesystem::path& p_projectPath
)
{
	const std::string normalizedPath = MakeNonWindowsStyle(p_path.string());

	if (!normalizedPath.empty() && normalizedPath[0] == ':')
	{
		return (p_enginePath / normalizedPath.substr(1)).lexically_normal();
	}

	return (p_projectPath / normalizedPath).lexically_normal();
}

namespace
{
	// Each format's extension in Lost, and in Overload, which Lost still reads
	constexpr std::pair<std::string_view, std::string_view> kFormatNames[] = {
		{ "loproject", "ovproject" },
		{ "loscene", "ovscene" },
		{ "loprefab", "ovprefab" },
		{ "lomat", "ovmat" },
		{ "lofx", "ovfx" },
		{ "lofxh", "ovfxh" },
		{ "loanim", "ovanim" },
	};
}

std::string OvTools::Utils::PathParser::GetFormatAlias(const std::string& p_path)
{
	const std::string ext = GetExtension(p_path);
	if (ext.size() >= p_path.size())
	{
		return {}; // No extension
	}

	std::string lower = ext;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

	for (const auto& [lost, overload] : kFormatNames)
	{
		if (lower == lost || lower == overload)
		{
			return p_path.substr(0, p_path.size() - ext.size()) + std::string(lower == lost ? overload : lost);
		}
	}

	return {};
}

std::filesystem::path OvTools::Utils::PathParser::FindFileToRead(const std::filesystem::path& p_file)
{
	std::error_code error;
	if (std::filesystem::exists(p_file, error))
	{
		return p_file;
	}

	if (const std::string alias = GetFormatAlias(p_file.string()); !alias.empty() && std::filesystem::exists(alias, error))
	{
		return alias;
	}

	return p_file;
}

namespace
{
	std::string LowerExtension(const std::filesystem::path& p_path)
	{
		std::string extension = p_path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](char p_character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(p_character))); });
		return extension;
	}
}

bool OvTools::Utils::PathParser::IsProjectFile(const std::filesystem::path& p_path)
{
	const std::string extension = LowerExtension(p_path);
	return extension == ".loproject" || extension == ".ovproject";
}

std::vector<std::filesystem::path> OvTools::Utils::PathParser::FindProjectFiles(const std::filesystem::path& p_folder)
{
	std::vector<std::filesystem::path> found;
	std::error_code error;

	for (std::filesystem::directory_iterator entry(p_folder, error), end; !error && entry != end; entry.increment(error))
	{
		std::error_code entryError;
		if (entry->is_regular_file(entryError) && IsProjectFile(entry->path()))
		{
			found.push_back(entry->path());
		}
	}

	if (error)
	{
		return {};
	}

	// The Lost name before Overload's, so that a project converted while its old file stays opens as converted
	std::sort(found.begin(), found.end(), [](const auto& p_a, const auto& p_b)
	{
		const bool aLost = LowerExtension(p_a) == ".loproject";
		const bool bLost = LowerExtension(p_b) == ".loproject";
		return aLost != bLost ? aLost : p_a.filename() < p_b.filename();
	});

	return found;
}
