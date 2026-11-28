# Packages

A package (`.lopak`) holds a game's files in one file, each compressed when that makes it smaller. This page sets out
the format, what a reader checks, and `LostPack`, the tool that makes and reads packages.

> **Status**: the format, its library (`OvTools::Filesystem::Package`), `LostPack`, and a game that reads its assets
> from packages (below) are in. A build does not write packages yet
> ([#8](https://github.com/Lost-Things-Studio/Lost/issues/8)): they are made with `LostPack` for now.

## What a package is

- **Compressed, not encrypted.** A file is stored with deflate (the zlib format) when that saves bytes, and as it is
  otherwise. Nothing is encrypted: a package keeps files together and checks they are whole, it does not hide them.
- **Checked.** The index has a CRC-32, each file has one, and the manifest has a hash of the whole content.
- **The same files give the same bytes.** Paths are sorted, the compression level is fixed and nothing records a date,
  so two packages of the same files, written by the same engine, are identical byte for byte.

## The format, version 1

Numbers are unsigned and little endian. A package is a header, then each file's bytes one after the other, then the
index.

**Header**, 32 bytes:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | `LOPK` |
| 4 | 4 | the format's version: 1 |
| 8 | 8 | where the index starts |
| 16 | 8 | the index's size: it runs to the end of the file |
| 24 | 4 | the CRC-32 of the index |
| 28 | 4 | reserved: 0 |

**The files' bytes** follow the header, in the order of the index, with nothing between them.

**The index**:
- `u32` the manifest's size, then the manifest, a JSON object in UTF-8:

  ```json
  {
  	"version": 1,
  	"lostVersion": "0.1.0",
  	"projectId": "Adventurers",
  	"fileCount": 110,
  	"contentHash": "fnv1a64:35c2fbeb6def4cab"
  }
  ```

  - `version` is the header's;
  - `lostVersion` is the engine that wrote the package;
  - `projectId` is the project it was made from, or empty;
  - `contentHash` is FNV-1a on 64 bits over each file, in the index's order: its path, a zero byte, its size as a
    `u64`, then its bytes. Two packages with the same content have the same hash, whatever project or engine wrote
    them.
- `u32` the number of files, then for each file:

  | Size | Field |
  | --- | --- |
  | 2 | the path's size |
  | that size | the path |
  | 1 | the method: 0 stored, 1 deflate |
  | 8 | where its bytes start, from the start of the package |
  | 8 | its size in the package |
  | 8 | its size once read |
  | 4 | the CRC-32 of its bytes once read |

**Paths** are relative to the folder that was packed, in UTF-8, with `/` between names. A name is never empty, `.` or
`..`, and a path holds no `\`, `:` or control character. Case is kept. Paths are sorted byte by byte, and each appears
once.

## What a reader refuses

A package is checked when it is opened, and each file when it is read. Each refusal says why:

- **when it is opened**:
  - not `LOPK`, a version newer than the engine reads, or reserved bits set;
  - an index that is not at the end of the file, a CRC that does not match, or a manifest that cannot be read or has a
    member the engine does not know;
  - an entry with a method the engine does not know, a path that is not valid or not in its place, bytes that are not
    where the previous file ends, or a size its stored bytes cannot hold (a stored file is the size of its bytes; deflate
    cannot exceed 1032 times);
- **when a file is read**: bytes that do not decompress to their size, or a CRC that does not match.

A later version of the format can add a method, an encryption for instance, which this version refuses.

## A game reads its packages

`OvGame` mounts the package of each of its two asset folders when there is one: the folder's name followed by
`.lopak`, next to the folder.

| Folder | Its package |
| --- | --- |
| `Data/User/Assets`, the project's assets | `Data/User/Assets.lopak` |
| `Data/Engine`, the engine's resources | `Data/Engine.lopak` |

With `--project` and `--engine` the rule is the same: `--project Game` reads `Game/Assets.lopak`, `--engine Engine`
reads `Engine.lopak` (see [Command line](CommandLine.md)). The project file, `Data/User/Game.ini`, stays a file of its
own.

**How a path is read** (`OvTools::Filesystem::VirtualFileSystem`):
- A path under a folder a package is mounted on is looked for in the package, under its path relative to the folder.
- A path the package does not hold, or outside every mounted folder, is read from the disk, as before. A game may so
  ship loose files next to its packages, which the packages do not hide.
- A file a package holds but cannot read (damaged) is an error, not looked for on the disk.
- **Case counts** in a package, even on Windows, whose disk does not care: a scene that names `textures/rock.png` does
  not find `Textures/Rock.png`.

Every read of an asset goes through it: textures, models and the files they name (a glTF's buffers), shaders and their
includes, materials, scenes, prefabs, sounds, fonts, scripts and `require`, input actions, translations, animation
graphs, height and density maps, and each asset's `.meta`. Each format's other name (`.ovscene` for `.loscene`) is
found in a package as on the disk.

**A package that cannot be read** (a newer version, a damaged index) stops the game before it opens its window, with
exit code 1 and the reason on the error output. The log names each package mounted, its number of files and its
content hash.

The editor mounts nothing: it reads and writes the project's files on the disk.

## LostPack

`LostPack` is built with the engine, as `Bin/<configuration>/LostPack/LostPack`:

```
LostPack pack <folder> <package> [--project-id <id>]
LostPack list <package>
LostPack verify <package>
LostPack extract <package> <folder>
```

- **`pack`** writes every regular file under the folder, followed into sub-folders; links are left out. It writes to
  `<package>.~tmp`, then renames it, so a failure leaves no half package. A package cannot be written inside the
  folder it packs.
- **`list`** shows the manifest, then each file: its size, its size in the package, its method and its path.
- **`verify`** reads every file, and checks the content hash.
- **`extract`** writes the files into a folder that is new or empty: nothing already there is overwritten.

It ends with 0 when it is done, 1 when the package or a file is at fault, and 2 when the command line is wrong.

## In C++

`OvTools/Filesystem/Package.h`:
- `PackageBuilder` gathers files with `AddFolder`, `AddFile` and `AddData`, then `Write` writes them;
- `PackageReader::Open` checks a package, then `GetManifest`, `GetEntries`, `Find`, `Read` and `Verify` read it;
- `PackageFormat::IsValidPath` says whether a package may hold a path.

`OvTools/Filesystem/VirtualFileSystem.h`:
- `Mount(folder, package)`, `Unmount`, `UnmountAll`, and `ScopedMount`, which unmounts when it goes;
- `Read`, `ReadText`, `Exists`, `IsDirectory` and `List`, for a package's files and the disk's alike. They may be called
  from any thread.

**Limits of this version**:
- each file is read whole into memory, when it is packed and when it is read;
- a `PackageReader` is not to be shared between threads: the virtual file system holds a lock while it reads one.
