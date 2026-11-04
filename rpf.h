/*
 * rpf.h - read, extract, create, and repack GTA 5 RPF7 archives.
 *
 * This is a single-file C99 library in the style of the stb libraries.
 * It handles OPEN, AES, and NG archives. It also extracts the keys from
 * GTA5.exe.
 *
 * Usage:
 *   In exactly one C file, define RPF_IMPLEMENTATION before the include.
 *
 *       #define RPF_IMPLEMENTATION
 *       #include "rpf.h"
 *
 *   In all other files, include rpf.h with no define.
 *
 * Example:
 *   This code reads one file of an archive. Then it writes a copy of
 *   the archive with two more files as an NG archive. The code does not
 *   show the checks of the return values.
 *
 *       RpfKeys *keys;
 *       RpfArchive *a;
 *       struct rpf_entry *e, *root, *dir;
 *       uint8_t *buf;
 *       size_t len;
 *
 *       rpf_keys_load(&keys, "keys");
 *       rpf_open(&a, "common.rpf", keys);
 *       e = rpf_find(a, "data/gta5_cache_y.dat");
 *       rpf_read(a, e, &buf, &len);
 *       fwrite(buf, 1, len, stdout);
 *       RPF_FREE(buf);
 *
 *       rpf_tree_from_archive(&root, a);
 *       dir = rpf_tree_mkdir(root, "mydata");
 *       rpf_tree_add_mem(dir, "note.txt", (const uint8_t *)"hi", 2);
 *       rpf_tree_add_path(dir, "note2.txt", "notes/note2.txt");
 *       rpf_write(root, "out/common.rpf", RPF_ENC_NG, keys);
 *       rpf_tree_free(root);
 *       rpf_close(a);
 *       rpf_keys_free(keys);
 *
 *   To make an archive with no source archive, start with
 *   rpf_tree_new. To make an archive from a directory on disk, use
 *   rpf_tree_from_dir. To extract all files, use rpf_extract_all.
 *
 * Keys:
 *   A key set is a folder of files. The file names are the names that
 *   CodeWalker uses, so a CodeWalker key dump folder works.
 *
 *       gtav_aes_key.dat              32 bytes
 *       gtav_ng_key.dat               101 * 272 bytes
 *       gtav_ng_decrypt_tables.dat    17 * 16 * 256 words
 *       gtav_ng_encrypt_tables.dat    17 * 16 * 256 words
 *       gtav_ng_encrypt_luts.dat      17 * 16 * 196608 bytes
 *       gtav_hash_lut.dat             256 bytes
 *
 *   A word has 32 bits in little-endian order. A missing file is not
 *   an error. A function that needs the missing part returns
 *   RPF_ENOKEY. An AES archive needs the AES key. To read an NG
 *   archive, the key set needs the NG keys, the NG decrypt tables, and
 *   the hash LUT. To write an NG archive, the key set needs the NG
 *   keys, the hash LUT, the NG encrypt tables, and the LUTs.
 *
 *   rpf_keys_extract finds key parts in GTA5.exe. Current Legacy
 *   executables have only the AES key and the hash LUT. For this
 *   reason, the NG keys and the NG decrypt tables must come from a key
 *   dump folder. The tool test/magic2keys.c makes this folder from the
 *   magic.dat file of CodeWalker. README.md tells how to use it. Or
 *   call rpf_keys_magic with the bytes of magic.dat to fill these parts
 *   in memory. It needs the AES key from rpf_keys_extract. Then
 *   call rpf_keys_derive_encrypt one time to make the NG encrypt tables
 *   and the LUTs. This takes some minutes. Save the result with
 *   rpf_keys_save.
 *
 * Scripts:
 *   A resource entry with a name that ends in ".ysc" has an encrypted
 *   body. No flag in the archive shows this. In an AES archive, the
 *   body uses AES. In all other archives, also in OPEN archives, the
 *   body uses NG. CodeWalker uses the same rule. For this reason, a
 *   .ysc entry in an OPEN archive needs the NG keys for read and for
 *   write.
 *
 * Large files:
 *   On Windows, the library uses _fseeki64 and _ftelli64. On other
 *   systems, it uses fseeko and ftello. On those systems, define
 *   _FILE_OFFSET_BITS 64 for the whole program, for example with the
 *   compiler flag -D_FILE_OFFSET_BITS=64. Then off_t has 64 bits and
 *   archives larger than 4 GB work.
 *
 *   On POSIX systems, the unit with RPF_IMPLEMENTATION must include
 *   rpf.h before all other headers. Then rpf.h can define
 *   _POSIX_C_SOURCE 200112L, which fseeko, ftello, opendir, readdir,
 *   stat, and mkdir need. If the unit cannot do this, it must define
 *   _POSIX_C_SOURCE (200112L or higher) and _FILE_OFFSET_BITS itself.
 *   On Windows, the unit with RPF_IMPLEMENTATION includes windows.h
 *   with WIN32_LEAN_AND_MEAN and NOMINMAX.
 *
 * Warning:
 *   The NG encryption of an archive entry depends on the name of the
 *   entry. Do not rename an entry in an NG archive. The entry then
 *   cannot be decrypted. The NG encryption of the table of contents
 *   depends on the file name of the archive. Do not rename an NG
 *   archive file. The archive then does not open.
 *
 * Credit:
 *   The archive format and the cryptography come from CodeWalker by
 *   dexyfex. CodeWalker uses the MIT license. This library ports parts
 *   of RpfFile.cs, GTACrypto.cs, and GTAKeys.cs.
 *
 *   The license notice of CodeWalker follows. It comes from the file
 *   Notice.txt of CodeWalker.
 *
 *     Copyright(c) 2017-2019 dexyfex
 *     Copyright(c) 2015 Neodymium
 *
 *     Permission is hereby granted, free of charge, to any person obtaining a
 *     copy of this software and associated documentation files (the
 *     "Software"), to deal in the Software without restriction, including
 *     without limitation the rights to use, copy, modify, merge, publish,
 *     distribute, sublicense, and/or sell copies of the Software, and to
 *     permit persons to whom the Software is furnished to do so, subject to
 *     the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *     OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *     MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *     TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *     SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef RPF_H
#define RPF_H

/*
 * These macros must come before the first system header. The Windows
 * macro stops MSVC from warning about fopen. The POSIX macro makes
 * fseeko and ftello visible in strict C99 mode. Only the unit with
 * RPF_IMPLEMENTATION defines the POSIX macro, so other units keep the
 * default feature macros of the system.
 */
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#elif defined(RPF_IMPLEMENTATION) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Error codes. Each function that can fail returns one of these. */
enum rpf_error {
	RPF_EIO = -1,		/* read, write, or seek failed */
	RPF_ENOMEM = -2,	/* memory allocation failed */
	RPF_EFORMAT = -3,	/* the data is not a valid archive */
	RPF_ENOKEY = -4,	/* the key material is missing */
	RPF_ECRYPT = -5,	/* decryption failed */
	RPF_ENOTFOUND = -6,	/* the entry does not exist */
	RPF_EINVAL = -7,	/* an argument is not valid */
	RPF_ETOOBIG = -8	/* a size is too big */
};

/* Return a static text that describes an error code. */
const char *rpf_strerror(int code);

/*
 * Decode the raw DEFLATE stream of inlen bytes at in. There is no zlib
 * header and there is no checksum. Allocate the output with RPF_MALLOC.
 * Set *out and *outlen. The caller frees *out with RPF_FREE. On an error,
 * set *out to NULL and *outlen to 0, and return RPF_EFORMAT or RPF_ENOMEM.
 */
int rpf_inflate(const uint8_t *in, size_t inlen, uint8_t **out,
    size_t *outlen);

/*
 * Encode the inlen bytes at in as a raw DEFLATE stream. There is no zlib
 * header and there is no checksum. The encoder uses fixed Huffman blocks
 * and stored blocks. Allocate the output with RPF_MALLOC. Set *out and
 * *outlen. The caller frees *out with RPF_FREE. On an error, set *out to
 * NULL and *outlen to 0, and return RPF_ENOMEM.
 */
int rpf_deflate(const uint8_t *in, size_t inlen, uint8_t **out,
    size_t *outlen);

/*
 * A key set. Each part of the key material is optional. A function
 * returns RPF_ENOKEY if it needs a part that the key set does not have.
 */
typedef struct rpf_keys RpfKeys;

/* Make an empty key set. Set *out. Return 0 or RPF_ENOMEM. */
int rpf_keys_new(RpfKeys **out);

/* Free a key set and all of its parts. keys can be NULL. */
void rpf_keys_free(RpfKeys *keys);

/*
 * Read the key files in the folder dir. A missing file leaves the
 * matching part empty. A file of the wrong size gives RPF_EFORMAT. If
 * the folder has none of the files, return RPF_ENOTFOUND. On an error,
 * set *out to NULL.
 */
int rpf_keys_load(RpfKeys **out, const char *dir);

/*
 * Write each part that the key set has to a file in the folder dir. The
 * folder must exist. Return 0, RPF_EIO, or RPF_ENOMEM.
 */
int rpf_keys_save(const RpfKeys *keys, const char *dir);

/*
 * Find the key material in the executable GTA5.exe at exepath. Make a
 * new key set. Fill each part that the executable has: the AES key, the
 * NG keys, the NG decrypt tables, and the hash LUT. A part that the
 * executable does not have stays NULL. A part with many pieces is
 * found only if the executable has all of its pieces. Current Legacy
 * executables have only the AES key and the hash LUT. For this reason,
 * ngkeys and dectables stay NULL. Read them from a key dump folder with
 * rpf_keys_load. A function that needs a missing part returns
 * RPF_ENOKEY later. Call progress, if not NULL, about once for each
 * 8 MB. Return 0 if the executable has at least one part. Return
 * RPF_ENOTFOUND if it has no part or if it does not open. Return
 * RPF_EIO or RPF_ENOMEM on a failure. On an error, set *out to NULL.
 */
int rpf_keys_extract(RpfKeys **out, const char *exepath,
    void (*progress)(const char *, void *), void *ud);

/*
 * Decode the len bytes at magic. This is the magic.dat resource of
 * CodeWalker. k must have the AES key. Fill the NG keys, the NG decrypt
 * tables, and the hash LUT of k. Replace a part that k has. Do not
 * change magic. Return 0, RPF_ENOKEY if k has no AES key, RPF_EFORMAT
 * if magic does not decode or a part does not agree with its SHA-1
 * digest, or RPF_ENOMEM. On an error, do not change k.
 */
int rpf_keys_magic(RpfKeys *k, const uint8_t *magic, size_t len);

/*
 * Derive the NG encrypt tables and LUTs from the decrypt tables. This
 * takes some time. Return 0, RPF_ENOKEY if the key set has no decrypt
 * tables, RPF_ENOMEM, or RPF_EINVAL.
 */
int rpf_keys_derive_encrypt(RpfKeys *k,
    void (*progress)(const char *, void *), void *ud);

/* An open archive. */
typedef struct rpf_archive RpfArchive;

/* The encryption tag in the archive header. NONE is 0. */
enum rpf_encryption {
	RPF_ENC_OPEN = 0x4E45504F,
	RPF_ENC_AES = 0x0FFFFFF9,
	RPF_ENC_NG = 0x0FEFFFFF
};

/* The kind of an entry. */
enum rpf_kind {
	RPF_KIND_DIR,
	RPF_KIND_BINARY,
	RPF_KIND_RESOURCE
};

/* The source of the data of an entry in a tree for writing. */
enum {
	RPF_SRC_NONE,
	RPF_SRC_ARCHIVE,
	RPF_SRC_PATH,
	RPF_SRC_MEM
};

/*
 * An entry of an archive or of a tree. The archive or the tree owns the
 * name and the children array. The children of a directory in an open
 * archive are in table order. rpf_write sorts them in the byte order
 * of the names as stored, with no change of case.
 */
struct rpf_entry {
	char *name;
	int kind;
	uint32_t size;		/* stored length */
	uint32_t usize;		/* uncompressed length (binary only) */
	uint32_t offset;	/* block offset in the source archive */
	int encrypted;		/* binary: flag; resource: name is .ysc */
	uint32_t sysflags;	/* resource only */
	uint32_t gfxflags;	/* resource only */
	struct rpf_entry *parent;
	struct rpf_entry **children;	/* directory only */
	size_t nchildren;
	int nested;		/* directory only: write as a .rpf entry */
	int src;		/* RPF_SRC_* */
	RpfArchive *srcarchive;
	char *srcpath;
	const uint8_t *srcmem;
	size_t srclen;
};

/*
 * Open the archive at path read-only and parse its table of contents.
 * keys can be NULL. Then AES and NG archives give RPF_ENOKEY. Set *out.
 * On an error, set *out to NULL. Return 0, RPF_ENOTFOUND if the file
 * does not open, RPF_EIO, RPF_ENOMEM, RPF_EFORMAT, RPF_ENOKEY,
 * RPF_ECRYPT if the table does not decrypt, or RPF_ETOOBIG. An NG
 * archive that has a new file name gives RPF_ECRYPT. More than 256
 * levels of directories give RPF_EFORMAT.
 */
int rpf_open(RpfArchive **out, const char *path, const RpfKeys *keys);

/* Close an archive and free its entries. a can be NULL. */
void rpf_close(RpfArchive *a);

/* Return the root directory entry of the archive. */
struct rpf_entry *rpf_root(RpfArchive *a);

/* Return the encryption tag of the archive, RPF_ENC_* or 0 for NONE. */
int rpf_encryption(const RpfArchive *a);

/*
 * Find the entry at path, for example "common/data/levels.xml". The
 * separators are '/' and '\'. The compare ignores ASCII case. Look in
 * this archive only, not in nested archives. Return NULL if no entry
 * matches.
 */
struct rpf_entry *rpf_find(RpfArchive *a, const char *path);

/*
 * Open the nested archive in the entry e of the archive parent. e must
 * be a binary entry that is stored without compression, with size 0. The
 * child archive shares the file and the key set of the parent. The
 * parent must stay open until the child is closed. Set *out. On an
 * error, set *out to NULL. Return 0, RPF_EINVAL if e is not such an
 * entry, RPF_EFORMAT if the data is not inside the parent or is not an
 * archive, or an error of rpf_open.
 */
int rpf_open_nested(RpfArchive **out, RpfArchive *parent,
    const struct rpf_entry *e);

/*
 * Read the stored bytes of the entry e exactly as they are in the
 * archive. The bytes can be compressed and encrypted. Allocate the
 * buffer with RPF_MALLOC. Set *out and *outlen. The caller frees *out
 * with RPF_FREE. On an error, set *out to NULL and *outlen to 0.
 * Return 0, RPF_EINVAL if e is a directory, RPF_EIO if the data is not
 * in the archive or the read fails, or RPF_ENOMEM.
 */
int rpf_read_raw(RpfArchive *a, const struct rpf_entry *e, uint8_t **out,
    size_t *outlen);

/*
 * Read the entry e in the export form. For a binary entry, this is the
 * plain bytes. For a resource, this is the RSC7 header and the
 * compressed body. The function makes the header from the entry, as
 * CodeWalker does, because the archive can hold other bytes there. Bits
 * 28 to 31 of sysflags give bits 4 to 7 of the version, and bits 28 to
 * 31 of gfxflags give bits 0 to 3. The bytes are not encrypted. A
 * binary entry with a compressed size that does not inflate to its
 * uncompressed size gives RPF_EFORMAT. A .ysc resource with a body that
 * does not inflate, or that inflates to more bytes than its page flags
 * give, gives RPF_ECRYPT. The buffer, *out and *outlen are as in
 * rpf_read_raw.
 * Note that a nested archive entry is read whole. To look inside it,
 * use rpf_open_nested. Return 0, RPF_EFORMAT, RPF_ENOKEY, RPF_ECRYPT,
 * or an error of rpf_read_raw.
 */
int rpf_read(RpfArchive *a, const struct rpf_entry *e, uint8_t **out,
    size_t *outlen);

/*
 * Write each file of the archive a in the export form under the
 * directory dir. Make dir and the directories of the archive as
 * necessary. If recurse is set, extract each nested .rpf archive into a
 * directory with its name, not into a file. A name in the archive that
 * has '/' or '\', or that has only dots and spaces, gives RPF_EFORMAT.
 * More than 256 levels of directories, with the nested archives, or more
 * than 32 levels of nested archives give RPF_EFORMAT. The function
 * checks the levels of each archive before it writes the files of that
 * archive. It copies a stored binary entry that is not encrypted in
 * parts, so a large nested archive is not read whole. The function
 * stops at the first error and does not remove the files that it wrote.
 * Return 0, RPF_EIO, RPF_ENOMEM, RPF_EFORMAT, or an error of rpf_read or
 * rpf_open_nested.
 */
int rpf_extract_all(RpfArchive *a, const char *dir, int recurse);

/*
 * Return the size in bytes of the memory that the page flags of a
 * resource describe. Return RPF_ETOOBIG if the size does not fit in an
 * int.
 */
int rpf_resource_size(uint32_t flags);

/*
 * A tree is a root directory entry and its children. The tree owns the
 * name, the children array, and the source path or the copy of the
 * source data of each entry. A name must not be empty, must not
 * contain '/' or '\', and must not be longer than 255 bytes. The name
 * compare ignores ASCII case. The parent or the entry given to a tree
 * function must be a node of a tree from rpf_tree_new. It must not be
 * an entry of an open archive.
 */

/* Make a new tree with an empty root directory. Return 0 or RPF_ENOMEM. */
int rpf_tree_new(struct rpf_entry **root);

/*
 * Make a new tree with a copy of the entries of the open archive a.
 * Each file of the tree gets its data from a. The archive must stay
 * open until rpf_write returns. rpf_open accepts 256 levels of
 * directories or fewer, so rpf_write accepts the tree. Return 0 or
 * RPF_ENOMEM.
 */
int rpf_tree_from_archive(struct rpf_entry **root, RpfArchive *a);

/*
 * Add a nested archive node name to parent, with a copy of the entries
 * of the open archive a. Each file of the node gets its data from a.
 * rpf_write writes the node as a .rpf entry with the encryption of the
 * new archive. An entry with the same name is replaced. Use this with
 * rpf_open_nested to change the encryption of a nested archive. The
 * archive must stay open until rpf_write returns. Return 0, RPF_EINVAL,
 * or RPF_ENOMEM.
 */
int rpf_tree_add_archive(struct rpf_entry *parent, const char *name,
    RpfArchive *a);

/*
 * Make a new tree with the files and directories under the directory
 * dir on disk. Each file of the tree gets its data from its path. A
 * subdirectory with a name that ends in ".rpf" becomes a nested archive
 * node: a directory with the nested flag set. rpf_write writes it as a
 * .rpf entry. On an error, free the tree and set *root to NULL. Return
 * 0, RPF_ENOTFOUND if a directory cannot be listed, RPF_EINVAL if dir
 * is empty or a name is not valid, or RPF_ENOMEM.
 */
int rpf_tree_from_dir(struct rpf_entry **root, const char *dir);

/*
 * Return the child directory name of parent. Make it if it does not
 * exist. parent must be a tree node, not an archive entry. Return NULL
 * if the name is not valid, if parent is not a directory, if a file
 * with the same name exists, or if memory is low.
 */
struct rpf_entry *rpf_tree_mkdir(struct rpf_entry *parent,
    const char *name);

/*
 * Add the file at path on disk to parent as name. rpf_write reads the
 * file. An entry with the same name is replaced. parent must be a tree
 * node, not an archive entry. Return 0, RPF_EINVAL, or RPF_ENOMEM.
 */
int rpf_tree_add_path(struct rpf_entry *parent, const char *name,
    const char *path);

/*
 * Add a copy of the len bytes at data to parent as name. An entry with
 * the same name is replaced. parent must be a tree node, not an archive
 * entry. Return 0, RPF_EINVAL, or RPF_ENOMEM.
 */
int rpf_tree_add_mem(struct rpf_entry *parent, const char *name,
    const uint8_t *data, size_t len);

/*
 * Remove the entry e from its parent and free it. e must be a tree
 * node, not an archive entry. Return 0, or RPF_EINVAL if e is a root.
 */
int rpf_tree_remove(struct rpf_entry *e);

/* Free a tree. root can be NULL. */
void rpf_tree_free(struct rpf_entry *root);

/*
 * Write the tree root as a new archive at path with the encryption
 * RPF_ENC_OPEN, RPF_ENC_AES, or RPF_ENC_NG. The data of each file that
 * starts with "RSC7" goes in as a resource. Other data goes in as a
 * binary entry. A file from an archive with the same encryption, or a
 * file that is not encrypted, goes in as it is stored. The NG table key
 * comes from the file name of path. Do not rename an NG archive. A
 * directory with the nested flag set goes in as a stored .rpf entry. The
 * function writes it first to the temporary file "<path>.<index>.tmp"
 * with the same encryption and keys, and then removes that file. keys
 * can be NULL for an OPEN archive with no .ysc entry. path must not be
 * the path of a source archive of the tree. If it is, return
 * RPF_EINVAL and do not change the file. The compare treats '/' and '\'
 * as equal, but it does not find two different paths to the same file.
 * A tree with more than 256 levels of directories, with the nested
 * archive nodes, gives RPF_EINVAL. On an error, remove the partial file.
 * Return 0, RPF_EINVAL, RPF_EIO, RPF_ENOMEM, RPF_ENOTFOUND if a source
 * file does not open, RPF_ETOOBIG, an error of rpf_read, or RPF_ENOKEY
 * if keys do not have the necessary parts. The function makes this
 * check before it makes the file.
 */
int rpf_write(const struct rpf_entry *root, const char *path,
    int encryption, const RpfKeys *keys);

#endif /* RPF_H */

#ifdef RPF_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

/* Replace these macros before the include to use another allocator. */
#ifndef RPF_MALLOC
#define RPF_MALLOC(n) malloc(n)
#endif
#ifndef RPF_REALLOC
#define RPF_REALLOC(p, n) realloc(p, n)
#endif
#ifndef RPF_FREE
#define RPF_FREE(p) free(p)
#endif

