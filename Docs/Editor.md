# Editor

What the editor does with the edits made to a scene: undoing and redoing them, the unsaved mark, and the question
asked before changes are lost.

## Undo and redo

| Action | Keys | Menu |
| --- | --- | --- |
| Undo | Ctrl+Z | Edit > Undo *edit* |
| Redo | Ctrl+Y, or Ctrl+Shift+Z | Edit > Redo *edit* |

- The Edit menu names the edit that would be undone or redone ("Undo Edit Transform of Player", "Redo Delete
  Goblin"), and greys the item when there is none.
- **In edit mode only**: while a game runs, nothing is undone, and nothing the game does is an edit. The edits made
  before the game are still there when it stops.
- **A text field being typed in keeps Ctrl+Z for itself**: it undoes the typing, not the scene.
- **An undo shows what it puts back**: the actor written back is selected, and an edit of the scene selects again the
  actor that was selected when it was made.
- **A new edit drops what could have been redone.**

### What makes an edit

**The selected actor's own data**, its name, tag, components and scripts, as the inspector and the gizmo change them.
- One edit per gesture: a slider dragged, a field typed in, a handle of the gizmo moved make one edit once they are let
  go, not one per frame.
- Its label says what changed: "Rename Player", "Add Component to Player", "Edit Transform of Player", "Edit Scripts of
  Player".
- Selecting another actor without changing anything makes no edit.

**The scene's structure**, each operation an edit of the whole scene:
- create (the Actors menu, a model or a prefab dropped in the scene view);
- delete, duplicate, paste;
- rename or move an actor in the hierarchy;
- a prefab instantiated, saved, applied or reverted.

What an operation does within its frame belongs to its edit. A cube created with the Actors menu is one edit, with its
renderers, its material and its name.

**The material editor's edits** are its own:
- they are undone with its Undo and Redo buttons, or Ctrl+Z and Ctrl+Y while it has the focus;
- each material starts its own edits, forgotten when another is opened;
- Save marks the material saved.

### What is not undone

- **Files.** Saving a scene or a material, saving an actor as a prefab, applying an actor to its prefab: undoing such
  an edit puts the scene back as it was, but the file keeps what was written.
- **The editor itself**: the camera, the panels, the selection alone.

### What forgets the edits

- **Another scene**: a new one, or one opened. Its edits start with it.
- **A file renamed or deleted in the asset browser**: the scene's references follow the file, and an edit put back
  would bring the old path back. The scene stays as unsaved as it was.

### Limits

An edit keeps its states as XML, the actor's for an edit of an actor and the scene's for an edit of its structure. The
history keeps the last 200 edits and at most 128 MB: past either limit, the oldest go first. In a large scene, an edit of
its structure takes as much as two copies of the scene file. `OvCore::SceneSystem::SceneHistory` holds them, on
`OvTools::Utils::StateHistory`, which a panel can use for edits of its own.

## Unsaved changes

- **The mark**: the window's title ends with `*` while the scene has changes that are not saved. An undo back to the
  state saved takes the mark away, and a redo brings it back.
- **The question**: before a new scene (File > New Scene, Ctrl+N), another scene (opened from the asset browser,
  dropped in the scene view) or closing the editor (File > Exit, the window's close button), the editor asks what to do
  with them:
  - **Save** saves the scene, then goes on. "Save As" given up leaves the scene unsaved, and the editor stays;
  - **Don't Save** goes on, and the changes are lost;
  - **Cancel**, or Escape, stays.
- **During a game**, the question is about the scene as it was edited. Save stops the game first.
