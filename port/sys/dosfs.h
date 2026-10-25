/* The DOS file system the game sees: the user's game folder (the data root, never
 * written) with an overlay directory laid over it (by default plat_home_dir()).
 *
 * Names are DOS names, relative to the game folder ("savegame\\objblkaa", "u6pal",
 * "MAPTILES.VGA"): '\\' or '/' separators, an optional "C:" and leading '\\' (the game
 * folder stands for the root of drive C:), "." and "..". Each part is upper-cased and cut
 * to 8.3 as DOS does, then matched without case against the host directories.
 *
 * Reads look in the overlay first, then in the data root. Creating, writing, deleting and
 * renaming happen in the overlay only: a data root file is copied there on its first
 * write; deleting or renaming away a file that exists in the data root records it as
 * deleted (a list kept in the overlay's ".dosfs-deleted"), so it is hidden from then on.
 *
 * The calls behave like the DOS functions (int 21h 3Ch..42h, 56h): they return 0 or a DOS
 * error code, and use DOS handles (lowest free of 20; 0-4 are the standard devices).
 */
#ifndef DOSFS_H
#define DOSFS_H

#include <stdint.h>

/* DOS error codes */
#define DOSERR_FUNCTION   1
#define DOSERR_NOFILE     2
#define DOSERR_NOPATH     3
#define DOSERR_TOOMANY    4
#define DOSERR_ACCESS     5
#define DOSERR_HANDLE     6
#define DOSERR_NOTSAME    0x11
#define DOSERR_WRITEFAULT 0x1D
#define DOSERR_READFAULT  0x1E

#define DOSFS_HANDLES 20

/* data_root: the game folder (0: plat_base_dir()). overlay: where writes go
 * (0: plat_home_dir()). Returns 0, or -1 if the data root is not a directory. May be called
 * again to switch folders (closes every file). Without a call, the first file operation
 * uses the defaults. */
int dosfs_init(const char *data_root, const char *overlay);
void dosfs_shutdown(void);

/* mode: 0 read, 1 write, 2 read/write (int 21h 3Dh AL) */
int dosfs_open(const char *path, int mode, int16_t *handle);
int dosfs_create(const char *path, int16_t *handle);    /* create or truncate, read/write */
int dosfs_close(int16_t handle);
int dosfs_read(int16_t handle, void *buf, uint16_t count, uint16_t *done);
/* count 0 sets the file size to the current position, as DOS does. */
int dosfs_write(int16_t handle, const void *buf, uint16_t count, uint16_t *done);
/* method 0 from the start, 1 from the current position, 2 from the end */
int dosfs_seek(int16_t handle, int method, int32_t offset, uint32_t *position);
int dosfs_delete(const char *path);
int dosfs_rename(const char *oldPath, const char *newPath);

/* Port helpers (not DOS calls). */
/* 1 if the DOS name is an existing file. */
int dosfs_exists(const char *path);
/* The host path a DOS name resolves to for reading (overlay or data root), or 0. */
const char *dosfs_host_path(const char *path);
/* The canonical DOS form of a name ("SAVEGAME\\OBJBLKAA"), or 0 if it is not valid. */
const char *dosfs_canonical(const char *path);

#endif