/*
 * Platform shims for directories. rpf__dir_open opens the directory
 * path for a listing. rpf__dir_next gives the next name, or NULL at the
 * end. rpf__is_dir gives 1 if path is a directory.
 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>

#define RPF__MKDIR(path) _mkdir(path)

struct rpf__dir {
	HANDLE h;
	WIN32_FIND_DATAA fd;
	int first;
};

static int
rpf__dir_open(struct rpf__dir *d, const char *path)
{
	char *pat;

	pat = RPF_MALLOC(strlen(path) + 3);
	if (pat == NULL)
		return RPF_ENOMEM;
	sprintf(pat, "%s/*", path);
	d->h = FindFirstFileA(pat, &d->fd);
	RPF_FREE(pat);
	if (d->h == INVALID_HANDLE_VALUE)
		return RPF_ENOTFOUND;
	d->first = 1;
	return 0;
}

static const char *
rpf__dir_next(struct rpf__dir *d)
{
	if (!d->first && !FindNextFileA(d->h, &d->fd))
		return NULL;
	d->first = 0;
	return d->fd.cFileName;
}

static void
rpf__dir_close(struct rpf__dir *d)
{
	FindClose(d->h);
}

static int
rpf__is_dir(const char *path)
{
	DWORD at;

	at = GetFileAttributesA(path);
	return at != INVALID_FILE_ATTRIBUTES &&
	    (at & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#else
#include <dirent.h>
#include <sys/stat.h>

#define RPF__MKDIR(path) mkdir(path, 0755)

struct rpf__dir {
	DIR *h;
};

static int
rpf__dir_open(struct rpf__dir *d, const char *path)
{
	d->h = opendir(path);
	return d->h == NULL ? RPF_ENOTFOUND : 0;
}

static const char *
rpf__dir_next(struct rpf__dir *d)
{
	struct dirent *de;

	de = readdir(d->h);
	return de == NULL ? NULL : de->d_name;
}

static void
rpf__dir_close(struct rpf__dir *d)
{
	closedir(d->h);
}

/* Use stat, because some file systems do not set d_type. */
static int
rpf__is_dir(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

/*
 * Make the directory path. Return 0, also if the directory exists.
 * Return RPF_EIO if the directory is not made.
 */
static int
rpf__mkdir(const char *path)
{
	if (RPF__MKDIR(path) == 0 || rpf__is_dir(path))
		return 0;
	return RPF_EIO;
}

/*
 * Make the directory path and each directory above it. The separators
 * are '/' and '\'. Return 0, RPF_ENOMEM, or RPF_EIO.
 */
static int
rpf__mkdir_p(const char *path)
{
	char *p;
	size_t i;
	int err;

	p = RPF_MALLOC(strlen(path) + 1);
	if (p == NULL)
		return RPF_ENOMEM;
	strcpy(p, path);
	err = 0;
	for (i = 1; err == 0 && p[i - 1] != '\0' && p[i] != '\0'; i++) {
		/* Make each part once, at the first separator after it. */
		if ((p[i] != '/' && p[i] != '\\') ||
		    p[i - 1] == '/' || p[i - 1] == '\\')
			continue;
		p[i] = '\0';
		err = rpf__mkdir(p);
		p[i] = path[i];
	}
	if (err == 0)
		err = rpf__mkdir(p);
	RPF_FREE(p);
	return err;
}

/* Move the file position to the byte offset off. Return 0 or RPF_EIO. */
static int
rpf__seek(FILE *f, int64_t off)
{
#ifdef _WIN32
	if (_fseeki64(f, off, SEEK_SET) != 0)
#else
	/* A 32-bit off_t cannot hold all offsets. */
	if ((int64_t)(off_t)off != off)
		return RPF_EIO;
	if (fseeko(f, (off_t)off, SEEK_SET) != 0)
#endif
		return RPF_EIO;
	return 0;
}

/* Return the file position in bytes, or -1 on error. */
static int64_t
rpf__tell(FILE *f)
{
#ifdef _WIN32
	return _ftelli64(f);
#else
	return (int64_t)ftello(f);
#endif
}

/* SHA-1 hash, as in FIPS 180-1. */

/* Rotate the 32-bit value x left by n bits. n is from 1 to 31. */
static uint32_t
rpf__rol32(uint32_t x, int n)
{
	return (x << n) | (x >> (32 - n));
}

/* Add one 64-byte block to the state h of five words. */
static void
rpf__sha1_block(uint32_t h[5], const uint8_t *p)
{
	uint32_t w[80], a, b, c, d, e, f, k, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 |
		    (uint32_t)p[4 * i + 1] << 16 |
		    (uint32_t)p[4 * i + 2] << 8 | (uint32_t)p[4 * i + 3];
	for (i = 16; i < 80; i++)
		w[i] = rpf__rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^
		    w[i - 16], 1);
	a = h[0];
	b = h[1];
	c = h[2];
	d = h[3];
	e = h[4];
	for (i = 0; i < 80; i++) {
		if (i < 20) {
			f = (b & c) | (~b & d);
			k = 0x5a827999UL;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1UL;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdcUL;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6UL;
		}
		t = rpf__rol32(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = rpf__rol32(b, 30);
		b = a;
		a = t;
	}
	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
}

/* Write the SHA-1 digest of len bytes at data to digest. */
static void
rpf__sha1(const uint8_t *data, size_t len, uint8_t digest[20])
{
	uint32_t h[5];
	uint8_t tail[128];
	uint64_t bits;
	size_t full, rest, n;
	int i;

	h[0] = 0x67452301UL;
	h[1] = 0xefcdab89UL;
	h[2] = 0x98badcfeUL;
	h[3] = 0x10325476UL;
	h[4] = 0xc3d2e1f0UL;
	full = len - len % 64;
	for (n = 0; n < full; n += 64)
		rpf__sha1_block(h, data + n);
	/* Copy the last bytes. Add the 0x80 byte and zero bytes. */
	rest = len - full;
	memset(tail, 0, sizeof(tail));
	if (rest > 0)
		memcpy(tail, data + full, rest);
	tail[rest] = 0x80;
	n = rest < 56 ? 64 : 128;
	/* End the padding with the length in bits, as a big-endian value. */
	bits = (uint64_t)len * 8;
	for (i = 0; i < 8; i++)
		tail[n - 1 - i] = (uint8_t)(bits >> (8 * i));
	rpf__sha1_block(h, tail);
	if (n == 128)
		rpf__sha1_block(h, tail + 64);
	for (i = 0; i < 20; i++)
		digest[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
}

/* AES-256 in ECB mode, as in FIPS-197. */

/* The S-box and the inverse S-box. */
static const uint8_t rpf__sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
	0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
	0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
	0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
	0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
	0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
	0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
	0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
	0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
	0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
	0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
	0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t rpf__isbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
	0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
	0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
	0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
	0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
	0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
	0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
	0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
	0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
	0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
	0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
	0x55, 0x21, 0x0c, 0x7d
};

/* Multiply x by 2 in the field GF(2^8). */
static uint8_t
rpf__xtime(uint8_t x)
{
	return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1b));
}

/* Multiply x by y in the field GF(2^8). */
static uint8_t
rpf__gmul(uint8_t x, uint8_t y)
{
	uint8_t r = 0;

	while (y != 0) {
		if (y & 1)
			r ^= x;
		x = rpf__xtime(x);
		y >>= 1;
	}
	return r;
}

/* Expand the 32-byte key to 15 round keys of 16 bytes (60 words). */
static void
rpf__aes_expand(const uint8_t key[32], uint8_t rk[240])
{
	uint8_t t[4], rcon = 1, x;
	int i, j;

	memcpy(rk, key, 32);
	for (i = 8; i < 60; i++) {
		memcpy(t, rk + 4 * (i - 1), 4);
		if (i % 8 == 0) {
			/* Rotate the word. Use the S-box and the constant. */
			x = t[0];
			t[0] = (uint8_t)(rpf__sbox[t[1]] ^ rcon);
			t[1] = rpf__sbox[t[2]];
			t[2] = rpf__sbox[t[3]];
			t[3] = rpf__sbox[x];
			rcon = rpf__xtime(rcon);
		} else if (i % 8 == 4) {
			for (j = 0; j < 4; j++)
				t[j] = rpf__sbox[t[j]];
		}
		for (j = 0; j < 4; j++)
			rk[4 * i + j] = (uint8_t)(rk[4 * (i - 8) + j] ^ t[j]);
	}
}

/* Add the round key to the state. */
static void
rpf__aes_addkey(uint8_t s[16], const uint8_t *rk)
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] ^= rk[i];
}

/* Replace each byte of the state with its value in the box. */
static void
rpf__aes_sub(uint8_t s[16], const uint8_t box[256])
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] = box[s[i]];
}

/*
 * Rotate row r of the state left by r bytes, or right if inv is set.
 * The state is column by column, so the byte of row r, column c is at
 * index 4 * c + r.
 */
static void
rpf__aes_shift(uint8_t s[16], int inv)
{
	uint8_t t[16];
	int r, c;

	memcpy(t, s, 16);
	for (c = 0; c < 4; c++)
		for (r = 0; r < 4; r++)
			if (inv)
				s[4 * ((c + r) % 4) + r] = t[4 * c + r];
			else
				s[4 * c + r] = t[4 * ((c + r) % 4) + r];
}

/* Mix each column of the state. Use the inverse matrix if inv is set. */
static void
rpf__aes_mix(uint8_t s[16], int inv)
{
	static const uint8_t fwd[4] = { 2, 3, 1, 1 };
	static const uint8_t rev[4] = { 14, 11, 13, 9 };
	const uint8_t *m = inv ? rev : fwd;
	uint8_t a[4];
	int c, r, k;

	for (c = 0; c < 4; c++) {
		memcpy(a, s + 4 * c, 4);
		for (r = 0; r < 4; r++) {
			s[4 * c + r] = 0;
			for (k = 0; k < 4; k++)
				s[4 * c + r] ^=
				    rpf__gmul(a[(r + k) % 4], m[k]);
		}
	}
}

/* Encrypt len bytes at buf in place with AES-256 ECB. Skip a partial block. */
static void
rpf__aes_encrypt(const uint8_t key[32], uint8_t *buf, size_t len)
{
	uint8_t rk[240];
	size_t n;
	int round;

	rpf__aes_expand(key, rk);
	for (n = 0; n + 16 <= len; n += 16) {
		rpf__aes_addkey(buf + n, rk);
		for (round = 1; round < 14; round++) {
			rpf__aes_sub(buf + n, rpf__sbox);
			rpf__aes_shift(buf + n, 0);
			rpf__aes_mix(buf + n, 0);
			rpf__aes_addkey(buf + n, rk + 16 * round);
		}
		rpf__aes_sub(buf + n, rpf__sbox);
		rpf__aes_shift(buf + n, 0);
		rpf__aes_addkey(buf + n, rk + 16 * 14);
	}
}

/* Decrypt len bytes at buf in place with AES-256 ECB. Skip a partial block. */
static void
rpf__aes_decrypt(const uint8_t key[32], uint8_t *buf, size_t len)
{
	uint8_t rk[240];
	size_t n;
	int round;

	rpf__aes_expand(key, rk);
	for (n = 0; n + 16 <= len; n += 16) {
		rpf__aes_addkey(buf + n, rk + 16 * 14);
		for (round = 13; round > 0; round--) {
			rpf__aes_shift(buf + n, 1);
			rpf__aes_sub(buf + n, rpf__isbox);
			rpf__aes_addkey(buf + n, rk + 16 * round);
			rpf__aes_mix(buf + n, 1);
		}
		rpf__aes_shift(buf + n, 1);
		rpf__aes_sub(buf + n, rpf__isbox);
		rpf__aes_addkey(buf + n, rk);
	}
}

/* Raw DEFLATE decoder, as in RFC 1951. */

/* The state of the decoder: the input, the bit buffer, and the output. */
struct rpf__inflate {
	const uint8_t *in;
	size_t inlen;
	size_t pos;		/* the next input byte to read */
	uint32_t bitbuf;	/* input bits, the first bit is bit 0 */
	int bitcnt;		/* the number of bits in bitbuf */
	uint8_t *out;
	size_t outlen;
	size_t outcap;
	size_t max;		/* the largest permitted outlen */
};

/* A canonical Huffman code. The symbols are in the order of the codes. */
struct rpf__huff {
	uint16_t count[16];	/* the number of codes of each length */
	uint16_t symbol[288];
};

/* The base values and the extra bits of the length and distance codes. */
static const uint16_t rpf__lbase[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
	59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t rpf__lext[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
	4, 5, 5, 5, 5, 0
};
static const uint16_t rpf__dbase[30] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
	513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t rpf__dext[30] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
	10, 11, 11, 12, 12, 13, 13
};

/* Read need bits (0 to 16). Return the value, or RPF_EFORMAT at the end. */
static int
rpf__inf_bits(struct rpf__inflate *s, int need)
{
	int val;

	while (s->bitcnt < need) {
		if (s->pos >= s->inlen)
			return RPF_EFORMAT;
		s->bitbuf |= (uint32_t)s->in[s->pos++] << s->bitcnt;
		s->bitcnt += 8;
	}
	val = (int)(s->bitbuf & ((1UL << need) - 1));
	s->bitbuf >>= need;
	s->bitcnt -= need;
	return val;
}

/*
 * Make room in the output for n more bytes. Return 0, RPF_EFORMAT if the
 * output then has more than s->max bytes, or RPF_ENOMEM.
 */
static int
rpf__inf_room(struct rpf__inflate *s, size_t n)
{
	size_t cap = s->outcap;
	uint8_t *p;

	if (n > (size_t)-1 - s->outlen)
		return RPF_ENOMEM;
	if (s->outlen + n > s->max)
		return RPF_EFORMAT;
	while (cap < s->outlen + n) {
		if (cap > (size_t)-1 / 2)
			return RPF_ENOMEM;
		cap *= 2;
		if (cap > s->max)
			cap = s->max;
	}
	if (cap != s->outcap) {
		p = RPF_REALLOC(s->out, cap);
		if (p == NULL)
			return RPF_ENOMEM;
		s->out = p;
		s->outcap = cap;
	}
	return 0;
}

/*
 * Build the table h for the n code lengths. A length of 0 means that the
 * symbol has no code. Return RPF_EFORMAT if the codes are too many. Also
 * return RPF_EFORMAT if the codes are not complete, but allow a code set
 * with one code when partial is set.
 */
static int
rpf__huff_build(struct rpf__huff *h, const uint8_t *length, int n,
    int partial)
{
	uint16_t offs[16];
	int sym, len, left;

	memset(h->count, 0, sizeof(h->count));
	for (sym = 0; sym < n; sym++)
		h->count[length[sym]]++;
	left = 1;
	for (len = 1; len < 16; len++) {
		left = (left << 1) - h->count[len];
		if (left < 0)
			return RPF_EFORMAT;
	}
	if (left > 0 && !(partial && n == h->count[0] + h->count[1]))
		return RPF_EFORMAT;
	offs[1] = 0;
	for (len = 1; len < 15; len++)
		offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
	for (sym = 0; sym < n; sym++)
		if (length[sym] != 0)
			h->symbol[offs[length[sym]]++] = (uint16_t)sym;
	return 0;
}

/* Read one symbol with the table h. Return it, or RPF_EFORMAT. */
static int
rpf__inf_decode(struct rpf__inflate *s, const struct rpf__huff *h)
{
	int len, bit, code = 0, first = 0, index = 0, count;

	for (len = 1; len < 16; len++) {
		bit = rpf__inf_bits(s, 1);
		if (bit < 0)
			return bit;
		code |= bit;
		count = h->count[len];
		if (code - count < first)
			return h->symbol[index + (code - first)];
		index += count;
		first = (first + count) << 1;
		code <<= 1;
	}
	return RPF_EFORMAT;
}

/* Decode the symbols of one block up to the end code. */
static int
rpf__inf_codes(struct rpf__inflate *s, const struct rpf__huff *lit,
    const struct rpf__huff *dist)
{
	int sym, bits, err;
	size_t len, off;

	for (;;) {
		sym = rpf__inf_decode(s, lit);
		if (sym < 0)
			return sym;
		if (sym == 256)
			return 0;
		if (sym < 256) {
			err = rpf__inf_room(s, 1);
			if (err != 0)
				return err;
			s->out[s->outlen++] = (uint8_t)sym;
			continue;
		}
		/* A length code. The codes 286 and 287 are not valid. */
		sym -= 257;
		if (sym >= 29)
			return RPF_EFORMAT;
		bits = rpf__inf_bits(s, rpf__lext[sym]);
		if (bits < 0)
			return bits;
		len = (size_t)rpf__lbase[sym] + (size_t)bits;
		/* A distance code. The codes 30 and 31 are not valid. */
		sym = rpf__inf_decode(s, dist);
		if (sym < 0)
			return sym;
		if (sym >= 30)
			return RPF_EFORMAT;
		bits = rpf__inf_bits(s, rpf__dext[sym]);
		if (bits < 0)
			return bits;
		off = (size_t)rpf__dbase[sym] + (size_t)bits;
		if (off > s->outlen)
			return RPF_EFORMAT;
		err = rpf__inf_room(s, len);
		if (err != 0)
			return err;
		/* Copy byte by byte. The source can overlap the target. */
		for (; len > 0; len--, s->outlen++)
			s->out[s->outlen] = s->out[s->outlen - off];
	}
}

/* Copy a stored block. */
static int
rpf__inf_stored(struct rpf__inflate *s)
{
	size_t len, nlen;
	int err;

	/* Drop the unused bits of the current byte. */
	s->bitbuf = 0;
	s->bitcnt = 0;
	if (s->inlen - s->pos < 4)
		return RPF_EFORMAT;
	len = (size_t)s->in[s->pos] | (size_t)s->in[s->pos + 1] << 8;
	nlen = (size_t)s->in[s->pos + 2] | (size_t)s->in[s->pos + 3] << 8;
	if (len != (~nlen & 0xffff))
		return RPF_EFORMAT;
	s->pos += 4;
	if (s->inlen - s->pos < len)
		return RPF_EFORMAT;
	err = rpf__inf_room(s, len);
	if (err != 0)
		return err;
	memcpy(s->out + s->outlen, s->in + s->pos, len);
	s->pos += len;
	s->outlen += len;
	return 0;
}

/* Decode a block with the fixed codes. */
static int
rpf__inf_fixed(struct rpf__inflate *s)
{
	struct rpf__huff lit, dist;
	uint8_t length[288];
	int i;

	for (i = 0; i < 144; i++)
		length[i] = 8;
	for (; i < 256; i++)
		length[i] = 9;
	for (; i < 280; i++)
		length[i] = 7;
	for (; i < 288; i++)
		length[i] = 8;
	if (rpf__huff_build(&lit, length, 288, 0) != 0)
		return RPF_EFORMAT;
	for (i = 0; i < 32; i++)
		length[i] = 5;
	if (rpf__huff_build(&dist, length, 32, 0) != 0)
		return RPF_EFORMAT;
	return rpf__inf_codes(s, &lit, &dist);
}

/* Decode a block with the codes that the block defines. */
static int
rpf__inf_dynamic(struct rpf__inflate *s)
{
	static const uint8_t order[19] = {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1,
		15
	};
	struct rpf__huff lencode, lit, dist;
	uint8_t length[320];
	int nlen, ndist, ncode, i, sym, rep, val;

	nlen = rpf__inf_bits(s, 5);
	ndist = rpf__inf_bits(s, 5);
	ncode = rpf__inf_bits(s, 4);
	if (nlen < 0 || ndist < 0 || ncode < 0)
		return RPF_EFORMAT;
	nlen += 257;
	ndist += 1;
	ncode += 4;
	if (nlen > 286 || ndist > 30)
		return RPF_EFORMAT;
	/* Read the lengths of the codes that code the code lengths. */
	memset(length, 0, 19);
	for (i = 0; i < ncode; i++) {
		val = rpf__inf_bits(s, 3);
		if (val < 0)
			return val;
		length[order[i]] = (uint8_t)val;
	}
	if (rpf__huff_build(&lencode, length, 19, 0) != 0)
		return RPF_EFORMAT;
	/* Read the code lengths of both tables as one list. */
	for (i = 0; i < nlen + ndist; ) {
		sym = rpf__inf_decode(s, &lencode);
		if (sym < 0)
			return sym;
		if (sym < 16) {
			length[i++] = (uint8_t)sym;
			continue;
		}
		val = 0;
		if (sym == 16) {
			if (i == 0)
				return RPF_EFORMAT;
			val = length[i - 1];
			rep = rpf__inf_bits(s, 2);
			if (rep >= 0)
				rep += 3;
		} else if (sym == 17) {
			rep = rpf__inf_bits(s, 3);
			if (rep >= 0)
				rep += 3;
		} else {
			rep = rpf__inf_bits(s, 7);
			if (rep >= 0)
				rep += 11;
		}
		if (rep < 0 || rep > nlen + ndist - i)
			return RPF_EFORMAT;
		while (rep-- > 0)
			length[i++] = (uint8_t)val;
	}
	/* The block must have a code for the end of the block. */
	if (length[256] == 0)
		return RPF_EFORMAT;
	if (rpf__huff_build(&lit, length, nlen, 1) != 0)
		return RPF_EFORMAT;
	if (rpf__huff_build(&dist, length + nlen, ndist, 1) != 0)
		return RPF_EFORMAT;
	return rpf__inf_codes(s, &lit, &dist);
}

/*
 * Decode as rpf_inflate. Return RPF_EFORMAT if the output has more than
 * max bytes. The check comes before the output grows, so bad data
 * cannot make a large allocation.
 */
static int
rpf__inflate_max(const uint8_t *in, size_t inlen, uint8_t **out,
    size_t *outlen, size_t max)
{
	struct rpf__inflate s;
	int last, type, err;

	*out = NULL;
	*outlen = 0;
	s.in = in;
	s.inlen = inlen;
	s.pos = 0;
	s.bitbuf = 0;
	s.bitcnt = 0;
	s.outlen = 0;
	s.outcap = 256;
	s.max = max;
	s.out = RPF_MALLOC(s.outcap);
	if (s.out == NULL)
		return RPF_ENOMEM;
	do {
		last = rpf__inf_bits(&s, 1);
		type = rpf__inf_bits(&s, 2);
		if (last < 0 || type < 0)
			err = RPF_EFORMAT;
		else if (type == 0)
			err = rpf__inf_stored(&s);
		else if (type == 1)
			err = rpf__inf_fixed(&s);
		else if (type == 2)
			err = rpf__inf_dynamic(&s);
		else
			err = RPF_EFORMAT;
		if (err != 0) {
			RPF_FREE(s.out);
			return err;
		}
	} while (!last);
	*out = s.out;
	*outlen = s.outlen;
	return 0;
}

int
rpf_inflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen)
{
	return rpf__inflate_max(in, inlen, out, outlen, SIZE_MAX);
}

/* Raw DEFLATE encoder, as in RFC 1951. */

/* The window size, the hash size, and the longest chain walk. */
#define RPF__DEF_WSIZE 32768
#define RPF__DEF_HBITS 15
#define RPF__DEF_CHAIN 64

/* The size of one block. A stored block holds 65535 bytes at most. */
#define RPF__DEF_BLOCK 65535

/*
 * The state of the encoder: the input, the bit buffer, the output, and
 * the hash chains. A hash chain value is a position plus 1. Zero means
 * that there is no position.
 */
struct rpf__deflate {
	const uint8_t *in;
	size_t inlen;
	uint8_t *out;
	size_t outlen;
	size_t outcap;
	uint32_t bitbuf;	/* output bits, the first bit is bit 0 */
	int bitcnt;		/* the number of bits in bitbuf */
	int err;		/* zero, or the first error */
	size_t *head;		/* the newest position of each hash */
	size_t *prev;		/* the older position with the same hash */
};

/* Append one byte to the output. Set s->err if the memory is not enough. */
static void
rpf__def_byte(struct rpf__deflate *s, unsigned int b)
{
	uint8_t *p;

	if (s->err != 0)
		return;
	if (s->outlen == s->outcap) {
		p = RPF_REALLOC(s->out, s->outcap * 2);
		if (p == NULL) {
			s->err = RPF_ENOMEM;
			return;
		}
		s->out = p;
		s->outcap *= 2;
	}
	s->out[s->outlen++] = (uint8_t)b;
}

/* Write the low n bits (0 to 16) of val. The first bit is the low bit. */
static void
rpf__def_bits(struct rpf__deflate *s, unsigned int val, int n)
{
	s->bitbuf |= (uint32_t)val << s->bitcnt;
	s->bitcnt += n;
	while (s->bitcnt >= 8) {
		rpf__def_byte(s, s->bitbuf & 0xff);
		s->bitbuf >>= 8;
		s->bitcnt -= 8;
	}
}

/* Write a Huffman code of n bits. The first bit is the high bit. */
static void
rpf__def_code(struct rpf__deflate *s, unsigned int code, int n)
{
	unsigned int rev = 0;
	int i;

	for (i = 0; i < n; i++)
		rev |= ((code >> i) & 1) << (n - 1 - i);
	rpf__def_bits(s, rev, n);
}

/* Write the fixed Huffman code of the literal or length symbol sym. */
static void
rpf__def_sym(struct rpf__deflate *s, int sym)
{
	if (sym < 144)
		rpf__def_code(s, 0x30 + sym, 8);
	else if (sym < 256)
		rpf__def_code(s, 0x190 + sym - 144, 9);
	else if (sym < 280)
		rpf__def_code(s, sym - 256, 7);
	else
		rpf__def_code(s, 0xc0 + sym - 280, 8);
}

/* Add the position i to the hash chains. Skip the end of the input. */
static void
rpf__def_insert(struct rpf__deflate *s, size_t i)
{
	uint32_t h;

	if (i + 3 > s->inlen)
		return;
	h = ((uint32_t)s->in[i] << 16) | ((uint32_t)s->in[i + 1] << 8) |
	    s->in[i + 2];
	h = (h * 2654435761u) >> (32 - RPF__DEF_HBITS);
	s->prev[i % RPF__DEF_WSIZE] = s->head[h];
	s->head[h] = i + 1;
}

/*
 * Find the longest match for the position i. The match must end before
 * end. Return its length, or 0 if it is shorter than 3 bytes. Set *dist.
 */
static size_t
rpf__def_match(struct rpf__deflate *s, size_t i, size_t end, size_t *dist)
{
	const uint8_t *in = s->in;
	size_t max, len, best = 0, cand, c;
	uint32_t h;
	int steps;

	max = end - i;
	if (max > 258)
		max = 258;
	h = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
	h = (h * 2654435761u) >> (32 - RPF__DEF_HBITS);
	cand = s->head[h];
	for (steps = 0; cand != 0 && steps < RPF__DEF_CHAIN; steps++) {
		c = cand - 1;
		if (i - c > RPF__DEF_WSIZE)
			break;
		len = 0;
		while (len < max && in[c + len] == in[i + len])
			len++;
		if (len > best) {
			best = len;
			*dist = i - c;
			if (len == max)
				break;
		}
		cand = s->prev[c % RPF__DEF_WSIZE];
		/* The slot can hold a newer position. Stop in that case. */
		if (cand != 0 && cand - 1 >= c)
			break;
	}
	/* A far match of 3 bytes is longer in bits than 3 literals. */
	if (best < 3 || (best == 3 && *dist > 4096))
		return 0;
	return best;
}

