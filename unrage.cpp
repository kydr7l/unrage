/*
 * unrage - list, test, extract, add, and delete the files of GTA 5 RPF
 * archives. The commands and the switches are as in 7z.
 */
extern "C" {
#include "rpf.h"
}

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace fs = std::filesystem;

/* rpf.c uses the default RPF_FREE, so free() frees the buffers of rpf_read. */
#define PATH_LEN 4096

enum { CMD_LIST, CMD_TEST };

struct totals {
	unsigned long long size, stored;
	unsigned long files, dirs, errors;
};

/* The open nested archives that a tree reads from. */
struct nested {
	RpfArchive **v;
	size_t len;
};

static RpfKeys *keys;
static const char *gtaexe;	/* -g<path>: the only GTA5.exe to use */
static const char *keynote;	/* the reason that the last fetch failed */
static bool cached;		/* keys came from the cache folder or a fetch */
static const char *outdir = ".";
static int recurse;
static int encryption = -1;	/* -1 keeps the encryption of the archive */

static void
usage(void)
{
	fputs(
"usage: unrage <command> [switches] <archive> [paths...]\n"
"\n"
"commands:\n"
"  l   list the entries\n"
"  t   test the archive: read and decode each file\n"
"  e   extract files to one directory, with no paths\n"
"  x   extract files with full paths\n"
"  a   add files and directories, make the archive if necessary\n"
"  d   delete entries\n"
"  fix write the archive and its nested archives again as NG\n"
"\n"
"switches:\n"
"  -o<dir>          write extracted files under dir (default .)\n"
"  -k<dir>          load the keys from dir (default ./keys if it exists)\n"
"  -g<path>         make the keys from this GTA5.exe if necessary\n"
"  -r               go into nested .rpf archives (l, t, e, x)\n"
"  -t<open|aes|ng>  encryption of the new archive (a, d, fix)\n"
"  --               stop the parse of switches\n", stderr);
}

static int
fail(const char *what, int err)
{
	fprintf(stderr, "unrage: %s: %s\n", what, rpf_strerror(err));
	if (err == RPF_ENOKEY && keynote != nullptr)
		fprintf(stderr, "unrage: keys: %s\n", keynote);
	return 1;
}

/* Compare n bytes of a and b. Ignore ASCII case, as rpf.h does. */
static int
ieqn(const char *a, const char *b, size_t n)
{
	for (; n > 0; n--, a++, b++) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
		if (*a == '\0')
			return 1;
	}
	return 1;
}

static int
is_rpf_name(const char *name)
{
	size_t n = strlen(name);

	return n >= 4 && ieqn(name + n - 4, ".rpf", 5);
}

/* The same check as rpf__name_safe: no separator, not only dots and spaces. */
static int
name_safe(const char *name)
{
	return strpbrk(name, "/\\") == NULL &&
	    name[strspn(name, ". ")] != '\0';
}

/* Append '/' and s to the path in buf. Return -1 if buf is too small. */
static int
path_add(char *buf, const char *s)
{
	size_t len = strlen(buf);
	int w = snprintf(buf + len, PATH_LEN - len, "%s%s", len ? "/" : "", s);

	if (w < 0 || (size_t)w >= PATH_LEN - len) {
		buf[len] = '\0';
		return -1;
	}
	return 0;
}

/* Put the path of e in the archive in buf. Return -1 if it is too long. */
static int
entry_path(const struct rpf_entry *e, char *buf)
{
	if (e->parent == NULL) {
		buf[0] = '\0';
		return 0;
	}
	if (entry_path(e->parent, buf) != 0)
		return -1;
	return path_add(buf, e->name);
}

static unsigned long long
entry_size(const struct rpf_entry *e)
{
	int s, g;

	if (e->kind != RPF_KIND_RESOURCE)
		return e->usize;
	s = rpf_resource_size(e->sysflags);
	g = rpf_resource_size(e->gfxflags);
	return (unsigned long long)(s > 0 ? s : 0) + (g > 0 ? g : 0);
}

/* A binary entry that is stored with no compression has size 0. */
static unsigned long long
entry_stored(const struct rpf_entry *e)
{
	return e->size ? e->size : e->usize;
}

