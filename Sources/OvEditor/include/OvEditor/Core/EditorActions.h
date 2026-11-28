/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/SceneSystem/SceneHistory.h>
#include <OvEditor/Core/Context.h>
#include <OvEditor/Core/GameBuilder.h>
#include <OvEditor/Core/PanelsManager.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Utils/PathParser.h>

#define EDITOR_EXEC(action) OvCore::Global::ServiceLocator::Get<OvEditor::Core::EditorActions>().action
#define EDITOR_BIND(method, ...) std::bind(&OvEditor::Core::EditorActions::method, &OvCore::Global::ServiceLocator::Get<OvEditor::Core::EditorActions>(), ##__VA_ARGS__)
#define EDITOR_EVENT(target) OvCore::Global::ServiceLocator::Get<OvEditor::Core::EditorActions>().target
#define EDITOR_CONTEXT(instance) OvCore::Global::ServiceLocator::Get<OvEditor::Core::EditorActions>().GetContext().instance
#define EDITOR_PANEL(type, id) OvCore::Global::ServiceLocator::Get<OvEditor::Core::EditorActions>().GetPanelsManager().GetPanelAs<type>(id)

namespace tinyxml2
{
	class XMLDocument;
}

namespace OvEditor::Core
{
	enum class EGizmoOperation;

	/**
	* A set of editor actions
	*/
	class EditorActions
	{
	public:
		/**
		* Constructor
		* @param p_context
		* @param p_panelsManager
		*/
		EditorActions(Context& p_context, PanelsManager& p_panelsManager);

		/**
		* Destructor
		*/
		virtual ~EditorActions();

		#pragma region TOOLS
		/**
		* Returns the context
		*/
		Context& GetContext();

		/**
		* Returns the panels manager
		*/
		PanelsManager& GetPanelsManager();
		#pragma endregion

		#pragma region SETTINGS
		/**
		* A simple enumeration that define two actor spawn modes
		*/
		enum class EActorSpawnMode { ORIGIN, FRONT };

		/**
		* Defines if new actors should be spawned at origin
		* @param p_value
		*/
		void SetActorSpawnAtOrigin(bool p_value);

		/**
		* Defines how new actors should be spawned
		* @param p_value
		*/
		void SetActorSpawnMode(EActorSpawnMode p_value);

		/**
		* Reset the editor layout
		*/
		void ResetLayout();

		/**
		* Defines the scene view camera speed
		* @param p_speed
		*/
		void SetSceneViewCameraSpeed(int p_speed);

		/**
		* Returns the scene view camera speed
		*/
		int GetSceneViewCameraSpeed();

		/**
		* Defines the asset view camera speed
		* @param p_speed
		*/
		void SetAssetViewCameraSpeed(int p_speed);

		/**
		* Returns the asset view camera speed
		*/
		int GetAssetViewCameraSpeed();

		/**
		* Resets the scene view camera position to the default one
		*/
		void ResetSceneViewCameraPosition();

		/**
		* Resets the scene view camera position to the default one
		*/
		void ResetAssetViewCameraPosition();
		#pragma endregion

		#pragma region GAME
		/**
		* Defines some states for the editor
		*/
		enum class EEditorMode { EDIT, PLAY, PAUSE, FRAME_BY_FRAME };

		/**
		* Returns the current editor state/mode
		*/
		EEditorMode GetCurrentEditorMode() const;

		/**
		* Defines the editor state/mode
		* @param p_newEditorMode
		*/
		void SetEditorMode(EEditorMode p_newEditorMode);

		/**
		* Start playing the current scene and update the editor mode
		*/
		void StartPlaying();

		/**
		* Pause the current playing scene and update the editor mode
		*/
		void PauseGame();

		/**
		* Stop platying the current scene and update the editor mode
		*/
		void StopPlaying();

		/**
		* Play the current frame and pause the editor
		*/
		void NextFrame();
		#pragma endregion

		#pragma region SCENE_VIEW
		/**
		* Sets the gizmo operation to use
		* @param p_operation
		*/
		void SetGizmoOperation(EGizmoOperation p_operation);

		/**
		* Returns the current gizmo operation
		*/
		EGizmoOperation GetGizmoOperation() const;

		/**
		* Sets whether the scene view renders in-game UI in screen space
		* @param p_enabled
		*/
		void SetSceneUIRenderingEnabled(bool p_enabled);

		/**
		* Toggles screen-space rendering mode for in-game UI in scene view
		*/
		void ToggleSceneUIRendering();

		/**
		* Returns whether scene view renders in-game UI in screen space
		*/
		bool IsSceneUIRenderingEnabled() const;
		#pragma endregion