/* Write the bytes from pos to end as one fixed Huffman block. */
static void
rpf__def_fixed(struct rpf__deflate *s, size_t pos, size_t end, int last)
{
	size_t i = pos, len, dist = 0, j;
	int k;

	rpf__def_bits(s, (unsigned int)last, 1);
	rpf__def_bits(s, 1, 2);
	while (i < end) {
		len = 0;
		if (end - i >= 3)
			len = rpf__def_match(s, i, end, &dist);
		if (len == 0) {
			rpf__def_sym(s, s->in[i]);
			rpf__def_insert(s, i);
			i++;
			continue;
		}
		for (k = 28; rpf__lbase[k] > len; k--)
			;
		rpf__def_sym(s, 257 + k);
		rpf__def_bits(s, (unsigned int)(len - rpf__lbase[k]),
		    rpf__lext[k]);
		for (k = 29; rpf__dbase[k] > dist; k--)
			;
		rpf__def_code(s, (unsigned int)k, 5);
		rpf__def_bits(s, (unsigned int)(dist - rpf__dbase[k]),
		    rpf__dext[k]);
		for (j = 0; j < len; j++)
			rpf__def_insert(s, i + j);
		i += len;
	}
	rpf__def_sym(s, 256);
}

/* Write the bytes from pos to end as one stored block. */
static void
rpf__def_stored(struct rpf__deflate *s, size_t pos, size_t end, int last)
{
	size_t n = end - pos, i;

	rpf__def_bits(s, (unsigned int)last, 1);
	rpf__def_bits(s, 0, 2);
	if (s->bitcnt > 0)
		rpf__def_bits(s, 0, 8 - s->bitcnt);
	rpf__def_bits(s, (unsigned int)n, 16);
	rpf__def_bits(s, (unsigned int)(~n & 0xffff), 16);
	for (i = pos; i < end; i++)
		rpf__def_byte(s, s->in[i]);
}

int
rpf_deflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen)
{
	struct rpf__deflate s;
	size_t pos = 0, end, mark, n;
	uint32_t bits;
	int last, cnt, pad;

	*out = NULL;
	*outlen = 0;
	s.in = in;
	s.inlen = inlen;
	s.outlen = 0;
	s.outcap = 256;
	s.bitbuf = 0;
	s.bitcnt = 0;
	s.err = 0;
	s.out = RPF_MALLOC(s.outcap);
	s.head = RPF_MALLOC(((size_t)1 << RPF__DEF_HBITS) * sizeof(size_t));
	s.prev = RPF_MALLOC(RPF__DEF_WSIZE * sizeof(size_t));
	if (s.out == NULL || s.head == NULL || s.prev == NULL) {
		RPF_FREE(s.out);
		RPF_FREE(s.head);
		RPF_FREE(s.prev);
		return RPF_ENOMEM;
	}
	memset(s.head, 0, ((size_t)1 << RPF__DEF_HBITS) * sizeof(size_t));
	do {
		end = inlen - pos > RPF__DEF_BLOCK ? pos + RPF__DEF_BLOCK :
		    inlen;
		last = end == inlen;
		n = end - pos;
		mark = s.outlen;
		bits = s.bitbuf;
		cnt = s.bitcnt;
		rpf__def_fixed(&s, pos, end, last);
		/* Compare the sizes in bits. Use the smaller block. */
		pad = (8 - (cnt + 3) % 8) % 8;
		if ((s.outlen - mark) * 8 + (size_t)s.bitcnt - (size_t)cnt >=
		    (size_t)(3 + pad + 32) + n * 8) {
			s.outlen = mark;
			s.bitbuf = bits;
			s.bitcnt = cnt;
			rpf__def_stored(&s, pos, end, last);
		}
		pos = end;
	} while (!last && s.err == 0);
	if (s.err == 0 && s.bitcnt > 0)
		rpf__def_bits(&s, 0, 8 - s.bitcnt);
	RPF_FREE(s.head);
	RPF_FREE(s.prev);
	if (s.err != 0) {
		RPF_FREE(s.out);
		return s.err;
	}
	*out = s.out;
	*outlen = s.outlen;
	return 0;
}

/* Hash the name with the lookup table. All arithmetic wraps at 32 bits. */
static uint32_t
rpf__ng_hash(const char *name, const uint8_t lut[256])
{
	uint32_t h = 0, t;

	for (; *name != '\0'; name++) {
		t = 1025u * ((uint32_t)lut[(uint8_t)*name] + h);
		h = (t >> 6) ^ t;
	}
	return 32769u * (((9u * h) >> 11) ^ (9u * h));
}

/* Return the index of the NG key for the hash and the length. */
static uint32_t
rpf__ng_keyidx(uint32_t hash, uint32_t length)
{
	return (uint32_t)(hash + length + 61u) % 101u;
}

/*
 * The byte of the block that each table of a round reads. Table 4i + m
 * of round A reads byte 4i + m. Round B reads these bytes instead.
 */
static const uint8_t rpf__ng_sel_a[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};
static const uint8_t rpf__ng_sel_b[16] = {
	0, 7, 10, 13, 1, 4, 11, 14, 2, 5, 8, 15, 3, 6, 9, 12
};

/* Decrypt one block in place. The tables of round r start at r * 4096. */
static void
rpf__ng_decrypt_block(uint8_t blk[16], const uint32_t key[68],
    const uint32_t *tables)
{
	const uint32_t *t;
	const uint8_t *sel;
	uint32_t x[4];
	int r, i, m, n;

	for (r = 0; r < 17; r++) {
		t = tables + (size_t)r * 16 * 256;
		sel = (r < 2 || r == 16) ? rpf__ng_sel_a : rpf__ng_sel_b;
		for (i = 0; i < 4; i++) {
			x[i] = key[r * 4 + i];
			for (m = 0; m < 4; m++) {
				n = sel[4 * i + m];
				x[i] ^= t[n * 256 + blk[n]];
			}
		}
		for (i = 0; i < 16; i++)
			blk[i] = (uint8_t)(x[i / 4] >> ((i % 4) * 8));
	}
}

/* Decrypt len bytes at buf in place. Skip a partial block at the end. */
static void
rpf__ng_decrypt(uint8_t *buf, size_t len, const uint8_t key[272],
    const uint32_t *tables)
{
	uint32_t k[68];
	size_t i;

	for (i = 0; i < 68; i++)
		k[i] = (uint32_t)key[4 * i] | (uint32_t)key[4 * i + 1] << 8 |
		    (uint32_t)key[4 * i + 2] << 16 |
		    (uint32_t)key[4 * i + 3] << 24;
	for (; len >= 16; buf += 16, len -= 16)
		rpf__ng_decrypt_block(buf, k, tables);
}

/*
 * An NG encrypt LUT. It gives one byte of the inverse of one round B
 * word. The layout is the layout of the file on disk.
 */
struct rpf_nglut {
	uint8_t lut0[256][256];
	uint8_t lut1[256][256];
	uint8_t indices[65536];
};

/* Return the byte that the LUT gives for the word v. */
static uint8_t
rpf__nglut_lookup(const struct rpf_nglut *l, uint32_t v)
{
	return l->lut0[l->lut1[l->indices[v >> 16]][(v >> 8) & 0xFF]]
	    [v & 0xFF];
}

/* The word of the block that each LUT of an encrypt round B reads. */
static const uint8_t rpf__ng_word_b[16] = {
	0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2
};

/*
 * Encrypt one block in place. Run the rounds in the order 16 to 0.
 * Rounds 0, 1, and 16 use the encrypt tables. The other rounds use the
 * LUTs of round r at luts[r * 16].
 */
static void
rpf__ng_encrypt_block(uint8_t blk[16], const uint32_t key[68],
    const uint32_t *enctables, const struct rpf_nglut *luts)
{
	const uint32_t *t;
	uint32_t x[4];
	uint8_t d[16];
	int r, i, m, n;

	for (r = 16; r >= 0; r--) {
		for (i = 0; i < 16; i++)
			d[i] = blk[i] ^
			    (uint8_t)(key[r * 4 + i / 4] >> ((i % 4) * 8));
		if (r >= 2 && r <= 15) {
			for (i = 0; i < 4; i++)
				x[i] = (uint32_t)d[4 * i] |
				    (uint32_t)d[4 * i + 1] << 8 |
				    (uint32_t)d[4 * i + 2] << 16 |
				    (uint32_t)d[4 * i + 3] << 24;
			for (i = 0; i < 16; i++)
				blk[i] = rpf__nglut_lookup(&luts[r * 16 + i],
				    x[rpf__ng_word_b[i]]);
			continue;
		}
		t = enctables + (size_t)r * 16 * 256;
		for (i = 0; i < 4; i++) {
			x[i] = 0;
			for (m = 0; m < 4; m++) {
				n = 4 * i + m;
				x[i] ^= t[n * 256 + d[n]];
			}
		}
		for (i = 0; i < 16; i++)
			blk[i] = (uint8_t)(x[i / 4] >> ((i % 4) * 8));
	}
}

/* Encrypt len bytes at buf in place. Skip a partial block at the end. */
static void
rpf__ng_encrypt(uint8_t *buf, size_t len, const uint8_t key[272],
    const uint32_t *enctables, const struct rpf_nglut *luts)
{
	uint32_t k[68];
	size_t i;

	for (i = 0; i < 68; i++)
		k[i] = (uint32_t)key[4 * i] | (uint32_t)key[4 * i + 1] << 8 |
		    (uint32_t)key[4 * i + 2] << 16 |
		    (uint32_t)key[4 * i + 3] << 24;
	for (; len >= 16; buf += 16, len -= 16)
		rpf__ng_encrypt_block(buf, k, enctables, luts);
}

/*
 * The maximum number of random rows for one word in rpf__ng_solve. An
 * invertible round needs about 2000 rows.
 */
#define RPF__NG_TRIES 65536

/*
 * Find the 16 encrypt tables that invert one round A. For each word,
 * solve a linear system over GF(2) with 1024 unknowns: one for each
 * value of each of the 4 input bytes. Make the rows from random words.
 * Word 16 of a row holds the 32 right sides, one for each output bit.
 * Set unknowns 0, 0x2ff, and 0x3ff to all ones. Return RPF_EINVAL if
 * the rows do not give a pivot for each unknown, because then the round
 * is not invertible.
 */
static int
rpf__ng_solve(const uint32_t *dec16, uint32_t *enc16)
{
	uint64_t (*piv)[17], row[17];
	uint32_t rng = 0x2545F491u, c, d;
	int w, j, k, m, n, npiv, tries;

	piv = RPF_MALLOC(1024 * sizeof(*piv));
	if (piv == NULL)
		return RPF_ENOMEM;
	for (w = 0; w < 4; w++) {
		memset(piv, 0, 1024 * sizeof(*piv));
		piv[0][0] = 1;
		piv[0x2ff][0x2ff / 64] = (uint64_t)1 << (0x2ff % 64);
		piv[0x3ff][0x3ff / 64] = (uint64_t)1 << (0x3ff % 64);
		piv[0][16] = piv[0x2ff][16] = piv[0x3ff][16] = 0xFFFFFFFFu;
		npiv = 3;
		for (tries = 0; npiv < 1024; tries++) {
			if (tries == RPF__NG_TRIES) {
				RPF_FREE(piv);
				return RPF_EINVAL;
			}
			/* Use xorshift32 for the random encrypted word. */
			rng ^= rng << 13;
			rng ^= rng >> 17;
			rng ^= rng << 5;
			c = rng;
			d = 0;
			for (m = 0; m < 4; m++)
				d ^= dec16[(4 * w + m) * 256 +
				    ((c >> (8 * m)) & 0xFF)];
			memset(row, 0, sizeof(row));
			for (m = 0; m < 4; m++) {
				n = m * 256 + (int)((d >> (8 * m)) & 0xFF);
				row[n / 64] |= (uint64_t)1 << (n % 64);
			}
			row[16] = c;
			for (k = 0; k < 1024; k++) {
				if (!((row[k / 64] >> (k % 64)) & 1))
					continue;
				if (!((piv[k][k / 64] >> (k % 64)) & 1)) {
					memcpy(piv[k], row, sizeof(row));
					npiv++;
					break;
				}
				for (n = 0; n < 17; n++)
					row[n] ^= piv[k][n];
			}
		}
		for (j = 1023; j >= 0; j--) {
			c = (uint32_t)piv[j][16];
			enc16[(4 * w + j / 256) * 256 + j % 256] = c;
			for (k = 0; k < j; k++)
				if ((piv[k][j / 64] >> (j % 64)) & 1)
					piv[k][16] ^= c;
		}
	}
	RPF_FREE(piv);
	return 0;
}

/*
 * Build the 16 encrypt LUTs that invert one round B. Enumerate all 2^32
 * inputs of each word. Keep the input bytes of each output word below
 * 65536 and of each output word with a zero low byte. The scratch must
 * hold 16 * 16777216 + 16 * 65536 bytes.
 */
static int
rpf__ng_build_luts(const uint32_t *dec16, struct rpf_nglut *out16,
    void *scratch)
{
	const uint32_t *t[4];
	uint8_t *big, *small, *g, *s, b[4];
	uint32_t a, x;
	size_t n;
	int b0, b1, b2, b3, i, k, m;

	big = scratch;
	small = big + (size_t)16 * 16777216;
	memset(scratch, 0, (size_t)16 * 16777216 + 16 * 65536);
	memset(out16, 0, 16 * sizeof(*out16));
	for (b3 = 0; b3 < 256; b3++) {
		b[3] = (uint8_t)b3;
		for (b2 = 0; b2 < 256; b2++) {
			b[2] = (uint8_t)b2;
			for (b1 = 0; b1 < 256; b1++) {
				b[1] = (uint8_t)b1;
				for (i = 0; i < 4; i++) {
					for (m = 0; m < 4; m++)
						t[m] = dec16 + 256 *
						    rpf__ng_sel_b[4 * i + m];
					a = t[1][b1] ^ t[2][b2] ^ t[3][b3];
					for (b0 = 0; b0 < 256; b0++) {
						x = a ^ t[0][b0];
						if (x >= 65536 &&
						    (x & 0xFF) != 0)
							continue;
						b[0] = (uint8_t)b0;
						for (m = 0; m < 4; m++) {
							n = rpf__ng_sel_b[
							    4 * i + m];
							if (x < 65536)
								small[n *
								    65536 +
								    x] = b[m];
							if ((x & 0xFF) == 0)
								big[n *
								    16777216 +
								    (x >> 8)] =
								    b[m];
						}
					}
				}
			}
		}
	}
	for (i = 0; i < 16; i++) {
		s = small + (size_t)i * 65536;
		g = big + (size_t)i * 16777216;
		for (k = 0; k < 256; k++)
			memcpy(out16[i].lut0[s[256 * k]], s + 256 * k, 256);
		for (k = 0; k < 256; k++)
			memcpy(out16[i].lut1[g[256 * k]], g + 256 * k, 256);
		for (k = 0; k < 65536; k++)
			out16[i].indices[k] = g[(size_t)256 * k];
	}
	return 0;
}

/*
 * Derive the encrypt tables and LUTs from the decrypt tables. Solve
 * rounds 0, 1, and 16. Build the LUTs of rounds 2 to 15. Set the unused
 * tables and LUTs to zero. Call progress, if not NULL, before each
 * round. Return 0, RPF_ENOMEM, or RPF_EINVAL.
 */
static int
rpf__ng_derive(const uint32_t *dectables, uint32_t *enctables,
    struct rpf_nglut *luts, void (*progress)(const char *, void *),
    void *ud)
{
	char msg[48];
	void *scratch;
	int r, err = 0;

	memset(enctables, 0, (size_t)17 * 16 * 256 * sizeof(uint32_t));
	memset(luts, 0, 2 * 16 * sizeof(*luts));
	memset(luts + 16 * 16, 0, 16 * sizeof(*luts));
	scratch = RPF_MALLOC((size_t)16 * 16777216 + 16 * 65536);
	if (scratch == NULL)
		return RPF_ENOMEM;
	for (r = 0; r < 17 && err == 0; r++) {
		if (progress != NULL) {
			sprintf(msg, "NG encrypt tables: round %d of 17",
			    r + 1);
			progress(msg, ud);
		}
		if (r < 2 || r == 16)
			err = rpf__ng_solve(dectables + (size_t)r * 4096,
			    enctables + (size_t)r * 4096);
		else
			err = rpf__ng_build_luts(dectables +
			    (size_t)r * 4096, luts + r * 16, scratch);
	}
	RPF_FREE(scratch);
	return err;
}

/*
 * A key set. Each part is NULL if the key set does not have it. The
 * tables have 17 * 16 * 256 words. The LUTs have 17 * 16 entries.
 */
struct rpf_keys {
	uint8_t *aes;			/* 32 bytes */
	uint8_t *ngkeys;		/* 101 * 272 bytes */
	uint32_t *dectables;		/* 278528 bytes in memory */
	uint8_t *hashlut;		/* 256 bytes */
	uint32_t *enctables;		/* 278528 bytes in memory */
	struct rpf_nglut *luts;		/* 17 * 16 * 196608 bytes */
};

/* The needs of a task. Use these with rpf__keys_has. */
enum {
	RPF__K_AES,		/* AES read and write */
	RPF__K_NGREAD,		/* NG read */
	RPF__K_NGWRITE		/* NG write */
};

#define RPF__NKEYFILES 6

/* One key file. The file holds little-endian words if words is set. */
struct rpf__keyfile {
	const char *name;
	size_t size;
	int words;
};

/* The key files. The order is the order of the parts in a key set. */
static const struct rpf__keyfile rpf__keyfiles[RPF__NKEYFILES] = {
	{ "gtav_aes_key.dat", 32, 0 },
	{ "gtav_ng_key.dat", 101 * 272, 0 },
	{ "gtav_ng_decrypt_tables.dat", 17 * 16 * 256 * 4, 1 },
	{ "gtav_ng_encrypt_tables.dat", 17 * 16 * 256 * 4, 1 },
	{ "gtav_ng_encrypt_luts.dat", (size_t)17 * 16 * 196608, 0 },
	{ "gtav_hash_lut.dat", 256, 0 }
};

/*
 * Read the whole file. Set *out and *len. The caller frees *out with
 * RPF_FREE. Return 0, RPF_ENOTFOUND if the file does not open, RPF_EIO,
 * RPF_ETOOBIG, or RPF_ENOMEM.
 */
static int
rpf__read_file(const char *path, uint8_t **out, size_t *len)
{
	FILE *f;
	int64_t size;
	uint8_t *buf;

	*out = NULL;
	*len = 0;
	f = fopen(path, "rb");
	if (f == NULL)
		return RPF_ENOTFOUND;
	size = -1;
	if (fseek(f, 0, SEEK_END) == 0)
		size = rpf__tell(f);
	if (size < 0 || rpf__seek(f, 0) != 0) {
		fclose(f);
		return RPF_EIO;
	}
	if ((int64_t)(size_t)size != size) {
		fclose(f);
		return RPF_ETOOBIG;
	}
	buf = RPF_MALLOC(size > 0 ? (size_t)size : 1);
	if (buf == NULL) {
		fclose(f);
		return RPF_ENOMEM;
	}
	if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
		RPF_FREE(buf);
		fclose(f);
		return RPF_EIO;
	}
	fclose(f);
	*out = buf;
	*len = (size_t)size;
	return 0;
}

/* Write len bytes to a new file. Return 0 or RPF_EIO. */
static int
rpf__write_file(const char *path, const uint8_t *data, size_t len)
{
	FILE *f;
	int err;

	f = fopen(path, "wb");
	if (f == NULL)
		return RPF_EIO;
	err = 0;
	if (len > 0 && fwrite(data, 1, len, f) != len)
		err = RPF_EIO;
	if (fclose(f) != 0)
		err = RPF_EIO;
	return err;
}

/*
 * Make the path dir/name. Set *out. The caller frees it with RPF_FREE.
 * Return 0 or RPF_ENOMEM.
 */
static int
rpf__keys_path(char **out, const char *dir, const char *name)
{
	char *p;

	p = RPF_MALLOC(strlen(dir) + strlen(name) + 2);
	if (p == NULL)
		return RPF_ENOMEM;
	sprintf(p, "%s/%s", dir, name);
	*out = p;
	return 0;
}

/* Return 1 if the key set has all the parts that the task needs. */
static int
rpf__keys_has(const RpfKeys *k, int what)
{
	switch (what) {
	case RPF__K_AES:
		return k->aes != NULL;
	case RPF__K_NGREAD:
		return k->ngkeys != NULL && k->dectables != NULL &&
		    k->hashlut != NULL;
	case RPF__K_NGWRITE:
		return k->ngkeys != NULL && k->hashlut != NULL &&
		    k->enctables != NULL && k->luts != NULL;
	default:
		return 0;
	}
}

int
rpf_keys_new(RpfKeys **out)
{
	RpfKeys *k;

	k = RPF_MALLOC(sizeof(*k));
	if (k == NULL) {
		*out = NULL;
		return RPF_ENOMEM;
	}
	memset(k, 0, sizeof(*k));
	*out = k;
	return 0;
}

void
rpf_keys_free(RpfKeys *keys)
{
	if (keys == NULL)
		return;
	RPF_FREE(keys->aes);
	RPF_FREE(keys->ngkeys);
	RPF_FREE(keys->dectables);
	RPF_FREE(keys->hashlut);
	RPF_FREE(keys->enctables);
	RPF_FREE(keys->luts);
	RPF_FREE(keys);
}

int
rpf_keys_load(RpfKeys **out, const char *dir)
{
	void *part[RPF__NKEYFILES];
	RpfKeys *k;
	uint8_t *buf;
	uint32_t *w;
	char *path;
	size_t len, i, j;
	int err, found;

	*out = NULL;
	k = NULL;
	found = 0;
	err = 0;
	for (i = 0; i < RPF__NKEYFILES; i++)
		part[i] = NULL;
	for (i = 0; i < RPF__NKEYFILES && err == 0; i++) {
		err = rpf__keys_path(&path, dir, rpf__keyfiles[i].name);
		if (err != 0)
			break;
		err = rpf__read_file(path, &buf, &len);
		RPF_FREE(path);
		if (err == RPF_ENOTFOUND) {
			err = 0;
			continue;
		}
		if (err != 0)
			break;
		if (len != rpf__keyfiles[i].size) {
			RPF_FREE(buf);
			err = RPF_EFORMAT;
			break;
		}
		if (rpf__keyfiles[i].words) {
			/* Assemble the words, so the byte order is fixed. */
			w = (uint32_t *)buf;
			for (j = 0; j < len / 4; j++)
				w[j] = (uint32_t)buf[4 * j] |
				    (uint32_t)buf[4 * j + 1] << 8 |
				    (uint32_t)buf[4 * j + 2] << 16 |
				    (uint32_t)buf[4 * j + 3] << 24;
		}
		part[i] = buf;
		found++;
	}
	if (err == 0 && found == 0)
		err = RPF_ENOTFOUND;
	if (err == 0)
		err = rpf_keys_new(&k);
	if (err != 0) {
		for (i = 0; i < RPF__NKEYFILES; i++)
			RPF_FREE(part[i]);
		return err;
	}
	k->aes = part[0];
	k->ngkeys = part[1];
	k->dectables = part[2];
	k->enctables = part[3];
	k->luts = part[4];
	k->hashlut = part[5];
	*out = k;
	return 0;
}

int
rpf_keys_save(const RpfKeys *keys, const char *dir)
{
	const void *part[RPF__NKEYFILES];
	const uint32_t *w;
	uint8_t *buf;
	char *path;
	size_t i, j;
	int err;

	part[0] = keys->aes;
	part[1] = keys->ngkeys;
	part[2] = keys->dectables;
	part[3] = keys->enctables;
	part[4] = keys->luts;
	part[5] = keys->hashlut;
	for (i = 0; i < RPF__NKEYFILES; i++) {
		if (part[i] == NULL)
			continue;
		err = rpf__keys_path(&path, dir, rpf__keyfiles[i].name);
		if (err != 0)
			return err;
		buf = NULL;
		if (rpf__keyfiles[i].words) {
			/* Write the words as little-endian bytes. */
			buf = RPF_MALLOC(rpf__keyfiles[i].size);
			if (buf == NULL) {
				RPF_FREE(path);
				return RPF_ENOMEM;
			}
			w = part[i];
			for (j = 0; j < rpf__keyfiles[i].size / 4; j++) {
				buf[4 * j] = (uint8_t)w[j];
				buf[4 * j + 1] = (uint8_t)(w[j] >> 8);
				buf[4 * j + 2] = (uint8_t)(w[j] >> 16);
				buf[4 * j + 3] = (uint8_t)(w[j] >> 24);
			}
			err = rpf__write_file(path, buf,
			    rpf__keyfiles[i].size);
		} else
			err = rpf__write_file(path, part[i],
			    rpf__keyfiles[i].size);
		RPF_FREE(buf);
		RPF_FREE(path);
		if (err != 0)
			return err;
	}
	return 0;
}

/* Key extraction from the executable. */

/* The SHA-1 digests of the key material in GTA5.exe. */
static const uint8_t rpf__hash_aes[1][20] = {
	{ 0xa0, 0x79, 0x61, 0x28, 0xa7, 0x75, 0x72, 0x0a, 0xc2, 0x04,
	  0xd9, 0x81, 0x9f, 0x68, 0xc1, 0x72, 0xe3, 0x95, 0x2c, 0x6d }
};