/* Open e as a nested archive if -r is set and e is a .rpf entry. */
static RpfArchive *
open_nested(RpfArchive *a, const struct rpf_entry *e)
{
	RpfArchive *n;

	if (!recurse || e->kind != RPF_KIND_BINARY || !is_rpf_name(e->name))
		return NULL;
	return rpf_open_nested(&n, a, e) == 0 ? n : NULL;
}

/*
 * A nested archive can point at its parent. The limit stops the loop.
 * rpf_extract_all has the same limit.
 */
constexpr int MAXNEST = 32;

static void
too_deep(const char *what)
{
	fprintf(stderr, "unrage: %s: more than %d levels of nested archives\n",
	    what, MAXNEST);
}

/*
 * List or test the children of d. buf holds the path of d. level is the
 * number of nested archives above d.
 */
static void
walk(RpfArchive *a, const struct rpf_entry *d, char *buf, int cmd,
    struct totals *t, int level)
{
	const struct rpf_entry *e;
	RpfArchive *n;
	uint8_t *data;
	size_t i, len, dlen;
	int err;

	len = strlen(buf);
	for (i = 0; i < d->nchildren; i++) {
		e = d->children[i];
		if (path_add(buf, e->name) != 0) {
			fprintf(stderr, "unrage: %s/%s: path too long\n", buf,
			    e->name);
			t->errors++;
			continue;
		}
		n = open_nested(a, e);
		if (n != NULL && level >= MAXNEST) {
			too_deep(buf);
			t->errors++;
			rpf_close(n);
			n = NULL;
		}
		if (cmd == CMD_LIST) {
			if (e->kind == RPF_KIND_DIR)
				printf("%12s %12s  D.  %s\n", "", "", buf);
			else
				printf("%12llu %12llu  %c%c  %s\n",
				    entry_size(e), entry_stored(e),
				    e->kind == RPF_KIND_RESOURCE ? 'R' : 'B',
				    e->encrypted ? 'E' : '.', buf);
		}
		if (e->kind == RPF_KIND_DIR) {
			t->dirs++;
			walk(a, e, buf, cmd, t, level);
		} else {
			t->files++;
			t->size += entry_size(e);
			t->stored += entry_stored(e);
			if (n != NULL) {
				walk(n, rpf_root(n), buf, cmd, t, level + 1);
			} else if (cmd == CMD_TEST) {
				err = rpf_read(a, e, &data, &dlen);
				if (err != 0) {
					fail(buf, err);
					t->errors++;
				}
				free(data);
			}
		}
		rpf_close(n);
		buf[len] = '\0';
	}
}

static const char *
encryption_name(int enc)
{
	switch (enc) {
	case RPF_ENC_OPEN: return "OPEN";
	case RPF_ENC_AES: return "AES";
	case RPF_ENC_NG: return "NG";
	}
	return "NONE";
}

static int
cmd_list(const char *path, int cmd)
{
	RpfArchive *a;
	struct totals t = {0, 0, 0, 0, 0};
	char buf[PATH_LEN] = "";
	int err;

	if ((err = rpf_open(&a, path, keys)) != 0)
		return fail(path, err);
	printf("Archive: %s\nEncryption: %s\n\n", path,
	    encryption_name(rpf_encryption(a)));
	if (cmd == CMD_LIST)
		printf("%12s %12s  Attr  Name\n"
		    "------------ ------------  ----  ------------\n",
		    "Size", "Stored");
	walk(a, rpf_root(a), buf, cmd, &t, 0);
	if (cmd == CMD_LIST)
		printf("------------ ------------  ----  ------------\n"
		    "%12llu %12llu        %lu files, %lu folders\n",
		    t.size, t.stored, t.files, t.dirs);
	else if (t.errors == 0)
		printf("Everything is Ok: %lu files\n", t.files);
	else
		printf("%lu errors in %lu files\n", t.errors, t.files);
	rpf_close(a);
	return t.errors != 0;
}

