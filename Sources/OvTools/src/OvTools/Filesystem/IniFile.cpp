/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/VirtualFileSystem.h>
#include <OvTools/Utils/String.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
	std::pair<std::string, std::string> ExtractKeyAndValue(std::string_view p_line)
	{
		std::string key;
		std::string value;

		std::string* currentBuffer = &key;

		for (const char c : p_line)
		{
			if (c == '=')
				currentBuffer = &value;
			else
				currentBuffer->push_back(c);
		}

		return { key, value };
	}

	bool IsValidLine(std::string_view p_attributeLine)
	{
		if (p_attributeLine.size() == 0)
			return false;

		if (p_attributeLine[0] == '#' || p_attributeLine[0] == ';' || p_attributeLine[0] == '[')
			return false;

		if (std::count(p_attributeLine.begin(), p_attributeLine.end(), '=') != 1)
			return false;

		return true;
	}
}

OvTools::Filesystem::IniFile::IniFile(const std::string& p_filePath) : m_filePath(p_filePath)
{
	Load();
}

void OvTools::Filesystem::IniFile::Reload()
{
	RemoveAll();
	Load();
}

bool OvTools::Filesystem::IniFile::Remove(const std::string & p_key)
{
	if (IsKeyExisting(p_key))
	{
		m_data.erase(p_key);
		return true;
	}

	return false;
}

void OvTools::Filesystem::IniFile::RemoveAll()
{
	m_data.clear();
}

bool OvTools::Filesystem::IniFile::IsKeyExisting(const std::string& p_key) const
{
	return m_data.find(p_key) != m_data.end();
}

std::vector<std::string> OvTools::Filesystem::IniFile::GetKeys() const
{
	std::vector<std::string> keys;
	keys.reserve(m_data.size());

	for (const auto& [key, value] : m_data)
	{
		keys.push_back(key);
	}

	return keys;
}

void OvTools::Filesystem::IniFile::RegisterPair(const std::string& p_key, const std::string& p_value)
{
	RegisterPair(std::make_pair(p_key, p_value));
}

void OvTools::Filesystem::IniFile::RegisterPair(const AttributePair& p_pair)
{
	m_data.insert(p_pair);
}

void OvTools::Filesystem::IniFile::Load()
{
	// Through the virtual file system: a project's settings or an asset's metadata may be in a package
	std::string text;
	std::string error;
	if (!OvTools::Filesystem::VirtualFileSystem::ReadText(m_filePath, text, error))
	{
		return;
	}

	std::istringstream lines(text);
	std::string currentLine;

	while (std::getline(lines, currentLine))
	{
		// Read as bytes: a line written on Windows keeps its "\r"
		if (!currentLine.empty() && currentLine.back() == '\r')
		{
			currentLine.pop_back();
		}

		if (IsValidLine(currentLine))
		{
			OvTools::Utils::String::Trim(currentLine);
			RegisterPair(ExtractKeyAndValue(currentLine));
		}
	}
}

void OvTools::Filesystem::IniFile::Rewrite() const
{
	std::ofstream outfile;
	outfile.open(m_filePath, std::ios_base::trunc);

	if (outfile.is_open())
	{
		// By name, so that a file written twice is the same file: the keys are kept in no particular order
		std::vector<const decltype(m_data)::value_type*> entries;
		entries.reserve(m_data.size());
		for (const auto& entry : m_data)
		{
			entries.push_back(&entry);
		}
		std::sort(entries.begin(), entries.end(), [](const auto* p_a, const auto* p_b) { return p_a->first < p_b->first; });

		for (const auto* entry : entries)
		{
			outfile << entry->first << "=" << entry->second << std::endl;
		}
	}

	outfile.close();
}
