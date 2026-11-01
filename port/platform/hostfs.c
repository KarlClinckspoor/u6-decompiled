/* replaces nothing: host file system helpers (hostfs.h). */
#include "hostfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

void hostfs_join(char *out, size_t size, const char *a, const char *b)
{
	size_t n = strlen(a);
	if (n && (a[n - 1] == '/' || a[n - 1] == '\\'))
		snprintf(out, size, "%s%s", a, b);
	else
		snprintf(out, size, "%s/%s", a, b);
}

int hostfs_is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int hostfs_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

int hostfs_mkdir(const char *path)
{
	if (hostfs_is_dir(path))
		return 0;
	return MKDIR(path);
}

int hostfs_mkdirs(const char *path)
{
	char buf[1024];
	size_t i;
	snprintf(buf, sizeof buf, "%s", path);
	for (i = 1; buf[i]; i++)
		if (buf[i] == '/' || buf[i] == '\\') {
			char c = buf[i];
			if (i == 2 && buf[1] == ':')	/* C:\ */
				continue;
			buf[i] = 0;
			hostfs_mkdir(buf);
			buf[i] = c;
		}
	return hostfs_mkdir(buf);
}

int hostfs_list(const char *dir, void (*fn)(const char *name, int is_dir, void *ud), void *ud)
{
	int n = 0;
#ifdef _WIN32
	char pat[1100];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	hostfs_join(pat, sizeof pat, dir, "*");
	h = FindFirstFileA(pat, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
			continue;
		fn(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, ud);
		n++;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir(dir);
	struct dirent *e;
	if (!d)
		return 0;
	while ((e = readdir(d))) {
		char p[1100];
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		hostfs_join(p, sizeof p, dir, e->d_name);
		fn(e->d_name, hostfs_is_dir(p), ud);
		n++;
	}
	closedir(d);
#endif
	return n;
}

int hostfs_copy_file(const char *from, const char *to)
{
	FILE *a = fopen(from, "rb"), *b;
	char buf[65536];
	size_t n;
	if (!a)
		return -1;
	b = fopen(to, "wb");
	if (!b) {
		fclose(a);
		return -1;
	}
	while ((n = fread(buf, 1, sizeof buf, a)) > 0)
		fwrite(buf, 1, n, b);
	fclose(a);
	return fclose(b);
}

struct copy_ctx { const char *from, *to; int err; };

static void copy_one(const char *name, int is_dir, void *ud)
{
	struct copy_ctx *c = ud;
	char f[1100], t[1100];
	hostfs_join(f, sizeof f, c->from, name);
	hostfs_join(t, sizeof t, c->to, name);
	if (is_dir)
		c->err |= hostfs_copy_tree(f, t);
	else
		c->err |= hostfs_copy_file(f, t) != 0;
}

int hostfs_copy_tree(const char *from, const char *to)
{
	struct copy_ctx c;
	c.from = from;
	c.to = to;
	c.err = 0;
	hostfs_mkdirs(to);
	hostfs_list(from, copy_one, &c);
	return c.err;
}

static void remove_one(const char *name, int is_dir, void *ud)
{
	char p[1100];
	hostfs_join(p, sizeof p, (const char *)ud, name);
	if (is_dir)
		hostfs_remove_tree(p);
	else
		remove(p);
}

int hostfs_remove_tree(const char *path)
{
	hostfs_list(path, remove_one, (void *)path);
#ifdef _WIN32
	return _rmdir(path);
#else
	return rmdir(path);
#endif
}

const char *hostfs_temp_dir(const char *prefix)
{
	static char path[1024];
	static int counter;
	const char *base = getenv("TMPDIR");
#ifdef _WIN32
	char tmp[MAX_PATH];
	if (!base) {
		GetTempPathA(sizeof tmp, tmp);
		base = tmp;
	}
	snprintf(path, sizeof path, "%s%s%lu-%d", base, prefix, (unsigned long)GetCurrentProcessId(), counter++);
#else
	if (!base)
		base = "/tmp";
	snprintf(path, sizeof path, "%s/%s%ld-%d", base, prefix, (long)getpid(), counter++);
#endif
	hostfs_mkdirs(path);
	return path;
}
