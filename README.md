# unrage

unrage is a command-line tool for GTA 5 RPF archives. It lists, tests,
extracts, adds, and deletes files, as `7z` and `unrar` do. It reads and
writes OPEN, AES, and NG archives, and it can go into nested archives.

The tool is one C++17 file in C style. It uses `rpf.h`, a single-file C99
library, for the archive format and the cryptography.

## Build

You need a C99 compiler and a C++17 compiler. On Windows, MinGW-w64 works.

```bat
mingw32-make            &:: build unrage.exe
mingw32-make test       &:: run the round-trip test
```

To build with no make, compile the two units:

```bat
gcc -std=c99 -O2 -D_FILE_OFFSET_BITS=64 -c rpf.c
g++ -std=c++17 -O2 -D_FILE_OFFSET_BITS=64 -c unrage.cpp
g++ -o unrage.exe unrage.o rpf.o
```

To run the test with no make, use PowerShell:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1
```

## Usage

```
unrage <command> [switches] <archive> [paths...]
```

| Command | Action |
|---------|--------|
| `l` | List the entries. |
| `t` | Test the archive. Read and decode each file. |
| `e` | Extract files to one directory, with no paths. |
| `x` | Extract files with full paths. |
| `a` | Add files and directories. Make the archive if it does not exist. |
| `d` | Delete entries. |

| Switch | Action |
|--------|--------|
| `-o<dir>` | Write the extracted files under `dir`. The default is the current directory. |
| `-k<dir>` | Load the keys from `dir`. The default is `./keys` if it exists. |
| `-r` | Go into nested `.rpf` archives. This applies to `l`, `t`, `e`, and `x`. |
| `-t<open\|aes\|ng>` | Set the encryption of the archive that `a` or `d` writes. |
| `--` | Stop the parse of switches. |

The paths in an archive use `/` or `\`. The compare ignores case.

The exit code is 0 on success, 1 on an error, and 2 on a usage error.

## Examples

List all entries, also the entries in nested archives:

```sh
unrage l -r -kkeys common.rpf
```

Extract all files to the directory `out`. Extract each nested archive into
a directory with its name:

```sh
unrage x -r -oout common.rpf
```

Extract one file and one directory with their paths:

```sh
unrage x -oout common.rpf data/gta5_cache_y.dat data/levels
```

Make a new OPEN archive from a file and a directory:

```sh
unrage a mymod.rpf readme.txt stream
```

Add a file to an existing archive. The file replaces an entry with the same
name. The archive keeps its encryption:

```sh
unrage a -kkeys dlc.rpf handling.meta
```

Delete a directory from an archive:

```sh
unrage d dlc.rpf data/old
```

## Keys

AES and NG archives need keys. A key set is a directory of files with the
names that CodeWalker uses, so a CodeWalker key dump directory works.

| File | Size |
|------|------|
| `gtav_aes_key.dat` | 32 bytes |
| `gtav_ng_key.dat` | 101 × 272 bytes |
| `gtav_ng_decrypt_tables.dat` | 17 × 16 × 256 words |
| `gtav_ng_encrypt_tables.dat` | 17 × 16 × 256 words |
| `gtav_ng_encrypt_luts.dat` | 17 × 16 × 196608 bytes |
| `gtav_hash_lut.dat` | 256 bytes |

An OPEN archive needs no keys, but a `.ysc` script in an OPEN archive needs
the NG keys. To read an NG archive, you need the NG keys, the decrypt
tables, and the hash LUT. To write an NG archive, you also need the encrypt
tables and the LUTs.

The keys are not part of this repository.

## Nested archives

With `-r`, `x` and `e` extract a nested archive into a directory with the
name of the archive, for example `x64a.rpf/`. When `a` adds a directory with
a name that ends in `.rpf`, it writes the directory as a nested archive. For
this reason, you can extract an archive with `-r` and then add the result
again, and you get the same layout.

`a` and `d` cannot change an entry inside a nested archive.

## Warnings

- The NG encryption of an entry depends on the name of the entry. Do not
  rename an entry in an NG archive.
- The NG encryption of the table of contents depends on the file name of the
  archive. Do not rename an NG archive file. The archive then does not open.
- `x` and `e` replace existing files with no prompt.
- `a` and `d` write the new archive to a temporary directory, and then
  replace the old archive. If one path fails, the tool does not write the
  archive.
- Make a copy of the game files before you change them.

## Limits

- Paths do not accept wildcards.
- The tool cannot get the keys from `GTA5.exe`. The function
  `rpf_keys_extract` in `rpf.h` can do this.

## License

unrage uses the ISC license. The CodeWalker parts of `rpf.h` keep the MIT
license of CodeWalker. Refer to `LICENSE`.

## Credit

The archive format and the cryptography in `rpf.h` come from
[CodeWalker](https://github.com/dexyfex/CodeWalker) by dexyfex. CodeWalker
uses the MIT license. The full license notice of CodeWalker is at the top of
`rpf.h`.
