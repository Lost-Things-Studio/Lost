/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CBoneAttachment.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/SceneSystem/SceneHistory.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvTools/Utils/StateHistory.h>

using OvCore::ECS::Actor;
using OvCore::ECS::Components::CBoneAttachment;
using OvCore::SceneSystem::SceneHistory;
using OvCore::SceneSystem::SceneManager;
using OvTools::Utils::StateHistory;

namespace
{
	Actor& Find(SceneManager& p_manager, uint64_t p_guid)
	{
		auto* actor = p_manager.GetCurrentScene()->FindActorByGUID(p_guid);
		REQUIRE(actor);
		return *actor;
	}

	std::vector<std::string> ChildNames(Actor& p_actor)
	{
		std::vector<std::string> names;
		for (auto* child : p_actor.GetChildren())
		{
			names.push_back(child->GetName());
		}
		return names;
	}
}

TEST_SUITE("Scene history")
{
	TEST_CASE("an actor's edit is undone and redone, the actor keeping its identity and place")
	{
		SceneManager manager;
		auto& scene = *manager.GetCurrentScene();
		auto& parent = scene.CreateActor("Parent");
		auto& actor = scene.CreateActor("Actor");
		actor.SetParent(parent);
		const uint64_t guid = actor.GetGUID();
		const int64_t id = actor.GetID();

		SceneHistory history;
		const std::string before = SceneHistory::SerializeActor(actor);
		actor.SetName("Renamed");
		actor.transform.SetLocalPosition({ 1.0f, 2.0f, 3.0f });
		actor.AddComponent<CBoneAttachment>().SetTarget(&parent);
		const std::string after = SceneHistory::SerializeActor(actor);
		history.RecordActor("Edit Actor", guid, before, after);

		REQUIRE(history.CanUndo());
		CHECK(history.GetUndoLabel() == "Edit Actor");
		CHECK(history.IsDirty());

		REQUIRE(history.Undo(manager));
		auto& undone = Find(manager, guid);
		CHECK(&undone == &actor);
		CHECK(undone.GetName() == "Actor");
		CHECK(undone.GetID() == id);
		CHECK(undone.GetParent() == &parent);
		CHECK(undone.GetComponent<CBoneAttachment>() == nullptr);
		CHECK(SceneHistory::SerializeActor(undone) == before);
		CHECK_FALSE(history.IsDirty());

		// Its place in the scene is not its own data: moving it changes none of it
		auto& other = scene.CreateActor("Other");
		actor.SetParent(other);
		CHECK(SceneHistory::SerializeActor(actor) == before);
		CHECK(before.find("<parent>") == std::string::npos);
		actor.SetParent(parent);

		REQUIRE(history.Redo(manager));
		CHECK(actor.GetName() == "Renamed");
		REQUIRE(actor.GetComponent<CBoneAttachment>());
		CHECK(actor.GetComponent<CBoneAttachment>()->GetTargetGUID() == parent.GetGUID());
		CHECK(SceneHistory::SerializeActor(actor) == after);
		CHECK(history.IsDirty());
	}

	TEST_CASE("a deleted actor comes back with its children, their GUIDs and their order")
	{
		SceneManager manager;
		auto& scene = *manager.GetCurrentScene();
		auto& root = scene.CreateActor("Root");
		auto& doomed = scene.CreateActor("Doomed");
		doomed.SetParent(root);
		for (const char* name : { "A", "B", "C" })
		{
			scene.CreateActor(name).SetParent(doomed);
		}
		const uint64_t rootGUID = root.GetGUID();
		const uint64_t doomedGUID = doomed.GetGUID();
		const uint64_t secondChildGUID = doomed.GetChildren()[1]->GetGUID();

		SceneHistory history;
		const std::string before = SceneHistory::SerializeScene(scene);
		doomed.MarkAsDestroy();
		const std::string after = SceneHistory::SerializeScene(scene);
		CHECK(after.find(std::to_string(doomedGUID)) == std::string::npos); // Marked, it is no longer part of the scene
		history.RecordScene("Delete Doomed", before, after, doomedGUID, 0);

		std::vector<std::optional<uint64_t>> shown;
		history.AppliedEvent += [&shown](std::optional<uint64_t> p_guid) { shown.push_back(p_guid); };

		REQUIRE(history.Undo(manager));
		auto& back = Find(manager, doomedGUID);
		CHECK(back.GetParent() == &Find(manager, rootGUID));
		CHECK(ChildNames(back) == std::vector<std::string>{ "A", "B", "C" });
		CHECK(back.GetChildren()[1]->GetGUID() == secondChildGUID);

		REQUIRE(history.Redo(manager));
		CHECK(manager.GetCurrentScene()->FindActorByGUID(doomedGUID) == nullptr);
		CHECK(manager.GetCurrentScene()->FindActorByGUID(rootGUID) != nullptr);

		// The actor deleted is shown again when it comes back, and nothing once it is gone
		CHECK(shown == std::vector<std::optional<uint64_t>>{ doomedGUID, std::nullopt });
	}

	TEST_CASE("a change of parent and a duplicate are undone with the whole scene")
	{
		SceneManager manager;
		auto& scene = *manager.GetCurrentScene();
		auto& left = scene.CreateActor("Left");
		auto& right = scene.CreateActor("Right");
		auto& item = scene.CreateActor("Item");
		item.SetParent(left);
		const uint64_t itemGUID = item.GetGUID();
		const uint64_t rightGUID = right.GetGUID();

		SceneHistory history;
		std::string before = SceneHistory::SerializeScene(scene);
		item.SetParent(right);
		history.RecordScene("Move Item", before, SceneHistory::SerializeScene(scene));

		before = SceneHistory::SerializeScene(*manager.GetCurrentScene());
		manager.GetCurrentScene()->CreateActor("Item (1)");
		history.RecordScene("Duplicate Item", before, SceneHistory::SerializeScene(*manager.GetCurrentScene()));
		CHECK(manager.GetCurrentScene()->GetActors().size() == 4);

		REQUIRE(history.Undo(manager));
		CHECK(manager.GetCurrentScene()->GetActors().size() == 3);
		CHECK(Find(manager, itemGUID).GetParent() == &Find(manager, rightGUID));

		REQUIRE(history.Undo(manager));
		CHECK(Find(manager, itemGUID).GetParent()->GetName() == "Left");
		CHECK_FALSE(history.CanUndo());
		CHECK(history.GetRedoLabel() == "Move Item");
	}

	TEST_CASE("a new edit drops what could be redone, the limits drop the oldest, and the saved mark follows")
	{
		SceneManager manager;
		auto& actor = manager.GetCurrentScene()->CreateActor("Actor");
		const uint64_t guid = actor.GetGUID();
		SceneHistory history;

		const auto rename = [&](const std::string& p_name)
		{
			auto& target = Find(manager, guid);
			const std::string before = SceneHistory::SerializeActor(target);
			target.SetName(p_name);
			history.RecordActor("Rename " + p_name, guid, before, SceneHistory::SerializeActor(target));
		};

		// The same state twice is no edit
		history.RecordActor("Nothing", guid, "<a/>", "<a/>");
		CHECK(history.GetEntryCount() == 0);
		CHECK_FALSE(history.IsDirty());

		rename("One");
		rename("Two");
		history.MarkSaved();
		CHECK_FALSE(history.IsDirty());

		REQUIRE(history.Undo(manager));
		CHECK(history.IsDirty());
		rename("Three"); // The saved state ("Two") can no longer come back
		CHECK_FALSE(history.CanRedo());
		CHECK(history.GetEntryCount() == 2);
		CHECK(history.IsDirty());
		REQUIRE(history.Undo(manager));
		CHECK(history.IsDirty());

		history.Clear();
		CHECK_FALSE(history.IsDirty());
		CHECK(history.GetSize() == 0);

		history.SetLimits(3, 1024 * 1024);
		for (const char* name : { "A", "B", "C", "D", "E" })
		{
			rename(name);
		}
		CHECK(history.GetEntryCount() == 3);
		CHECK(history.GetUndoLabel() == "Rename E");
		while (history.CanUndo())
		{
			history.Undo(manager);
		}
		CHECK(Find(manager, guid).GetName() == "B"); // The edits to A and B are gone
		CHECK(history.IsDirty()); // The saved state was among them
	}

	TEST_CASE("an edit whose actor is gone is passed over, and the edits before it still undo")
	{
		SceneManager manager;
		auto& scene = *manager.GetCurrentScene();
		auto& keeper = scene.CreateActor("Keeper");
		auto& goner = scene.CreateActor("Goner");
		const uint64_t keeperGUID = keeper.GetGUID();
		SceneHistory history;

		std::string before = SceneHistory::SerializeActor(keeper);
		keeper.SetName("Kept");
		history.RecordActor("Rename Keeper", keeperGUID, before, SceneHistory::SerializeActor(keeper));

		before = SceneHistory::SerializeActor(goner);
		goner.SetName("Gone");
		history.RecordActor("Rename Goner", goner.GetGUID(), before, SceneHistory::SerializeActor(goner));
		goner.MarkAsDestroy();

		CHECK_FALSE(history.Undo(manager));
		CHECK(history.GetUndoLabel() == "Rename Keeper");
		REQUIRE(history.Undo(manager));
		CHECK(Find(manager, keeperGUID).GetName() == "Keeper");
	}

	TEST_CASE("the state history keeps its cursor, its saved mark and its limits")
	{
		StateHistory<int> history;
		std::vector<std::string> states;
		int changes = 0;
		history.ChangedEvent += [&changes] { ++changes; };

		CHECK_FALSE(history.Record({ "Same", 1, "a", "a" }));
		CHECK(history.GetEntryCount() == 0);
		CHECK(changes == 0);

		REQUIRE(history.Record({ "One", 1, "a", "b" }));
		REQUIRE(history.Record({ "Two", 2, "b", "c" }));
		CHECK(history.GetSize() == std::string("Oneab").size() + std::string("Twobc").size());
		REQUIRE(history.GetUndoEntry());
		CHECK(history.GetUndoEntry()->target == 2);
		CHECK(history.GetRedoEntry() == nullptr);

		// Saved after "Two", then everything undone: dirty, and clean again once redone
		history.MarkSaved();
		history.StepBack();
		history.StepBack();
		CHECK_FALSE(history.CanUndo());
		CHECK(history.GetRedoLabel() == "One");
		CHECK(history.IsDirty());
		history.StepForward();
		history.StepForward();
		CHECK_FALSE(history.IsDirty());

		// Limits lowered while everything is undone: the edits furthest from being redone go, the saved mark with them
		history.StepBack();
		history.StepBack();
		history.SetLimits(1, 1024);
		CHECK(history.GetEntryCount() == 1);
		CHECK(history.GetRedoLabel() == "One");
		history.StepForward();
		CHECK(history.IsDirty()); // "Two" was saved, and is gone

		// A clear forgets the edits; kept dirty only if asked, and only if it was
		history.Clear(true);
		CHECK(history.GetEntryCount() == 0);
		CHECK(history.IsDirty());
		history.Clear(true);
		CHECK(history.IsDirty());
		history.MarkSaved();
		history.Clear(true);
		CHECK_FALSE(history.IsDirty());

		// The byte limit drops the oldest edits, never the last one
		history.SetLimits(10, 12);
		REQUIRE(history.Record({ "A", 1, "xxxx", "yyyy" })); // 9 bytes
		REQUIRE(history.Record({ "B", 2, "yyyy", "zzzz" })); // 18 in all: "A" goes
		CHECK(history.GetEntryCount() == 1);
		CHECK(history.GetUndoLabel() == "B");
		REQUIRE(history.Record({ "C", 3, "zzzz", std::string(40, 'w') }));
		CHECK(history.GetEntryCount() == 1);
		CHECK(history.GetUndoLabel() == "C");
	}
}
