/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tinyxml2.h>

#include <OvCore/Helpers/AssetFormat.h>
#include <OvCore/Helpers/XmlFile.h>
#include <OvCore/Resources/Loaders/MaterialLoader.h>
#include <OvDebug/Logger.h>

namespace
{
	// A material of a version newer than the engine reads is left alone: read in part, it would lose what the engine
	// does not know once saved
	bool CheckVersion(const tinyxml2::XMLNode* p_root, const std::string& p_path)
	{
		std::string error;
		if (const auto* root = p_root ? p_root->ToElement() : nullptr;
			root && !OvCore::Helpers::AssetFormat::ReadVersion(*root, OvCore::Helpers::AssetFormat::kMaterialVersion, p_path, error))
		{
			OVLOG_ERROR("[MATERIAL] " + error);
			return false;
		}

		return true;
	}
}

OvCore::Resources::Material * OvCore::Resources::Loaders::MaterialLoader::Create(const std::string & p_path)
{
	tinyxml2::XMLDocument doc;
	OvCore::Helpers::LoadXmlFile(doc, p_path);
	if (!doc.Error())
	{
		tinyxml2::XMLNode* root = doc.FirstChild();

		if (!CheckVersion(root, p_path))
		{
			return nullptr;
		}

		Material* material = new Material();

		material->OnDeserialize(doc, root);

		return material;
	}
	else
	{
		return nullptr;
	}
}

void OvCore::Resources::Loaders::MaterialLoader::Reload(Material& p_material, const std::string& p_path)
{
	tinyxml2::XMLDocument doc;
	OvCore::Helpers::LoadXmlFile(doc, p_path);
	if (!doc.Error())
	{
		tinyxml2::XMLNode* root = doc.FirstChild();

		if (!CheckVersion(root, p_path))
		{
			return;
		}

		p_material.OnDeserialize(doc, root);
		OVLOG_INFO("[MATERIAL] \"" + p_path + "\" Reloaded");
	}
}

void OvCore::Resources::Loaders::MaterialLoader::Save(Material& p_material, const std::string& p_path)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* node = doc.NewElement("root");
	OvCore::Helpers::AssetFormat::WriteVersion(*node, OvCore::Helpers::AssetFormat::kMaterialVersion);
	doc.InsertFirstChild(node);

	p_material.OnSerialize(doc, node);

	if (doc.SaveFile(p_path.c_str()) == tinyxml2::XML_SUCCESS)
		OVLOG_INFO("[MATERIAL] \"" + p_path + "\": Saved");
	else
		OVLOG_ERROR("[MATERIAL] \"" + p_path + "\": Failed to save");
}

bool OvCore::Resources::Loaders::MaterialLoader::Destroy(Material *& p_material)
{
	if (p_material)
	{
		delete p_material;
		p_material = nullptr;

		return true;
	}

	return false;
}