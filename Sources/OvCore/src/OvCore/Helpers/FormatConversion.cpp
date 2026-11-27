/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

#include <OvCore/Helpers/FormatConversion.h>
#include <OvTools/Utils/PathParser.h>

namespace
{
	using OvTools::Utils::PathParser;

	// The extensions of Overload's text formats a reference can name, the longest first so that ".ovfxh" is not read
	// as ".ovfx"
	constexpr std::array<std::string_view, 6> kReferencedExtensions = { "ovscene", "ovprefab", "ovanim", "ovfxh", "ovmat", "ovfx" };

	// What can come before a path in a file: what ends the search back for its start
	constexpr std::string_view kPathStops = "\"'<>=\n\r\t(),;";

	std::string LowerExtension(const std::filesystem::path& p_path)
	{
		std::string extension = p_path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](char p_character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(p_character))); });
		return extension;
	}

	bool HasOverloadName(const std::filesystem::path& p_path)
	{
		return LowerExtension(p_path).starts_with(".ov") && !PathParser::GetFormatAlias(p_path.string()).empty();
	}

	std::string Normalize(std::string p_path)
	{
		std::replace(p_path.begin(), p_path.end(), '\\', '/');
		while (p_path.starts_with("./"))
		{
			p_path.erase(0, 2);
		}
		return p_path;
	}

	std::string Trim(std::string_view p_text)
	{
		// "\r" too, which ends each line of a file written on Windows
		const auto first = p_text.find_first_not_of(" \t\r");
		const auto last = p_text.find_last_not_of(" \t\r");
		return first == std::string_view::npos ? std::string{} : std::string{ p_text.substr(first, last - first + 1) };
	}

	bool IsNameCharacter(char p_character)
	{
		return std::isalnum(static_cast<unsigned char>(p_character)) || p_character == '_';
	}

	std::optional<std::string> ReadFile(const std::filesystem::path& p_path)
	{
		std::ifstream file(p_path, std::ios::binary);
		if (!file)
		{
			return std::nullopt;
		}
		return std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}

	bool WriteFile(const std::filesystem::path& p_path, const std::string& p_content)
	{
		std::ofstream file(p_path, std::ios::binary | std::ios::trunc);
		file << p_content;
		return static_cast<bool>(file);
	}

	/**
	* Whether a path written in a file, ending with one of Overload's names, is to take its Lost name
	*/
	using Convertible = std::function<bool(const std::string&)>;

	/**
	* The reference's extension, if an Overload name ends there
	*/
	std::optional<std::string_view> ExtensionAt(std::string_view p_text, size_t p_dot)
	{
		for (const auto extension : kReferencedExtensions)
		{
			const size_t end = p_dot + 1 + extension.size();
			if (p_text.compare(p_dot + 1, extension.size(), extension) == 0 && (end >= p_text.size() || !IsNameCharacter(p_text[end])))
			{
				return extension;
			}
		}
		return std::nullopt;
	}

	/**
	* Gives each convertible reference of a text its Lost name, changing its extension only
	* @return whether the text changed
	*/
	bool ConvertReferences(std::string& p_text, const Convertible& p_convertible)
	{
		bool changed = false;

		for (size_t dot = p_text.find(".ov"); dot != std::string::npos; dot = p_text.find(".ov", dot + 1))
		{
			const auto extension = ExtensionAt(p_text, dot);
			if (!extension)
			{
				continue;
			}

			const size_t start = p_text.find_last_of(kPathStops, dot);
			const size_t from = start == std::string::npos ? 0 : start + 1;
			const std::string path = Normalize(Trim(std::string_view{ p_text }.substr(from, dot + 1 + extension->size() - from)));

			if (p_convertible(path))
			{
				p_text.replace(dot + 1, 2, "lo");
				changed = true;
			}
		}

		return changed;
	}

	/**
	* Gives the strings of a Lua line that are exactly a convertible path their Lost name
	* @return whether the line changed
	*/
	bool ConvertLuaStrings(std::string& p_line, const Convertible& p_convertible)
	{
		bool changed = false;

		for (size_t index = 0; index < p_line.size(); ++index)
		{
			// The strings of a comment are converted as the code's: a reader may copy them
			const char quote = p_line[index];
			if (quote != '"' && quote != '\'')
			{
				continue;
			}

			// The string's end, past its escaped characters
			size_t end = index + 1;
			while (end < p_line.size() && p_line[end] != quote)
			{
				end += p_line[end] == '\\' ? size_t{ 2 } : size_t{ 1 };
			}

			if (end >= p_line.size())
			{
				break; // Not closed on this line
			}

			const std::string content = p_line.substr(index + 1, end - index - 1);
			const size_t dot = content.rfind('.');
			const auto extension = dot == std::string::npos ? std::nullopt : ExtensionAt(content, dot);

			if (extension && dot + 1 + extension->size() == content.size() && p_convertible(Normalize(content)))
			{
				p_line.replace(index + 1 + dot + 1, 2, "lo");
				changed = true;
			}

			index = end;
		}

		return changed;
	}

	std::string LostName(const std::string& p_path)
	{
		const size_t dot = p_path.rfind(".ov");
		return dot == std::string::npos ? p_path : p_path.substr(0, dot) + ".lo" + p_path.substr(dot + 3);
	}

	std::string Relative(const std::filesystem::path& p_path, const std::filesystem::path& p_base)
	{
		return Normalize(p_path.lexically_relative(p_base).string());
	}

	bool IsTextFormat(const std::filesystem::path& p_path)
	{
		using EFileType = PathParser::EFileType;

		switch (PathParser::GetFileType(p_path.string()))
		{
		case EFileType::SCENE:
		case EFileType::PREFAB:
		case EFileType::MATERIAL:
		case EFileType::SHADER:
		case EFileType::SHADER_PART:
		case EFileType::ANIMATION_GRAPH:
			return true;
		default:
			return PathParser::IsProjectFile(p_path);
		}
	}
}

