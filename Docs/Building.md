# Building a game

How the editor turns a project into a game a player runs: what a build holds, its packages and prepared models, and
how to build from the command line.

## Building

The **Build** menu has three entries:
- **Build game** asks for a folder, which must not exist yet, and builds the game in it;
- **Build game and run** does the same, then starts the game;
- **Temporary build** builds the game in the editor's own data folder (`TempBuild`), replacing the previous one, then
  starts it.

The build type is the project's (`build_type`, in Project Settings > Build): Debug, Release or Publish. It chooses the
game executable the build takes from the editor's `Builder` folder. Debug is unoptimized and checks every OpenGL call,
so it is only for finding a fault; Publish is Release without the profiler. The editor has the executables of the
configurations it was built with.

The build runs on the editor's thread: the editor waits for it, and its console then says how long it took, how many
files it wrote and models it prepared, and the size of the data before and after. A build that fails leaves no folder
behind, and says why in a message box and in the console.

## What a build holds

```
<build folder>/
    <executable_name>[.exe]         the game (and, on Windows, its libraries)
    Data/User/Game.ini              the project file, as it is on the disk
    Data/User/Assets.lopak          the project's assets
    Data/Engine.lopak               the engine's resources
```

The game reads its assets from the packages (see [Packages](Packages.md#a-game-reads-its-packages)), and its settings
from `Game.ini`. Settings changed in Project Settings reach the build once applied: the build copies the file.

**Before anything is written**, the build checks that the game executable of the build type is there, and that the
start scene (`start_scene`) is in the assets, under either name of its format. Either missing, nothing is built.

## The packages

- **The project's package holds all of `Assets`**: scenes, prefabs, materials, shaders, scripts, textures, sounds,
  fonts, translations, input actions, metadata (`.meta`) and any other file. A script may load an asset from a path it
  builds at run time (`Resources.GetModel("Models/" .. name)`), so the build does not guess which assets are used.
- **Its manifest holds the project's ID** (`project_id`).
- **The engine's package leaves out what only the editor uses**: `Lua/`, the definitions for a Lua language server,
  and `Libraries/UI/Docs/`.
- **A package or a prepared model inside `Assets` is left out**, with a warning: they are what a build makes.
- **A file name a package cannot hold** (one with `:`, `\` or a control character) stops the build, naming the file.
- **The same project gives the same packages**, byte for byte.

## Prepared models

**Each model** (`.fbx`, `.obj`, `.gltf`, `.glb`, `.vrm`) is imported by the build, with the settings of its `.meta` as
the editor imports it, and written in the package as a **prepared model**: `Models/Rock.fbx.lomodel` for
`Models/Rock.fbx`. Its file is left out of the package. The game loads the prepared model as it is, without importing
the file again: meshes, skeleton, animations and the materials and textures the model holds.

- **The paths do not change**: scenes, scripts and animators still name `Models/Rock.fbx`, and so do the assets inside
  a model (`Models/Rock.fbx:embedded_texture_0.png`).
- **The model's other files stay in the package**: its `.meta`, a glTF's `.bin`, an OBJ's `.mtl`, the textures it
  names.
- **A model the build cannot import** keeps its file in the package, with a warning, and the game imports it as before
  (and fails as before).
- **A prepared model that cannot be read** (damaged, or of a newer version) is an error in the game's log, and the
  game imports the model's file instead when the package holds it.

The format is in [Asset formats](AssetFormats.md#prepared-models-lomodel).

**What it changes**: the game no longer imports its models, which is the slowest part of loading a big one. A prepared
model stores its vertices as the GPU takes them, so a package may be larger than one holding the models' files:
Adventurers' assets take 66 MB with prepared models, 53 MB with the models' files.

## Without packages

**Packages**, in Project Settings > Build (`build_packages`), is on by default. Off, the build copies the project's
`Assets` to `Data/User/Assets` and the engine's resources to `Data/Engine`, as they are, with no prepared model: a
build to look into, or to let players change files. The game reads such folders as it always has.

## From the command line

```
OvEditor --build <project folder> <build folder> [--type Debug|Release|Publish]
```

builds the project's game as **Build game** does, without opening a window: for a continuous integration, or a script.
The build type is the project's, unless `--type` gives one. The editor takes the engine's resources and the game
executables next to it (its `Data/Engine` and `Builder` folders), or else in the working folder.

It prints what it built and its warnings, and ends with:

| Code | Meaning |
| --- | --- |
| 0 | The game is built |
| 1 | It cannot be: the project is newer than the editor, the executable or the start scene is missing, a file cannot be read or packed. The reason is on the error output, and no build folder is left |
| 2 | The command line is wrong, or the project folder holds no project file |

On Windows, the Publish editor has no console: its exit code is what tells.