/* Write the export form of the file e to outdir/rel. */
static int
write_out(RpfArchive *a, const struct rpf_entry *e, const char *rel)
{
	char dest[PATH_LEN];
	std::error_code ec;
	uint8_t *data;
	size_t len;
	FILE *f;
	int err, w;

	w = snprintf(dest, sizeof dest, "%s/%s", outdir, rel);
	if (w < 0 || (size_t)w >= sizeof dest) {
		fprintf(stderr, "unrage: %s: path too long\n", rel);
		return 1;
	}
	if ((err = rpf_read(a, e, &data, &len)) != 0)
		return fail(rel, err);
	fs::create_directories(fs::path(dest).parent_path(), ec);
	f = fopen(dest, "wb");
	if (f == NULL || fwrite(data, 1, len, f) != len) {
		free(data);
		if (f != NULL)
			fclose(f);
		return fail(dest, RPF_EIO);
	}
	free(data);
	if (fclose(f) != 0)
		return fail(dest, RPF_EIO);
	printf("%s\n", rel);
	return 0;
}

/*
 * Extract the entry e and all entries under it. buf holds the path of e
 * in the archive. If flat is set, put each file in outdir with its own
 * name only. level is the number of nested archives above e. Return the
 * number of errors.
 */
static unsigned long
extract(RpfArchive *a, const struct rpf_entry *e, char *buf, int flat,
    int level)
{
	const struct rpf_entry *d;
	RpfArchive *n;
	unsigned long errors = 0;
	size_t i, len;

	/* A bad name can write outside of outdir. */
	if (e->parent != NULL && !name_safe(e->name)) {
		fprintf(stderr, "unrage: %s: unsafe name\n", buf);
		return 1;
	}
	n = open_nested(a, e);
	if (n != NULL && level >= MAXNEST) {
		too_deep(buf);
		rpf_close(n);
		return 1;
	}
	if (e->kind != RPF_KIND_DIR && n == NULL)
		return write_out(a, e, flat ? e->name : buf);
	d = n != NULL ? rpf_root(n) : e;
	len = strlen(buf);
	for (i = 0; i < d->nchildren; i++) {
		if (path_add(buf, d->children[i]->name) != 0) {
			fprintf(stderr, "unrage: %s: path too long\n", buf);
			errors++;
			continue;
		}
		errors += extract(n != NULL ? n : a, d->children[i], buf, flat,
		    n != NULL ? level + 1 : level);
		buf[len] = '\0';
	}
	rpf_close(n);
	return errors;
}

static int
cmd_extract(const char *path, char **paths, int npaths, int flat)
{
	const struct rpf_entry *e;
	RpfArchive *a;
	unsigned long errors = 0;
	char buf[PATH_LEN];
	int err, i;

	if ((err = rpf_open(&a, path, keys)) != 0)
		return fail(path, err);
	if (npaths == 0 && !flat) {
		/* rpf_extract_all copies large nested archives in parts. */
		err = rpf_extract_all(a, outdir, recurse);
		rpf_close(a);
		if (err != 0)
			return fail(path, err);
		printf("Everything is Ok\n");
		return 0;
	}
	if (npaths == 0) {
		buf[0] = '\0';
		errors = extract(a, rpf_root(a), buf, flat, 0);
	}
	for (i = 0; i < npaths; i++) {
		if ((e = rpf_find(a, paths[i])) == NULL) {
			fail(paths[i], RPF_ENOTFOUND);
			errors++;
		} else if (entry_path(e, buf) != 0) {
			fprintf(stderr, "unrage: %s: path too long\n", paths[i]);
			errors++;
		} else {
			errors += extract(a, e, buf, flat, 0);
		}
	}
	rpf_close(a);
	if (errors != 0) {
		printf("%lu errors\n", errors);
		return 1;
	}
	printf("Everything is Ok\n");
	return 0;
}

/* Return the child of the tree directory d with the n bytes of name. */
static struct rpf_entry *
tree_child(struct rpf_entry *d, const char *name, size_t n)
{
	size_t i;

	for (i = 0; i < d->nchildren; i++)
		if (strlen(d->children[i]->name) == n &&
		    ieqn(d->children[i]->name, name, n))
			return d->children[i];
	return NULL;
}

