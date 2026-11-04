# unrage

unrage is a command-line tool for GTA 5 RPF archives, as `7z` and `unrar`
are for their archives. It reads and writes OPEN, AES, and NG archives.

## Build

You need MinGW-w64.

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

| Switch | Action |
|--------|--------|
| `-o<dir>` | Write the extracted files under `dir`. |
| `-k<dir>` | Load the keys from `dir`. The default is `keys`. |
| `-r` | Go into nested `.rpf` archives. |
| `-t<open\|aes\|ng>` | Set the encryption of the archive that `a` or `d` writes. |

```bat
unrage l -r common.rpf
unrage x -r -oout common.rpf
unrage a mymod.rpf stream
unrage d dlc.rpf data/old
```

## Keys

AES and NG archives need keys. Use a CodeWalker key dump directory. The keys
are not part of this repository.

Do not rename an NG archive or an entry in it. The NG encryption uses the
names.

## License

ISC. `rpf.h` contains parts of
[CodeWalker](https://github.com/dexyfex/CodeWalker) by dexyfex, which uses
the MIT license. Refer to `LICENSE`.
