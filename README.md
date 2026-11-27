# Lost

Lost is a free, open-source 3D game engine written in C++20, with Lua as its scripting language and an editor to build
games with. It started as a fork of [Overload](https://github.com/Overload-Technologies/Overload) 1.9 and keeps its
editor, runtime and project format, with its physics moved to [Jolt](https://github.com/jrouwe/JoltPhysics) and a
growing set of systems on top.

Lost is meant to be a general-purpose engine: nothing in it is tied to a particular game, and features are added
because more than one kind of game needs them.

## Features

- Lua scripting, sandboxed, with a documented and versioned API, `require`, and a standard library: classes, signals,
  timers, tweens, pools, state machines and save files
- An editor with prefabs, a material editor and a profiler
- Physically-based rendering, shadows, post-processing, custom shaders on a public shader API, and levels of detail
- Rigid body physics on Jolt Physics: collision layers, queries, a character controller, ragdolls, height fields and
  mesh colliders
- Navigation meshes and path finding
- Skeletal animation: layers and masks, retargeting, a state graph, look-at and spring bones
- GPU particles
- Terrains drawn in chunks with levels of detail, and instanced vegetation
- A UI library: text in every script a font has, text input, clipping, and lit text in the world
- Input actions that players can rebind, and gamepads
- Spatial audio with buses and one-shot sounds
- Localization tables
- Sandboxed file storage, JSON and versioned save files
- A command line to run a game headless, for tests and benchmarks
- Windows and Linux

## Building

### Windows (MSVC)

```powershell
git clone https://github.com/Lost-Things-Studio/lost
cd lost
.\gen_proj.bat vs2022
.\Lost.sln
```

### Linux (Clang)

```bash
git clone https://github.com/Lost-Things-Studio/lost
cd lost
./gen_proj.sh gmake
make config=release_x64 -j$(nproc)
```

The editor needs `zenity` on Linux.

### Tests

The build also makes `OvTests`, the engine's tests, and `Tests/run_game_checks.sh` runs the in-game checks with a built
`OvGame`. The CI runs both; see [`Docs/Testing.md`](Docs/Testing.md).

## Versions

`VERSION.txt` holds Lost's own version, following semantic versioning, and scripts can read it with
`Application.GetEngineVersion()`. The Overload version Lost is based on is kept as `OVERLOAD_VERSION`.
[`CHANGELOG.md`](CHANGELOG.md) lists the changes from one version to the next.

## Documentation

- [`Docs/`](Docs/README.md) covers scripting, physics, rendering, animation, saving and input, with the rules and
  pitfalls of each.
- The Lua API is described in `Resources/Engine/Lua/`, one definition file per type, which editors with a Lua language
  server pick up.
- The UI library documents itself in `Resources/Engine/Libraries/UI/Docs/`.

## Architecture

The engine is split into libraries and two applications:

- `OvDebug`: logging and assertions
- `OvTools`: serialization, file system, platform, events, clock
- `OvMaths`: vectors, matrices, quaternions, transforms
- `OvAudio`: audio engine, on SoLoud
- `OvPhysics`: physics engine, on Jolt Physics
- `OvRendering`: rendering engine, on BareGL
- `OvWindowing`: windows and inputs, on GLFW
- `OvUI`: widget-based UI, on Dear ImGui
- `OvCore`: scenes, components, scripting and resources
- `OvGame`: the data-driven executable that runs a game
- `OvEditor`: the editor
- `OvTests`: the engine's tests

## Contributing

One branch and one pull request per change, against `main`. A change should be generic (it would serve a game of
another genre as well), configurable rather than hard-coded, documented in `Docs/` and the changelog, and keep existing
projects working. See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Dependencies

Lost builds its dependencies from source, in `Dependencies/`, each with its licence:

| Library | Used for | Licence |
| --- | --- | --- |
| [Assimp](https://github.com/assimp/assimp) | Importing models | BSD 3-Clause |
| BareGL | The OpenGL layer of the renderer | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | The editor's interface | MIT |
| [doctest](https://github.com/doctest/doctest) | The engine's tests | MIT |
| [FreeType](https://freetype.org) | Fonts | FreeType License |
| [GLFW](https://www.glfw.org) | Windows, inputs and gamepads | zlib |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Physics | MIT |
| [Lua](https://www.lua.org) | Scripting | MIT |
| [Premake](https://github.com/premake/premake-core) | Generating the projects | BSD 3-Clause |
| [Recast & Detour](https://github.com/recastnavigation/recastnavigation) | Navigation meshes and paths | zlib |
| [sol2](https://github.com/ThePhD/sol2) | The Lua bindings | MIT |
| [SoLoud](https://github.com/jarikomppa/soloud) | Audio | zlib |
| [stb_image](https://github.com/nothings/stb) | Decoding images | Public domain or MIT |
| [TinyXML-2](https://github.com/leethomason/tinyxml2) | Scene, prefab and material files | zlib |
| [zlib](https://zlib.net) | Packages, and the models Assimp decompresses | zlib |
| [Tracy](https://github.com/wolfpld/tracy) | The profiler | BSD 3-Clause |

Where Lost changes a dependency's code, a `Lost:` comment marks the change.

## License

Lost is released under the MIT license, as Overload is. See [`LICENSE`](LICENSE).

Overload was created by [Benjamin Viranin](https://github.com/BenjaminViranin), [Max Brun](https://github.com/maxbrundev)
and [Adrien Givry](https://github.com/adriengivry).
