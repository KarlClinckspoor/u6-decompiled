/* Host file system helpers for the port's own files (recordings, settings, the staged
 * saved game): directories, listing, copying. POSIX and Windows. */
#ifndef PORT_HOSTFS_H
#define PORT_HOSTFS_H

#include <stddef.h>

int hostfs_mkdir(const char *path);                 /* 0 or already there */
int hostfs_mkdirs(const char *path);                /* with parents */
int hostfs_is_dir(const char *path);
int hostfs_exists(const char *path);
/* calls fn for each entry (not . or ..); returns the number of entries */
int hostfs_list(const char *dir, void (*fn)(const char *name, int is_dir, void *ud), void *ud);
int hostfs_copy_file(const char *from, const char *to);
int hostfs_copy_tree(const char *from, const char *to);
int hostfs_remove_tree(const char *path);
/* path joining with '/' */
void hostfs_join(char *out, size_t size, const char *a, const char *b);
/* a fresh temporary directory (static buffer) */
const char *hostfs_temp_dir(const char *prefix);

#endif
