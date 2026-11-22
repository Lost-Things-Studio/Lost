/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <unordered_set>

#include <tinyxml2.h>

#include <OvDebug/Logger.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CTransform.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/SceneSystem/SceneHistory.h>
#include <OvCore/SceneSystem/SceneManager.h>

namespace
{
	std::string Print(const tinyxml2::XMLDocument& p_doc)
	{
		tinyxml2::XMLPrinter printer(nullptr, true);
		p_doc.Print(&printer);
		return std::string(printer.CStr(), static_cast<size_t>(printer.CStrSize() > 0 ? printer.CStrSize() - 1 : 0));
	}

	void SetText(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLElement& p_node, const char* p_name, const std::string& p_value)
	{
		auto* field = p_node.FirstChildElement(p_name);
		if (!field)
		{
			field = p_doc.NewElement(p_name);
			p_node.InsertEndChild(field);
		}
		field->SetText(p_value.c_str());
	}

}

OvCore::SceneSystem::SceneHistory::SceneHistory()
{
	m_history.ChangedEvent += [this] { ChangedEvent.Invoke(); };
}

std::string OvCore::SceneSystem::SceneHistory::SerializeActor(ECS::Actor& p_actor)
{
	tinyxml2::XMLDocument doc;
	auto* actorsNode = doc.NewElement("actors");
	doc.InsertFirstChild(actorsNode);
	p_actor.OnSerialize(doc, actorsNode);

	// Its id and its parent are its place in the scene, which an edit of its own data leaves as it is
	if (auto* actorNode = actorsNode->FirstChildElement("actor"))
	{
		for (const char* field : { "id", "parent" })
		{
			if (auto* node = actorNode->FirstChildElement(field))
			{
				actorNode->DeleteChild(node);
			}
		}
	}

	return Print(doc);
}

bool OvCore::SceneSystem::SceneHistory::RestoreActor(ECS::Actor& p_actor, const std::string& p_xml)
{
	tinyxml2::XMLDocument doc;
	if (doc.Parse(p_xml.c_str(), p_xml.size()) != tinyxml2::XML_SUCCESS)
	{
		return false;
	}

	auto* actorsNode = doc.FirstChildElement("actors");
	auto* actorNode = actorsNode ? actorsNode->FirstChildElement("actor") : nullptr;
	if (!actorNode)
	{
		return false;
	}

	// Its identity and its place in the hierarchy stay as they are: only its own data is written back
	SetText(doc, *actorNode, "id", std::to_string(p_actor.GetID()));
	SetText(doc, *actorNode, "guid", std::to_string(p_actor.GetGUID()));
	SetText(doc, *actorNode, "parent", std::to_string(p_actor.GetParentID()));

	ClearActor(p_actor);
	p_actor.OnDeserialize(doc, actorNode);
	return true;
}

std::string OvCore::SceneSystem::SceneHistory::SerializeScene(Scene& p_scene)
{
	tinyxml2::XMLDocument doc;
	auto* root = doc.NewElement("root");
	doc.InsertFirstChild(root);
	p_scene.OnSerialize(doc, root);

	// An actor marked for destruction is still in the scene until the end of the frame, but no longer part of it
	std::unordered_set<std::string> destroyed;
	for (auto* actor : p_scene.GetActors())
	{
		if (actor && !actor->IsAlive())
		{
			destroyed.insert(std::to_string(actor->GetGUID()));
		}
	}

	if (!destroyed.empty())
	{
		if (auto* actors = root->FirstChildElement("scene") ? root->FirstChildElement("scene")->FirstChildElement("actors") : nullptr)
		{
			for (auto* actor = actors->FirstChildElement("actor"); actor;)
			{
				auto* next = actor->NextSiblingElement("actor");
				const auto* guid = actor->FirstChildElement("guid");
				if (guid && guid->GetText() && destroyed.contains(guid->GetText()))
				{
					actors->DeleteChild(actor);
				}
				actor = next;
			}
		}
	}

	return Print(doc);
}

bool OvCore::SceneSystem::SceneHistory::RestoreScene(SceneManager& p_sceneManager, const std::string& p_xml)
{
	tinyxml2::XMLDocument doc;
	if (doc.Parse(p_xml.c_str(), p_xml.size()) != tinyxml2::XML_SUCCESS)
	{
		return false;
	}

	const bool loadedFromDisk = p_sceneManager.IsCurrentSceneLoadedFromDisk();
	const std::string sourcePath = p_sceneManager.GetCurrentSceneSourcePath();

	if (!p_sceneManager.LoadSceneFromMemory(doc))
	{
		return false;
	}

	// The scene is still the one saved at that path
	if (loadedFromDisk)
	{
		p_sceneManager.StoreCurrentSceneSourcePath(sourcePath);
	}

	return true;
}