		#pragma region ACTOR_CREATION_DESTRUCTION
		/**
		* Create an actor with the given component type
		* @param p_focusOnCreation
		* @param p_parent
		*/
		template<typename T> OvCore::ECS::Actor& CreateMonoComponentActor(bool p_focusOnCreation = true, OvCore::ECS::Actor* p_parent = nullptr);

		/**
		* Calculate the position where to spawn the actor using the current camera position and forward
		* @param p_distanceToCamera
		*/
		OvMaths::FVector3 CalculateActorSpawnPoint(float p_distanceToCamera);

		/**
		* Create an empty actor
		* @param p_focusOnCreation
		* @param p_parent
		* @param p_name
		*/
		OvCore::ECS::Actor&	CreateEmptyActor(bool p_focusOnCreation = true, OvCore::ECS::Actor* p_parent = nullptr, const std::string& p_name = "");

		/**
		* Create an actor with a model renderer and a material renderer. The model renderer with use the model identified
		* by the given path
		* @param p_path
		* @param p_focusOnCreation
		* @param p_parent
		* @param p_name
		*/
		OvCore::ECS::Actor&	CreateActorWithModel(const std::string& p_path, bool p_focusOnCreation = true, OvCore::ECS::Actor* p_parent = nullptr, const std::string& p_name = "");

		/**
		* Destroy an actor from his scene
		* @param p_focusOnCreation
		* @param p_parent
		*/
		bool DestroyActor(OvCore::ECS::Actor& p_actor);

		/**
		* Duplicate an actor
		* @param p_toDuplicate
		* @param p_forcedParent
		* @param p_focus
		* @param p_keepSourceParentIfNoForcedParent
		*/
		void DuplicateActor
		(
			OvCore::ECS::Actor& p_toDuplicate,
			OvCore::ECS::Actor* p_forcedParent = nullptr,
			bool p_focus = true,
			bool p_keepSourceParentIfNoForcedParent = true
		);

		/**
		* Save an actor hierarchy to a prefab file
		* @param p_actor
		* @param p_path
		*/
		void SaveActorAsPrefab(OvCore::ECS::Actor& p_actor, const std::string& p_path);

		/**
		* Instantiate a prefab file in the current scene
		* @param p_path
		*/
		OvCore::ECS::Actor* InstantiatePrefab(const std::string& p_path);

		/**
		* Apply the current actor hierarchy state to its prefab source.
		* Returns true on success.
		* @param p_actor
		*/
		bool ApplyActorToPrefab(OvCore::ECS::Actor& p_actor);

		/**
		* Revert an actor hierarchy from its prefab source.
		* Keeps existing actor GUIDs to preserve scene/script references.
		* Returns true on success.
		* @param p_actor
		*/
		bool RevertActorToPrefab(OvCore::ECS::Actor& p_actor);
		#pragma endregion

		#pragma region ACTOR_MANIPULATION
		/**
		* Copy the given actor in the editor copy buffer
		* @param p_actor
		*/
		void CopyActor(OvCore::ECS::Actor& p_actor);

		/**
		* Paste the copied actor next to the given actor (same parent), or at root if null.
		* If the target actor is at root, the pasted actor is also pasted at root.
		* @param p_parent
		*/
		void PasteActor(OvCore::ECS::Actor* p_parent = nullptr);

		/**
		* Select an actor and show him in inspector
		* @param p_target
		*/
		void SelectActor(OvCore::ECS::Actor& p_target);

		/**
		* Unselect any selected actor and clearing the inspector
		*/
		void UnselectActor();

		/**
		* Returns true if any actor is selected
		*/
		bool IsAnyActorSelected() const;

		/**
		* Returns the selected actor. Make sur you verified that an actor is selected
		* with IsAnyActorSelected() before calling this method
		*/
		OvCore::ECS::Actor& GetSelectedActor() const;

		/**
		* Moves the camera to the target actor
		*/
		void MoveToTarget(OvCore::ECS::Actor& p_target);
		#pragma endregion

		#pragma region RESOURCE_MANAGEMENT
		/**
		* Compile every loaded shaders
		*/
		void CompileShaders();

		/**
		* Compile the given shader
		*/
		void CompileShader(OvRendering::Resources::Shader& p_shader);

		/**
		* Compile the shader at the given resource path
		*/
		void CompileShader(const std::filesystem::path& p_shaderPath);

		/**
		* Save every materials to their respective files
		*/
		void SaveMaterials();

		/**
		* Regenerate project files for the scripting backend
		*/
		void RegenerateScriptingProjectFiles();
		