bool OvCore::Helpers::FormatConversion::Report::HasChanges() const
{
	return !renamed.empty() || !rewritten.empty() || !luaChanges.empty();
}

OvCore::Helpers::FormatConversion::Report OvCore::Helpers::FormatConversion::ConvertProject(const std::filesystem::path& p_projectFolder, const std::filesystem::path& p_engineAssets)
{
	Report report;
	const std::filesystem::path assets = p_projectFolder / "Assets";
	std::error_code error;

	// The files of the project: its project files, and what its Assets folder holds
	std::vector<std::filesystem::path> files = PathParser::FindProjectFiles(p_projectFolder);
	for (std::filesystem::recursive_directory_iterator entry(assets, error), end; !error && entry != end; entry.increment(error))
	{
		std::error_code entryError;
		if (entry->is_regular_file(entryError))
		{
			files.push_back(entry->path());
		}
	}

	if (error)
	{
		report.problems.push_back(std::format("{} cannot be read through: {}", Relative(assets, p_projectFolder), error.message()));
	}

	// What takes its Lost name, and the paths a reference gives them, relative to Assets
	std::vector<std::filesystem::path> toRename;
	std::set<std::string> renamedReferences;

	for (const auto& file : files)
	{
		if (!HasOverloadName(file))
		{
			continue;
		}

		const std::filesystem::path target = PathParser::GetFormatAlias(file.string());
		if (std::filesystem::exists(target, error))
		{
			report.problems.push_back(std::format("{} keeps its name: {} exists already", Relative(file, p_projectFolder), Relative(target, p_projectFolder)));
			continue;
		}

		toRename.push_back(file);
		renamedReferences.insert(Relative(file, assets));
	}

	const Convertible convertible = [&](const std::string& p_path)
	{
		std::error_code existsError;

		if (p_path.starts_with(':'))
		{
			// The engine's resource: once it has its Lost name only
			const std::filesystem::path engineFile = p_engineAssets / p_path.substr(1);
			return std::filesystem::exists(LostName(engineFile.string()), existsError) && !std::filesystem::exists(engineFile, existsError);
		}

		if (renamedReferences.contains(p_path))
		{
			return true;
		}

		// A file renamed before: the reference to its old name follows it
		const std::filesystem::path projectFile = assets / p_path;
		return std::filesystem::exists(LostName(projectFile.string()), existsError) && !std::filesystem::exists(projectFile, existsError);
	};

	// The references, while every file still has its name
	for (const auto& file : files)
	{
		const bool lua = LowerExtension(file) == ".lua";
		if (!lua && !IsTextFormat(file))
		{
			continue;
		}

		auto content = ReadFile(file);
		if (!content)
		{
			report.problems.push_back(std::format("{} cannot be read", Relative(file, p_projectFolder)));
			continue;
		}

		const std::string before = *content;
		const bool renamed = std::find(toRename.begin(), toRename.end(), file) != toRename.end();
		const std::filesystem::path finalName = renamed ? std::filesystem::path(PathParser::GetFormatAlias(file.string())) : file;

		if (lua)
		{
			std::istringstream lines(before);
			std::string line;
			std::string converted;
			size_t number = 0;

			while (std::getline(lines, line))
			{
				++number;
				const std::string original = line;
				if (ConvertLuaStrings(line, convertible))
				{
					report.luaChanges.push_back({ Relative(finalName, p_projectFolder), number, Trim(original), Trim(line) });
				}
				converted += line;
				if (!lines.eof())
				{
					converted += '\n';
				}
			}

			*content = converted;
		}
		else
		{
			ConvertReferences(*content, convertible);
		}

		if (*content != before)
		{
			if (WriteFile(file, *content))
			{
				report.rewritten.push_back(Relative(finalName, p_projectFolder));
			}
			else
			{
				report.problems.push_back(std::format("{} cannot be written", Relative(file, p_projectFolder)));
			}
		}
	}

	// Then the names, each file with its metadata
	for (const auto& file : toRename)
	{
		const std::filesystem::path target = PathParser::GetFormatAlias(file.string());
		std::filesystem::rename(file, target, error);

		if (error)
		{
			report.problems.push_back(std::format("{} cannot be renamed: {}", Relative(file, p_projectFolder), error.message()));
			continue;
		}

		report.renamed.emplace_back(Relative(file, p_projectFolder), Relative(target, p_projectFolder));

		const std::filesystem::path metadata = file.string() + ".meta";
		if (std::filesystem::exists(metadata, error))
		{
			std::filesystem::rename(metadata, target.string() + ".meta", error);
			if (error)
			{
				report.problems.push_back(std::format("{} cannot be renamed: {}", Relative(metadata, p_projectFolder), error.message()));
			}
		}
	}

	// In the same order on every platform, whatever order the folders are listed in
	std::sort(report.renamed.begin(), report.renamed.end());
	std::sort(report.rewritten.begin(), report.rewritten.end());
	std::stable_sort(report.luaChanges.begin(), report.luaChanges.end(), [](const LuaChange& p_a, const LuaChange& p_b) { return p_a.file < p_b.file; });
	std::sort(report.problems.begin(), report.problems.end());
	return report;
}