/* Find path in a tree. The separators are '/' and '\'. */
static struct rpf_entry *
tree_find(struct rpf_entry *root, const char *path)
{
	size_t n;

	while (root != NULL && *path != '\0') {
		n = strcspn(path, "/\\");
		if (n > 0) {
			if (root->kind != RPF_KIND_DIR)
				return NULL;
			root = tree_child(root, path, n);
		}
		path += n + (path[n] != '\0');
	}
	return root;
}

/* Add the directory src on disk to parent as name, with all its files. */
static int
add_dir(struct rpf_entry *parent, const fs::path &src, const char *name)
{
	struct rpf_entry *d, *old;
	std::error_code ec;
	fs::directory_iterator it(src, ec), end;
	int err;

	if (ec)
		return RPF_ENOTFOUND;
	/* A directory replaces a file with the same name, as a file does. */
	old = tree_child(parent, name, strlen(name));
	if (old != NULL && old->kind != RPF_KIND_DIR)
		rpf_tree_remove(old);
	if ((d = rpf_tree_mkdir(parent, name)) == NULL)
		return RPF_EINVAL;
	/* As in rpf_tree_from_dir, a ".rpf" directory is a nested archive. */
	d->nested = is_rpf_name(name);
	for (; !ec && it != end; it.increment(ec)) {
		std::string sub = it->path().string();
		std::string subname = it->path().filename().string();

		if (it->is_directory(ec))
			err = add_dir(d, it->path(), subname.c_str());
		else
			err = rpf_tree_add_path(d, subname.c_str(), sub.c_str());
		if (err != 0)
			return fail(sub.c_str(), err), err;
	}
	return ec ? RPF_EIO : 0;
}

static int
add_arg(struct rpf_entry *root, const char *arg)
{
	std::error_code ec;
	fs::path p = fs::weakly_canonical(arg, ec);
	std::string name = p.filename().string();

	if (ec || !fs::exists(p, ec))
		return RPF_ENOTFOUND;
	if (fs::is_directory(p, ec))
		return add_dir(root, p, name.c_str());
	return rpf_tree_add_path(root, name.c_str(), arg);
}

/* Put the path of the key cache folder in buf. Return 0 or -1. */
static int
key_cache(char *buf)
{
	static const char name[] = "unrage-keys";
	DWORD n = GetTempPathA(PATH_LEN, buf);

	if (n == 0 || n + sizeof name > PATH_LEN)
		return -1;
	memcpy(buf + n, name, sizeof name);
	return 0;
}

static void
progress(const char *msg, void *ud)
{
	(void)ud;
	fprintf(stderr, "unrage: %s\n", msg);
}

/* Close the archives of n, the last one first, and free the list. */
static void
close_nested(struct nested *n)
{
	while (n->len > 0)
		rpf_close(n->v[--n->len]);
	free(n->v);
	n->v = nullptr;
}

/*
 * Write the tree root as the archive path. If a is not NULL, it is the
 * open archive at path. Then write to a temporary directory with the
 * same file name, because the NG key comes from the file name. Then
 * replace path. Free root, close the archives of n if n is not NULL,
 * and close a.
 */
static int
save(struct rpf_entry *root, RpfArchive *a, struct nested *n,
    const char *path, int enc)
{
	std::error_code ec;
	fs::path tmpdir = fs::path(path).concat(".unrage-tmp");
	fs::path tmp = a != NULL ? tmpdir / fs::path(path).filename() :
	    fs::path(path);
	char dir[PATH_LEN];
	int err;

	if (a != NULL)
		fs::create_directory(tmpdir, ec);
	err = rpf_write(root, tmp.string().c_str(), enc, keys);
	/*
	 * rpf_write checks the keys before it makes the file. A .ysc body
	 * uses NG also in an OPEN archive, so only AES needs no tables.
	 */
	if (err == RPF_ENOKEY && enc != RPF_ENC_AES && keys != nullptr &&
	    rpf_keys_derive_encrypt(keys, progress, nullptr) == 0) {
		err = rpf_write(root, tmp.string().c_str(), enc, keys);
		if (cached && (key_cache(dir) != 0 ||
		    rpf_keys_save(keys, dir) != 0))
			fprintf(stderr,
			    "unrage: cannot save the keys to the cache\n");
	}
	rpf_tree_free(root);
	/* A nested archive shares the file of a. Close it first. */
	if (n != nullptr)
		close_nested(n);
	rpf_close(a);
	if (a == NULL)
		return err;
	if (err == 0) {
		fs::rename(tmp, path, ec);
		if (ec)
			err = RPF_EIO;
	}
	fs::remove(tmp, ec);
	fs::remove(tmpdir, ec);
	return err;
}

