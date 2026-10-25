/* The DOS file system over the host's (dosfs.h).
 *
 * Standard C stdio does the file I/O. Listing a directory, testing for a directory,
 * creating one and setting a file's size are not in standard C: the "host" functions at
 * the top do them with POSIX or Win32 calls (the only OS-specific code of port/sys; they
 * could move into the platform layer later).
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dosfs.h"
#include "../platform/plat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define HPATH 1024       /* host paths */
#define MAXCOMP 32       /* parts of a DOS path */
#define CANON 400        /* canonical DOS path */

/* ================= host ================= */

static int upper_eq(const char *host, const char *canon)
{
	while (*host && *canon) {
		int a = (unsigned char)*host, b = (unsigned char)*canon;
		if (a >= 'a' && a <= 'z')
			a -= 'a' - 'A';
		if (a != b)
			return 0;
		host++;
		canon++;
	}
	return *host == 0 && *canon == 0;
}

/* The entry of directory dir whose name is canon without case (an exact match first). */
static int host_find(const char *dir, const char *canon, char *out, size_t outsz)
{
	int found = 0;
#ifdef _WIN32
	char pattern[HPATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	if ((size_t)snprintf(pattern, sizeof pattern, "%s\\*", dir) >= sizeof pattern)
		return 0;
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	do {
		const char *nm = fd.cFileName;
#else
	DIR *d = opendir(dir);
	struct dirent *e;

	if (!d)
		return 0;
	while ((e = readdir(d)) != 0) {
		const char *nm = e->d_name;
#endif
		if (upper_eq(nm, canon) && strlen(nm) < outsz) {
			if (strcmp(nm, canon) == 0) {
				strcpy(out, nm);
				found = 2;
				break;
			}
			if (!found) {
				strcpy(out, nm);
				found = 1;
			}
		}
#ifdef _WIN32
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	}
	closedir(d);
#endif
	return found != 0;
}

/* 0 missing, 1 file, 2 directory */
static int host_kind(const char *p)
{
#ifdef _WIN32
	DWORD a = GetFileAttributesA(p);
	if (a == INVALID_FILE_ATTRIBUTES)
		return 0;
	return (a & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;
#else
	struct stat st;
	if (stat(p, &st) != 0)
		return 0;
	if (S_ISDIR(st.st_mode))
		return 2;
	return S_ISREG(st.st_mode) ? 1 : 0;
#endif
}

static int host_mkdir(const char *p)
{
#ifdef _WIN32
	return CreateDirectoryA(p, 0) ? 0 : -1;
#else
	return mkdir(p, 0777);
#endif
}

static int host_truncate(FILE *f, long size)
{
	fflush(f);
#ifdef _WIN32
	return _chsize_s(_fileno(f), size) == 0 ? 0 : -1;
#else
	return ftruncate(fileno(f), (off_t)size);
#endif
}

static int host_copy(const char *src, const char *dst)
{
	static unsigned char buf[32768];
	FILE *in, *out;
	size_t n;
	int err = 0;

	in = fopen(src, "rb");
	if (!in)
		return -1;
	out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		return -1;
	}
	while ((n = fread(buf, 1, sizeof buf, in)) > 0)
		if (fwrite(buf, 1, n, out) != n) {
			err = -1;
			break;
		}
	if (ferror(in))
		err = -1;
	fclose(in);
	if (fclose(out) != 0)
		err = -1;
	return err;
}

/* out = dir/name; out may be dir. */
static int join(char *out, size_t outsz, const char *dir, const char *name)
{
	char tmp[HPATH];
	size_t l = strlen(dir);
	const char *sep = (l && (dir[l - 1] == '/' || dir[l - 1] == '\\')) ? "" : "/";

	if ((size_t)snprintf(tmp, sizeof tmp, "%s%s%s", dir, sep, name) >= sizeof tmp ||
	    strlen(tmp) >= outsz)
		return -1;
	strcpy(out, tmp);
	return 0;
}

/* ================= DOS names ================= */

typedef struct {
	int n;
	char c[MAXCOMP][13];
} DosPath;

static int bad_char(int c)
{
	return c <= ' ' || strchr("\"*+,/:;<=>?[\\]|", c) != 0 || c == 0x7f;
}

/* One part of a name as DOS stores it: upper case, 8.3. */
static int canon_part(const char *s, size_t len, char *out)
{
	char base[9], ext[4];
	size_t i, nb = 0, ne = 0;
	int inext = 0;

	for (i = 0; i < len; i++) {
		int c = (unsigned char)s[i];
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		if (c == '.') {
			if (inext)
				return -1;
			inext = 1;
			continue;
		}
		if (bad_char(c))
			return -1;
		if (!inext) {
			if (nb < 8)
				base[nb++] = (char)c;   /* DOS cuts longer names */
		} else if (ne < 3) {
			ext[ne++] = (char)c;
		}
	}
	if (nb == 0)
		return -1;
	memcpy(out, base, nb);
	if (ne) {
		out[nb] = '.';
		memcpy(out + nb + 1, ext, ne);
		out[nb + 1 + ne] = 0;
	} else {
		out[nb] = 0;
	}
	return 0;
}

static int dos_parse(const char *path, DosPath *dp)
{
	const char *s = path;

	dp->n = 0;
	if (!s)
		return DOSERR_NOPATH;
	if (s[0] && s[1] == ':') {
		if (toupper((unsigned char)s[0]) != 'C')
			return DOSERR_NOPATH;   /* the port has drive C: only */
		s += 2;
	}
	if (*s == '\\' || *s == '/')
		s++;
	if (!*s)
		return DOSERR_NOFILE;
	for (;;) {
		const char *e = s;
		size_t len;
		int last;

		while (*e && *e != '\\' && *e != '/')
			e++;
		len = (size_t)(e - s);
		last = (*e == 0);
		if (len == 0)
			return DOSERR_NOPATH;
		if (len == 1 && s[0] == '.') {
			/* stays here */
		} else if (len == 2 && s[0] == '.' && s[1] == '.') {
			if (dp->n > 0)
				dp->n--;
		} else {
			if (dp->n == MAXCOMP)
				return DOSERR_NOPATH;
			if (canon_part(s, len, dp->c[dp->n]) != 0)
				return last ? DOSERR_NOFILE : DOSERR_NOPATH;
			dp->n++;
		}
		if (last)
			break;
		s = e + 1;
	}
	if (dp->n == 0)
		return DOSERR_NOFILE;
	return 0;
}

static void dos_join(const DosPath *dp, int n, char *out)
{
	int i;

	out[0] = 0;
	for (i = 0; i < n; i++) {
		if (i)
			strcat(out, "\\");
		strcat(out, dp->c[i]);
	}
}

/* ================= state ================= */

typedef struct {
	int used, device, mode, cow;   /* cow: open on the data root file, copy on write */
	FILE *fp;
	uint32_t pos;
	char canon[CANON];
	char host[HPATH];
} DFile;

static int inited;
static char data_root[HPATH], overlay_root[HPATH], wo_file[HPATH];
static DFile ft[DOSFS_HANDLES];

static char **wo;          /* deleted data root files (canonical names) */
static int nwo, capwo;

static void ensure_init(void)
{
	if (!inited)
		dosfs_init(0, 0);
}

static void wo_save(void)
{
	FILE *f = fopen(wo_file, "w");
	int i;

	if (!f) {
		plat_log("dosfs: cannot write %s", wo_file);
		return;
	}
	for (i = 0; i < nwo; i++)
		fprintf(f, "%s\n", wo[i]);
	fclose(f);
}

static int wo_find(const char *canon)
{
	int i;
	for (i = 0; i < nwo; i++)
		if (strcmp(wo[i], canon) == 0)
			return i;
	return -1;
}

static void wo_add_mem(const char *canon)
{
	if (wo_find(canon) >= 0)
		return;
	if (nwo == capwo) {
		int nc = capwo ? capwo * 2 : 16;
		char **n = (char **)realloc(wo, (size_t)nc * sizeof *n);
		if (!n)
			return;
		wo = n;
		capwo = nc;
	}
	wo[nwo] = (char *)malloc(strlen(canon) + 1);
	if (!wo[nwo])
		return;
	strcpy(wo[nwo++], canon);
}

static void wo_add(const char *canon)
{
	if (wo_find(canon) >= 0)
		return;
	wo_add_mem(canon);
	wo_save();
}

static void wo_del(const char *canon)
{
	int i = wo_find(canon);
	if (i < 0)
		return;
	free(wo[i]);
	wo[i] = wo[--nwo];
	wo_save();
}

static void wo_load(void)
{
	char line[CANON + 2];
	FILE *f = fopen(wo_file, "r");

	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		while (l && (line[l - 1] == '\n' || line[l - 1] == '\r'))
			line[--l] = 0;
		if (l)
			wo_add_mem(line);
	}
	fclose(f);
}

static void close_all(void)
{
	int i;

	for (i = 0; i < DOSFS_HANDLES; i++) {
		if (ft[i].used && ft[i].fp)
			fclose(ft[i].fp);
		memset(&ft[i], 0, sizeof ft[i]);
	}
	for (i = 0; i < 5; i++) {   /* stdin, stdout, stderr, aux, prn */
		ft[i].used = 1;
		ft[i].device = 1;
		ft[i].mode = 2;
	}
	for (i = 0; i < nwo; i++)
		free(wo[i]);
	nwo = 0;
}

int dosfs_init(const char *data, const char *overlay)
{
	if (!data)
		data = plat_base_dir();
	if (!overlay)
		overlay = plat_home_dir();
	if (!data)
		data = ".";
	if (!overlay)
		overlay = ".";
	close_all();
	inited = 1;
	if (strlen(data) >= HPATH - 32 || strlen(overlay) >= HPATH - 32) {
		plat_log("dosfs: folder name too long");
		return -1;
	}
	strcpy(data_root, data);
	strcpy(overlay_root, overlay);
	if (host_kind(overlay_root) == 0)
		host_mkdir(overlay_root);
	join(wo_file, sizeof wo_file, overlay_root, ".dosfs-deleted");
	if (strcmp(data_root, overlay_root) == 0)
		plat_log("dosfs: warning: the overlay is the game folder itself, it will be written");
	wo_load();
	if (host_kind(data_root) != 2) {
		plat_log("dosfs: game folder %s not found", data_root);
		return -1;
	}
	return 0;
}

void dosfs_shutdown(void)
{
	close_all();
	inited = 0;
}

/* ================= lookup ================= */

/* Walk the first n parts under root: 0 missing, 1 file, 2 directory; out = host path. */
static int resolve(const char *root, const DosPath *dp, int n, char *out)
{
	char cur[HPATH], name[256];
	int i, k = 2;

	strcpy(cur, root);
	for (i = 0; i < n; i++) {
		if (k != 2 || !host_find(cur, dp->c[i], name, sizeof name))
			return 0;
		if (join(cur, sizeof cur, cur, name) != 0)
			return 0;
		k = host_kind(cur);
		if (k == 0)
			return 0;
	}
	strcpy(out, cur);
	return k;
}

typedef struct {
	int kind;    /* 0 none, 1 file, 2 directory */
	int where;   /* 1 overlay, 2 data root */
	char host[HPATH];
} Found;

static void lookup_n(const DosPath *dp, int n, Found *f)
{
	char canon[CANON];

	f->where = 1;
	f->kind = resolve(overlay_root, dp, n, f->host);
	if (f->kind)
		return;
	f->where = 2;
	f->kind = resolve(data_root, dp, n, f->host);
	if (f->kind == 1) {
		dos_join(dp, n, canon);
		if (wo_find(canon) >= 0)
			f->kind = 0;
	}
}

static int parent_exists(const DosPath *dp)
{
	Found f;
	lookup_n(dp, dp->n - 1, &f);
	return f.kind == 2;
}

/* The overlay path for a DOS name, creating its directories (named as in the data root
 * when they exist there). 0 or a DOS error. */
static int overlay_path(const DosPath *dp, char *out)
{
	char cur[HPATH], name[256], tmp[HPATH];
	int i;

	strcpy(cur, overlay_root);
	for (i = 0; i < dp->n; i++) {
		if (!host_find(cur, dp->c[i], name, sizeof name)) {
			if (resolve(data_root, dp, i + 1, tmp)) {
				const char *b = tmp + strlen(tmp);
				while (b > tmp && b[-1] != '/' && b[-1] != '\\')
					b--;
				strcpy(name, b);
			} else {
				strcpy(name, dp->c[i]);
			}
		}
		if (join(cur, sizeof cur, cur, name) != 0)
			return DOSERR_NOPATH;
		if (i < dp->n - 1) {
			int k = host_kind(cur);
			if (k == 1)
				return DOSERR_NOPATH;
			if (k == 0 && host_mkdir(cur) != 0)
				return DOSERR_NOPATH;
		}
	}
	strcpy(out, cur);
	return 0;
}

static DFile *get(int16_t h)
{
	ensure_init();
	if (h < 0 || h >= DOSFS_HANDLES || !ft[h].used)
		return 0;
	return &ft[h];
}

static int free_handle(void)
{
	int i;
	for (i = 0; i < DOSFS_HANDLES; i++)
		if (!ft[i].used)
			return i;
	return -1;
}

/* Every handle open on canon now uses the overlay file host. */
static void repoint(const char *canon, const char *host, int skip)
{
	int i;

	for (i = 0; i < DOSFS_HANDLES; i++) {
		DFile *f = &ft[i];
		if (i == skip || !f->used || f->device || strcmp(f->canon, canon) != 0)
			continue;
		if (f->fp)
			fclose(f->fp);
		f->fp = fopen(host, f->mode == 0 ? "rb" : "r+b");
		f->cow = 0;
		strcpy(f->host, host);
		if (!f->fp)
			plat_log("dosfs: cannot reopen %s", host);
	}
}

/* First write to a file of the data root: copy it to the overlay. */
static int copy_up(DFile *f)
{
	DosPath dp;
	char host[HPATH];
	int err;

	if (dos_parse(f->canon, &dp) != 0)
		return DOSERR_ACCESS;
	err = overlay_path(&dp, host);
	if (err)
		return err;
	if (host_copy(f->host, host) != 0) {
		plat_log("dosfs: cannot copy %s to %s", f->host, host);
		return DOSERR_ACCESS;
	}
	repoint(f->canon, host, -1);
	return f->fp ? 0 : DOSERR_ACCESS;
}

/* ================= DOS calls ================= */

int dosfs_open(const char *path, int mode, int16_t *handle)
{
	DosPath dp;
	Found fo;
	int err, h;
	FILE *fp;

	ensure_init();
	if (mode < 0 || mode > 2)
		return DOSERR_FUNCTION;   /* DOS: invalid access code (0Ch); 1 is close enough */
	err = dos_parse(path, &dp);
	if (err)
		return err;
	lookup_n(&dp, dp.n, &fo);
	if (fo.kind == 0)
		return parent_exists(&dp) ? DOSERR_NOFILE : DOSERR_NOPATH;
	if (fo.kind == 2)
		return DOSERR_ACCESS;
	h = free_handle();
	if (h < 0)
		return DOSERR_TOOMANY;
	if (fo.where == 1 && mode != 0)
		fp = fopen(fo.host, "r+b");
	else
		fp = fopen(fo.host, "rb");
	if (!fp)
		return DOSERR_ACCESS;
	memset(&ft[h], 0, sizeof ft[h]);
	ft[h].used = 1;
	ft[h].mode = mode;
	ft[h].cow = (fo.where == 2);
	ft[h].fp = fp;
	dos_join(&dp, dp.n, ft[h].canon);
	strcpy(ft[h].host, fo.host);
	*handle = (int16_t)h;
	return 0;
}

int dosfs_create(const char *path, int16_t *handle)
{
	DosPath dp;
	Found fo;
	char host[HPATH], canon[CANON];
	int err, h;
	FILE *fp;

	ensure_init();
	err = dos_parse(path, &dp);
	if (err)
		return err;
	lookup_n(&dp, dp.n, &fo);
	if (fo.kind == 2)
		return DOSERR_ACCESS;
	if (!parent_exists(&dp))
		return DOSERR_NOPATH;
	h = free_handle();
	if (h < 0)
		return DOSERR_TOOMANY;
	err = overlay_path(&dp, host);
	if (err)
		return err;
	fp = fopen(host, "w+b");
	if (!fp)
		return DOSERR_ACCESS;
	dos_join(&dp, dp.n, canon);
	wo_del(canon);
	repoint(canon, host, h);
	memset(&ft[h], 0, sizeof ft[h]);
	ft[h].used = 1;
	ft[h].mode = 2;
	ft[h].fp = fp;
	strcpy(ft[h].canon, canon);
	strcpy(ft[h].host, host);
	*handle = (int16_t)h;
	return 0;
}

int dosfs_close(int16_t handle)
{
	DFile *f = get(handle);

	if (!f)
		return DOSERR_HANDLE;
	if (f->fp)
		fclose(f->fp);
	memset(f, 0, sizeof *f);
	return 0;
}

static long file_size(DFile *f)
{
	if (fseek(f->fp, 0, SEEK_END) != 0)
		return -1;
	return ftell(f->fp);
}

int dosfs_read(int16_t handle, void *buf, uint16_t count, uint16_t *done)
{
	DFile *f = get(handle);
	size_t n;

	*done = 0;
	if (!f)
		return DOSERR_HANDLE;
	if (f->device)
		return 0;
	if (f->mode == 1)
		return DOSERR_ACCESS;
	if (!f->fp)
		return DOSERR_READFAULT;
	if (fseek(f->fp, (long)f->pos, SEEK_SET) != 0)
		return DOSERR_READFAULT;
	n = count ? fread(buf, 1, count, f->fp) : 0;
	if (n == 0 && count && ferror(f->fp)) {
		clearerr(f->fp);
		return DOSERR_READFAULT;
	}
	clearerr(f->fp);
	f->pos += (uint32_t)n;
	*done = (uint16_t)n;
	return 0;
}

int dosfs_write(int16_t handle, const void *buf, uint16_t count, uint16_t *done)
{
	DFile *f = get(handle);
	size_t n;
	int err;

	*done = 0;
	if (!f)
		return DOSERR_HANDLE;
	if (f->device) {
		*done = count;
		return 0;
	}
	if (f->mode == 0)
		return DOSERR_ACCESS;
	if (f->cow) {
		err = copy_up(f);
		if (err)
			return err;
	}
	if (!f->fp)
		return DOSERR_WRITEFAULT;
	if (count == 0) {
		/* DOS: writing 0 bytes sets the end of the file at the position */
		if (host_truncate(f->fp, (long)f->pos) != 0)
			return DOSERR_WRITEFAULT;
		return 0;
	}
	if (fseek(f->fp, (long)f->pos, SEEK_SET) != 0)
		return DOSERR_WRITEFAULT;
	n = fwrite(buf, 1, count, f->fp);
	if (fflush(f->fp) != 0)
		clearerr(f->fp);
	clearerr(f->fp);
	/* a short count without an error is what DOS gives when the disk is full */
	f->pos += (uint32_t)n;
	*done = (uint16_t)n;
	return 0;
}

int dosfs_seek(int16_t handle, int method, int32_t offset, uint32_t *position)
{
	DFile *f = get(handle);
	uint32_t base;

	if (!f)
		return DOSERR_HANDLE;
	if (method < 0 || method > 2)
		return DOSERR_FUNCTION;
	if (f->device) {
		if (position)
			*position = 0;
		return 0;
	}
	if (method == 0) {
		base = 0;
	} else if (method == 1) {
		base = f->pos;
	} else {
		long s;
		if (!f->fp || (s = file_size(f)) < 0)
			return DOSERR_READFAULT;
		base = (uint32_t)s;
	}
	f->pos = base + (uint32_t)offset;
	if (position)
		*position = f->pos;
	return 0;
}

int dosfs_delete(const char *path)
{
	DosPath dp;
	Found fo;
	char canon[CANON], tmp[HPATH];
	int err;

	ensure_init();
	err = dos_parse(path, &dp);
	if (err)
		return err;
	lookup_n(&dp, dp.n, &fo);
	if (fo.kind == 0)
		return parent_exists(&dp) ? DOSERR_NOFILE : DOSERR_NOPATH;
	if (fo.kind == 2)
		return DOSERR_ACCESS;
	dos_join(&dp, dp.n, canon);
	if (fo.where == 1) {
		if (remove(fo.host) != 0)
			return DOSERR_ACCESS;
		if (resolve(data_root, &dp, dp.n, tmp) == 1)
			wo_add(canon);
	} else {
		wo_add(canon);
	}
	return 0;
}

int dosfs_rename(const char *oldPath, const char *newPath)
{
	DosPath od, nd;
	Found fo, fn;
	char host[HPATH], tmp[HPATH], ocanon[CANON], ncanon[CANON];
	int err;

	ensure_init();
	err = dos_parse(oldPath, &od);
	if (err)
		return err;
	err = dos_parse(newPath, &nd);
	if (err)
		return err == DOSERR_NOFILE ? DOSERR_ACCESS : err;
	lookup_n(&od, od.n, &fo);
	if (fo.kind == 0)
		return parent_exists(&od) ? DOSERR_NOFILE : DOSERR_NOPATH;
	if (fo.kind == 2)
		return DOSERR_ACCESS;
	if (!parent_exists(&nd))
		return DOSERR_NOPATH;
	lookup_n(&nd, nd.n, &fn);
	if (fn.kind != 0)
		return DOSERR_ACCESS;
	err = overlay_path(&nd, host);
	if (err)
		return err;
	dos_join(&od, od.n, ocanon);
	dos_join(&nd, nd.n, ncanon);
	if (fo.where == 1) {
		if (host_kind(host) != 0 || rename(fo.host, host) != 0)
			return DOSERR_ACCESS;
		if (resolve(data_root, &od, od.n, tmp) == 1)
			wo_add(ocanon);
	} else {
		if (host_copy(fo.host, host) != 0)
			return DOSERR_ACCESS;
		wo_add(ocanon);
	}
	wo_del(ncanon);
	return 0;
}

int dosfs_exists(const char *path)
{
	DosPath dp;
	Found fo;

	ensure_init();
	if (dos_parse(path, &dp) != 0)
		return 0;
	lookup_n(&dp, dp.n, &fo);
	return fo.kind == 1;
}

const char *dosfs_host_path(const char *path)
{
	static Found fo;
	DosPath dp;

	ensure_init();
	if (dos_parse(path, &dp) != 0)
		return 0;
	lookup_n(&dp, dp.n, &fo);
	return fo.kind == 1 ? fo.host : 0;
}

const char *dosfs_canonical(const char *path)
{
	static char canon[CANON];
	DosPath dp;

	if (dos_parse(path, &dp) != 0)
		return 0;
	dos_join(&dp, dp.n, canon);
	return canon;
}