static const uint8_t rpf__hash_ngkeys[101][20] = {
	{ 0xeb, 0x09, 0x15, 0x12, 0x03, 0x97, 0xce, 0x2e, 0x17, 0x82,
	  0x8d, 0xd7, 0x7e, 0x32, 0x18, 0xd9, 0x77, 0xa1, 0x85, 0xf7 },
	{ 0xd7, 0x40, 0xb3, 0xc8, 0x9f, 0xe3, 0xa1, 0x9a, 0x92, 0x65,
	  0xdc, 0xee, 0xfb, 0x45, 0x4c, 0x16, 0x2d, 0xb4, 0x53, 0x68 },
	{ 0x85, 0xf4, 0x3e, 0x5b, 0xab, 0x4a, 0xbc, 0xad, 0x76, 0xd8,
	  0x07, 0x15, 0xbb, 0x39, 0x32, 0xe9, 0xe7, 0xec, 0xc5, 0xe2 },
	{ 0xb8, 0x6d, 0x64, 0x1e, 0x44, 0x17, 0xf3, 0xdd, 0xd8, 0x01,
	  0x98, 0x55, 0x14, 0xef, 0x79, 0x73, 0x22, 0xf2, 0xd6, 0x57 },
	{ 0xe1, 0x27, 0xae, 0xdb, 0x60, 0x43, 0x51, 0x31, 0xab, 0x6d,
	  0x44, 0xf8, 0xbc, 0x02, 0x46, 0xb9, 0x52, 0x38, 0xb1, 0x0b },
	{ 0x15, 0xbe, 0x8f, 0x36, 0xb7, 0xe7, 0x26, 0xb3, 0x28, 0x7a,
	  0x77, 0x85, 0x88, 0x85, 0x94, 0x12, 0xeb, 0x0c, 0xaa, 0x66 },
	{ 0x28, 0x72, 0x6a, 0xec, 0x3b, 0x14, 0x19, 0x7c, 0x0e, 0x54,
	  0x8b, 0xaa, 0x5c, 0x8d, 0xf5, 0x12, 0x84, 0x98, 0xc4, 0x75 },
	{ 0xad, 0xdc, 0xb1, 0xbe, 0xf3, 0x98, 0x6e, 0x4a, 0x3a, 0x31,
	  0xf4, 0xed, 0xa4, 0xdb, 0xce, 0xb6, 0x1a, 0x6c, 0x91, 0x47 },
	{ 0xb9, 0xd4, 0xf6, 0xa1, 0x19, 0x90, 0x66, 0x47, 0xd1, 0x85,
	  0x83, 0x08, 0x3f, 0xcf, 0x4e, 0x1d, 0xfb, 0x0f, 0x74, 0x26 },
	{ 0x0f, 0x3f, 0x35, 0x99, 0x1a, 0xc0, 0xb1, 0xaa, 0xb1, 0x12,
	  0x84, 0x05, 0x9d, 0x96, 0x9b, 0x4a, 0xc7, 0xa0, 0x3b, 0x13 },
	{ 0x3b, 0xa9, 0x01, 0x6a, 0x98, 0x44, 0x47, 0x0d, 0x46, 0x33,
	  0xef, 0x2e, 0x6a, 0xc6, 0x3c, 0x77, 0xaf, 0x9a, 0xcc, 0x99 },
	{ 0xdb, 0xf6, 0x28, 0x3c, 0xf8, 0x2e, 0xdc, 0x4e, 0x1d, 0xa5,
	  0xcb, 0xdd, 0xa6, 0xfa, 0x21, 0x8e, 0xb8, 0xae, 0x72, 0xbb },
	{ 0xa4, 0xbf, 0x04, 0xe7, 0x2f, 0x5e, 0x9d, 0xbd, 0x7b, 0xf8,
	  0x50, 0x77, 0x67, 0x69, 0xb6, 0x2f, 0x56, 0x8d, 0xc7, 0xf9 },
	{ 0x6a, 0xae, 0xa0, 0x64, 0xb9, 0xbb, 0xe7, 0xf1, 0xed, 0xae,
	  0xf4, 0x7f, 0x8b, 0x83, 0x5f, 0x0e, 0xa8, 0xd5, 0x06, 0x6b },
	{ 0x1b, 0xb1, 0x18, 0xb6, 0xc1, 0x19, 0x2b, 0x98, 0xaf, 0x11,
	  0x98, 0xcb, 0x73, 0xc3, 0xe1, 0xe5, 0xae, 0x48, 0xdd, 0xa1 },
	{ 0x4b, 0x97, 0x33, 0x2a, 0x27, 0x28, 0xff, 0x46, 0xd8, 0xbd,
	  0x73, 0x61, 0x5d, 0x14, 0xcf, 0x47, 0xd3, 0xc2, 0xc9, 0xa0 },
	{ 0x66, 0x15, 0x42, 0x97, 0x46, 0x57, 0x96, 0x16, 0x5a, 0x87,
	  0x6f, 0x5e, 0xf6, 0x84, 0xb3, 0xa0, 0xdb, 0x5c, 0x6d, 0x58 },
	{ 0xf5, 0xb5, 0xe7, 0x6f, 0x77, 0xfe, 0x13, 0xac, 0xc2, 0xf9,
	  0x2d, 0xea, 0xd0, 0xd9, 0xc5, 0xcd, 0x05, 0x9a, 0xc0, 0x48 },
	{ 0x8a, 0x0c, 0x59, 0xbd, 0x88, 0x0d, 0x28, 0x18, 0x3d, 0xe9,
	  0x8b, 0x2c, 0xb0, 0x9a, 0xd0, 0xe1, 0xbe, 0x0c, 0x03, 0x23 },
	{ 0x26, 0xa7, 0xbc, 0x77, 0x22, 0x5f, 0x67, 0xbc, 0x3d, 0xd5,
	  0xd5, 0x05, 0x53, 0xb9, 0xd8, 0x2d, 0xd2, 0x80, 0x80, 0x26 },
	{ 0x66, 0x40, 0x17, 0x57, 0x5b, 0x70, 0x7b, 0x76, 0x71, 0x91,
	  0x56, 0xed, 0xfb, 0x5e, 0xc9, 0x7b, 0x55, 0x77, 0x5c, 0x40 },
	{ 0xec, 0x90, 0xab, 0xb0, 0x46, 0x6e, 0x62, 0xf1, 0xb9, 0xdb,
	  0xfb, 0xf2, 0x4e, 0xae, 0x2f, 0x20, 0x54, 0x74, 0xa1, 0xda },
	{ 0x4a, 0xa3, 0x51, 0x73, 0x61, 0x98, 0x34, 0x18, 0x8d, 0x9e,
	  0x76, 0x59, 0x89, 0x68, 0x2a, 0x1c, 0x1a, 0x2d, 0x27, 0x77 },
	{ 0xb5, 0xdf, 0x02, 0x8b, 0xb9, 0x86, 0x0c, 0x84, 0x0d, 0x75,
	  0x45, 0x77, 0xff, 0x9c, 0x02, 0xeb, 0x7d, 0x65, 0x09, 0x29 },
	{ 0x86, 0x26, 0xf9, 0xf0, 0xa7, 0xa9, 0x79, 0x55, 0x72, 0x14,
	  0x6f, 0x92, 0xac, 0xc1, 0x06, 0x12, 0xcd, 0x60, 0x75, 0x7d },
	{ 0xcd, 0xa4, 0xc6, 0x4f, 0x0f, 0x94, 0x65, 0x11, 0x57, 0x4d,
	  0xf5, 0xc6, 0xa0, 0x19, 0x8e, 0x7b, 0x51, 0xee, 0xbc, 0x07 },
	{ 0xc2, 0xb6, 0x51, 0x28, 0xc5, 0xbf, 0x14, 0x0c, 0xdd, 0x58,
	  0xfb, 0xae, 0xff, 0x43, 0x4a, 0x69, 0x89, 0x23, 0x37, 0x1a },
	{ 0x14, 0x30, 0xff, 0x46, 0xea, 0xfb, 0xca, 0x55, 0xdf, 0x49,
	  0x4a, 0x1b, 0x7f, 0xcd, 0x5f, 0xdd, 0xc7, 0x6e, 0x71, 0x2e },
	{ 0xd0, 0xb6, 0x7e, 0x92, 0xa0, 0xfb, 0x5a, 0x74, 0xcb, 0x2c,
	  0xfa, 0x60, 0x25, 0xab, 0xe4, 0x7b, 0x88, 0xe4, 0xd8, 0xae },
	{ 0xae, 0xdb, 0x38, 0xaa, 0x93, 0xea, 0x10, 0xbc, 0xff, 0x5d,
	  0x7b, 0xab, 0x8f, 0x95, 0x6f, 0x4a, 0x03, 0xa8, 0xf2, 0xf2 },
	{ 0x81, 0xbf, 0xda, 0x4a, 0x64, 0x97, 0x3c, 0x36, 0x64, 0x52,
	  0x13, 0xae, 0xb5, 0x39, 0xd3, 0x1d, 0xf6, 0xea, 0x8c, 0x99 },
	{ 0x9a, 0x1d, 0xe6, 0x96, 0x0f, 0x2c, 0xf2, 0x3f, 0x46, 0xcd,
	  0x14, 0x04, 0xd4, 0xee, 0x6e, 0xa7, 0x73, 0xe9, 0x1a, 0x8a },
	{ 0xb6, 0xe6, 0xd2, 0xfa, 0xe3, 0x95, 0x1c, 0xdd, 0x28, 0x6c,
	  0x97, 0xa2, 0xe1, 0x54, 0x0f, 0x72, 0xab, 0x63, 0x20, 0x5d },
	{ 0xcf, 0x57, 0x29, 0xaf, 0x92, 0x38, 0x1d, 0x0b, 0x5f, 0xce,
	  0x3f, 0x11, 0x77, 0x5e, 0x75, 0x96, 0xbe, 0xbd, 0x27, 0x25 },
	{ 0x19, 0x8e, 0xc1, 0x12, 0xfc, 0xb3, 0xd9, 0x93, 0x8a, 0x6c,
	  0xa5, 0x13, 0x5d, 0xfb, 0x6d, 0x77, 0xf6, 0xdc, 0x2f, 0xee },
	{ 0x04, 0x81, 0x55, 0x6c, 0xd2, 0x59, 0xfb, 0xad, 0xf8, 0xe1,
	  0xf1, 0xa7, 0x85, 0x02, 0xa6, 0xf4, 0x7f, 0xa2, 0x2f, 0x62 },
	{ 0x88, 0x0e, 0xbd, 0x6a, 0xa6, 0x47, 0x17, 0xad, 0xa1, 0xe6,
	  0xe0, 0x97, 0x74, 0x53, 0x9c, 0xfe, 0x38, 0xfb, 0x91, 0x52 },
	{ 0xa1, 0xae, 0x7d, 0x57, 0x6f, 0xf8, 0x85, 0xe2, 0xa8, 0xfd,
	  0xb6, 0x01, 0xe3, 0x55, 0x65, 0x60, 0xcd, 0xd8, 0xd3, 0x30 },
	{ 0x80, 0x26, 0xa6, 0x9b, 0x78, 0x2c, 0x0f, 0xd6, 0x4c, 0xe5,
	  0x19, 0x42, 0xac, 0xc8, 0x42, 0x07, 0x54, 0x28, 0xff, 0x14 },
	{ 0x89, 0x03, 0xca, 0x8a, 0x1f, 0xd4, 0x73, 0x0f, 0x68, 0x8a,
	  0x61, 0x0d, 0xac, 0xfa, 0x64, 0x7d, 0xc8, 0xaa, 0x96, 0xf2 },
	{ 0x99, 0x37, 0x9a, 0x14, 0x51, 0x14, 0xb5, 0xd5, 0x99, 0x6a,
	  0x57, 0x9c, 0xf2, 0xe3, 0x38, 0xaf, 0x70, 0x9c, 0x49, 0xc9 },
	{ 0x0f, 0x2d, 0x55, 0x01, 0x05, 0x44, 0x55, 0x66, 0x0e, 0xfc,
	  0x4f, 0xb2, 0xba, 0x29, 0x9b, 0x5b, 0xca, 0x29, 0xbc, 0x1a },
	{ 0xf1, 0x38, 0x5b, 0x67, 0xf4, 0xaf, 0xbe, 0xc9, 0x4f, 0x7a,
	  0x00, 0xa6, 0x8b, 0x0e, 0x70, 0x0b, 0xa8, 0x6a, 0x81, 0xee },
	{ 0x36, 0xb1, 0x05, 0xb7, 0xed, 0x94, 0xca, 0x95, 0xd3, 0x9e,
	  0x1c, 0xe2, 0x4d, 0xd9, 0x71, 0x66, 0x46, 0xf2, 0x9b, 0x59 },
	{ 0x75, 0xc6, 0x12, 0x28, 0xe2, 0x7f, 0x61, 0xf0, 0x4b, 0x71,
	  0xcd, 0x4d, 0xe8, 0xe3, 0x9a, 0xe4, 0x20, 0xc2, 0x5c, 0xe6 },
	{ 0x37, 0x0e, 0x44, 0xd7, 0x27, 0x99, 0xe6, 0x2b, 0xb1, 0x34,
	  0xfa, 0xb7, 0x45, 0x33, 0xf7, 0xcd, 0x3c, 0x0d, 0x77, 0xf5 },
	{ 0xaa, 0x46, 0xce, 0xa4, 0x4d, 0x0a, 0xce, 0xdc, 0x81, 0x59,
	  0xec, 0xcf, 0x1e, 0x0f, 0xb4, 0x15, 0xe8, 0x27, 0x18, 0x9e },
	{ 0xc0, 0x0d, 0x76, 0xbd, 0x70, 0x3b, 0x1b, 0x6f, 0x11, 0x1a,
	  0x84, 0x2b, 0x0b, 0x19, 0xc6, 0x55, 0xc6, 0x7f, 0x0f, 0x6c },
	{ 0xf7, 0xa9, 0x4e, 0x1f, 0x06, 0x03, 0xb9, 0xd7, 0x36, 0x0a,
	  0x34, 0x7f, 0x41, 0x94, 0x1d, 0x57, 0xa1, 0xd0, 0x0a, 0xfa },
	{ 0xcc, 0xaf, 0x7e, 0x30, 0x26, 0xde, 0xc8, 0xc9, 0x3c, 0x27,
	  0x8f, 0x40, 0x32, 0xf5, 0xab, 0xc4, 0xcb, 0x90, 0x6d, 0xae },
	{ 0x39, 0x5d, 0x47, 0xf2, 0xf8, 0x91, 0xff, 0xd7, 0xe1, 0x75,
	  0xf3, 0xa0, 0xfa, 0xc3, 0xc2, 0x86, 0x0d, 0xe1, 0xe2, 0x45 },
	{ 0xb1, 0xc9, 0x1c, 0x3e, 0x77, 0x20, 0xee, 0x9c, 0xbc, 0xa4,
	  0x1e, 0x3e, 0x6c, 0x34, 0x40, 0x08, 0x09, 0x27, 0xa7, 0xf5 },
	{ 0x61, 0x92, 0x6e, 0xf1, 0xa1, 0x0f, 0x71, 0x25, 0x1c, 0xee,
	  0xe4, 0x94, 0x8b, 0x29, 0xea, 0x7e, 0x90, 0x36, 0xe5, 0xe5 },
	{ 0xd5, 0xde, 0x91, 0xb6, 0x37, 0x2c, 0x00, 0x73, 0xb8, 0x80,
	  0x17, 0x19, 0xf4, 0xc0, 0xce, 0xe6, 0xdf, 0xfb, 0xb5, 0xfc },
	{ 0x01, 0xe0, 0x43, 0xc0, 0xda, 0x86, 0xe9, 0x73, 0x51, 0xfa,
	  0x2e, 0x4a, 0xf1, 0xa4, 0xf6, 0x5b, 0x6a, 0x97, 0x7b, 0x39 },
	{ 0x80, 0x85, 0x51, 0x20, 0xba, 0x2e, 0x31, 0x6b, 0xd7, 0xb8,
	  0x09, 0x30, 0xa5, 0x60, 0xcc, 0x0e, 0x8b, 0x9a, 0x91, 0xd7 },
	{ 0x06, 0x06, 0xc0, 0xb8, 0x2a, 0x97, 0x94, 0x7d, 0x25, 0x2c,
	  0xd3, 0x87, 0x8a, 0x9d, 0xbe, 0xfd, 0xec, 0x17, 0xb4, 0x98 },
	{ 0x5f, 0xd7, 0x58, 0xeb, 0x71, 0x3a, 0x4b, 0xe0, 0x8e, 0x29,
	  0x2a, 0xc7, 0x55, 0x75, 0x91, 0xa7, 0x06, 0xa3, 0x2b, 0x2c },
	{ 0x2a, 0x3c, 0x3f, 0xa1, 0x1c, 0xfa, 0x0f, 0x5f, 0x75, 0x4c,
	  0x65, 0x56, 0xe0, 0xd9, 0x52, 0x06, 0x11, 0xd6, 0xb4, 0x0b },
	{ 0xfa, 0xe6, 0xb7, 0x90, 0xb7, 0x87, 0x91, 0xa9, 0x10, 0x84,
	  0xa1, 0xb5, 0xca, 0x9a, 0x70, 0x7d, 0x56, 0x07, 0xe4, 0x5f },
	{ 0x8f, 0x9a, 0xa7, 0x48, 0xf1, 0x14, 0x50, 0x67, 0xfa, 0xc8,
	  0x0e, 0x18, 0x26, 0x48, 0xed, 0x61, 0x8c, 0x84, 0xac, 0xce },
	{ 0x39, 0xb7, 0xfd, 0x2f, 0xb4, 0xeb, 0x43, 0xe3, 0xc1, 0xce,
	  0xf1, 0x5d, 0x9e, 0xf7, 0x16, 0xa6, 0x81, 0x12, 0x25, 0xba },
	{ 0xeb, 0x28, 0xf6, 0x34, 0x62, 0x49, 0xa7, 0x99, 0x16, 0x2e,
	  0x0b, 0xee, 0xe5, 0x58, 0x92, 0x90, 0xae, 0x8c, 0xcf, 0x3f },
	{ 0x51, 0xda, 0xc7, 0x9d, 0x41, 0x87, 0xb3, 0x94, 0x43, 0xfe,
	  0x3b, 0xc7, 0xf9, 0x19, 0x72, 0x47, 0x55, 0xad, 0x27, 0x19 },
	{ 0x0c, 0xaf, 0xfe, 0x08, 0x02, 0x3e, 0xd8, 0xb5, 0xac, 0x2a,
	  0x14, 0x18, 0xbd, 0x42, 0x05, 0xab, 0xd7, 0x43, 0x3e, 0x9b },
	{ 0x0f, 0x0b, 0x7a, 0x98, 0xe8, 0x89, 0x63, 0xe9, 0x16, 0xc6,
	  0x78, 0x78, 0x86, 0x18, 0x7f, 0x3f, 0xcb, 0x7f, 0x9c, 0x39 },
	{ 0xb6, 0x4a, 0x86, 0x10, 0xab, 0x90, 0xe1, 0xf2, 0xed, 0xd2,
	  0x2b, 0xac, 0x42, 0x38, 0x33, 0x1a, 0xf8, 0x95, 0xb7, 0x5b },
	{ 0x57, 0x77, 0x12, 0x3e, 0x8f, 0x6a, 0x2b, 0x7f, 0x95, 0x14,
	  0x14, 0xa0, 0x9e, 0xe0, 0x1b, 0x36, 0x1c, 0x3c, 0xa1, 0xd0 },
	{ 0x24, 0x87, 0x11, 0x86, 0x54, 0xbc, 0x51, 0x30, 0x3e, 0xa7,
	  0x79, 0xc5, 0xb2, 0x64, 0x76, 0x97, 0x78, 0x31, 0x3e, 0xbb },
	{ 0x75, 0x60, 0xa9, 0x20, 0xa9, 0x1a, 0xe7, 0x49, 0x1b, 0x14,
	  0xd2, 0x7d, 0xef, 0x75, 0xfb, 0x0b, 0xfc, 0x26, 0x8f, 0xa6 },
	{ 0xb0, 0x7b, 0x51, 0x09, 0x5d, 0x44, 0xba, 0xab, 0x05, 0xf8,
	  0x9b, 0x5e, 0x8a, 0xc1, 0xbc, 0x73, 0xe1, 0x32, 0x64, 0xa5 },
	{ 0x1f, 0x8b, 0x33, 0x1d, 0xef, 0x6e, 0xbe, 0x11, 0x21, 0x2f,
	  0x2b, 0x61, 0xb8, 0x5e, 0xcc, 0xed, 0xea, 0xa7, 0x21, 0xd9 },
	{ 0x72, 0x71, 0x1e, 0xa9, 0x46, 0x60, 0x3e, 0x30, 0xa2, 0x0e,
	  0x96, 0xde, 0x49, 0x6b, 0x15, 0x79, 0xce, 0xfb, 0xee, 0x4a },
	{ 0xf7, 0x8a, 0x72, 0x5f, 0xcf, 0x7d, 0xdd, 0xaa, 0x39, 0xbe,
	  0x6d, 0x48, 0x81, 0x90, 0xd0, 0xec, 0x66, 0x71, 0x95, 0xbc },
	{ 0x68, 0x12, 0x9a, 0xab, 0x54, 0x6e, 0x0d, 0xa8, 0x23, 0xfe,
	  0x07, 0x42, 0xea, 0xe3, 0x02, 0x6a, 0xda, 0x07, 0x48, 0xbe },
	{ 0xae, 0x2e, 0x8d, 0x33, 0x41, 0x58, 0x2e, 0x47, 0x41, 0x0f,
	  0xdb, 0x24, 0xcb, 0xd3, 0xc5, 0x00, 0x1b, 0x3c, 0x40, 0xd2 },
	{ 0xcd, 0xb4, 0xc7, 0x16, 0x3c, 0x12, 0x40, 0x7c, 0x66, 0x4e,
	  0x4d, 0x4e, 0x2e, 0x2f, 0xea, 0xd4, 0x8a, 0xd9, 0x37, 0xd3 },
	{ 0x4b, 0xb9, 0x75, 0x5b, 0x31, 0xff, 0x6f, 0xd8, 0xbd, 0xaa,
	  0x6f, 0xd9, 0xf9, 0xfb, 0x15, 0x8b, 0x73, 0x20, 0x11, 0x49 },
	{ 0xd4, 0x9b, 0x7a, 0x75, 0xed, 0x05, 0x88, 0xf4, 0x2a, 0x89,
	  0x52, 0xbd, 0x84, 0xb2, 0x90, 0xbc, 0xe6, 0xaf, 0x4a, 0x0f },
	{ 0xe5, 0x44, 0x7d, 0x79, 0xd6, 0x8b, 0xec, 0x55, 0xbb, 0x5e,
	  0xf5, 0x5b, 0x84, 0xa7, 0xc3, 0x13, 0x5f, 0xd0, 0x0e, 0xb2 },
	{ 0x64, 0x88, 0x46, 0xcd, 0xac, 0xb5, 0x31, 0x5d, 0x5f, 0x80,
	  0x35, 0xcc, 0x8a, 0x52, 0x3f, 0xa8, 0xe6, 0x90, 0xdc, 0x75 },
	{ 0x2f, 0x02, 0x4d, 0xc8, 0x48, 0xfc, 0xdf, 0xba, 0x73, 0x5e,
	  0x82, 0x88, 0x09, 0x9d, 0x00, 0x87, 0x2b, 0xed, 0x00, 0x27 },
	{ 0xc3, 0x42, 0x7f, 0x3a, 0x4b, 0x9c, 0xce, 0x8f, 0xa0, 0x4d,
	  0x55, 0x76, 0x03, 0x01, 0xdc, 0xcd, 0xfa, 0x37, 0x56, 0x73 },
	{ 0xaa, 0x5c, 0xa4, 0x67, 0x47, 0xf9, 0x64, 0x49, 0x4b, 0x83,
	  0x8a, 0x24, 0xe3, 0xb5, 0x32, 0xe6, 0x39, 0xb3, 0x43, 0x9c },
	{ 0xc3, 0xf4, 0x6e, 0x4d, 0x3a, 0x30, 0x08, 0x44, 0xd7, 0x2d,
	  0x56, 0x70, 0xe6, 0xa6, 0x9f, 0x0a, 0x86, 0xc9, 0xbb, 0xf9 },
	{ 0x40, 0xe8, 0x7f, 0x72, 0x92, 0xca, 0x36, 0xf5, 0x35, 0x30,
	  0xbf, 0xa4, 0x60, 0x89, 0x38, 0xd8, 0x77, 0x6e, 0x70, 0x2b },
	{ 0x9e, 0xcd, 0x04, 0x16, 0x90, 0xd7, 0x9d, 0xf2, 0x50, 0xc8,
	  0xf6, 0xe9, 0x8f, 0xc7, 0x71, 0x93, 0x80, 0xb0, 0x26, 0xac },
	{ 0x74, 0xe1, 0xad, 0x78, 0x3b, 0xa4, 0x0b, 0x0f, 0xcf, 0x70,
	  0x9a, 0x74, 0x15, 0xed, 0xce, 0x94, 0x17, 0xa3, 0x3c, 0xa9 },
	{ 0x4a, 0x2a, 0xa2, 0xd4, 0x71, 0x38, 0x55, 0x9a, 0x8f, 0xa2,
	  0x2a, 0x48, 0xe5, 0x57, 0xea, 0xa2, 0x43, 0x65, 0x13, 0xf9 },
	{ 0x2f, 0x23, 0x20, 0xad, 0xaa, 0x87, 0x97, 0x6c, 0x26, 0xc3,
	  0xf5, 0x86, 0x80, 0x2c, 0x9b, 0x6a, 0x1c, 0xe4, 0x9a, 0xa5 },
	{ 0xf4, 0x79, 0x16, 0x18, 0xab, 0x41, 0xd4, 0xe3, 0x73, 0x7f,
	  0xe9, 0x79, 0x07, 0xa2, 0xc2, 0xb8, 0x59, 0x3d, 0x86, 0x38 },
	{ 0x9a, 0x65, 0x84, 0x31, 0x29, 0xd1, 0x18, 0x2a, 0x45, 0xf4,
	  0x28, 0xb9, 0x31, 0x5a, 0xfa, 0xca, 0xae, 0xd0, 0xa7, 0xb9 },
	{ 0x06, 0x0e, 0x28, 0x25, 0x42, 0x51, 0xac, 0x22, 0x59, 0x49,
	  0x9e, 0x43, 0xf6, 0xd5, 0x6d, 0x52, 0x28, 0x8f, 0xba, 0xd0 },
	{ 0xec, 0x55, 0xe5, 0xa1, 0xda, 0x41, 0x22, 0x76, 0x9f, 0x08,
	  0x98, 0xe4, 0xbb, 0xea, 0xe3, 0x59, 0x32, 0x84, 0xaf, 0xc1 },
	{ 0xc0, 0xf7, 0x43, 0x77, 0xb1, 0x56, 0x50, 0xfc, 0xca, 0xf7,
	  0x36, 0x89, 0x1c, 0xf9, 0xf4, 0x45, 0xd3, 0xd0, 0x02, 0x27 },
	{ 0x6d, 0x1d, 0xee, 0x18, 0x63, 0xd0, 0xb8, 0xba, 0x2a, 0x19,
	  0xd6, 0x93, 0x88, 0xf2, 0x27, 0x03, 0xf7, 0x01, 0x97, 0x13 },
	{ 0x07, 0x25, 0x38, 0x62, 0x0c, 0xec, 0x14, 0x68, 0x1b, 0xcf,
	  0x62, 0xe9, 0x65, 0xb4, 0x46, 0xe7, 0x68, 0xea, 0x82, 0xf4 },
	{ 0xe7, 0x68, 0x6e, 0x1f, 0x20, 0x62, 0x69, 0xd7, 0x2a, 0xb6,
	  0x7b, 0xeb, 0xb5, 0xb9, 0x2c, 0xbd, 0x43, 0xcb, 0xc8, 0xf4 },
	{ 0x68, 0xff, 0xd2, 0xb6, 0x25, 0xa3, 0x06, 0x7c, 0xfa, 0xe5,
	  0x47, 0x84, 0x89, 0x83, 0x5f, 0xec, 0x91, 0x0e, 0x13, 0x93 },
	{ 0x92, 0xf9, 0xf4, 0x11, 0xb7, 0xe9, 0xdc, 0x7e, 0xde, 0xe4,
	  0xf2, 0x22, 0x3a, 0x38, 0xc2, 0x7d, 0x57, 0xc2, 0xdb, 0xb8 },
	{ 0xb3, 0x9b, 0x96, 0xd9, 0x78, 0xe3, 0xf5, 0xc5, 0xb7, 0x6b,
	  0x12, 0x07, 0xc8, 0xaa, 0x1d, 0xb1, 0x4c, 0x85, 0x38, 0x39 }
};