static int
cmd_modify(const char *path, char **paths, int npaths, int del)
{
	struct rpf_entry *root, *e;
	RpfArchive *a = NULL;
	std::error_code ec;
	int bad = 0, enc, err, i;

	if (fs::exists(path, ec)) {
		if ((err = rpf_open(&a, path, keys)) != 0)
			return fail(path, err);
		err = rpf_tree_from_archive(&root, a);
	} else if (del) {
		return fail(path, RPF_ENOTFOUND);
	} else {
		err = rpf_tree_new(&root);
	}
	if (err != 0) {
		rpf_close(a);
		return fail(path, err);
	}
	for (i = 0; i < npaths; i++) {
		if (!del)
			err = add_arg(root, paths[i]);
		else if ((e = tree_find(root, paths[i])) == NULL)
			err = RPF_ENOTFOUND;
		else
			err = rpf_tree_remove(e);
		if (err != 0)
			bad = fail(paths[i], err);
	}
	/* Do not write an archive with only a part of the changes. */
	if (bad) {
		rpf_tree_free(root);
		rpf_close(a);
		return 1;
	}
	enc = encryption;
	if (enc < 0)
		enc = a != NULL && rpf_encryption(a) != 0 ? rpf_encryption(a) :
		    RPF_ENC_OPEN;
	if ((err = save(root, a, nullptr, path, enc)) != 0)
		return fail(path, err);
	printf("Everything is Ok\n");
	return 0;
}

/*
 * Open each nested archive under the directory d of the archive a. Put
 * it in the tree directory t as a nested archive node, and do the same
 * in it. Add each open archive to n. level is the number of nested
 * archives above d. Return 0 or an error.
 */
static int
nest(RpfArchive *a, const struct rpf_entry *d, struct rpf_entry *t,
    struct nested *n, int level)
{
	const struct rpf_entry *e;
	struct rpf_entry *c;
	RpfArchive *sub, **v;
	size_t i;
	int err;

	for (i = 0; i < d->nchildren; i++) {
		e = d->children[i];
		c = tree_child(t, e->name, strlen(e->name));
		if (c != nullptr && e->kind == RPF_KIND_DIR) {
			if ((err = nest(a, e, c, n, level)) != 0)
				return err;
			continue;
		}
		if (c == nullptr || e->kind != RPF_KIND_BINARY ||
		    !is_rpf_name(e->name))
			continue;
		if (level >= MAXNEST)
			return too_deep(e->name), RPF_EFORMAT;
		if ((err = rpf_open_nested(&sub, a, e)) != 0)
			return fail(e->name, err), err;
		if (!(v = (RpfArchive **)realloc(n->v,
		    (n->len + 1) * sizeof *v))) {
			rpf_close(sub);
			return fail(e->name, RPF_ENOMEM), RPF_ENOMEM;
		}
		n->v = v;
		n->v[n->len++] = sub;
		/* The new node replaces c. */
		if ((err = rpf_tree_add_archive(t, e->name, sub)) != 0)
			return fail(e->name, err), err;
		c = tree_child(t, e->name, strlen(e->name));
		if ((err = nest(sub, rpf_root(sub), c, n, level + 1)) != 0)
			return err;
	}
	return 0;
}

/*
 * Write the archive path again with the encryption of -t, or NG. Write
 * each nested archive again with the same encryption.
 */
static int
cmd_fix(const char *path)
{
	struct nested n = {nullptr, 0};
	struct rpf_entry *root;
	RpfArchive *a;
	int err;

	if ((err = rpf_open(&a, path, keys)) != 0)
		return fail(path, err);
	if ((err = rpf_tree_from_archive(&root, a)) != 0) {
		rpf_close(a);
		return fail(path, err);
	}
	if (nest(a, rpf_root(a), root, &n, 0) != 0) {
		rpf_tree_free(root);
		close_nested(&n);
		rpf_close(a);
		return 1;
	}
	err = save(root, a, &n, path, encryption < 0 ? RPF_ENC_NG :
	    encryption);
	if (err != 0)
		return fail(path, err);
	printf("Everything is Ok\n");
	return 0;
}

