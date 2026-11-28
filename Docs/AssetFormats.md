# Asset formats

The rules every Lost file format follows: its name, how it says its version, how it is written, and how an older file
is read. Each format's content is described on its own page; this page is what they share.

> **Status**: the engine reads both names of each format and writes Lost's, in version 1. Its own resources
> (`Resources/Engine`, `Resources/Editor`) have Lost's names, and a command converts a project (below). The package
> format is in, with `LostPack` (see [Packages](Packages.md)), and so is the prepared model, which a game's build
> writes (see [Building](Building.md)).

## The formats

| Format | Extension | Read as well | Content | Version marker | Version |
| --- | --- | --- | --- | --- | --- |
| Project | `.loproject` | `.ovproject` | INI (`key=value` lines, see [Projects](Projects.md)) | the key `format_version` | 1 |
| Scene | `.loscene` | `.ovscene` | XML, `<root><scene>` | the attribute `version` on `<root>` | 1 |
| Prefab | `.loprefab` | `.ovprefab` | XML, `<root><prefab>` | the attribute `version` on `<root>` | 1 |
| Material | `.lomat` | `.ovmat` | XML, `<root>` with `<shader>`, `<settings>`, `<uniforms>` | the attribute `version` on `<root>` | 1 |
| Shader | `.lofx` | `.ovfx` | GLSL with the engine's directives | the shader API's version (see below) | |
| Shader include | `.lofxh` | `.ovfxh` | GLSL with the engine's directives | the shader API's version | |
| Animation graph | `.loanim` | `.ovanim` | JSON (see [Animation](Animation.md#the-graph-file)) | the member `"version"` | 1 |
| Asset metadata | `<asset>.meta` | | INI, next to the asset it describes | the key `format_version` | 1 |
| Prepared model | `<model>.lomodel` | | binary, a model as its import gives it, made by a build (below) | a header | 1 |
| Package | `.lopak` | | binary, a project's files compressed (see [Packages](Packages.md)) | a header | 1 |

**A package and a prepared model are not assets**: they are what a build makes of a project's files, never files inside
its `Assets`, so they have no file type in the editor.

**Formats that already have their own rules** keep them:
- Lua scripts (`.lua`) are code, with the Lua API's version (see [Scripting](Scripting.md));
- input actions and translations are JSON files the project names (see [Input](Input.md), [Localization](Localization.md));
- save files are written in an envelope with their version (see [Saving](Saving.md#versions)).

**Files Lost imports and never writes** keep their own formats: models (`.fbx`, `.obj`, `.gltf`, `.glb`, `.vrm`),
textures, sounds and fonts. A game's build writes each model's import as a prepared model (below), next to it in the
game's package.

## Names

- **Extensions**: `.lo` followed by a short lowercase name. A new format takes the next free one and adds its row above.
- **Overload's names are still read**: `.ovX` and `.loX` are the same file type (one `PathParser::EFileType`). They are
  found, listed, opened and referenced the same way.
- **A file asked for by one name is found under the other**: a scene that names `Materials/Rock.ovmat` reads
  `Materials/Rock.lomat` once the file has been renamed, and so do the resource managers, `#include`, scenes, prefabs,
  projects and the files a component reads itself, such as an animator's graph (`PathParser::FindFileToRead`). The resource is kept under the name the file has, so that what refers to
  it is saved with that name. A file is never written under the other name.
- **A folder holds one project file**, `.loproject` or `.ovproject`. Given a folder with both, the editor opens the Lost
  one, and `OvGame --project` asks for the file to be named.
- **What the editor writes** (a new scene, a material, Save As) has the Lost extension.
- **A file's name says nothing of its version**: the version is inside it.
- **Metadata** goes in `<asset>.meta`, the asset's full name followed by `.meta` (`Rock.png.meta`). The asset browser
  moves and deletes it with its asset.

## Versions

- **Each text format has its own version**, a whole number starting at 1. It goes up when the format changes in a way an
  older reader would misread: a value moved, renamed or read differently. A new optional value, which older readers
  ignore, does not change it.
- **A file without a marker is version 0**: a file from Overload, or from Lost before the format had a version.
- **The marker comes first** in XML and JSON, where a reader finds it without reading the rest. An INI file is read
  whole, and its keys are written by name (below): the marker is where its name puts it.

  ```xml
  <root version="1">
      <scene>...</scene>
  </root>
  ```

  ```ini
  executable_name=Game
  format_version=1
  fullscreen=false
  ```

  ```json
  {
      "version": 1,
      "parameters": [],
      "layers": []
  }
  ```

- **Shaders** have no version of their own: a shader states the shader API it needs, through `SHADER_API_VERSION_MAJOR`
  and `SHADER_API_VERSION_MINOR` (see [Shaders](Shaders.md#version)).
- **Binary formats** start with a magic number of four characters (`LOPK`, `LOMD`) and their version, as numbers in
  little endian.

## Prepared models (`.lomodel`)

A prepared model is a model as its import gives it (meshes, skeleton, animations, the materials and textures it holds),
written by a game's build so that the game does not import the model again (see [Building](Building.md#prepared-models)).
It is named after its model: `Rock.fbx.lomodel` for `Rock.fbx`. The same model, imported with the same settings,
gives the same bytes. In C++: `OvRendering::Resources::PreparedModel` (`Write`, `Read`).

**Version 1**: a header of 24 bytes, then the body. Numbers are little endian, floats IEEE 754 on 32 bits, a text is
its length (4 bytes) then its bytes in UTF-8, a list is its count (4 bytes) then its items.

| Header, in bytes | Content |
| --- | --- |
| 0 | `LOMD` |
| 4 | the version, 1 |
| 8 | reserved, 0 |
| 12 | the CRC-32 of the body (zlib's) |
| 16 | the size of the body, 8 bytes |

The body, in this order:
1. **The materials' names**, the slots the model's meshes refer to: a list of texts.
2. **The textures the model holds or names**: a list of: its source (1 byte: 0 a file next to the model, 1 encoded in
   the model, 2 raw texels), the path the model gives, its extension, its encoded bytes (a count of 8 bytes, then
   them), its width and height, its texels in RGBA8 (a count of 8 bytes, then them).
3. **The materials the model holds**: a list of: its name, its albedo (4 floats), metallic, roughness, emissive colour
   (3 floats), emissive intensity, then the index of each of its textures (albedo, metallic, roughness, ambient
   occlusion, normal, height, emissive, opacity), `0xFFFFFFFF` for none.
4. **The skeleton**: 1 byte, 0 for none. Then its nodes, a list of: name, parent (-1 for the root, else a node before
   it), bone (-1 for none), bind transform (16 floats), bind position (3), rotation (4, x y z w) and scale (3). Then its
   bones, a list of: name, node, offset matrix (16 floats).
5. **The meshes**: a list of: skinned (1 byte), material slot, vertex count, index count, the vertices (position 3,
   texture coordinates 2, normal 3, tangent 3, bitangent 3 floats; a skinned vertex adds 4 bone indices and 4
   weights), then the indices, three per triangle.
6. **The animations**: a list of: name, duration and ticks per second (floats), then its tracks, a list of: node, then
   its position, rotation and scale keys, each a list of a time and a value.

**A reader refuses**, with why, a file that is not one, of a newer version, with reserved bits set, cut short or longer
than its header says, whose CRC does not match, whose counts its bytes cannot hold, or that names what it does not hold
(a vertex, a parent node, a bone, a node, a texture), or texels that are not its size.

## Reading and writing

**Reading**
- **Any version up to the engine's is read.** An older file is brought up to the latest in memory, one version at a
  time, each step a migration function of its format. The file on disk is left as it is until it is saved.
- **A version newer than the engine's is refused**, with an error that names the file, its version and the newest
  version the engine reads. Nothing of it is loaded: a newer file is never half read.
- **An unknown element or key is left alone** in XML and INI, so that a newer optional value does not stop an older
  reader. An animation graph refuses unknown members, which catches a typo: the members it does not use itself are
  listed by name (`version`, and `editor` for the graph editor).
- **What a refused file does**:
  - a project: the editor says so and does not open it; the game does not start, and ends with exit code 1;
  - a scene, a prefab, a material, a graph: it is not loaded, with an error that names it;
  - an asset's metadata: the asset is read with the default settings, with a warning.
- **Version 1 reads as version 0 does.** It only adds the marker, and the order below; no migration function is
  needed yet.

**Writing**
- **A file is always written in the latest version**, with its marker.
- **Text**: UTF-8 without a byte order mark, and a dot as the decimal separator whatever the system's language.
- **Names**: XML elements and project keys are `snake_case`, JSON members `camelCase` (`exitTime`), and the keys of an
  asset's metadata capitals (`MIN_FILTER`), as they have always been.
- **Order**: everything is written in a fixed order, so that a file saved twice is the same file and a change shows as
  itself in version control. Where the engine keeps values in no order of its own, they are written sorted by name: a
  material's uniforms and features, an INI file's keys.
- **Booleans** are `true` and `false`.
- **GUIDs** are unsigned 64-bit numbers, in decimal.
- **Paths**:
  - relative to the project's `Assets` folder, or to the engine's resources with a leading `:`
    (`:Shaders/Standard.lofx`);
  - written with `/`, both `/` and `\` read;
  - an asset inside a model is named after it with `:` (`Models/Rock.fbx:embedded_material_0`).

## Moving a project to the Lost formats

- **Overload's files keep working**: the engine reads both names, and a project may mix them. A project that names
  the engine's resources by Overload's names (`:Shaders/Standard.ovfx`) still finds them under Lost's.
- **What the editor saves** takes the latest version. What it makes takes the Lost extension: a new scene, prefab,
  material, shader, shader include, animation graph or project, and Save As. A file opened keeps its name when saved.
- **A command converts a whole project**: File > Convert Project to Lost Formats, in the editor
  (`OvCore::Helpers::FormatConversion`).
  - It asks to save the scene's changes first, then whether to go on.
  - It renames each `.ovX` file of the project's `Assets` folder, and its project file, to `.loX`, with its metadata
    (`<asset>.meta`).
  - It rewrites the references to them, and to the engine's resources, in the project file, scenes, prefabs,
    materials, shaders and animation graphs. Only the extension of a reference changes: a path keeps its folders and
    its separators.
  - In Lua scripts, it changes a string that is exactly such a path, comments included, and lists each line it
    changed. A path a script builds itself (`"Shaders/" .. sName .. ".ovfx"`) is not seen: it still works, through the
    other name, and is the reader's to change.
  - A file whose Lost name is taken already keeps its name, and so do the references to it; the report says so.
  - The files keep their version: they take the latest when the editor saves them.
  - The report goes to `LostFormatsConversion.txt` in the project's folder, and the editor closes, since the files it
    has open have new names. Running it again changes nothing.
  - The command changes files on disk at once: a project under version control can be looked over, and set back, there.
- **Save files are not assets**: their envelope versions them, and no conversion touches them.

## Adding a format

1. Pick its extension, `.lo<name>`, and add its row to the table above.
2. Give it a version marker, starting at 1, and a migration function for each later version.
3. Add its file type to `PathParser` (`GetFileType`, `FileTypeToString`, `StringToFileType`), so that the asset browser,
   the pickers and `PropagateFileRename` know it.
4. Test it in `OvTests`: a file of each version read, the latest written and read back, a newer version refused.
5. Describe its content on its own page, and link it here.