static const uint8_t rpf__hash_dectables[272][20] = {
	{ 0xce, 0xab, 0x97, 0x16, 0xbd, 0xec, 0x4e, 0xb6, 0xb0, 0x28,
	  0xd3, 0x56, 0x39, 0xe4, 0x5f, 0x2f, 0x3d, 0x8a, 0xc0, 0x3a },
	{ 0x61, 0x86, 0x8d, 0xfd, 0xf3, 0x86, 0x92, 0xb0, 0xc0, 0x71,
	  0xf6, 0xb1, 0xc8, 0x09, 0x67, 0x41, 0x22, 0xd4, 0xb3, 0x03 },
	{ 0xbd, 0x28, 0x5f, 0x22, 0xd5, 0x7c, 0x53, 0xeb, 0x15, 0xc7,
	  0x28, 0x30, 0x11, 0x2c, 0xd8, 0x0b, 0xf9, 0xdb, 0x0d, 0xc1 },
	{ 0xce, 0x23, 0xf5, 0xc5, 0x68, 0x26, 0xf5, 0x3f, 0x78, 0x03,
	  0xd4, 0xe8, 0x43, 0xcd, 0x19, 0x9a, 0x4d, 0x82, 0xcb, 0xd8 },
	{ 0xa6, 0xf3, 0xbb, 0xdf, 0x03, 0xfa, 0xca, 0xf8, 0x93, 0x2d,
	  0x8e, 0x2e, 0x3b, 0x82, 0x5f, 0x18, 0xd5, 0x4b, 0xf3, 0x8a },
	{ 0x5b, 0xd1, 0x4c, 0xb2, 0x42, 0x1d, 0x70, 0x11, 0xba, 0xc1,
	  0x1e, 0x97, 0xf9, 0xe5, 0x53, 0xe4, 0x80, 0xa4, 0xa9, 0x26 },
	{ 0x2f, 0x7a, 0x79, 0x53, 0xdb, 0x77, 0x49, 0x9a, 0xdf, 0x2b,
	  0x76, 0x19, 0xde, 0x05, 0xfb, 0xbc, 0xcb, 0x37, 0x9c, 0x0d },
	{ 0xc3, 0x6c, 0xe4, 0x15, 0x62, 0xd8, 0x55, 0xac, 0x25, 0x17,
	  0x38, 0x32, 0xd3, 0xfc, 0x1a, 0x13, 0xe1, 0xe4, 0x90, 0x67 },
	{ 0x31, 0x41, 0xe6, 0xeb, 0xe2, 0xa1, 0xc2, 0xfa, 0x44, 0xd9,
	  0x72, 0x8e, 0x8d, 0x55, 0x0e, 0xf0, 0x28, 0xb1, 0x19, 0xc6 },
	{ 0x3d, 0x40, 0x6e, 0xbb, 0xee, 0x33, 0x93, 0xab, 0xc8, 0xce,
	  0x1c, 0xbe, 0x06, 0x94, 0xb1, 0xca, 0xf6, 0x72, 0x7f, 0x66 },
	{ 0x62, 0x1c, 0x68, 0x38, 0x79, 0x92, 0x33, 0xda, 0xc3, 0xc9,
	  0x78, 0x5a, 0x35, 0xe7, 0x44, 0x88, 0x16, 0x86, 0x9c, 0x64 },
	{ 0xd5, 0x93, 0xd9, 0xdd, 0x0c, 0x91, 0xb6, 0x32, 0xa7, 0x96,
	  0x45, 0x4a, 0x57, 0x6b, 0x05, 0xe2, 0x86, 0xcc, 0xb8, 0x86 },
	{ 0xc9, 0x33, 0x33, 0xb8, 0x35, 0x07, 0x1d, 0xc4, 0xf0, 0x16,
	  0xa4, 0xb8, 0x93, 0x60, 0x2b, 0xf5, 0x11, 0x9e, 0x8b, 0x1f },
	{ 0xda, 0x99, 0x31, 0x62, 0x1e, 0xe0, 0xac, 0xbe, 0xf8, 0xf4,
	  0xf9, 0x95, 0x9b, 0x2d, 0x1d, 0x15, 0x69, 0x57, 0x8d, 0xc2 },
	{ 0xb9, 0x20, 0xe8, 0x41, 0x15, 0xca, 0x2b, 0xbf, 0x0a, 0x16,
	  0x79, 0xe9, 0xdc, 0x7c, 0xd2, 0xd7, 0xf3, 0xf3, 0xec, 0x99 },
	{ 0xf6, 0x2b, 0x9d, 0x1c, 0xee, 0xce, 0x56, 0xc7, 0x61, 0x28,
	  0x3a, 0x17, 0x94, 0xf3, 0xa3, 0x09, 0x3b, 0xe7, 0x4f, 0xde },
	{ 0x35, 0xcd, 0x71, 0xc3, 0xa4, 0x31, 0x29, 0xa3, 0x57, 0x2a,
	  0x52, 0xa0, 0x5d, 0x83, 0x95, 0x87, 0xbc, 0x64, 0x0e, 0xa3 },
	{ 0xd0, 0x71, 0xc0, 0x43, 0x57, 0xce, 0xf5, 0x86, 0xd8, 0x04,
	  0xff, 0x7b, 0x5c, 0x9b, 0x24, 0x7e, 0x4b, 0x05, 0x2b, 0xf0 },
	{ 0xf5, 0xd1, 0x72, 0xa4, 0x92, 0x78, 0xa9, 0x84, 0x53, 0x49,
	  0xad, 0x73, 0x17, 0x8f, 0x2f, 0xee, 0x8b, 0x0a, 0xad, 0xbf },
	{ 0xa5, 0x9e, 0x9b, 0x49, 0x3a, 0xaf, 0x92, 0xda, 0x8f, 0x4f,
	  0x3c, 0x20, 0x88, 0x12, 0x89, 0xe1, 0x62, 0x56, 0x95, 0xda },
	{ 0xde, 0x5d, 0x57, 0xa5, 0x08, 0x52, 0xb6, 0xf4, 0x79, 0x8d,
	  0x9e, 0x52, 0xc4, 0xc5, 0x72, 0x24, 0x5c, 0x13, 0x21, 0xb4 },
	{ 0x2a, 0x9a, 0x5b, 0x76, 0xf4, 0xbb, 0xd1, 0x56, 0x1f, 0x23,
	  0xe7, 0xb5, 0x15, 0xa1, 0xab, 0x1f, 0x6e, 0x33, 0x43, 0xb0 },
	{ 0xe7, 0x4d, 0x0c, 0x5f, 0x4b, 0x78, 0x6d, 0x32, 0x48, 0x3d,
	  0x10, 0x0c, 0xa0, 0xaa, 0x4a, 0x1f, 0xca, 0xb7, 0x4f, 0x50 },
	{ 0x56, 0xf1, 0xa4, 0xc1, 0x06, 0xde, 0x83, 0xda, 0x93, 0x51,
	  0x32, 0x8b, 0xd1, 0xca, 0xaa, 0x8c, 0x2e, 0x74, 0xbb, 0x77 },
	{ 0x1d, 0x50, 0xf9, 0xd2, 0xe6, 0x9b, 0xec, 0x48, 0xcd, 0x38,
	  0x58, 0x79, 0x38, 0xe7, 0xc3, 0xeb, 0xcf, 0x77, 0xe7, 0x7b },
	{ 0x1e, 0xe3, 0x16, 0x88, 0x2a, 0xb9, 0xd7, 0xae, 0x4a, 0xd9,
	  0xeb, 0xc8, 0x0d, 0x39, 0xa8, 0x5d, 0xf5, 0x60, 0x19, 0xd1 },
	{ 0x36, 0x45, 0x6e, 0x8c, 0xd8, 0x1d, 0x0c, 0x6b, 0x3c, 0xa9,
	  0xa5, 0xef, 0x2a, 0xfb, 0x08, 0xc4, 0x3e, 0x18, 0x40, 0xae },
	{ 0xaa, 0x57, 0x3c, 0xcc, 0x91, 0x64, 0x9c, 0x17, 0xfa, 0xfa,
	  0xec, 0x8c, 0x13, 0x7c, 0x43, 0xfd, 0xb1, 0x0c, 0xfc, 0x8e },
	{ 0x3b, 0xe4, 0xc0, 0xa3, 0xf5, 0x4d, 0x30, 0x75, 0xbd, 0x94,
	  0xf5, 0x17, 0x41, 0x85, 0x8a, 0xd8, 0x8a, 0x70, 0x14, 0xa6 },
	{ 0x01, 0x60, 0xbd, 0x1e, 0x73, 0x92, 0x1e, 0xd4, 0x92, 0x03,
	  0x8f, 0x68, 0x48, 0x9e, 0x6d, 0x53, 0x1f, 0x5a, 0x7d, 0xe6 },
	{ 0x4c, 0xea, 0x08, 0xf0, 0x3a, 0xa2, 0x7f, 0xf1, 0xbd, 0x61,
	  0x31, 0xc1, 0xc3, 0x79, 0xff, 0x56, 0x04, 0x06, 0x49, 0x04 },
	{ 0x57, 0x74, 0x33, 0xe6, 0x6c, 0xfa, 0x46, 0xfa, 0x29, 0xb5,
	  0xf0, 0x26, 0x53, 0xee, 0x07, 0x40, 0xe2, 0xc2, 0x56, 0xf4 },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0xaa, 0x78, 0xf9, 0x79, 0xbd, 0xa6, 0x8b, 0x04, 0x71, 0x07,
	  0x04, 0x63, 0xbf, 0x39, 0x8f, 0xd9, 0x96, 0xb0, 0x79, 0xb5 },
	{ 0x65, 0xf2, 0x36, 0xec, 0xad, 0xab, 0x20, 0xa4, 0xd3, 0x20,
	  0x12, 0xea, 0xd0, 0xb9, 0xb9, 0x14, 0x94, 0xa8, 0x8d, 0x8a },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x11, 0x7e, 0x60, 0x65, 0x63, 0x55, 0x11, 0xa7, 0x69, 0x7c,
	  0xef, 0x0a, 0x80, 0x83, 0x59, 0x65, 0x12, 0x27, 0xb5, 0xff },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0xa8, 0xe7, 0x2c, 0xec, 0x82, 0xa7, 0x63, 0x7a, 0x11, 0xbe,
	  0xa8, 0x49, 0xba, 0xcb, 0x5c, 0x6a, 0xf0, 0xb1, 0x91, 0xbc },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0xee, 0x00, 0x26, 0xdb, 0x8e, 0x04, 0x1d, 0x2c, 0x49, 0x3f,
	  0x96, 0x2e, 0x63, 0x4c, 0x50, 0x6e, 0xed, 0x43, 0x14, 0x86 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0xa2, 0xc7, 0x69, 0xe5, 0xef, 0xba, 0xa2, 0xd3, 0xd8, 0x0b,
	  0x2c, 0xde, 0x77, 0xff, 0x53, 0x23, 0xe1, 0xf7, 0xd6, 0x86 },
	{ 0x6d, 0xe1, 0x55, 0xdc, 0x31, 0x4c, 0x0a, 0x4a, 0xdb, 0xc1,
	  0xe2, 0x9b, 0x79, 0x86, 0xb3, 0x58, 0xc3, 0xa9, 0x2b, 0xb2 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0x47, 0xdb, 0xed, 0x88, 0x96, 0xe6, 0x7b, 0xc5, 0xe5, 0xb7,
	  0xbf, 0x3b, 0xb9, 0x9d, 0x64, 0x95, 0xff, 0x64, 0x4d, 0x2f },
	{ 0x50, 0x0d, 0x72, 0xae, 0xe8, 0x93, 0xe8, 0x3e, 0x55, 0x1f,
	  0x8f, 0x50, 0xef, 0x9b, 0xa9, 0x49, 0xcd, 0x5b, 0x55, 0x94 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x17, 0xc4, 0x7d, 0x7c, 0x5f, 0x60, 0x8a, 0xe4, 0xc3, 0x7c,
	  0x41, 0x40, 0xc9, 0x82, 0x17, 0x58, 0x7d, 0x34, 0x07, 0x4d },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x34, 0x03, 0xaa, 0xc3, 0x70, 0x93, 0xe7, 0xac, 0xdf, 0x73,
	  0xc0, 0x4c, 0xba, 0x74, 0xd6, 0x22, 0xef, 0x47, 0x44, 0x47 },
	{ 0x66, 0xa0, 0x4c, 0xdc, 0xd1, 0xfd, 0x45, 0xa4, 0x06, 0x6a,
	  0xfc, 0xf9, 0x7e, 0x89, 0xcd, 0x4d, 0xfb, 0x90, 0xbd, 0x69 },
	{ 0x86, 0xf8, 0x6c, 0xd8, 0xbf, 0x0a, 0xe9, 0x1d, 0xbe, 0xad,
	  0xa7, 0x03, 0x19, 0x40, 0x43, 0xa3, 0xb0, 0x82, 0x6c, 0x8a },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0x09, 0x96, 0x7e, 0x27, 0x89, 0xdb, 0xb6, 0xf9, 0xa0, 0xef,
	  0x4e, 0x18, 0xb6, 0xb3, 0xdd, 0xb8, 0xa9, 0x26, 0x84, 0x7f },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0x0e, 0xcc, 0x47, 0x5d, 0x42, 0x4a, 0xbc, 0x2d, 0xbe, 0xff,
	  0x23, 0x81, 0x59, 0x71, 0x16, 0x8a, 0x4d, 0xd4, 0x22, 0x0b },
	{ 0x48, 0x3c, 0xce, 0xd1, 0x93, 0x38, 0x36, 0xd6, 0x71, 0x69,
	  0x5f, 0xde, 0x6d, 0x8c, 0x42, 0x4d, 0xed, 0x97, 0x9d, 0x41 },
	{ 0xba, 0x3e, 0x65, 0x52, 0x88, 0xf2, 0x85, 0xbb, 0xbc, 0x83,
	  0x31, 0xbe, 0x98, 0xbf, 0x74, 0xa1, 0x40, 0xa9, 0xa4, 0xa4 },
	{ 0x73, 0x35, 0xec, 0xb8, 0x47, 0xad, 0x78, 0xbb, 0xe6, 0x04,
	  0xb9, 0xb0, 0xfb, 0x1c, 0x9a, 0xb8, 0xc2, 0xc8, 0xbb, 0xc9 },
	{ 0xc5, 0x61, 0x49, 0x75, 0x11, 0xbb, 0xd4, 0x72, 0xe9, 0x7f,
	  0xd4, 0x40, 0x76, 0x39, 0x62, 0x92, 0x18, 0x74, 0xee, 0x4e },
	{ 0xfc, 0xc2, 0xed, 0x2d, 0xdb, 0xc8, 0xc6, 0x32, 0x69, 0xb4,
	  0x0b, 0xfb, 0x23, 0xda, 0x74, 0x52, 0x46, 0x32, 0x57, 0x37 },
	{ 0x32, 0x3d, 0x56, 0x5f, 0x62, 0x30, 0x7e, 0x60, 0x72, 0xbd,
	  0x80, 0x8e, 0xa5, 0xed, 0x40, 0x14, 0x4d, 0x05, 0x61, 0xcb },
	{ 0x10, 0x31, 0xc7, 0xe0, 0x00, 0x14, 0x20, 0xa5, 0x0c, 0x28,
	  0x29, 0xf5, 0xe8, 0xa9, 0xf5, 0xb5, 0xe3, 0xad, 0xaf, 0x01 },
	{ 0x19, 0xf9, 0x24, 0x23, 0xc9, 0xc8, 0x0a, 0x3a, 0xe3, 0x9b,
	  0xb6, 0x83, 0x19, 0x01, 0x0f, 0x3a, 0x07, 0xaf, 0x19, 0x48 },
	{ 0x68, 0x3b, 0xf5, 0xb1, 0xbb, 0x4a, 0x1e, 0x84, 0xd2, 0x67,
	  0x07, 0x35, 0x8f, 0x5c, 0xee, 0xe4, 0x8d, 0x1a, 0x40, 0x40 },
	{ 0x96, 0xbe, 0xea, 0x2f, 0x31, 0x27, 0x79, 0x0d, 0x9d, 0xaa,
	  0x89, 0x1c, 0x2b, 0x25, 0xf9, 0x3d, 0xc3, 0xf7, 0x8d, 0x33 },
	{ 0xeb, 0xe1, 0xf6, 0x24, 0x34, 0x9f, 0x34, 0xa4, 0x98, 0x9b,
	  0x6c, 0xb5, 0x4d, 0x71, 0xc6, 0x17, 0x5e, 0x49, 0xef, 0x06 },
	{ 0xb6, 0xf8, 0x03, 0x89, 0x76, 0x2c, 0x30, 0xaf, 0xa4, 0x8b,
	  0xfa, 0xd5, 0xb2, 0x1e, 0xde, 0x7b, 0x16, 0xea, 0xde, 0x00 },
	{ 0xe3, 0x5c, 0x12, 0x32, 0xe6, 0xff, 0x18, 0x07, 0x4f, 0xb2,
	  0x54, 0x72, 0x0c, 0xc0, 0x02, 0x03, 0x62, 0xfa, 0xcf, 0xb5 },
	{ 0x22, 0x35, 0xb4, 0xc4, 0x39, 0x5c, 0x8d, 0x32, 0x78, 0xd3,
	  0x18, 0xff, 0xa9, 0xa9, 0x49, 0x71, 0x6a, 0x15, 0x89, 0x86 },
	{ 0xca, 0xd0, 0xfe, 0xb1, 0x57, 0x03, 0x24, 0xae, 0xc7, 0x0a,
	  0xe6, 0x4c, 0x95, 0x77, 0xb9, 0xa2, 0x52, 0x6f, 0x82, 0x14 },
	{ 0x61, 0x28, 0xa2, 0x29, 0xa6, 0x05, 0x1f, 0x9b, 0x75, 0xee,
	  0x05, 0xe6, 0x76, 0x89, 0x4a, 0x93, 0x7a, 0xd9, 0xd3, 0xa7 },
	{ 0xba, 0xa2, 0xa7, 0xf0, 0x9d, 0x90, 0xcf, 0x88, 0x2a, 0x10,
	  0x51, 0x43, 0x1d, 0xb6, 0xe8, 0xb8, 0x14, 0x4b, 0xcc, 0xb1 },
	{ 0xac, 0x87, 0x83, 0xba, 0xb9, 0xbf, 0xb3, 0x82, 0x4b, 0x11,
	  0x21, 0x51, 0x10, 0x54, 0xc9, 0x8d, 0x1c, 0x91, 0x57, 0x93 },
	{ 0x5c, 0xd9, 0x28, 0xde, 0x94, 0xa8, 0xa7, 0x80, 0x38, 0x40,
	  0x85, 0x7b, 0x9b, 0x90, 0x4c, 0xb9, 0xac, 0x46, 0x1a, 0xe4 },
	{ 0x1b, 0xb9, 0x0e, 0x90, 0x54, 0x94, 0x30, 0xd7, 0xb6, 0xed,
	  0x22, 0x73, 0xe5, 0x0e, 0xd9, 0xf8, 0xd1, 0x89, 0x07, 0xc6 },
	{ 0xb5, 0xfb, 0x97, 0x72, 0x29, 0x80, 0x46, 0x9e, 0x98, 0x32,
	  0x16, 0xa8, 0xdd, 0x67, 0xe3, 0x08, 0xe5, 0x2f, 0xd4, 0x7b },
	{ 0x99, 0xd7, 0x23, 0x19, 0xeb, 0x25, 0x35, 0x50, 0x85, 0xb2,
	  0x07, 0xf6, 0x35, 0x05, 0x9a, 0x1e, 0xb7, 0x70, 0xea, 0xb0 },
	{ 0x43, 0x98, 0x81, 0x8b, 0x29, 0x39, 0xb0, 0xa8, 0x4a, 0xe9,
	  0xf3, 0x5e, 0x34, 0x08, 0xb6, 0x06, 0x43, 0xc9, 0x65, 0x9a },
	{ 0x22, 0x29, 0x15, 0x79, 0xf0, 0x0e, 0x98, 0x23, 0x40, 0x02,
	  0xe7, 0x42, 0x73, 0xfe, 0xc6, 0xb9, 0x19, 0xd1, 0x75, 0x95 },
	{ 0xc4, 0x01, 0xa3, 0xb4, 0xda, 0x0b, 0xba, 0xa3, 0x0d, 0x7a,
	  0xef, 0x7d, 0xd9, 0x3d, 0xc3, 0x30, 0x6d, 0xc0, 0x92, 0x58 },
	{ 0x0e, 0xbb, 0x73, 0x38, 0xf7, 0x1b, 0x31, 0x63, 0x3b, 0x08,
	  0x61, 0x2d, 0xf1, 0x70, 0xad, 0x52, 0x61, 0x9e, 0xe8, 0x35 },
	{ 0x68, 0xa8, 0xc5, 0x39, 0x6c, 0x87, 0x6d, 0xe1, 0x60, 0x7b,
	  0x1c, 0x78, 0xd2, 0x75, 0xf0, 0xb0, 0xd0, 0x15, 0x00, 0xfb },
	{ 0x98, 0xcc, 0xa6, 0xc6, 0x2c, 0xe2, 0x56, 0xfe, 0x15, 0xd0,
	  0x93, 0xde, 0x11, 0x0c, 0xda, 0x11, 0x7c, 0x5d, 0x9d, 0x1d },
	{ 0x8e, 0xe8, 0x1a, 0x0d, 0x52, 0xaa, 0x76, 0x2d, 0xc4, 0x93,
	  0x79, 0xe9, 0xb6, 0x44, 0x63, 0xd9, 0x64, 0x76, 0x8e, 0xf1 },
	{ 0xec, 0xb5, 0x72, 0x84, 0x60, 0x55, 0xaf, 0xce, 0x9a, 0x1e,
	  0x4d, 0x8b, 0x21, 0x87, 0x5e, 0x25, 0xbe, 0x0c, 0xc0, 0x4e },
	{ 0xb6, 0xf4, 0xf6, 0xbc, 0x64, 0x34, 0xc3, 0xb5, 0x2d, 0xde,
	  0xba, 0xf2, 0xca, 0xbf, 0x7f, 0x52, 0xcd, 0xe5, 0x45, 0xc1 },
	{ 0x33, 0xe2, 0xbe, 0xbd, 0xaa, 0x14, 0x04, 0xc6, 0x23, 0x96,
	  0xf4, 0x69, 0x78, 0x33, 0x48, 0xb6, 0x5d, 0x1a, 0xf0, 0xc0 },
	{ 0xee, 0x0b, 0x31, 0xc0, 0x16, 0x73, 0x23, 0xd7, 0xe4, 0x82,
	  0xb1, 0xe2, 0x38, 0xde, 0x99, 0xd7, 0x92, 0x66, 0x59, 0xab },
	{ 0x8f, 0x9e, 0xc7, 0xab, 0x3f, 0x5b, 0x97, 0xcf, 0xac, 0x1d,
	  0xba, 0x83, 0x88, 0x67, 0x90, 0xef, 0xe1, 0xef, 0xa4, 0xe1 },
	{ 0x2c, 0xf3, 0xf2, 0x12, 0x07, 0xdd, 0x8f, 0x2c, 0xba, 0x24,
	  0x10, 0x4f, 0xda, 0x7b, 0xec, 0x21, 0x01, 0x16, 0xd4, 0x45 },
	{ 0xb9, 0xe1, 0x5d, 0xe5, 0xa0, 0x83, 0xc1, 0xaa, 0x9e, 0xab,
	  0x09, 0x9b, 0x7e, 0x09, 0x39, 0x5e, 0x2c, 0xe0, 0x2b, 0xe4 },
	{ 0x43, 0x23, 0xb0, 0xaa, 0x5f, 0x01, 0x51, 0x74, 0x89, 0x04,
	  0xa4, 0xb2, 0xc4, 0x03, 0x3d, 0x5e, 0x9a, 0xf0, 0x1d, 0xbb }
};

