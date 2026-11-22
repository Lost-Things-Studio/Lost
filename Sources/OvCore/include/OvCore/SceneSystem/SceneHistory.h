/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <OvTools/Eventing/Event.h>
#include <OvTools/Utils/StateHistory.h>

namespace OvCore::ECS { class Actor; }

namespace OvCore::SceneSystem
{
	class Scene;
	class SceneManager;

	/**
	* What edits changed in a scene, so that they can be undone and redone. An entry keeps the state before and after
	* an edit: the XML of one actor's own data, found again by its GUID, or the XML of the whole scene for an edit that
	* changes its structure (actors made, removed, moved in the hierarchy)
	*/
	class SceneHistory
	{
	public:
		/**
		* What an entry's states are of
		*/
		struct Target
		{
			bool wholeScene = false;
			uint64_t guid = 0; // The actor, for an actor's entry
			uint64_t focusBefore = 0; // For a scene's entry: the actor to show once the state before is back (0 for none)
			uint64_t focusAfter = 0; // ... and once the state after is back
		};

		SceneHistory();
		SceneHistory(const SceneHistory&) = delete;
		SceneHistory& operator=(const SceneHistory&) = delete;

		/**
		* The XML of an actor's own data (name, tag, components, behaviours), without its children, its id or its parent
		* @param p_actor
		*/
		static std::string SerializeActor(ECS::Actor& p_actor);

		/**
		* Writes an actor's own data back from SerializeActor's XML. The actor keeps its id, GUID and place in the
		* hierarchy; its components and behaviours are made again from the XML
		* @param p_actor
		* @param p_xml
		* @return false if the XML holds no actor
		*/
		static bool RestoreActor(ECS::Actor& p_actor, const std::string& p_xml);

		/**
		* The XML of a whole scene, as a scene file holds it. Actors marked for destruction are left out, as they will be
		* gone at the end of the frame
		* @param p_scene
		*/
		static std::string SerializeScene(Scene& p_scene);

		/**
		* Loads a scene back from SerializeScene's XML, keeping the path it is saved to
		* @param p_sceneManager
		* @param p_xml
		* @return false if the XML holds no scene
		*/
		static bool RestoreScene(SceneManager& p_sceneManager, const std::string& p_xml);

		/**
		* Removes an actor's components (its transform aside) and behaviours
		* @param p_actor
		*/
		static void ClearActor(ECS::Actor& p_actor);

		/**
		* Adds an edit of one actor. Nothing is added if the states are the same. What could have been redone is dropped
		* @param p_label (shown as "Undo <label>")
		* @param p_guid
		* @param p_before (SerializeActor)
		* @param p_after (SerializeActor)
		* @return true if the edit was added
		*/
		bool RecordActor(std::string p_label, uint64_t p_guid, std::string p_before, std::string p_after);

		/**
		* Adds an edit of the scene's structure. Nothing is added if the states are the same
		* @param p_label
		* @param p_before (SerializeScene)
		* @param p_after (SerializeScene)
		* @param p_focusBefore (the GUID of the actor to show when the state before comes back, 0 for none)
		* @param p_focusAfter (the GUID of the actor to show when the state after comes back, 0 for none)
		* @return true if the edit was added
		*/
		bool RecordScene(std::string p_label, std::string p_before, std::string p_after, uint64_t p_focusBefore = 0, uint64_t p_focusAfter = 0);

		/**
		* Puts the state before the last edit back. An edit whose actor is no longer in the scene is passed over, so
		* that the edits before it still can be undone
		* @param p_sceneManager
		* @return false if there is nothing to undo, or its actor is no longer in the scene
		*/
		bool Undo(SceneManager& p_sceneManager);

		/**
		* Puts the state after the last undone edit back
		* @param p_sceneManager
		* @return false if there is nothing to redo, or its actor is no longer in the scene
		*/
		bool Redo(SceneManager& p_sceneManager);

		bool CanUndo() const;
		bool CanRedo() const;

		/**
		* The label of the edit Undo would undo, or an empty string
		*/
		const std::string& GetUndoLabel() const;

		/**
		* The label of the edit Redo would redo, or an empty string
		*/
		const std::string& GetRedoLabel() const;

		/**
		* Forgets every edit. The scene counts as saved unless p_keepDirty and it was not
		* @param p_keepDirty
		*/
		void Clear(bool p_keepDirty = false);

		/**
		* The scene as it is now is the one saved
		*/
		void MarkSaved();

		/**
		* True if edits were made, or undone, since the scene was saved
		*/
		bool IsDirty() const;

		size_t GetEntryCount() const;

		/**
		* The memory the entries take, in bytes (their XML)
		*/
		size_t GetSize() const;

		/**
		* Bounds what is kept: the oldest edits go once either limit is passed. 200 edits and 128 MB by default
		* @param p_maxEntries
		* @param p_maxBytes
		*/
		void SetLimits(size_t p_maxEntries, size_t p_maxBytes);

		/**
		* After an undo or a redo: the GUID of the actor to show (the actor written back, or the one the state put back
		* had in view), or nothing
		*/
		OvTools::Eventing::Event<std::optional<uint64_t>> AppliedEvent;

		/**
		* After any change of the entries, the cursor or the saved mark
		*/
		OvTools::Eventing::Event<> ChangedEvent;

	private:
		using History = OvTools::Utils::StateHistory<Target>;

		bool Apply(SceneManager& p_sceneManager, const History::Entry& p_entry, const std::string& p_state);
		void NotifyApplied(const History::Entry& p_entry, bool p_before);

		History m_history;
	};
}