static bool
is_file(const char *path)
{
	DWORD a = GetFileAttributesA(path);

	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Put the path dir\name in buf. Return false if it is too long. */
static bool
cache_file(char *buf, const char *dir, const char *name)
{
	int w = snprintf(buf, PATH_LEN, "%s\\%s", dir, name);

	return w > 0 && w < PATH_LEN;
}

/* Return true if the cache folder dir has all the files that a read needs. */
static bool
cache_ok(const char *dir)
{
	static const char *const names[] = {
		"gtav_aes_key.dat", "gtav_ng_key.dat",
		"gtav_ng_decrypt_tables.dat", "gtav_hash_lut.dat"
	};
	char path[PATH_LEN];
	size_t i;

	for (i = 0; i < sizeof names / sizeof names[0]; i++)
		if (!cache_file(path, dir, names[i]) || !is_file(path))
			return false;
	return true;
}

/*
 * Put the path of GTA5.exe in buf. Use only -g if it is set. Otherwise,
 * look in each folder above the archive, then in the registry.
 */
static int
find_exe(char *buf, const char *archive)
{
	static const char *const values[] = {
		"InstallFolder", "InstallFolderSteam", "InstallFolderEpic"
	};
	char dir[PATH_LEN];
	DWORD n, size;
	size_t i, len;
	char *p;
	int w;

	if (gtaexe != nullptr) {
		w = snprintf(buf, PATH_LEN, "%s", gtaexe);
		return w > 0 && w < PATH_LEN && is_file(buf) ? 0 :
		    RPF_ENOTFOUND;
	}
	/* GetFullPathNameA also accepts a path that does not exist. */
	n = GetFullPathNameA(archive, PATH_LEN, dir, nullptr);
	if (n > 0 && n < PATH_LEN) {
		while ((p = strrchr(dir, '\\')) != nullptr) {
			*p = '\0';
			w = snprintf(buf, PATH_LEN, "%s\\GTA5.exe", dir);
			if (w > 0 && w < PATH_LEN && is_file(buf))
				return 0;
		}
	}
	for (i = 0; i < sizeof values / sizeof values[0]; i++) {
		size = PATH_LEN;
		if (RegGetValueA(HKEY_LOCAL_MACHINE,
		    "SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V",
		    values[i], RRF_RT_REG_SZ, nullptr, dir, &size) !=
		    ERROR_SUCCESS)
			continue;
		len = strlen(dir);
		w = snprintf(buf, PATH_LEN, "%s%sGTA5.exe", dir,
		    len > 0 && dir[len - 1] == '\\' ? "" : "\\");
		if (w > 0 && w < PATH_LEN && is_file(buf))
			return 0;
	}
	return RPF_ENOTFOUND;
}

/*
 * Make the read keys from GTA5.exe and the embedded magic.dat. Save them
 * to the cache folder and replace keys. On an error, set keynote.
 */
static int
fetch_keys(const char *archive)
{
	static const char *const encfiles[] = {
		"gtav_ng_encrypt_tables.dat", "gtav_ng_encrypt_luts.dat"
	};
	char exe[PATH_LEN], dir[PATH_LEN], path[PATH_LEN];
	const uint8_t *magic = nullptr;
	HGLOBAL mem = nullptr;
	HRSRC res;
	RpfKeys *k;
	size_t i;
	bool ok;
	int err;

	if (find_exe(exe, archive) != 0) {
		keynote = "GTA5.exe not found; use -g<path> to GTA5.exe or "
		    "-k<dir>";
		return RPF_ENOTFOUND;
	}
	fprintf(stderr, "unrage: make the keys from %s\n", exe);
	if ((err = rpf_keys_extract(&k, exe, nullptr, nullptr)) != 0) {
		keynote = rpf_strerror(err);
		return err;
	}
	if ((res = FindResourceA(nullptr, "MAGIC", RT_RCDATA)) != nullptr)
		mem = LoadResource(nullptr, res);
	if (mem != nullptr)
		magic = (const uint8_t *)LockResource(mem);
	err = magic != nullptr ?
	    rpf_keys_magic(k, magic, SizeofResource(nullptr, res)) :
	    RPF_ENOTFOUND;
	if (err != 0) {
		rpf_keys_free(k);
		keynote = rpf_strerror(err);
		return err;
	}
	/* A save error is not fatal. The keys stay in memory. */
	ok = key_cache(dir) == 0 && (CreateDirectoryA(dir, nullptr) ||
	    GetLastError() == ERROR_ALREADY_EXISTS);
	/* The new keys have no encrypt tables. An old bad file stops each load. */
	for (i = 0; ok && i < sizeof encfiles / sizeof encfiles[0]; i++)
		if (cache_file(path, dir, encfiles[i]))
			DeleteFileA(path);
	if (!ok || rpf_keys_save(k, dir) != 0)
		fprintf(stderr, "unrage: cannot save the keys to the cache\n");
	rpf_keys_free(keys);
	keys = k;
	cached = true;
	return 0;
}

int
main(int argc, char **argv)
{
	const char *keydir = NULL;
	char cache[PATH_LEN];
	char **pos;
	int npos = 0, switches = 1, err, i, ret;
	bool fix;

	pos = (char **)malloc(argc * sizeof *pos);
	if (pos == NULL)
		return fail("unrage", RPF_ENOMEM);
	for (i = 1; i < argc; i++) {
		char *s = argv[i];

		if (!switches || s[0] != '-' || s[1] == '\0') {
			pos[npos++] = s;
		} else if (strcmp(s, "--") == 0) {
			switches = 0;
		} else if (s[1] == 'o' && s[2] != '\0') {
			outdir = s + 2;
		} else if (s[1] == 'k' && s[2] != '\0') {
			keydir = s + 2;
		} else if (s[1] == 'g' && s[2] != '\0') {
			gtaexe = s + 2;
		} else if (strcmp(s, "-r") == 0) {
			recurse = 1;
		} else if (ieqn(s, "-topen", 7)) {
			encryption = RPF_ENC_OPEN;
		} else if (ieqn(s, "-taes", 6)) {
			encryption = RPF_ENC_AES;
		} else if (ieqn(s, "-tng", 5)) {
			encryption = RPF_ENC_NG;
		} else {
			fprintf(stderr, "unrage: unknown switch %s\n", s);
			usage();
			return 2;
		}
	}
	fix = npos > 0 && strcmp(pos[0], "fix") == 0;
	/* fix takes the archive only. */
	if (npos < 2 || (!fix && strlen(pos[0]) != 1) || (fix && npos != 2)) {
		usage();
		return 2;
	}

	if (keydir != NULL) {
		if ((err = rpf_keys_load(&keys, keydir)) != 0)
			return fail(keydir, err);
	} else if (fs::is_directory("keys")) {
		err = rpf_keys_load(&keys, "keys");
		if (err != 0 && err != RPF_ENOTFOUND)
			return fail("keys", err);
	} else if (key_cache(cache) == 0 && cache_ok(cache) &&
	    rpf_keys_load(&keys, cache) == 0) {
		cached = true;
	} else {
		/* A failed fetch is not fatal. OPEN archives need no keys. */
		fetch_keys(pos[1]);
	}

	if (fix) {
		ret = cmd_fix(pos[1]);
	} else {
		switch (pos[0][0]) {
		case 'l': ret = cmd_list(pos[1], CMD_LIST); break;
		case 't': ret = cmd_list(pos[1], CMD_TEST); break;
		case 'e': ret = cmd_extract(pos[1], pos + 2, npos - 2, 1); break;
		case 'x': ret = cmd_extract(pos[1], pos + 2, npos - 2, 0); break;
		case 'a': ret = cmd_modify(pos[1], pos + 2, npos - 2, 0); break;
		case 'd': ret = cmd_modify(pos[1], pos + 2, npos - 2, 1); break;
		default:
			fprintf(stderr, "unrage: unknown command %s\n", pos[0]);
			usage();
			ret = 2;
		}
	}
	rpf_keys_free(keys);
	free(pos);
	return ret;
}