std::string OvCore::Helpers::FormatConversion::Describe(const Report& p_report)
{
	std::string text = std::format(
		"{} files renamed, {} rewritten, {} lines of Lua changed{}\n",
		p_report.renamed.size(),
		p_report.rewritten.size(),
		p_report.luaChanges.size(),
		p_report.problems.empty() ? "" : std::format(", {} left as they were", p_report.problems.size())
	);

	if (!p_report.renamed.empty())
	{
		text += "\nRenamed:\n";
		for (const auto& [from, to] : p_report.renamed)
		{
			text += std::format("  {} -> {}\n", Normalize(from.string()), Normalize(to.string()));
		}
	}

	if (!p_report.rewritten.empty())
	{
		text += "\nReferences changed in:\n";
		for (const auto& file : p_report.rewritten)
		{
			text += std::format("  {}\n", Normalize(file.string()));
		}
	}

	if (!p_report.luaChanges.empty())
	{
		text += "\nLua lines changed (a path a script builds itself is not seen: check them):\n";
		for (const auto& change : p_report.luaChanges)
		{
			text += std::format("  {}:{}\n    - {}\n    + {}\n", Normalize(change.file.string()), change.line, change.before, change.after);
		}
	}

	if (!p_report.problems.empty())
	{
		text += "\nLeft as they were:\n";
		for (const auto& problem : p_report.problems)
		{
			text += std::format("  {}\n", problem);
		}
	}

	return text;
}