static const uint8_t rpf__hash_lut[1][20] = {
	{ 0x88, 0xd3, 0x79, 0x3b, 0x8e, 0x7a, 0x6c, 0xac, 0xaa, 0x8b,
	  0x89, 0x28, 0x97, 0xbe, 0x72, 0x8d, 0x9e, 0x7f, 0xba, 0xd4 }
};

#define RPF__HS_CHUNK 1048576
#define RPF__HS_MAXWIN 1024
#define RPF__HS_REPORT (8 * 1048576)

/*
 * Find the windows of the file that have the digests in hashes. Test
 * the window at every offset that is a multiple of 8. Copy the window
 * for hashes[i] to out + i * window. Read the file in chunks. Carry
 * the end of each chunk to the next chunk, so that each window is
 * hashed once. Stop when all digests are found. Call progress, if not
 * NULL, once for each 8 MB. Return 0 if the file has all windows,
 * RPF_ENOTFOUND if it has not or if it does not open, RPF_EIO, or
 * RPF_ENOMEM. If the window is more than RPF__HS_MAXWIN bytes, return
 * RPF_EINVAL.
 */
static int
rpf__hash_search(const char *path, const uint8_t (*hashes)[20],
    int nhashes, size_t window, uint8_t *out,
    void (*progress)(const char *, void *), void *ud)
{
	uint8_t digest[20];
	char msg[48];
	FILE *f;
	uint8_t *buf, *found;
	size_t have, n, p;
	uint64_t total, report;
	int i, nfound, err;

	if (window > RPF__HS_MAXWIN)
		return RPF_EINVAL;
	f = fopen(path, "rb");
	if (f == NULL)
		return RPF_ENOTFOUND;
	buf = RPF_MALLOC(RPF__HS_CHUNK + RPF__HS_MAXWIN);
	found = RPF_MALLOC((size_t)nhashes);
	if (buf == NULL || found == NULL) {
		RPF_FREE(buf);
		RPF_FREE(found);
		fclose(f);
		return RPF_ENOMEM;
	}
	memset(found, 0, (size_t)nhashes);
	have = 0;
	total = 0;
	report = RPF__HS_REPORT;
	nfound = 0;
	err = 0;
	while (nfound < nhashes) {
		n = fread(buf + have, 1, RPF__HS_CHUNK, f);
		if (n == 0) {
			if (ferror(f))
				err = RPF_EIO;
			break;
		}
		have += n;
		total += n;
		/* Test each window that ends inside the buffer. */
		for (p = 0; p + window <= have && nfound < nhashes; p += 8) {
			rpf__sha1(buf + p, window, digest);
			for (i = 0; i < nhashes; i++) {
				if (found[i] ||
				    memcmp(digest, hashes[i], 20) != 0)
					continue;
				memcpy(out + (size_t)i * window, buf + p,
				    window);
				found[i] = 1;
				nfound++;
			}
		}
		if (progress != NULL && total >= report) {
			sprintf(msg, "Searching: %lu MB",
			    (unsigned long)(total >> 20));
			progress(msg, ud);
		}
		while (total >= report)
			report += RPF__HS_REPORT;
		/*
		 * Keep the bytes from the first window that is not tested.
		 * It is less than window bytes, so it fits in the overlap.
		 */
		memmove(buf, buf + p, have - p);
		have -= p;
	}
	RPF_FREE(buf);
	RPF_FREE(found);
	fclose(f);
	if (err != 0)
		return err;
	return nfound == nhashes ? 0 : RPF_ENOTFOUND;
}

/*
 * Find one part of the key material. Allocate nhashes * window bytes
 * and set *dst. On an error, set *dst to NULL.
 */
static int
rpf__extract_part(uint8_t **dst, const char *path,
    const uint8_t (*hashes)[20], int nhashes, size_t window,
    void (*progress)(const char *, void *), void *ud)
{
	int err;

	*dst = RPF_MALLOC((size_t)nhashes * window);
	if (*dst == NULL)
		return RPF_ENOMEM;
	err = rpf__hash_search(path, hashes, nhashes, window, *dst,
	    progress, ud);
	if (err != 0) {
		RPF_FREE(*dst);
		*dst = NULL;
	}
	return err;
}

int
rpf_keys_extract(RpfKeys **out, const char *exepath,
    void (*progress)(const char *, void *), void *ud)
{
	RpfKeys *k;
	uint8_t *raw;
	uint32_t *w;
	size_t i;
	int err, nfound;

	raw = NULL;
	err = rpf_keys_new(&k);
	if (err != 0) {
		*out = NULL;
		return err;
	}
	/* A part that is not found stays NULL. Only a real error stops. */
	nfound = 0;
	err = rpf__extract_part(&k->aes, exepath, rpf__hash_aes, 1, 32,
	    progress, ud);
	if (err == 0)
		nfound++;
	if (err == 0 || err == RPF_ENOTFOUND)
		err = rpf__extract_part(&k->ngkeys, exepath, rpf__hash_ngkeys,
		    101, 272, progress, ud);
	if (err == 0)
		nfound++;
	if (err == 0 || err == RPF_ENOTFOUND)
		err = rpf__extract_part(&raw, exepath, rpf__hash_dectables,
		    272, 1024, progress, ud);
	if (err == 0) {
		/* Make the little-endian words in place. */
		w = (uint32_t *)raw;
		for (i = 0; i < 17 * 16 * 256; i++)
			w[i] = (uint32_t)raw[4 * i] |
			    (uint32_t)raw[4 * i + 1] << 8 |
			    (uint32_t)raw[4 * i + 2] << 16 |
			    (uint32_t)raw[4 * i + 3] << 24;
		k->dectables = w;
		nfound++;
	}
	if (err == 0 || err == RPF_ENOTFOUND)
		err = rpf__extract_part(&k->hashlut, exepath, rpf__hash_lut,
		    1, 256, progress, ud);
	if (err == 0)
		nfound++;
	if (err == RPF_ENOTFOUND)
		err = nfound > 0 ? 0 : RPF_ENOTFOUND;
	if (err != 0) {
		rpf_keys_free(k);
		k = NULL;
	}
	*out = k;
	return err;
}

/*
 * The decode of the magic.dat resource of CodeWalker. The algorithm
 * comes from UseMagicData in GTAKeys.cs of CodeWalker.
 */

#define RPF__MG_NGKEYS (101 * 272)
#define RPF__MG_DECTABLES (17 * 16 * 256 * 4)
#define RPF__MG_HASHLUT 256
#define RPF__MG_AWCKEY 16
#define RPF__MG_PLAIN \
	(RPF__MG_NGKEYS + RPF__MG_DECTABLES + RPF__MG_HASHLUT + RPF__MG_AWCKEY)

/*
 * The state of the seeded System.Random of .NET. This is the legacy
 * subtractive generator of Knuth.
 */
struct rpf__netrand {
	int seed[56];
	int inext;
	int inextp;
};

/*
 * Return v as an int with the two's complement rule. C99 defines this
 * for every v. A plain cast of a value above INT32_MAX does not have a
 * defined result.
 */
static int
rpf__s32(uint32_t v)
{
	return v >= 0x80000000u ? -(int)(0xFFFFFFFFu - v) - 1 : (int)v;
}

/*
 * Return a - b with the wrap of 32-bit arithmetic, as unchecked .NET
 * code does. Signed overflow in C has no defined result.
 */
static int
rpf__wsub(int a, int b)
{
	return rpf__s32((uint32_t)a - (uint32_t)b);
}

/* Set the state of the generator from the seed, as .NET does. */
static void
rpf__netrand_init(struct rpf__netrand *r, int seed)
{
	int ii, mj, mk, i, k, sub;

	if (seed == INT32_MIN)
		sub = INT32_MAX;
	else
		sub = seed < 0 ? -seed : seed;
	mj = 161803398 - sub;
	r->seed[55] = mj;
	mk = 1;
	for (i = 1; i < 55; i++) {
		ii = (21 * i) % 55;
		r->seed[ii] = mk;
		mk = mj - mk;
		if (mk < 0)
			mk += INT32_MAX;
		mj = r->seed[ii];
	}
	for (k = 1; k < 5; k++) {
		for (i = 1; i < 56; i++) {
			r->seed[i] = rpf__wsub(r->seed[i],
			    r->seed[1 + (i + 30) % 55]);
			if (r->seed[i] < 0)
				r->seed[i] += INT32_MAX;
		}
	}
	r->inext = 0;
	r->inextp = 21;
}

/* Return the next sample of the generator, 0 to INT32_MAX - 1. */
static int
rpf__netrand_sample(struct rpf__netrand *r)
{
	int a, b, v;

	a = r->inext + 1;
	if (a >= 56)
		a = 1;
	b = r->inextp + 1;
	if (b >= 56)
		b = 1;
	v = rpf__wsub(r->seed[a], r->seed[b]);
	if (v == INT32_MAX)
		v--;
	if (v < 0)
		v += INT32_MAX;
	r->seed[a] = v;
	r->inext = a;
	r->inextp = b;
	return v;
}

/* Subtract n bytes of NextBytes from buf, byte by byte, modulo 256. */
static void
rpf__netrand_sub(struct rpf__netrand *r, uint8_t *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		buf[i] = (uint8_t)(buf[i] - rpf__netrand_sample(r) % 256);
}

/* Return the Jenkins one-at-a-time hash of n bytes, as JenkHash does. */
static uint32_t
rpf__jenk(const uint8_t *d, size_t n)
{
	uint32_t h;
	size_t i;

	h = 0;
	for (i = 0; i < n; i++) {
		h += d[i];
		h += h << 10;
		h ^= h >> 6;
	}
	h += h << 3;
	h ^= h >> 11;
	h += h << 15;
	return h;
}

/* Return 1 if each of the n pieces of size bytes at p has its digest. */
static int
rpf__pieces_ok(const uint8_t *p, const uint8_t (*hashes)[20], int n,
    size_t size)
{
	uint8_t digest[20];
	int i;

	for (i = 0; i < n; i++) {
		rpf__sha1(p + (size_t)i * size, size, digest);
		if (memcmp(digest, hashes[i], 20) != 0)
			return 0;
	}
	return 1;
}

int
rpf_keys_magic(RpfKeys *k, const uint8_t *magic, size_t len)
{
	struct rpf__netrand r;
	uint8_t *m, *plain, *ngkeys, *hashlut;
	const uint8_t *raw;
	uint32_t *dec;
	size_t plen, i;
	int err;

	if (k->aes == NULL)
		return RPF_ENOKEY;
	m = RPF_MALLOC(len > 0 ? len : 1);
	if (m == NULL)
		return RPF_ENOMEM;
	memcpy(m, magic, len);
	/* .NET casts the hash to int. Keep the bits of the hash. */
	rpf__netrand_init(&r, rpf__s32(rpf__jenk(k->aes, 32)));
	for (i = 0; i < 4; i++)
		rpf__netrand_sub(&r, m, len);
	rpf__aes_decrypt(k->aes, m, len);
	/* The limit stops a wrong AES key from a large output. */
	err = rpf__inflate_max(m, len, &plain, &plen, RPF__MG_PLAIN);
	RPF_FREE(m);
	if (err == RPF_ENOMEM)
		return err;
	if (err == 0 && plen != RPF__MG_PLAIN)
		err = RPF_EFORMAT;
	/* Check each part before k changes. */
	raw = NULL;
	if (err == 0) {
		raw = plain + RPF__MG_NGKEYS;
		if (!rpf__pieces_ok(plain, rpf__hash_ngkeys, 101, 272) ||
		    !rpf__pieces_ok(raw, rpf__hash_dectables, 272, 1024) ||
		    !rpf__pieces_ok(raw + RPF__MG_DECTABLES, rpf__hash_lut,
		    1, RPF__MG_HASHLUT))
			err = RPF_EFORMAT;
	}
	if (err != 0) {
		RPF_FREE(plain);
		return RPF_EFORMAT;
	}

	ngkeys = RPF_MALLOC(RPF__MG_NGKEYS);
	dec = RPF_MALLOC(RPF__MG_DECTABLES);
	hashlut = RPF_MALLOC(RPF__MG_HASHLUT);
	if (ngkeys == NULL || dec == NULL || hashlut == NULL) {
		RPF_FREE(ngkeys);
		RPF_FREE(dec);
		RPF_FREE(hashlut);
		RPF_FREE(plain);
		return RPF_ENOMEM;
	}
	memcpy(ngkeys, plain, RPF__MG_NGKEYS);
	/* Assemble the words, so the byte order is fixed. */
	for (i = 0; i < RPF__MG_DECTABLES / 4; i++)
		dec[i] = (uint32_t)raw[4 * i] |
		    (uint32_t)raw[4 * i + 1] << 8 |
		    (uint32_t)raw[4 * i + 2] << 16 |
		    (uint32_t)raw[4 * i + 3] << 24;
	memcpy(hashlut, raw + RPF__MG_DECTABLES, RPF__MG_HASHLUT);
	RPF_FREE(plain);
	RPF_FREE(k->ngkeys);
	RPF_FREE(k->dectables);
	RPF_FREE(k->hashlut);
	k->ngkeys = ngkeys;
	k->dectables = dec;
	k->hashlut = hashlut;
	return 0;
}

int
rpf_keys_derive_encrypt(RpfKeys *k, void (*progress)(const char *, void *),
    void *ud)
{
	uint32_t *enc;
	struct rpf_nglut *luts;
	int err;

	if (k->dectables == NULL)
		return RPF_ENOKEY;
	enc = RPF_MALLOC((size_t)17 * 16 * 256 * sizeof(uint32_t));
	luts = RPF_MALLOC((size_t)17 * 16 * sizeof(*luts));
	if (enc == NULL || luts == NULL) {
		RPF_FREE(enc);
		RPF_FREE(luts);
		return RPF_ENOMEM;
	}
	err = rpf__ng_derive(k->dectables, enc, luts, progress, ud);
	if (err != 0) {
		RPF_FREE(enc);
		RPF_FREE(luts);
		return err;
	}
	RPF_FREE(k->enctables);
	RPF_FREE(k->luts);
	k->enctables = enc;
	k->luts = luts;
	return 0;
}

/* Archive reading. */

#define RPF__MAGIC 0x52504637
#define RPF__DIRMARK 0x7FFFFF00

/*
 * An open archive. The entries are one allocation in the order of the
 * table of contents. The name of each entry points into names.
 */
struct rpf_archive {
	FILE *f;
	int ownsfile;		/* close f in rpf_close */
	int64_t base;		/* byte offset of the archive in f */
	int64_t size;		/* byte length of the archive */
	uint32_t enc;		/* encryption tag */
	const RpfKeys *keys;
	char *name;		/* file name of the archive */
	char *path;		/* path of the file on disk */
	struct rpf_entry *root;
	struct rpf_entry *entries;
	size_t nentries;
	char *names;		/* copy of the name blob */
};

/* Return the little-endian 32-bit value at p. */
static uint32_t
rpf__get32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	    (uint32_t)p[3] << 24;
}

/* Put the 32-bit value v at p in little-endian order. */
static void
rpf__put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

/*
 * The largest number of directory levels below the root of an archive.
 * The tree functions use recursion, so a deeper archive does not open.
 */
#define RPF__MAXDEPTH 256

/* Return the ASCII lowercase form of c. */
static int
rpf__lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Return 1 if name equals the len bytes at s. Ignore ASCII case. */
static int
rpf__name_eq(const char *name, const char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (rpf__lower((unsigned char)name[i]) !=
		    rpf__lower((unsigned char)s[i]))
			return 0;
	return name[len] == '\0';
}

/* Return 1 if the name ends in ".ysc". Ignore ASCII case. */
static int
rpf__is_ysc(const char *name)
{
	size_t n;

	n = strlen(name);
	return n >= 4 && rpf__name_eq(name + n - 4, ".ysc", 4);
}

/* Return 1 if the name ends in ".rpf". Ignore ASCII case. */
static int
rpf__is_rpf(const char *name)
{
	size_t n;

	n = strlen(name);
	return n >= 4 && rpf__name_eq(name + n - 4, ".rpf", 4);
}

/*
 * Decrypt len bytes at buf in place. Use AES if aes is set. Else use NG
 * with the key for the name and the length. The key set k can be NULL.
 * Return 0 or RPF_ENOKEY.
 */
static int
rpf__decrypt(const RpfKeys *k, int aes, const char *name, uint32_t length,
    uint8_t *buf, size_t len)
{
	uint32_t i;

	if (aes) {
		if (k == NULL || !rpf__keys_has(k, RPF__K_AES))
			return RPF_ENOKEY;
		rpf__aes_decrypt(k->aes, buf, len);
		return 0;
	}
	if (k == NULL || !rpf__keys_has(k, RPF__K_NGREAD))
		return RPF_ENOKEY;
	i = rpf__ng_keyidx(rpf__ng_hash(name, k->hashlut), length);
	rpf__ng_decrypt(buf, len, k->ngkeys + (size_t)i * 272, k->dectables);
	return 0;
}

/*
 * Read len bytes at the byte offset off in the archive. Return 0,
 * RPF_EFORMAT if the range is not in the archive, or RPF_EIO.
 */
static int
rpf__read_at(RpfArchive *a, int64_t off, uint8_t *buf, size_t len)
{
	if (off < 0 || off > a->size ||
	    (uint64_t)len > (uint64_t)(a->size - off))
		return RPF_EFORMAT;
	if (rpf__seek(a->f, a->base + off) != 0)
		return RPF_EIO;
	if (len > 0 && fread(buf, 1, len, a->f) != len)
		return RPF_EIO;
	return 0;
}

/*
 * Make the entries of the archive from the entry table ent and the name
 * blob names. Link each entry to its parent and to its children. Return
 * 0, RPF_EFORMAT, RPF_ENOMEM, RPF_ETOOBIG, or RPF_EIO. On an error,
 * rpf_close frees the parts that this function made.
 */
static int
rpf__parse_toc(RpfArchive *a, const uint8_t *ent, const uint8_t *names,
    uint32_t nent, uint32_t nlen)
{
	struct rpf_entry *e, **queue;
	const uint8_t *p;
	uint8_t *seen, b[16];
	uint64_t v, n;
	uint32_t w1, noff, idx, cnt, j;
	size_t i, head, tail, lend;
	int err, level;

	if (nent == 0)
		return RPF_EFORMAT;
	n = (uint64_t)nent * sizeof(*a->entries);
	if ((uint64_t)(size_t)n != n)
		return RPF_ETOOBIG;
	/* On a 32-bit system, nlen + 1 can be 0 in a size_t. */
	if ((size_t)nlen + 1 == 0)
		return RPF_ETOOBIG;
	seen = RPF_MALLOC(nent);
	queue = RPF_MALLOC((size_t)nent * sizeof(*queue));
	a->entries = RPF_MALLOC((size_t)n);
	a->names = RPF_MALLOC((size_t)nlen + 1);
	err = RPF_ENOMEM;
	if (seen == NULL || queue == NULL || a->entries == NULL ||
	    a->names == NULL)
		goto out;
	memset(seen, 0, nent);
	memset(a->entries, 0, (size_t)n);
	memcpy(a->names, names, nlen);
	a->names[nlen] = '\0';
	a->nentries = nent;
	a->root = a->entries;

	/* Decode each entry. */
	err = RPF_EFORMAT;
	for (i = 0; i < nent; i++) {
		p = ent + 16 * i;
		e = &a->entries[i];
		w1 = rpf__get32(p + 4);
		v = (uint64_t)w1 << 32 | rpf__get32(p);
		if (w1 == RPF__DIRMARK) {
			e->kind = RPF_KIND_DIR;
			noff = rpf__get32(p);
		} else {
			e->kind = (v >> 63) != 0 ? RPF_KIND_RESOURCE :
			    RPF_KIND_BINARY;
			noff = (uint32_t)(v & 0xFFFF);
			e->size = (uint32_t)(v >> 16 & 0xFFFFFF);
			e->offset = (uint32_t)(v >> 40 & 0xFFFFFF);
		}
		if (noff >= nlen || memchr(a->names + noff, 0, nlen - noff) ==
		    NULL)
			goto out;
		e->name = a->names + noff;
		if (e->kind == RPF_KIND_BINARY) {
			e->usize = rpf__get32(p + 8);
			e->encrypted = (int)rpf__get32(p + 12);
			if (rpf__get32(p + 12) > 1)
				goto out;
		} else if (e->kind == RPF_KIND_RESOURCE) {
			e->offset &= 0x7FFFFF;
			e->sysflags = rpf__get32(p + 8);
			e->gfxflags = rpf__get32(p + 12);
			e->encrypted = rpf__is_ysc(e->name);
			if (e->size == 0xFFFFFF) {
				/* The RSC7 header holds the real size. */
				err = rpf__read_at(a, (int64_t)e->offset * 512,
				    b, 16);
				if (err != 0)
					goto out;
				err = RPF_EFORMAT;
				e->size = (uint32_t)b[7] |
				    (uint32_t)b[14] << 8 |
				    (uint32_t)b[5] << 16 |
				    (uint32_t)b[2] << 24;
			}
		}
	}

	/* Give each directory its children. Each entry has one parent. */
	if (a->root->kind != RPF_KIND_DIR)
		goto out;
	for (i = 0; i < nent; i++) {
		e = &a->entries[i];
		if (e->kind != RPF_KIND_DIR)
			continue;
		idx = rpf__get32(ent + 16 * i + 8);
		cnt = rpf__get32(ent + 16 * i + 12);
		if (idx < 1 || (uint64_t)idx + cnt > nent ||
		    (i >= idx && i - idx < cnt))
			goto out;
		if (cnt == 0)
			continue;
		e->children = RPF_MALLOC((size_t)cnt * sizeof(*e->children));
		if (e->children == NULL) {
			err = RPF_ENOMEM;
			goto out;
		}
		e->nchildren = cnt;
		for (j = 0; j < cnt; j++) {
			if (seen[idx + j])
				goto out;
			seen[idx + j] = 1;
			e->children[j] = &a->entries[idx + j];
			e->children[j]->parent = e;
		}
	}

	/*
	 * Walk the tree from the root, one level after the other. An entry
	 * in a cycle or with no parent is not in the walk. lend is the end
	 * of the current level in the queue.
	 */
	queue[0] = a->root;
	tail = 1;
	lend = 1;
	level = 0;
	for (head = 0; head < tail; head++) {
		if (head == lend) {
			level++;
			lend = tail;
		}
		if (level > RPF__MAXDEPTH && queue[head]->kind == RPF_KIND_DIR)
			goto out;
		for (j = 0; j < queue[head]->nchildren; j++)
			queue[tail++] = queue[head]->children[j];
	}
	if (tail != nent)
		goto out;
	err = 0;
out:
	RPF_FREE(seen);
	RPF_FREE(queue);
	return err;
}