		/**
		* Open the given path in the code editor.
		* Returns true if the operation was successful
		* @param p_path
		* @param p_workdir (optional) working directory
		*/
		bool OpenInCodeEditor(
			const std::filesystem::path& p_path,
			OvTools::Utils::OptRef<const std::filesystem::path> p_workdir = std::nullopt
		);

		/**
		* Import an asset
		* @param p_initialDestinationDirectory
		*/
		bool ImportAsset(const std::string& p_initialDestinationDirectory);

		/**
		* Open a save dialog to create a new script file at a user-chosen location.
		* Returns the absolute path of the created script, or empty on cancellation.
		* @param p_initialDirectory  Directory the dialog opens in
		* @param p_initialName       Suggested filename (without extension)
		*/
		std::string CreateScript(const std::string& p_initialDirectory, const std::string& p_initialName);

		/**
		* Import an asset at location
		* @param p_destination
		*/
		bool ImportAssetAtLocation(const std::string& p_destination);

		/**
		* Returns the real path of a resource (complete absolute path)
		* @param p_path
		*/
		std::string GetRealPath(const std::string& p_path);

		/**
		* Returns the resource path of a file
		* @param p_path
		* @param p_isFromEngine
		*/
		std::string GetResourcePath(const std::string& p_path, bool p_isFromEngine = false);

		/**
		* Returns the script path of a file (relative to projectAssetsPath, forward-slash separated)
		* @param p_path
		*/
		std::string GetScriptPath(const std::string& p_path);

		/**
		* Migrates scripts from a legacy Scripts/ folder into Assets/Scripts/.
		* If a Scripts/ folder is found in the project root, prompts the user and
		* moves it into Assets/, updating all scene files accordingly.
		*/
		void MigrateScripts();

		/**
		* Propagate the folder rename everywhere (Resource manager, scenes, materials...)
		* @param p_previousName
		* @param p_newName
		*/
		void PropagateFolderRename(std::string p_previousName, const std::string p_newName);

		/**
		* Propagate the folder destruction everywhere (Resource manager, scenes, materials...)
		* @param p_folderPath
		*/
		void PropagateFolderDestruction(std::string p_folderPath);

		/**
		* Propagate the file rename everywhere it is used
		* @param p_previousName
		* @param p_newName
		*/
		void PropagateFileRename(std::string p_previousName, std::string p_newName);

		/**
		* Propagate the file rename through concerned files
		* @param p_previousName
		* @param p_newName
		* @param p_fileType
		*/
		void PropagateFileRenameThroughSavedFilesOfType(const std::string& p_previousName, const std::string& p_newName, OvTools::Utils::PathParser::EFileType p_fileType);
		#pragma endregion

		#pragma region SCENE
		/**
		* Load an empty scene. Any unsaved changes to the current scene will be discarded
		*/
		void LoadEmptyScene();

		/**
		* Save the current scene to the given path
		* @param p_sceneToSave
		* @param p_path
		*/
		void SaveSceneToDisk(OvCore::SceneSystem::Scene& p_scene, const std::string& p_path);

		/**
		* Load a scene from the disk
		* @param p_path
		* @param p_absolute
		*/
		void LoadSceneFromDisk(const std::string& p_path, bool p_absolute = false);

		/**
		* Returns true if the current scene has been loaded from disk
		*/
		bool IsCurrentSceneLoadedFromDisk() const;

		/**
		* Save the current scene to its disk location
		*/
		void SaveSceneChanges();

		/**
		* Save the current scene to a new disk location (Can create a duplication of the scene file)
		*/
		void SaveAs();

		/**
		* Moves the project to the Lost formats (OvCore::Helpers::FormatConversion), once the scene's changes are saved or
		* given up and the user agrees. The report goes to LostFormatsConversion.txt in the project's folder, and the
		* editor closes: the files it has open have new names
		*/
		void ConvertProjectToLostFormats();
		#pragma endregion

		#pragma region HISTORY
		/**
		* The edits made to the scene open, to undo and redo them
		*/
		OvCore::SceneSystem::SceneHistory& GetSceneHistory();

		/**
		* Undoes the last edit of the scene, once the frame is over. Only in edit mode
		*/
		void Undo();

		/**
		* Redoes the last edit undone, once the frame is over. Only in edit mode
		*/
		void Redo();

		/**
		* True in edit mode, if there is an edit to undo
		*/
		bool CanUndo() const;

		/**
		* True in edit mode, if there is an edit to redo
		*/
		bool CanRedo() const;

