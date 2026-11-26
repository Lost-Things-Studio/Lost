/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <format>
#include <stdexcept>

#include <tinyxml2.h>

#include <OvCore/Helpers/AssetFormat.h>
#include <OvDebug/Logger.h>
#include <OvTools/Filesystem/IniFile.h>

void OvCore::Helpers::AssetFormat::WriteVersion(tinyxml2::XMLElement& p_root, int p_version)
{
	p_root.SetAttribute("version", p_version);
}

std::optional<int> OvCore::Helpers::AssetFormat::ReadVersion(const tinyxml2::XMLElement& p_root, int p_latest, const std::string& p_file, std::string& p_error)
{
	const char* text = p_root.Attribute("version");
	if (!text)
	{
		return 0;
	}

	int version = 0;
	if (p_root.QueryIntAttribute("version", &version) != tinyxml2::XML_SUCCESS)
	{
		p_error = std::format("{}: its version \"{}\" is not a whole number", p_file, text);
		return std::nullopt;
	}

	return CheckVersion(version, p_latest, p_file, p_error) ? std::optional<int>(version) : std::nullopt;
}

std::optional<int> OvCore::Helpers::AssetFormat::ReadVersion(const OvTools::Filesystem::IniFile& p_file, int p_latest, const std::string& p_name, std::string& p_error)
{
	if (!p_file.IsKeyExisting(kIniVersionKey))
	{
		return 0;
	}

	const std::string text = p_file.Get<std::string>(kIniVersionKey);
	int version = 0;

	try
	{
		size_t read = 0;
		version = std::stoi(text, &read);
		if (read != text.size())
		{
			throw std::invalid_argument(text);
		}
	}
	catch (const std::exception&)
	{
		p_error = std::format("{}: its version \"{}\" is not a whole number", p_name, text);
		return std::nullopt;
	}

	return CheckVersion(version, p_latest, p_name, p_error) ? std::optional<int>(version) : std::nullopt;
}

bool OvCore::Helpers::AssetFormat::CheckMetadata(OvTools::Filesystem::IniFile& p_file, const std::string& p_name)
{
	std::string error;
	if (ReadVersion(p_file, kMetadataVersion, p_name, error))
	{
		return true;
	}

	OVLOG_WARNING(error + ": the asset is read with the default settings");
	p_file.RemoveAll();
	return false;
}

void OvCore::Helpers::AssetFormat::WriteMetadataVersion(OvTools::Filesystem::IniFile& p_file)
{
	if (!p_file.Set<int>(kIniVersionKey, kMetadataVersion))
	{
		p_file.Add<int>(kIniVersionKey, kMetadataVersion);
	}
}

bool OvCore::Helpers::AssetFormat::CheckVersion(int p_version, int p_latest, const std::string& p_file, std::string& p_error)
{
	if (p_version < 0)
	{
		p_error = std::format("{}: its version {} is not a version", p_file, p_version);
		return false;
	}

	if (p_version > p_latest)
	{
		p_error = std::format("{} is version {} of its format, newer than this engine reads (version {} at most)", p_file, p_version, p_latest);
		return false;
	}

	return true;
}
