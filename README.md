# unrage

unrage is a command-line tool for GTA 5 RPF archives, as `7z` and `unrar`
are for their archives. It reads and writes OPEN, AES, and NG archives.

## Build

You need Visual Studio 2022 or its Build Tools, with the C++ tools.

```bat
build
build test
build clean
```

## Usage

```
unrage <command> [switches] <archive> [paths...]
```

| Command | Action |
|---------|--------|
| `l` | List the entries. |
| `t` | Test the archive. |
| `e` | Extract files with no paths. |
| `x` | Extract files with full paths. |
| `a` | Add files and directories. |
| `d` | Delete entries. |
| `fix` | Write the archive and all its nested archives again as NG. |

| Switch | Action |
|--------|--------|
| `-o<dir>` | Write the extracted files under `dir`. |
| `-k<dir>` | Load the keys from `dir`. The default is `keys`. |
| `-g<path>` | Make the keys from the `GTA5.exe` at `path`. |
| `-r` | Go into nested `.rpf` archives. |
| `-t<open\|aes\|ng>` | Set the encryption of the archive that `a`, `d`, or `fix` writes. |

```bat
unrage l -r common.rpf
unrage x -r -oout common.rpf
unrage a mymod.rpf stream
unrage d dlc.rpf data/old
unrage fix dlc.rpf
```

`a` and `d` copy a nested archive as it is. `fix` writes each nested archive
again with the encryption of the outer archive.

## Keys

AES and NG archives need keys. unrage looks for the keys in this order:

1. The directory that `-k<dir>` gives.
2. The directory `keys`.
3. The cache directory `%TEMP%\unrage-keys`.
4. A new key set that unrage makes.

unrage makes the keys only if you do not give `-k` and `keys` does not exist.
It finds the AES key in `GTA5.exe` and uses it to decode `magic.dat`. Then it
saves the keys in the cache directory. unrage finds `GTA5.exe` with
`-g<path>`, or in the directories above the archive, or in the registry.
`magic.dat` comes from [CodeWalker](https://github.com/dexyfex/CodeWalker),
and `unrage.exe` contains it.

A key directory can also be a CodeWalker key dump directory.

Do not rename an NG archive or an entry in it. The NG encryption uses the
names.

## License

ISC. `rpf.h` contains parts of
[CodeWalker](https://github.com/dexyfex/CodeWalker) by dexyfex, which uses
the MIT license. Refer to `LICENSE`.