void OvCore::SceneSystem::SceneHistory::ClearActor(ECS::Actor& p_actor)
{
	const auto components = p_actor.GetComponents();
	for (const auto& component : components)
	{
		if (!dynamic_cast<ECS::Components::CTransform*>(component.get()))
		{
			p_actor.RemoveComponent(*component);
		}
	}

	const auto behaviourNames = p_actor.GetBehavioursOrder();
	for (const auto& behaviourName : behaviourNames)
	{
		p_actor.RemoveBehaviour(behaviourName);
	}
}

bool OvCore::SceneSystem::SceneHistory::RecordActor(std::string p_label, uint64_t p_guid, std::string p_before, std::string p_after)
{
	return m_history.Record({ std::move(p_label), Target{ false, p_guid }, std::move(p_before), std::move(p_after) });
}

bool OvCore::SceneSystem::SceneHistory::RecordScene(std::string p_label, std::string p_before, std::string p_after, uint64_t p_focusBefore, uint64_t p_focusAfter)
{
	return m_history.Record({ std::move(p_label), Target{ true, 0, p_focusBefore, p_focusAfter }, std::move(p_before), std::move(p_after) });
}

bool OvCore::SceneSystem::SceneHistory::Undo(SceneManager& p_sceneManager)
{
	const History::Entry* entry = m_history.GetUndoEntry();
	if (!entry)
	{
		return false;
	}

	// An edit whose actor is gone cannot be put back: it is passed over, so that the edits before it still can be
	const History::Entry applied = *entry;
	const bool done = Apply(p_sceneManager, applied, applied.before);
	if (!done)
	{
		OVLOG_WARNING("Undo: \"" + applied.label + "\" could not be undone, its actor is no longer in the scene");
	}

	m_history.StepBack();
	NotifyApplied(applied, true);
	return done;
}

bool OvCore::SceneSystem::SceneHistory::Redo(SceneManager& p_sceneManager)
{
	const History::Entry* entry = m_history.GetRedoEntry();
	if (!entry)
	{
		return false;
	}

	const History::Entry applied = *entry;
	const bool done = Apply(p_sceneManager, applied, applied.after);
	if (!done)
	{
		OVLOG_WARNING("Redo: \"" + applied.label + "\" could not be redone, its actor is no longer in the scene");
	}

	m_history.StepForward();
	NotifyApplied(applied, false);
	return done;
}

bool OvCore::SceneSystem::SceneHistory::CanUndo() const
{
	return m_history.CanUndo();
}

bool OvCore::SceneSystem::SceneHistory::CanRedo() const
{
	return m_history.CanRedo();
}

const std::string& OvCore::SceneSystem::SceneHistory::GetUndoLabel() const
{
	return m_history.GetUndoLabel();
}

const std::string& OvCore::SceneSystem::SceneHistory::GetRedoLabel() const
{
	return m_history.GetRedoLabel();
}

void OvCore::SceneSystem::SceneHistory::Clear(bool p_keepDirty)
{
	m_history.Clear(p_keepDirty);
}

void OvCore::SceneSystem::SceneHistory::MarkSaved()
{
	m_history.MarkSaved();
}

bool OvCore::SceneSystem::SceneHistory::IsDirty() const
{
	return m_history.IsDirty();
}

size_t OvCore::SceneSystem::SceneHistory::GetEntryCount() const
{
	return m_history.GetEntryCount();
}

size_t OvCore::SceneSystem::SceneHistory::GetSize() const
{
	return m_history.GetSize();
}

void OvCore::SceneSystem::SceneHistory::SetLimits(size_t p_maxEntries, size_t p_maxBytes)
{
	m_history.SetLimits(p_maxEntries, p_maxBytes);
}

bool OvCore::SceneSystem::SceneHistory::Apply(SceneManager& p_sceneManager, const History::Entry& p_entry, const std::string& p_state)
{
	if (p_entry.target.wholeScene)
	{
		return RestoreScene(p_sceneManager, p_state);
	}

	auto* scene = p_sceneManager.GetCurrentScene();
	auto* actor = scene ? scene->FindActorByGUID(p_entry.target.guid) : nullptr;
	return actor && actor->IsAlive() && RestoreActor(*actor, p_state);
}

void OvCore::SceneSystem::SceneHistory::NotifyApplied(const History::Entry& p_entry, bool p_before)
{
	const uint64_t focus = !p_entry.target.wholeScene ? p_entry.target.guid :
		p_before ? p_entry.target.focusBefore : p_entry.target.focusAfter;

	AppliedEvent.Invoke(focus != 0 ? std::optional<uint64_t>(focus) : std::nullopt);
}
