# Packages

A package (`.lopak`) holds a game's files in one file, each compressed when that makes it smaller. This page sets out
the format, what a reader checks, and `LostPack`, the tool that makes and reads packages.

> **Status**: the format, its library (`OvTools::Filesystem::Package`) and `LostPack` are in. The engine does not load
> assets from a package yet ([#9](https://github.com/Lost-Things-Studio/Lost/issues/9)), and a build does not write
> one yet ([#8](https://github.com/Lost-Things-Studio/Lost/issues/8)).

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

**Limits of this version**:
- each file is read whole into memory, when it is packed and when it is read;
- a `PackageReader` is not to be shared between threads.
