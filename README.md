# Lost

Lost is a free, open-source 3D game engine written in C++20, with Lua as its scripting language and an editor to build
games with. It started as a fork of [Overload](https://github.com/Overload-Technologies/Overload) 1.9 and keeps its
editor, runtime and project format, with its physics moved to [Jolt](https://github.com/jrouwe/JoltPhysics) and a
growing set of systems on top.

Lost is meant to be a general-purpose engine: nothing in it is tied to a particular game, and features are added
because more than one kind of game needs them.

## Features

- Lua scripting, sandboxed, with a documented API
- Editor with prefabs, a material editor and a profiler
- Physically-based rendering, shadows, post-processing and custom shaders
- Rigid body physics, ragdolls, height fields and mesh colliders (Jolt Physics)
- Skeletal animation with layers and masks
- Spatial audio (SoLoud)
- Sandboxed file storage and JSON for save files
- Windows and Linux

## Building

### Windows (MSVC)

```powershell
git clone https://github.com/Lost-Things-Studio/lost
cd lost
.\gen_proj.bat vs2022
.\Overload.sln
```

### Linux (Clang)

```bash
git clone https://github.com/Lost-Things-Studio/lost
cd lost
./gen_proj.sh gmake
make config=release_x64 -j$(nproc)
```

The editor needs `zenity` on Linux.

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

## Contributing

One branch and one pull request per change, against `main`. A change should be generic (it would serve a game of
another genre as well), configurable rather than hard-coded, documented in `Docs/` and the changelog, and keep existing
projects working. See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

Lost is released under the MIT license, as Overload is. See [`LICENSE`](LICENSE).

Overload was created by [Benjamin Viranin](https://github.com/BenjaminViranin), [Max Brun](https://github.com/maxbrundev)
and [Adrien Givry](https://github.com/adriengivry).
