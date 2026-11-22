# Asset formats

The rules every Lost file format follows: its name, how it says its version, how it is written, and how an older file
is read. Each format's content is described on its own page; this page is what they share.

> **Status**: this page sets the conventions. The engine still writes Overload's extensions, without a version. The move
> to the `.lo*` names and to version 1 comes with the formats' migration ([#5](https://github.com/Lost-Things-Studio/Lost/issues/5),
> [#6](https://github.com/Lost-Things-Studio/Lost/issues/6)). The "Today" column says what the engine does until then.

## The formats

| Format | Extension | Read as well | Content | Version marker | Today |
| --- | --- | --- | --- | --- | --- |
| Project | `.loproject` | `.ovproject` | INI (`key=value` lines) | the key `format_version` | `.ovproject`, no version |
| Scene | `.loscene` | `.ovscene` | XML, `<root><scene>` | the attribute `version` on `<root>` | `.ovscene`, no version |
| Prefab | `.loprefab` | `.ovprefab` | XML, `<root><prefab>` | the attribute `version` on `<root>` | `.ovprefab`, no version |
| Material | `.lomat` | `.ovmat` | XML, `<root>` with `<shader>`, `<settings>`, `<uniforms>` | the attribute `version` on `<root>` | `.ovmat`, no version |
| Shader | `.lofx` | `.ovfx` | GLSL with the engine's directives | the shader API's version (see below) | `.ovfx` |
| Shader include | `.lofxh` | `.ovfxh` | GLSL with the engine's directives | the shader API's version | `.ovfxh` |
| Animation graph | `.loanim` | `.ovanim` | JSON (see [Animation](Animation.md#the-graph-file)) | the member `"version"` | `.ovanim`, no version, unknown members refused |
| Asset metadata | `<asset>.meta` | | INI, next to the asset it describes | the key `format_version` | no version |
| Prepared model | `.lomodel` | | binary, made by a build | a header | to come ([#8](https://github.com/Lost-Things-Studio/Lost/issues/8)) |
| Package | `.lopak` | | binary, made by a build | a header | to come ([#7](https://github.com/Lost-Things-Studio/Lost/issues/7)) |

**Formats that already have their own rules** keep them:
- Lua scripts (`.lua`) are code, with the Lua API's version (see [Scripting](Scripting.md));
- input actions and translations are JSON files the project names (see [Input](Input.md), [Localization](Localization.md));
- save files are written in an envelope with their version (see [Saving](Saving.md#versions)).

**Files Lost imports and never writes** keep their own formats: models (`.fbx`, `.obj`, `.gltf`, `.glb`), textures,
sounds and fonts.

## Names

- **Extensions**: `.lo` followed by a short lowercase name. A new format takes the next free one and adds its row above.
- **Overload's names are still read**: `.ovX` and `.loX` are the same file type (one `PathParser::EFileType`). They are
  found, listed, opened and referenced the same way.
- **What the editor writes** (a new scene, a material, Save As) has the Lost extension.
- **A file's name says nothing of its version**: the version is inside it.
- **Metadata** goes in `<asset>.meta`, the asset's full name followed by `.meta` (`Rock.png.meta`). The asset browser
  moves and deletes it with its asset.

## Versions

- **Each text format has its own version**, a whole number starting at 1. It goes up when the format changes in a way an
  older reader would misread: a value moved, renamed or read differently. A new optional value, which older readers
  ignore, does not change it.
- **A file without a marker is version 0**: a file from Overload, or from Lost before the format had a version.
- **The marker comes first**, where a reader finds it without reading the rest:

  ```xml
  <root version="1">
      <scene>...</scene>
  </root>
  ```

  ```ini
  format_version=1
  executable_name=Game
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
- **Binary formats** start with a magic number of four characters (`LOPK`) and their version, as numbers in little
  endian.

## Reading and writing

**Reading**
- **Any version up to the engine's is read.** An older file is brought up to the latest in memory, one version at a
  time, each step a migration function of its format. The file on disk is left as it is until it is saved.
- **A version newer than the engine's is refused**, with an error that names the file, its version and the newest
  version the engine reads. Nothing of it is loaded: a newer file is never half read.
- **An unknown element or key is left alone** in XML and INI, so that a newer optional value does not stop an older
  reader. An animation graph refuses unknown members, which catches a typo: the members it does not use itself are
  listed by name (`version`, and `editor` for the graph editor).

**Writing**
- **A file is always written in the latest version**, with its marker.
- **Text**: UTF-8 without a byte order mark, and a dot as the decimal separator whatever the system's language.
- **Names**: XML elements, INI keys and JSON members are `snake_case`.
- **Order**: everything is written in a fixed order, so that a file saved twice is the same file and a change shows as
  itself in version control. A material's uniforms are the exception today, written in their map's order: from
  version 1 they are sorted by name.
- **Booleans** are `true` and `false`.
- **GUIDs** are unsigned 64-bit numbers, in decimal.
- **Paths**:
  - relative to the project's `Assets` folder, or to the engine's resources with a leading `:`
    (`:Shaders/Standard.ovfx`);
  - written with `/`, both `/` and `\` read;
  - an asset inside a model is named after it with `:` (`Models/Rock.fbx:embedded_material_0`).

## Moving a project to the Lost formats

- **Overload's files keep working**: the engine reads both names, and a project may mix them.
- **What the editor saves** takes the Lost extension and the latest version.
- **A command converts a whole project** (to come with [#5](https://github.com/Lost-Things-Studio/Lost/issues/5)):
  - it renames each `.ovX` file to `.loX`;
  - it rewrites every reference to them in scenes, prefabs, materials, graphs and the project file;
  - it reports the Lua strings it changed, line by line, since a script may build a path the command cannot see.
- **Save files are not assets**: their envelope versions them, and no conversion touches them.

## Adding a format

1. Pick its extension, `.lo<name>`, and add its row to the table above.
2. Give it a version marker, starting at 1, and a migration function for each later version.
3. Add its file type to `PathParser` (`GetFileType`, `FileTypeToString`, `StringToFileType`), so that the asset browser,
   the pickers and `PropagateFileRename` know it.
4. Test it in `OvTests`: a file of each version read, the latest written and read back, a newer version refused.
5. Describe its content on its own page, and link it here.
