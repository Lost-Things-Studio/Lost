/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string_view>
#include <vector>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CBoneAttachment.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/SceneSystem/Scene.h>

using OvCore::SceneSystem::Scene;

namespace
{
	/**
	* A component's data node, as a scene file holds it
	*/
	struct DataNode
	{
		tinyxml2::XMLDocument doc;
		tinyxml2::XMLElement* node = doc.NewElement("data");

		DataNode()
		{
			doc.InsertFirstChild(node);
		}

		void Add(const char* p_name, const char* p_text)
		{
			tinyxml2::XMLElement* element = doc.NewElement(p_name);
			element->SetText(p_text);
			node->InsertEndChild(element);
		}

		void RemoveStartingWith(std::string_view p_prefix)
		{
			std::vector<tinyxml2::XMLElement*> removed;
			for (auto* element = node->FirstChildElement(); element; element = element->NextSiblingElement())
			{
				if (std::string_view(element->Name()).starts_with(p_prefix))
					removed.push_back(element);
			}

			for (auto* element : removed)
				node->DeleteChild(element);
		}
	};
}

TEST_SUITE("Component files")
{
	TEST_CASE("a transform read from a file without UI data has none")
	{
		// The editor reads an existing actor anew when it applies a prefab to it: UI data it had must not stay
		Scene scene("", "");
		auto& source = scene.CreateActor("Source");
		auto& target = scene.CreateActor("Target");
		target.transform.EnableUIData();
		REQUIRE(target.transform.HasUIData());

		DataNode data;
		source.transform.OnSerialize(data.doc, data.node);
		data.RemoveStartingWith("ui_");

		target.transform.OnDeserialize(data.doc, data.node);
		CHECK_FALSE(target.transform.HasUIData());
	}

	TEST_CASE("a bone attachment's target is saved and read back")
	{
		Scene scene("", "");
		auto& holder = scene.CreateActor("Holder");
		auto& sword = scene.CreateActor("Sword");
		auto& copy = scene.CreateActor("Copy");

		auto& attachment = sword.AddComponent<OvCore::ECS::Components::CBoneAttachment>();
		attachment.SetTarget(&holder);

		DataNode data;
		attachment.OnSerialize(data.doc, data.node);

		const auto* target = data.node->FirstChildElement("target");
		REQUIRE(target);
		CHECK(std::string_view(target->Attribute("type")) == "actor");

		auto& read = copy.AddComponent<OvCore::ECS::Components::CBoneAttachment>();
		read.OnDeserialize(data.doc, data.node);
		CHECK(read.GetTargetGUID() == holder.GetGUID());
	}

	TEST_CASE("a skinned mesh renderer of the former format keeps its animation settings")
	{
		// Before the layers, the settings were children of the component itself: they become the first layer's
		Scene scene("", "");
		auto& actor = scene.CreateActor("Character");
		auto& renderer = actor.AddComponent<OvCore::ECS::Components::CSkinnedMeshRenderer>();

		DataNode data;
		data.Add("animation", "Walk");
		data.Add("playing", "false");
		data.Add("looping", "false");
		data.Add("playback_speed", "2.5");

		renderer.OnDeserialize(data.doc, data.node);

		REQUIRE(renderer.GetLayerCount() >= 1);
		CHECK_FALSE(renderer.IsPlaying(0));
		CHECK_FALSE(renderer.IsLooping(0));
		CHECK(renderer.GetPlaybackSpeed(0) == doctest::Approx(2.5f));
	}
}