/*
 * Open the archive of size bytes at the byte offset base in f. path is
 * the path of the file on disk. name is the file name of the archive.
 * If ownsfile is set, rpf_close closes f. On an error, close f if
 * ownsfile is set. Set *out. On an error, set *out to NULL.
 */
static int
rpf__open_at(RpfArchive **out, FILE *f, int ownsfile, int64_t base,
    int64_t size, const char *path, const char *name, const RpfKeys *keys)
{
	RpfArchive *a;
	uint8_t hdr[16], *ent, *names;
	uint32_t nent, nlen;
	uint64_t elen;
	int err;

	*out = NULL;
	a = RPF_MALLOC(sizeof(*a));
	if (a == NULL) {
		if (ownsfile)
			fclose(f);
		return RPF_ENOMEM;
	}
	memset(a, 0, sizeof(*a));
	a->f = f;
	a->ownsfile = ownsfile;
	a->base = base;
	a->size = size;
	a->keys = keys;
	ent = NULL;
	names = NULL;
	a->name = RPF_MALLOC(strlen(name) + 1);
	a->path = RPF_MALLOC(strlen(path) + 1);
	err = RPF_ENOMEM;
	if (a->name == NULL || a->path == NULL)
		goto out;
	strcpy(a->name, name);
	strcpy(a->path, path);
	err = rpf__read_at(a, 0, hdr, 16);
	if (err != 0)
		goto out;
	nent = rpf__get32(hdr + 4);
	nlen = rpf__get32(hdr + 8);
	a->enc = rpf__get32(hdr + 12);
	elen = (uint64_t)nent * 16;
	err = RPF_EFORMAT;
	if (rpf__get32(hdr) != RPF__MAGIC || nent == 0 ||
	    16 + elen + nlen > (uint64_t)size)
		goto out;
	err = RPF_ETOOBIG;
	if ((uint64_t)(size_t)elen != elen)
		goto out;
	ent = RPF_MALLOC((size_t)elen);
	/* This is a temporary buffer. rpf__parse_toc keeps its own copy. */
	names = RPF_MALLOC(nlen > 0 ? (size_t)nlen : 1);
	err = RPF_ENOMEM;
	if (ent == NULL || names == NULL)
		goto out;
	err = rpf__read_at(a, 16, ent, (size_t)elen);
	if (err == 0)
		err = rpf__read_at(a, 16 + (int64_t)elen, names, nlen);
	/* Decrypt the two parts of the table separately. Unknown is NG. */
	if (err == 0 && a->enc != 0 && a->enc != RPF_ENC_OPEN) {
		err = rpf__decrypt(keys, a->enc == RPF_ENC_AES, name,
		    (uint32_t)size, ent, (size_t)elen);
		if (err == 0)
			err = rpf__decrypt(keys, a->enc == RPF_ENC_AES, name,
			    (uint32_t)size, names, nlen);
		/* A wrong key or a new file name gives a bad entry 0. */
		if (err == 0 && rpf__get32(ent + 4) != RPF__DIRMARK)
			err = RPF_ECRYPT;
	}
	if (err == 0)
		err = rpf__parse_toc(a, ent, names, nent, nlen);
out:
	RPF_FREE(ent);
	RPF_FREE(names);
	if (err != 0) {
		rpf_close(a);
		return err;
	}
	*out = a;
	return 0;
}

/* Return the file name part of path, after the last '/' or '\'. */
static const char *
rpf__basename(const char *path)
{
	const char *name, *p;

	name = path;
	for (p = path; *p != '\0'; p++)
		if (*p == '/' || *p == '\\')
			name = p + 1;
	return name;
}

int
rpf_open(RpfArchive **out, const char *path, const RpfKeys *keys)
{
	FILE *f;
	int64_t size;

	*out = NULL;
	f = fopen(path, "rb");
	if (f == NULL)
		return RPF_ENOTFOUND;
	size = -1;
	if (fseek(f, 0, SEEK_END) == 0)
		size = rpf__tell(f);
	if (size < 0) {
		fclose(f);
		return RPF_EIO;
	}
	return rpf__open_at(out, f, 1, 0, size, path, rpf__basename(path),
	    keys);
}

void
rpf_close(RpfArchive *a)
{
	size_t i;

	if (a == NULL)
		return;
	for (i = 0; i < a->nentries; i++)
		RPF_FREE(a->entries[i].children);
	RPF_FREE(a->entries);
	RPF_FREE(a->names);
	RPF_FREE(a->name);
	RPF_FREE(a->path);
	if (a->ownsfile)
		fclose(a->f);
	RPF_FREE(a);
}

struct rpf_entry *
rpf_root(RpfArchive *a)
{
	return a->root;
}

int
rpf_encryption(const RpfArchive *a)
{
	return (int)a->enc;
}

struct rpf_entry *
rpf_find(RpfArchive *a, const char *path)
{
	struct rpf_entry *e;
	size_t len, i;

	e = a->root;
	for (;;) {
		while (*path == '/' || *path == '\\')
			path++;
		if (*path == '\0')
			return e;
		len = strcspn(path, "/\\");
		for (i = 0; i < e->nchildren; i++)
			if (rpf__name_eq(e->children[i]->name, path, len))
				break;
		if (i == e->nchildren)
			return NULL;
		e = e->children[i];
		path += len;
	}
}

/*
 * Decrypt the len stored bytes at buf in place if the entry e is
 * encrypted. A .ysc resource is encrypted after its RSC7 header. Use AES
 * for an AES archive and NG for all other archives. Return 0,
 * RPF_EFORMAT if a .ysc resource has no header, or RPF_ENOKEY.
 */
static int
rpf__decrypt_entry(RpfArchive *a, const struct rpf_entry *e, uint8_t *buf,
    size_t len)
{
	int aes;

	if (!e->encrypted)
		return 0;
	aes = a->enc == RPF_ENC_AES;
	if (e->kind == RPF_KIND_BINARY)
		return rpf__decrypt(a->keys, aes, e->name, e->usize, buf, len);
	if (len < 16)
		return RPF_EFORMAT;
	return rpf__decrypt(a->keys, aes, e->name, e->size, buf + 16,
	    len - 16);
}

int
rpf_read_raw(RpfArchive *a, const struct rpf_entry *e, uint8_t **out,
    size_t *outlen)
{
	uint8_t *buf;
	int64_t off;
	size_t len;
	int err;

	*out = NULL;
	*outlen = 0;
	if (e->kind == RPF_KIND_DIR)
		return RPF_EINVAL;
	len = e->kind == RPF_KIND_BINARY && e->size == 0 ? e->usize : e->size;
	/* Check the range before the allocation. */
	off = (int64_t)e->offset * 512;
	if (off > a->size || (uint64_t)len > (uint64_t)(a->size - off))
		return RPF_EIO;
	buf = RPF_MALLOC(len > 0 ? len : 1);
	if (buf == NULL)
		return RPF_ENOMEM;
	err = rpf__read_at(a, off, buf, len);
	if (err != 0) {
		RPF_FREE(buf);
		return err;
	}
	*out = buf;
	*outlen = len;
	return 0;
}

int
rpf_read(RpfArchive *a, const struct rpf_entry *e, uint8_t **out,
    size_t *outlen)
{
	uint8_t *raw, *plain;
	size_t rawlen, plainlen, max;
	int err, sys, gfx;

	err = rpf_read_raw(a, e, &raw, &rawlen);
	if (err != 0) {
		*out = NULL;
		*outlen = 0;
		return err;
	}
	err = rpf__decrypt_entry(a, e, raw, rawlen);
	if (err == 0 && e->kind == RPF_KIND_BINARY && e->size != 0) {
		err = rpf__inflate_max(raw, rawlen, &plain, &plainlen,
		    e->usize);
		RPF_FREE(raw);
		raw = plain;
		rawlen = plainlen;
		if (err == 0 && rawlen != e->usize)
			err = RPF_EFORMAT;
	} else if (err == 0 && e->kind == RPF_KIND_RESOURCE &&
	    e->encrypted) {
		/*
		 * Only test the body. A body that does not inflate is bad.
		 * The page flags give the largest body size.
		 */
		sys = rpf_resource_size(e->sysflags);
		gfx = rpf_resource_size(e->gfxflags);
		max = sys >= 0 && gfx >= 0 ? (size_t)sys + (size_t)gfx :
		    (size_t)256 << 20;
		err = rpf__inflate_max(raw + 16, rawlen - 16, &plain,
		    &plainlen, max);
		RPF_FREE(plain);
		if (err != 0 && err != RPF_ENOMEM)
			err = RPF_ECRYPT;
	}
	if (err == 0 && e->kind == RPF_KIND_RESOURCE && rawlen >= 16) {
		/*
		 * Make the RSC7 header from the entry. The stored header can
		 * hold other bytes, for example the size of a large resource.
		 */
		rpf__put32(raw, 0x37435352);
		rpf__put32(raw + 4, (e->sysflags >> 28 & 0xF) << 4 |
		    (e->gfxflags >> 28 & 0xF));
		rpf__put32(raw + 8, e->sysflags);
		rpf__put32(raw + 12, e->gfxflags);
	}
	if (err != 0) {
		RPF_FREE(raw);
		*out = NULL;
		*outlen = 0;
		return err;
	}
	*out = raw;
	*outlen = rawlen;
	return 0;
}

int
rpf_open_nested(RpfArchive **out, RpfArchive *parent,
    const struct rpf_entry *e)
{
	int64_t off;

	*out = NULL;
	if (e->kind != RPF_KIND_BINARY || e->size != 0)
		return RPF_EINVAL;
	off = (int64_t)e->offset * 512;
	if (off > parent->size || (int64_t)e->usize > parent->size - off)
		return RPF_EFORMAT;
	return rpf__open_at(out, parent->f, 0, parent->base + off, e->usize,
	    parent->path, e->name, parent->keys);
}

#define RPF__COPY_CHUNK 1048576

/*
 * Copy len bytes at the byte offset srcoff in src to the byte offset
 * dstoff in dst. Use one buffer of 1 MB or less. If pad is set, then
 * write zero bytes up to the next multiple of 512. Return 0,
 * RPF_ENOMEM, or RPF_EIO.
 */
static int
rpf__copy_blocks(FILE *dst, int64_t dstoff, FILE *src, int64_t srcoff,
    uint64_t len, int pad)
{
	static const uint8_t zero[512] = { 0 };
	uint8_t *buf;
	uint64_t done;
	size_t n, zlen;
	int err;

	n = len < RPF__COPY_CHUNK ? (size_t)len : RPF__COPY_CHUNK;
	buf = RPF_MALLOC(n > 0 ? n : 1);
	if (buf == NULL)
		return RPF_ENOMEM;
	err = 0;
	if (rpf__seek(src, srcoff) != 0 || rpf__seek(dst, dstoff) != 0)
		err = RPF_EIO;
	for (done = 0; err == 0 && done < len; done += n) {
		n = len - done < RPF__COPY_CHUNK ? (size_t)(len - done) :
		    RPF__COPY_CHUNK;
		if (fread(buf, 1, n, src) != n || fwrite(buf, 1, n, dst) != n)
			err = RPF_EIO;
	}
	RPF_FREE(buf);
	zlen = pad ? (size_t)((512 - len % 512) % 512) : 0;
	if (err == 0 && zlen > 0 && fwrite(zero, 1, zlen, dst) != zlen)
		err = RPF_EIO;
	return err;
}

/*
 * The largest number of nested archives that rpf_extract_all opens. It
 * also makes RPF__MAXDEPTH directory levels or fewer.
 */
#define RPF__MAXNEST 32

/*
 * Return 1 if the directory d has max levels of directories or fewer
 * below it. Return 0 if not. The walk stops after max levels.
 */
static int
rpf__depth_ok(const struct rpf_entry *d, int max)
{
	size_t i;

	if (max < 0)
		return 0;
	for (i = 0; i < d->nchildren; i++)
		if (d->children[i]->kind == RPF_KIND_DIR &&
		    !rpf__depth_ok(d->children[i], max - 1))
			return 0;
	return 1;
}

/*
 * Copy the stored binary entry e of the archive a in parts to a new
 * file at path. e must not be encrypted. Return 0, RPF_ENOMEM, or
 * RPF_EIO.
 */
static int
rpf__extract_stored(RpfArchive *a, const struct rpf_entry *e,
    const char *path)
{
	int64_t off;
	FILE *f;
	int err;

	off = (int64_t)e->offset * 512;
	if (off > a->size || (int64_t)e->usize > a->size - off)
		return RPF_EIO;
	f = fopen(path, "wb");
	if (f == NULL)
		return RPF_EIO;
	err = rpf__copy_blocks(f, 0, a->f, a->base + off, e->usize, 0);
	if (fclose(f) != 0 && err == 0)
		err = RPF_EIO;
	return err;
}

/*
 * Return 1 if the archive name is safe as a file name on disk. A safe
 * name has no '/' or '\' and does not have only dots and spaces.
 * Windows removes dots and spaces at the end of a name, so ".. " is "..".
 */
static int
rpf__name_safe(const char *name)
{
	return strpbrk(name, "/\\") == NULL &&
	    name[strspn(name, ". ")] != '\0';
}

/*
 * Write the files of the directory d of the archive a under the
 * directory dir. depth is the number of directory levels of dir below
 * the directory of rpf_extract_all. nest is the number of nested
 * archives above a. Return as rpf_extract_all.
 */
static int
rpf__extract_dir(RpfArchive *a, const struct rpf_entry *d,
    const char *dir, int recurse, int depth, int nest)
{
	const struct rpf_entry *e;
	RpfArchive *c;
	uint8_t *buf;
	size_t i, len;
	char *path;
	int err;

	err = 0;
	for (i = 0; err == 0 && i < d->nchildren; i++) {
		e = d->children[i];
		if (!rpf__name_safe(e->name))
			return RPF_EFORMAT;
		err = rpf__keys_path(&path, dir, e->name);
		if (err != 0)
			return err;
		if (e->kind == RPF_KIND_DIR) {
			err = rpf__mkdir(path);
			if (err == 0)
				err = rpf__extract_dir(a, e, path, recurse,
				    depth + 1, nest);
		} else if (recurse && e->kind == RPF_KIND_BINARY &&
		    e->size == 0 && rpf__is_rpf(e->name)) {
			c = NULL;
			err = RPF_EFORMAT;
			if (nest < RPF__MAXNEST)
				err = rpf_open_nested(&c, a, e);
			/* Check the depth before the first write. */
			if (err == 0 && !rpf__depth_ok(c->root,
			    RPF__MAXDEPTH - depth - 1))
				err = RPF_EFORMAT;
			if (err == 0)
				err = rpf__mkdir(path);
			if (err == 0)
				err = rpf__extract_dir(c, c->root, path,
				    recurse, depth + 1, nest + 1);
			rpf_close(c);
		} else if (e->kind == RPF_KIND_BINARY && e->size == 0 &&
		    !e->encrypted) {
			/* Do not read a large stored entry into memory. */
			err = rpf__extract_stored(a, e, path);
		} else {
			err = rpf_read(a, e, &buf, &len);
			if (err == 0) {
				err = rpf__write_file(path, buf, len);
				RPF_FREE(buf);
			}
		}
		RPF_FREE(path);
	}
	return err;
}

int
rpf_extract_all(RpfArchive *a, const char *dir, int recurse)
{
	int err;

	if (!rpf__depth_ok(a->root, RPF__MAXDEPTH))
		return RPF_EFORMAT;
	err = rpf__mkdir_p(dir);
	if (err == 0)
		err = rpf__extract_dir(a, a->root, dir, recurse, 0, 0);
	return err;
}

int
rpf_resource_size(uint32_t flags)
{
	uint64_t n, size;

	n = (uint64_t)(flags >> 27 & 0x1) +
	    ((uint64_t)(flags >> 26 & 0x1) << 1) +
	    ((uint64_t)(flags >> 25 & 0x1) << 2) +
	    ((uint64_t)(flags >> 24 & 0x1) << 3) +
	    ((uint64_t)(flags >> 17 & 0x7F) << 4) +
	    ((uint64_t)(flags >> 11 & 0x3F) << 5) +
	    ((uint64_t)(flags >> 7 & 0xF) << 6) +
	    ((uint64_t)(flags >> 5 & 0x3) << 7) +
	    ((uint64_t)(flags >> 4 & 0x1) << 8);
	size = ((uint64_t)0x200 << (flags & 0xF)) * n;
	if (size > 0x7FFFFFFF)
		return RPF_ETOOBIG;
	return (int)size;
}

/* Tree building and archive writing. */

/*
 * Return 1 if name is a valid entry name. A valid name is not empty, has
 * no '/' or '\', and has 255 bytes or fewer. Return 0 if not.
 */
static int
rpf__name_ok(const char *name)
{
	size_t n;

	if (name == NULL)
		return 0;
	n = strlen(name);
	return n > 0 && n <= 255 && strpbrk(name, "/\\") == NULL;
}

/* Make a tree node with a copy of name. Return NULL if memory is low. */
static struct rpf_entry *
rpf__tree_node(const char *name, int kind)
{
	struct rpf_entry *e;

	e = RPF_MALLOC(sizeof(*e));
	if (e == NULL)
		return NULL;
	memset(e, 0, sizeof(*e));
	e->name = RPF_MALLOC(strlen(name) + 1);
	if (e->name == NULL) {
		RPF_FREE(e);
		return NULL;
	}
	strcpy(e->name, name);
	e->kind = kind;
	return e;
}

/*
 * Put the node e in the directory parent. If parent has a child with
 * the same name, free that child and put e in its place. On an error,
 * free e. Return 0 or RPF_ENOMEM.
 */
static int
rpf__tree_link(struct rpf_entry *parent, struct rpf_entry *e)
{
	struct rpf_entry **c;
	size_t i, n;

	e->parent = parent;
	n = parent->nchildren;
	for (i = 0; i < n; i++)
		if (rpf__name_eq(parent->children[i]->name, e->name,
		    strlen(e->name))) {
			rpf_tree_free(parent->children[i]);
			parent->children[i] = e;
			return 0;
		}
	c = RPF_REALLOC(parent->children, (n + 1) * sizeof(*c));
	if (c == NULL) {
		rpf_tree_free(e);
		return RPF_ENOMEM;
	}
	c[n] = e;
	parent->children = c;
	parent->nchildren = n + 1;
	return 0;
}

int
rpf_tree_new(struct rpf_entry **root)
{
	*root = rpf__tree_node("", RPF_KIND_DIR);
	return *root == NULL ? RPF_ENOMEM : 0;
}

/*
 * Copy the archive entry s and its children to a new tree node. A file
 * node keeps the values of s and gets its data from the archive a.
 * Return NULL if memory is low.
 */
static struct rpf_entry *
rpf__tree_copy(const struct rpf_entry *s, RpfArchive *a)
{
	struct rpf_entry *e;
	size_t i;

	e = rpf__tree_node(s->name, s->kind);
	if (e == NULL)
		return NULL;
	if (s->kind != RPF_KIND_DIR) {
		e->size = s->size;
		e->usize = s->usize;
		e->offset = s->offset;
		e->encrypted = s->encrypted;
		e->sysflags = s->sysflags;
		e->gfxflags = s->gfxflags;
		e->src = RPF_SRC_ARCHIVE;
		e->srcarchive = a;
		return e;
	}
	if (s->nchildren > 0) {
		e->children = RPF_MALLOC(s->nchildren * sizeof(*e->children));
		if (e->children == NULL) {
			rpf_tree_free(e);
			return NULL;
		}
	}
	for (i = 0; i < s->nchildren; i++) {
		e->children[i] = rpf__tree_copy(s->children[i], a);
		if (e->children[i] == NULL) {
			rpf_tree_free(e);
			return NULL;
		}
		e->children[i]->parent = e;
		e->nchildren = i + 1;
	}
	return e;
}

int
rpf_tree_from_archive(struct rpf_entry **root, RpfArchive *a)
{
	*root = rpf__tree_copy(a->root, a);
	return *root == NULL ? RPF_ENOMEM : 0;
}

int
rpf_tree_add_archive(struct rpf_entry *parent, const char *name,
    RpfArchive *a)
{
	struct rpf_entry *e;
	char *n;

	if (parent == NULL || parent->kind != RPF_KIND_DIR ||
	    !rpf__name_ok(name) || a == NULL)
		return RPF_EINVAL;
	e = rpf__tree_copy(a->root, a);
	n = RPF_MALLOC(strlen(name) + 1);
	if (e == NULL || n == NULL) {
		rpf_tree_free(e);
		RPF_FREE(n);
		return RPF_ENOMEM;
	}
	/* The copy of the root has an empty name. */
	strcpy(n, name);
	RPF_FREE(e->name);
	e->name = n;
	e->nested = 1;
	return rpf__tree_link(parent, e);
}

/*
 * Add the files and the directories under the directory dir on disk to
 * the tree directory parent. Return as rpf_tree_from_dir.
 */
static int
rpf__tree_dir(struct rpf_entry *parent, const char *dir)
{
	struct rpf__dir d;
	struct rpf_entry *e;
	const char *name;
	char *path;
	int err;

	err = rpf__dir_open(&d, dir);
	if (err != 0)
		return err;
	while (err == 0 && (name = rpf__dir_next(&d)) != NULL) {
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
			continue;
		err = rpf__keys_path(&path, dir, name);
		if (err != 0)
			break;
		if (!rpf__is_dir(path))
			err = rpf_tree_add_path(parent, name, path);
		else if ((e = rpf_tree_mkdir(parent, name)) == NULL) {
			/*
			 * shortcut: a file and a directory with names that
			 * differ only in case also give RPF_ENOMEM. Find the
			 * cause if a caller needs it.
			 */
			err = rpf__name_ok(name) ? RPF_ENOMEM : RPF_EINVAL;
		} else {
			e->nested = rpf__is_rpf(name);
			err = rpf__tree_dir(e, path);
		}
		RPF_FREE(path);
	}
	rpf__dir_close(&d);
	return err;
}

int
rpf_tree_from_dir(struct rpf_entry **root, const char *dir)
{
	int err;

	*root = NULL;
	if (dir == NULL || dir[0] == '\0')
		return RPF_EINVAL;
	err = rpf_tree_new(root);
	if (err == 0)
		err = rpf__tree_dir(*root, dir);
	if (err != 0) {
		rpf_tree_free(*root);
		*root = NULL;
	}
	return err;
}

struct rpf_entry *
rpf_tree_mkdir(struct rpf_entry *parent, const char *name)
{
	struct rpf_entry *e;
	size_t i;

	if (parent == NULL || parent->kind != RPF_KIND_DIR ||
	    !rpf__name_ok(name))
		return NULL;
	for (i = 0; i < parent->nchildren; i++) {
		e = parent->children[i];
		/* Do not replace a file with a directory. */
		if (rpf__name_eq(e->name, name, strlen(name)))
			return e->kind == RPF_KIND_DIR ? e : NULL;
	}
	e = rpf__tree_node(name, RPF_KIND_DIR);
	if (e == NULL || rpf__tree_link(parent, e) != 0)
		return NULL;
	return e;
}

int
rpf_tree_add_path(struct rpf_entry *parent, const char *name,
    const char *path)
{
	struct rpf_entry *e;

	if (parent == NULL || parent->kind != RPF_KIND_DIR ||
	    !rpf__name_ok(name) || path == NULL)
		return RPF_EINVAL;
	/* The kind is a placeholder. rpf_write finds the kind in the data. */
	e = rpf__tree_node(name, RPF_KIND_BINARY);
	if (e == NULL)
		return RPF_ENOMEM;
	e->src = RPF_SRC_PATH;
	e->srcpath = RPF_MALLOC(strlen(path) + 1);
	if (e->srcpath == NULL) {
		rpf_tree_free(e);
		return RPF_ENOMEM;
	}
	strcpy(e->srcpath, path);
	return rpf__tree_link(parent, e);
}