		/**
		* Starts an edit of the scene's structure (actors made, removed, moved in the hierarchy). It is recorded once the
		* frame is over, with everything the frame did to the scene: an edit started again in the same frame is part of
		* the first. Only in edit mode
		* @param p_label (shown as "Undo <label>")
		* @param p_nameSelection (if true, the label ends with the name of the actor selected once the frame is over)
		*/
		void BeginSceneEdit(const std::string& p_label, bool p_nameSelection = false);

		/**
		* Records the edits of the frame: the edit of the scene started in it, then what changed in the selected actor
		* (in the inspector, with the gizmo), once no field or handle is held. The editor calls it at the end of each frame
		*/
		void RecordEdits();

		/**
		* True while a field, a handle or a drag is held, with the mouse or the keyboard: the edit it makes is not over yet
		*/
		static bool IsEditHeld();

		/**
		* True if the scene has changes that are not saved
		*/
		bool IsSceneDirty() const;

		/**
		* Runs an action that leaves the scene open (a new scene, another one, closing the editor), once the user says
		* what to do with its unsaved changes: save them, drop them, or stay. Runs it at once if there are none
		* @param p_what (what the action does, as "open another scene")
		* @param p_action
		*/
		void ConfirmLeaveScene(const std::string& p_what, std::function<void()> p_action);

		/**
		* Holds a close of the editor's window back until the unsaved changes of the scene are saved or dropped. The editor
		* calls it at the end of each frame
		*/
		void HandleCloseRequest();
		#pragma endregion

		#pragma region SCRIPTING
		/**
		* Refresh every scripts (Re-interpret)
		*/
		void RefreshScripts();
		#pragma endregion

		#pragma region BUILDING
		/**
		* Ask the user to select the build folder
		*/
		std::optional<std::string> SelectBuildFolder();

		/**
		* Build the current project
		* @param p_autoRun
		* @param p_tempFolder
		*/
		void Build(bool p_autoRun = false, bool p_tempFolder = false);

		/**
		* Build the current project at the given location
		* @param p_buildType
		* @param p_buildPath
		* @param p_autoRun
		*/
		void BuildAtLocation(
			EBuildType p_buildType,
			const std::filesystem::path&,
			bool p_autoRun = false
		);
		#pragma endregion

		#pragma region PROFILNIG
		/**
		* Open the profiler
		*/
		void OpenProfiler();
		#pragma endregion

		#pragma region ACTION_SYSTEM
		/**
		* Prepare an action for a future call
		* @param p_action
		* @param p_frames
		*/
		void DelayAction(std::function<void()> p_action, uint32_t p_frames = 1);

		/**
		* Execute every actions that should be executed at this frame (Decrement the frame counter for each actions)
		*/
		void ExecuteDelayedActions();
		#pragma endregion

	public:
		OvTools::Eventing::Event<OvCore::ECS::Actor&> ActorSelectedEvent;
		OvTools::Eventing::Event<OvCore::ECS::Actor&> ActorUnselectedEvent;
		OvTools::Eventing::Event<EEditorMode> EditorModeChangedEvent;
		OvTools::Eventing::Event<EGizmoOperation> EditorOperationChanged;
		OvTools::Eventing::Event<bool> SceneUIRenderingChangedEvent;
		OvTools::Eventing::Event<> PlayEvent;

	private:
		Context& m_context;
		PanelsManager& m_panelsManager;

		EActorSpawnMode m_actorSpawnMode = EActorSpawnMode::ORIGIN;
		EEditorMode m_editorMode = EEditorMode::EDIT;
		bool m_sceneUIRenderingEnabled = false;

		std::vector<std::pair<uint32_t, std::function<void()>>> m_delayedActions;
		OvTools::Eventing::ListenerID m_quitListener;

		std::unique_ptr<tinyxml2::XMLDocument> m_sceneBackup;

		void LoadEmptySceneNow();
		void LoadSceneFromDiskNow(const std::string& p_path, bool p_absolute);
		void ApplyHistory(bool p_undo);
		void FlushEdits(bool p_whileHeld);
		void TrackSelection();
		void UpdateWindowTitle();

		struct PendingSceneEdit
		{
			std::string label;
			bool nameSelection = false;
			std::string before;
			uint64_t focus = 0;
		};

		OvCore::SceneSystem::SceneHistory m_sceneHistory;
		std::optional<PendingSceneEdit> m_pendingSceneEdit;
		std::optional<uint64_t> m_historyFocus;
		uint64_t m_trackedGUID = 0; // The selected actor, whose edits are recorded
		std::string m_trackedState; // ... and its data when they last were
		bool m_closeConfirmed = false;
		std::string m_windowTitle;
	};
}

#include <OvEditor/Core/EditorActions.inl>