int
rpf_tree_add_mem(struct rpf_entry *parent, const char *name,
    const uint8_t *data, size_t len)
{
	struct rpf_entry *e;
	uint8_t *copy;

	if (parent == NULL || parent->kind != RPF_KIND_DIR ||
	    !rpf__name_ok(name) || (data == NULL && len > 0))
		return RPF_EINVAL;
	e = rpf__tree_node(name, RPF_KIND_BINARY);
	if (e == NULL)
		return RPF_ENOMEM;
	copy = RPF_MALLOC(len > 0 ? len : 1);
	if (copy == NULL) {
		rpf_tree_free(e);
		return RPF_ENOMEM;
	}
	if (len > 0)
		memcpy(copy, data, len);
	e->src = RPF_SRC_MEM;
	e->srcmem = copy;
	e->srclen = len;
	return rpf__tree_link(parent, e);
}

int
rpf_tree_remove(struct rpf_entry *e)
{
	struct rpf_entry *p;
	size_t i;

	if (e == NULL || e->parent == NULL)
		return RPF_EINVAL;
	p = e->parent;
	for (i = 0; i < p->nchildren && p->children[i] != e; i++)
		;
	if (i == p->nchildren)
		return RPF_EINVAL;
	memmove(p->children + i, p->children + i + 1,
	    (p->nchildren - i - 1) * sizeof(*p->children));
	p->nchildren--;
	rpf_tree_free(e);
	return 0;
}

void
rpf_tree_free(struct rpf_entry *root)
{
	size_t i;

	if (root == NULL)
		return;
	for (i = 0; i < root->nchildren; i++)
		rpf_tree_free(root->children[i]);
	RPF_FREE(root->children);
	RPF_FREE(root->name);
	RPF_FREE(root->srcpath);
	/* The tree owns the copy of the source data. */
	RPF_FREE((void *)root->srcmem);
	RPF_FREE(root);
}

/*
 * Return 1 if the tree node e, which is not a root, is one entry with
 * no entries below it in the table. This is a file or a nested archive
 * node. Return 0 for a directory.
 */
static int
rpf__tree_leaf(const struct rpf_entry *e)
{
	return e->kind != RPF_KIND_DIR || e->nested;
}

/*
 * Return the number of table entries for the tree e. A nested archive
 * node below e is one entry.
 */
static size_t
rpf__tree_count(const struct rpf_entry *e)
{
	size_t n, i;

	n = 1;
	for (i = 0; i < e->nchildren; i++)
		n += rpf__tree_leaf(e->children[i]) ? 1 :
		    rpf__tree_count(e->children[i]);
	return n;
}

/*
 * Compare two entries for qsort by the names as stored, byte by byte,
 * with no change of case. Directories and files are in one order.
 * CodeWalker uses this order and says that the game crashes with
 * another order.
 */
static int
rpf__entry_cmp(const void *pa, const void *pb)
{
	const struct rpf_entry *a, *b;

	a = *(struct rpf_entry *const *)pa;
	b = *(struct rpf_entry *const *)pb;
	return strcmp(a->name, b->name);
}

/*
 * Put the entries of the tree root in archive order in *out. The root
 * is at index 0. The children of each directory are one sorted run.
 * After the run, the walk goes into each child directory in order. It
 * does not go into a nested archive node, which is one entry. Set
 * (*first)[i] to the index of the first child of the directory at i.
 * Set *n to the number of entries. The caller frees *out and *first
 * with RPF_FREE. Return 0, RPF_ENOMEM, or RPF_ETOOBIG.
 */
static int
rpf__tree_flatten(const struct rpf_entry *root, struct rpf_entry ***out,
    uint32_t **first, size_t *n)
{
	struct rpf_entry **flat, *d;
	uint32_t *fst;
	size_t *stack, total, top, cnt, len, i, j;

	*out = NULL;
	*first = NULL;
	*n = 0;
	total = rpf__tree_count(root);
	if ((uint64_t)total > 0xFFFFFFFF)
		return RPF_ETOOBIG;
	flat = RPF_MALLOC(total * sizeof(*flat));
	fst = RPF_MALLOC(total * sizeof(*fst));
	stack = RPF_MALLOC(total * sizeof(*stack));
	if (flat == NULL || fst == NULL || stack == NULL) {
		RPF_FREE(flat);
		RPF_FREE(fst);
		RPF_FREE(stack);
		return RPF_ENOMEM;
	}
	/* The stack holds the indices of directories to visit. */
	flat[0] = (struct rpf_entry *)root;
	len = 1;
	stack[0] = 0;
	top = 1;
	while (top > 0) {
		i = stack[--top];
		d = flat[i];
		fst[i] = (uint32_t)len;
		cnt = d->nchildren;
		if (cnt == 0)
			continue;
		memcpy(flat + len, d->children, cnt * sizeof(*flat));
		qsort(flat + len, cnt, sizeof(*flat), rpf__entry_cmp);
		/* Push in reverse. Then the first directory pops first. */
		for (j = cnt; j > 0; j--)
			if (!rpf__tree_leaf(flat[len + j - 1]))
				stack[top++] = len + j - 1;
		len += cnt;
	}
	RPF_FREE(stack);
	*out = flat;
	*first = fst;
	*n = total;
	return 0;
}

/*
 * Encode the entry e in the 16 bytes at out. For a directory, offset is
 * the index of the first child and size is the number of children. For
 * a file, offset is the block offset of the data. A resource of
 * 0xFFFFFF bytes or more gets 0xFFFFFF in the size field. The data must
 * then hold the real size in its RSC7 header. Return 0, or RPF_ETOOBIG
 * if a value does not fit its field.
 */
static int
rpf__encode_entry(uint8_t out[16], const struct rpf_entry *e,
    uint32_t nameoff, uint32_t size, uint32_t offset, uint32_t usize,
    uint32_t sysflags, uint32_t gfxflags)
{
	uint64_t v;
	uint32_t w2, w3;

	if (e->kind == RPF_KIND_DIR) {
		rpf__put32(out, nameoff);
		rpf__put32(out + 4, RPF__DIRMARK);
		rpf__put32(out + 8, offset);
		rpf__put32(out + 12, size);
		return 0;
	}
	if (e->kind == RPF_KIND_RESOURCE) {
		/* Bit 23 of the offset field marks a resource. */
		if (offset > 0x7FFFFF)
			return RPF_ETOOBIG;
		if (size > 0xFFFFFF)
			size = 0xFFFFFF;
		offset |= 0x800000;
		w2 = sysflags;
		w3 = gfxflags;
	} else {
		w2 = usize;
		w3 = e->encrypted != 0;
	}
	if (nameoff > 0xFFFF || size > 0xFFFFFF || offset > 0xFFFFFF)
		return RPF_ETOOBIG;
	v = (uint64_t)nameoff | (uint64_t)size << 16 | (uint64_t)offset << 40;
	rpf__put32(out, (uint32_t)v);
	rpf__put32(out + 4, (uint32_t)(v >> 32));
	rpf__put32(out + 8, w2);
	rpf__put32(out + 12, w3);
	return 0;
}

/*
 * Write the len bytes at data at the byte offset off in f. Then write
 * zero bytes up to the next multiple of 512. Return 0 or RPF_EIO.
 */
static int
rpf__write_data(FILE *f, int64_t off, const uint8_t *data, size_t len)
{
	static const uint8_t zero[512] = { 0 };
	size_t pad;

	pad = (512 - len % 512) % 512;
	if (rpf__seek(f, off) != 0 ||
	    (len > 0 && fwrite(data, 1, len, f) != len) ||
	    (pad > 0 && fwrite(zero, 1, pad, f) != pad))
		return RPF_EIO;
	return 0;
}

/* Return 1 if the name ends in ".rpf" or ".awc". Ignore ASCII case. */
static int
rpf__is_stored(const char *name)
{
	size_t n;

	n = strlen(name);
	return n >= 4 && (rpf__name_eq(name + n - 4, ".rpf", 4) ||
	    rpf__name_eq(name + n - 4, ".awc", 4));
}

/*
 * Encrypt len bytes at buf in place for the encryption enc. Use AES if
 * enc is RPF_ENC_AES. Else use NG with the key for the name and the
 * length. The key set k must have the necessary parts.
 */
static void
rpf__encrypt(const RpfKeys *k, int enc, const char *name, uint32_t length,
    uint8_t *buf, size_t len)
{
	uint32_t i;

	if (enc == RPF_ENC_AES) {
		rpf__aes_encrypt(k->aes, buf, len);
		return;
	}
	i = rpf__ng_keyidx(rpf__ng_hash(name, k->hashlut), length);
	rpf__ng_encrypt(buf, len, k->ngkeys + (size_t)i * 272, k->enctables,
	    k->luts);
}

/*
 * Return 1 if the paths a and b have the same bytes. A '/' and a '\'
 * are equal. Return 0 if not.
 */
static int
rpf__same_path(const char *a, const char *b)
{
	for (; *a != '\0' && *b != '\0'; a++, b++)
		if (*a != *b && !((*a == '/' || *a == '\\') &&
		    (*b == '/' || *b == '\\')))
			return 0;
	return *a == *b;
}

/*
 * Return 1 if rpf_write copies the stored bytes of the file e with no
 * change for the encryption enc. This is true for a file from an
 * archive if the file is not encrypted or if the archive has the
 * encryption enc. An archive with the tag 0 is OPEN. An unknown tag is
 * NG.
 */
static int
rpf__is_raw(const struct rpf_entry *e, int enc)
{
	uint32_t m;

	if (e->src != RPF_SRC_ARCHIVE)
		return 0;
	if (!e->encrypted)
		return 1;
	m = e->srcarchive->enc;
	if (m == 0)
		m = RPF_ENC_OPEN;
	else if (m != RPF_ENC_OPEN && m != RPF_ENC_AES)
		m = RPF_ENC_NG;
	return m == (uint32_t)enc;
}

/*
 * Make the bytes to store for the file entry e from its path or memory
 * source. Data that starts with an RSC7 header is a resource and goes
 * in as it is. A resource of 0xFFFFFF bytes or more gets its real size
 * in the RSC7 header bytes 7, 14, 5, and 2. Other data is a binary
 * entry. A binary entry is compressed, but a .rpf or .awc entry or data
 * that does not compress to less than 0x1000000 bytes is stored. Set
 * *data to a buffer that the caller frees with RPF_FREE. Set the other
 * values for the entry. Return 0, RPF_EINVAL if e has no such source,
 * RPF_ETOOBIG, RPF_ENOMEM, or an error of rpf__read_file.
 */
static int
rpf__entry_payload(const struct rpf_entry *e, uint8_t **data, size_t *len,
    int *kind, uint32_t *size, uint32_t *usize, uint32_t *sysflags,
    uint32_t *gfxflags)
{
	const uint8_t *in;
	uint8_t *own, *z;
	size_t n, zlen;
	int err;

	*data = NULL;
	*len = 0;
	*kind = RPF_KIND_BINARY;
	*size = *usize = *sysflags = *gfxflags = 0;
	own = NULL;
	if (e->src == RPF_SRC_PATH) {
		err = rpf__read_file(e->srcpath, &own, &n);
		if (err != 0)
			return err;
		in = own;
	} else if (e->src == RPF_SRC_MEM) {
		in = e->srcmem;
		n = e->srclen;
	} else
		return RPF_EINVAL;
	if ((uint64_t)n > 0xFFFFFFFF) {
		RPF_FREE(own);
		return RPF_ETOOBIG;
	}
	if (n >= 16 && memcmp(in, "RSC7", 4) == 0) {
		*kind = RPF_KIND_RESOURCE;
		*size = (uint32_t)n;
		*sysflags = rpf__get32(in + 8);
		*gfxflags = rpf__get32(in + 12);
	} else {
		*usize = (uint32_t)n;
		if (!rpf__is_stored(e->name)) {
			err = rpf_deflate(in, n, &z, &zlen);
			if (err != 0) {
				RPF_FREE(own);
				return err;
			}
			if (zlen < 0x1000000) {
				RPF_FREE(own);
				*data = z;
				*len = zlen;
				*size = (uint32_t)zlen;
				return 0;
			}
			RPF_FREE(z);
		}
	}
	if (own == NULL) {
		own = RPF_MALLOC(n > 0 ? n : 1);
		if (own == NULL)
			return RPF_ENOMEM;
		if (n > 0)
			memcpy(own, in, n);
	}
	if (*kind == RPF_KIND_RESOURCE && n >= 0xFFFFFF) {
		own[7] = (uint8_t)n;
		own[14] = (uint8_t)(n >> 8);
		own[5] = (uint8_t)(n >> 16);
		own[2] = (uint8_t)(n >> 24);
	}
	*data = own;
	*len = n;
	return 0;
}

/*
 * Copy the file at src in parts to the byte offset pos in f as a stored
 * binary entry with the name of e. A stored file has no change. pos /
 * 512 must fit in 24 bits. Encode the table entry at out and set *len
 * as rpf__write_entry does.
 */
static int
rpf__write_stored(FILE *f, int64_t pos, const struct rpf_entry *e,
    const char *src, uint32_t nameoff, uint8_t out[16], uint64_t *len)
{
	struct rpf_entry tmp;
	int64_t off;
	FILE *in;
	int err;

	*len = 0;
	in = fopen(src, "rb");
	if (in == NULL)
		return RPF_ENOTFOUND;
	off = -1;
	if (fseek(in, 0, SEEK_END) == 0)
		off = rpf__tell(in);
	err = off < 0 ? RPF_EIO : (uint64_t)off > 0xFFFFFFFF ?
	    RPF_ETOOBIG : 0;
	tmp = *e;
	tmp.kind = RPF_KIND_BINARY;
	tmp.encrypted = 0;
	if (err == 0)
		err = rpf__encode_entry(out, &tmp, nameoff, 0,
		    (uint32_t)((uint64_t)pos / 512), (uint32_t)off, 0, 0);
	if (err == 0) {
		*len = (uint64_t)off;
		err = rpf__copy_blocks(f, pos, in, 0, *len, 1);
	}
	fclose(in);
	return err;
}

/*
 * Write the data of the file e at the byte offset pos in f for the
 * encryption enc. Encode the table entry of e with the name offset
 * nameoff at out. Set *len to the number of data bytes before the
 * padding. Return 0 or an error of rpf_write.
 */
static int
rpf__write_entry(FILE *f, int64_t pos, const struct rpf_entry *e,
    uint32_t nameoff, int enc, const RpfKeys *keys, uint8_t out[16],
    uint64_t *len)
{
	struct rpf_entry tmp;
	RpfArchive *a;
	uint8_t *data, *exp;
	uint32_t size, usize, sysflags, gfxflags, block;
	int64_t off;
	size_t n;
	int err;

	*len = 0;
	if ((uint64_t)pos / 512 > 0xFFFFFF)
		return RPF_ETOOBIG;
	block = (uint32_t)((uint64_t)pos / 512);
	tmp = *e;
	if (rpf__is_raw(e, enc)) {
		/* Copy the stored bytes. Keep the values of the entry. */
		a = e->srcarchive;
		*len = e->kind == RPF_KIND_BINARY && e->size == 0 ? e->usize :
		    e->size;
		off = (int64_t)e->offset * 512;
		if (off > a->size || (int64_t)*len > a->size - off)
			return RPF_EIO;
		err = rpf__encode_entry(out, e, nameoff, e->size, block,
		    e->usize, e->sysflags, e->gfxflags);
		if (err == 0)
			err = rpf__copy_blocks(f, pos, a->f, a->base + off,
			    *len, 1);
		return err;
	}
	if (e->src == RPF_SRC_PATH && rpf__is_stored(e->name))
		return rpf__write_stored(f, pos, e, e->srcpath, nameoff, out,
		    len);
	exp = NULL;
	if (e->src == RPF_SRC_ARCHIVE) {
		/* Use the export form as a memory source. */
		err = rpf_read(e->srcarchive, e, &exp, &n);
		if (err != 0)
			return err;
		tmp.src = RPF_SRC_MEM;
		tmp.srcmem = exp;
		tmp.srclen = n;
	}
	err = rpf__entry_payload(&tmp, &data, &n, &tmp.kind, &size, &usize,
	    &sysflags, &gfxflags);
	RPF_FREE(exp);
	if (err != 0)
		return err;
	/* An OPEN archive has no encrypted binary entries. */
	tmp.encrypted = tmp.kind == RPF_KIND_BINARY && e->encrypted &&
	    enc != RPF_ENC_OPEN;
	if (tmp.encrypted)
		rpf__encrypt(keys, enc, e->name, usize, data, n);
	else if (tmp.kind == RPF_KIND_RESOURCE && rpf__is_ysc(e->name))
		rpf__encrypt(keys, enc, e->name, (uint32_t)n, data + 16,
		    n - 16);
	err = rpf__encode_entry(out, &tmp, nameoff, size, block, usize,
	    sysflags, gfxflags);
	if (err == 0)
		err = rpf__write_data(f, pos, data, n);
	RPF_FREE(data);
	*len = n;
	return err;
}

static int rpf__write_tree(const struct rpf_entry *root, const char *path,
    const char *ngname, int enc, const RpfKeys *keys);

/*
 * Write the nested archive node e at the byte offset pos in f. First
 * write the tree e to the temporary file "<path>.<index>.tmp" with the
 * encryption enc, the keys, and the name of e as the NG name. Then copy
 * that file in as a stored binary entry and remove it. Encode the table
 * entry at out and set *len as rpf__write_entry does.
 */
static int
rpf__write_nested(FILE *f, int64_t pos, const struct rpf_entry *e,
    uint32_t nameoff, const char *path, size_t index, int enc,
    const RpfKeys *keys, uint8_t out[16], uint64_t *len)
{
	char *tmp;
	int err;

	*len = 0;
	if ((uint64_t)pos / 512 > 0xFFFFFF)
		return RPF_ETOOBIG;
	/* The index has 10 digits or fewer. */
	tmp = RPF_MALLOC(strlen(path) + 16);
	if (tmp == NULL)
		return RPF_ENOMEM;
	sprintf(tmp, "%s.%lu.tmp", path, (unsigned long)index);
	err = rpf__write_tree(e, tmp, e->name, enc, keys);
	if (err == 0)
		err = rpf__write_stored(f, pos, e, tmp, nameoff, out, len);
	remove(tmp);
	RPF_FREE(tmp);
	return err;
}

/*
 * Check the names and the sources of the files under the tree directory
 * d before rpf_write makes the file at path. Go into the nested archive
 * nodes too. havekeys is 1 if the keys have the parts for the
 * encryption enc. Return 0, RPF_EINVAL, or RPF_ENOKEY.
 */
static int
rpf__tree_check(const struct rpf_entry *d, const char *path, int enc,
    int havekeys)
{
	const struct rpf_entry *e;
	size_t i;
	int err;

	for (i = 0; i < d->nchildren; i++) {
		e = d->children[i];
		/* Do not write over a source archive. */
		if (!rpf__name_ok(e->name) || (e->src == RPF_SRC_ARCHIVE &&
		    rpf__same_path(path, e->srcarchive->path)))
			return RPF_EINVAL;
		if (e->kind == RPF_KIND_DIR) {
			err = rpf__tree_check(e, path, enc, havekeys);
			if (err != 0)
				return err;
		} else if (!havekeys && rpf__is_ysc(e->name) &&
		    !rpf__is_raw(e, enc))
			return RPF_ENOKEY;
	}
	return 0;
}

/*
 * Write the tree root as an archive at path with the encryption enc.
 * ngname is the archive name for the NG key. Return as rpf_write.
 */
static int
rpf__write_tree(const struct rpf_entry *root, const char *path,
    const char *ngname, int enc, const RpfKeys *keys)
{
	struct rpf_entry **flat;
	uint32_t *first, *noff;
	uint8_t *toc;
	uint64_t nlen, toclen, len;
	int64_t pos;
	size_t n, i;
	int err, pass, need, havekeys;
	FILE *f;

	if (root == NULL || root->kind != RPF_KIND_DIR || path == NULL ||
	    (enc != RPF_ENC_OPEN && enc != RPF_ENC_AES && enc != RPF_ENC_NG))
		return RPF_EINVAL;
	/* rpf_open does not accept a deeper archive. */
	if (!rpf__depth_ok(root, RPF__MAXDEPTH))
		return RPF_EINVAL;
	err = rpf__tree_flatten(root, &flat, &first, &n);
	if (err != 0)
		return err;
	toc = NULL;
	f = NULL;
	noff = RPF_MALLOC(n * sizeof(*noff));
	err = RPF_ENOMEM;
	if (noff == NULL)
		goto out;

	/*
	 * Check the tree and the keys before the output file opens. AES
	 * encrypts the .ysc bodies in an AES archive. NG encrypts them in
	 * all other archives.
	 */
	need = enc == RPF_ENC_AES ? RPF__K_AES : RPF__K_NGWRITE;
	havekeys = keys != NULL && rpf__keys_has(keys, need);
	err = RPF_ENOKEY;
	if (enc != RPF_ENC_OPEN && !havekeys)
		goto out;
	err = rpf__tree_check(root, path, enc, havekeys);
	if (err != 0)
		goto out;

	/*
	 * The root has the empty name at offset 0. Put the file names
	 * first, because a file entry has a 16-bit name offset.
	 */
	nlen = 1;
	noff[0] = 0;
	for (pass = 0; pass < 2; pass++)
		for (i = 1; i < n; i++) {
			if (rpf__tree_leaf(flat[i]) == pass)
				continue;
			noff[i] = (uint32_t)nlen;
			nlen += strlen(flat[i]->name) + 1;
		}
	toclen = (16 + 16 * (uint64_t)n + nlen + 511) / 512 * 512;
	err = RPF_ETOOBIG;
	if (nlen > 0xFFFFFFFF || (uint64_t)(size_t)toclen != toclen)
		goto out;
	toc = RPF_MALLOC((size_t)toclen);
	err = RPF_ENOMEM;
	if (toc == NULL)
		goto out;
	memset(toc, 0, (size_t)toclen);
	rpf__put32(toc, RPF__MAGIC);
	rpf__put32(toc + 4, (uint32_t)n);
	rpf__put32(toc + 8, (uint32_t)nlen);
	rpf__put32(toc + 12, (uint32_t)enc);
	for (i = 1; i < n; i++)
		strcpy((char *)toc + 16 + 16 * n + noff[i], flat[i]->name);

	err = RPF_EIO;
	f = fopen(path, "wb");
	if (f == NULL)
		goto out;
	/* Write the data of each file after the table of contents. */
	pos = (int64_t)toclen;
	for (i = 0; i < n; i++) {
		/* The root is a directory also if it is a nested node. */
		if (i == 0 || !rpf__tree_leaf(flat[i])) {
			err = rpf__encode_entry(toc + 16 + 16 * i, flat[i],
			    noff[i], (uint32_t)flat[i]->nchildren, first[i],
			    0, 0, 0);
			continue;
		}
		if (flat[i]->kind == RPF_KIND_DIR)
			err = rpf__write_nested(f, pos, flat[i], noff[i], path,
			    i, enc, keys, toc + 16 + 16 * i, &len);
		else
			err = rpf__write_entry(f, pos, flat[i], noff[i], enc,
			    keys, toc + 16 + 16 * i, &len);
		if (err != 0)
			goto out;
		pos += (int64_t)((len + 511) / 512 * 512);
	}
	/*
	 * Encrypt the entry table and the name blob separately. The NG key
	 * needs the final size of the file. The padding stays plain.
	 */
	if (enc != RPF_ENC_OPEN) {
		rpf__encrypt(keys, enc, ngname, (uint32_t)pos, toc + 16,
		    16 * n);
		rpf__encrypt(keys, enc, ngname, (uint32_t)pos,
		    toc + 16 + 16 * n, (size_t)nlen);
	}
	err = rpf__write_data(f, 0, toc, (size_t)toclen);
out:
	if (f != NULL) {
		if (fclose(f) != 0 && err == 0)
			err = RPF_EIO;
		if (err != 0)
			remove(path);
	}
	RPF_FREE(flat);
	RPF_FREE(first);
	RPF_FREE(noff);
	RPF_FREE(toc);
	return err;
}

int
rpf_write(const struct rpf_entry *root, const char *path, int encryption,
    const RpfKeys *keys)
{
	if (path == NULL)
		return RPF_EINVAL;
	return rpf__write_tree(root, path, rpf__basename(path), encryption,
	    keys);
}

const char *
rpf_strerror(int code)
{
	switch (code) {
	case 0:
		return "no error";
	case RPF_EIO:
		return "i/o error";
	case RPF_ENOMEM:
		return "out of memory";
	case RPF_EFORMAT:
		return "bad archive format";
	case RPF_ENOKEY:
		return "key material missing";
	case RPF_ECRYPT:
		return "decryption failed";
	case RPF_ENOTFOUND:
		return "not found";
	case RPF_EINVAL:
		return "invalid argument";
	case RPF_ETOOBIG:
		return "size too big";
	default:
		return "unknown error";
	}
}

#endif /* RPF_IMPLEMENTATION */
