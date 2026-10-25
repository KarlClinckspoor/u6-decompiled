/* LZW.ASM (+ LZ.INC), LOADNEAR.ASM, LOADFAR.ASM: whole files loaded into memory the
 * routine allocates (docs/port/U_OSILIB.md).
 *
 * The files are DOS files of the port's file system (port/sys/dosfs.c) through plain DOS
 * calls (no OSI error handler), as in DOS. Near memory is the host's heap (malloc, as U's
 * C has it), far memory the Turbo C far heap in the DOS arena (farmalloc, compat.h).
 *
 * The unpacking is GAME.EXE's: LZ.INC is the same file in both programs, and
 * port/sys/lzw.c's decompress() is its port. U's LZW_load gives the decoder two buffers of
 * its near heap (the table, 3000h bytes, and the 1 KB input buffer), freed afterwards;
 * decompress() takes one work segment with the table at 0 and the input buffer at +300h:
 * the port lends it UOSI_LZW_WORKSEG, low memory of the arena that nothing else uses (DOS
 * itself in a PC), so the output is the same and nothing of U's memory moves. */
#include "uosi.h"
#include "ufar.h"
#include "../../sys/dosfs.h"
#include "../../platform/plat.h"

#include <stdlib.h>

/* port/sys/lzw.c (GAME's INFLATE.ASM + LZ.INC; not a name of U's: not renamed) */
int decompress(int16_t fd, uint32_t offset, void *dest, uint16_t workSeg);

#define UOSI_LZW_WORKSEG 0x0600   /* 0600:0000..33FF: linear 6000h..93FFh */

/* C_13BC_000C, LZW_loadFile (C_02D3): open the file (read/write), read its 4-byte header
 * (the unpacked size), farmalloc (size >> 4) + 1 paragraphs and unpack into them.
 * Returns the data, 0 when the file cannot be opened or read. In DOS a failed open also
 * closes handle 0 when the flags say so (the port does not), and a farmalloc that fails
 * is not noticed (the data goes to 0000:0000; the port returns 0, logged). */
void *LZW_load(char *name)
{
	int16_t h;
	uint8_t hdr[4];
	uint16_t done, paras;
	uint32 size;
	void *dest;

	if (dosfs_open(name, 2, &h) != 0)
		return 0;
	if (dosfs_read(h, hdr, 4, &done) != 0) {
		dosfs_close(h);
		return 0;
	}
	size = (uint32)hdr[0] | ((uint32)hdr[1] << 8) | ((uint32)hdr[2] << 16) | ((uint32)hdr[3] << 24);
	paras = (uint16_t)((uint16_t)(size >> 4) + 1);
	dest = farmalloc((uint32)paras << 4);
	if (!dest) {
		plat_log("U: LZW_load(%s): no far memory for %lu bytes (DOS unpacks to 0000:0000)",
			name, (unsigned long)size);
		dosfs_close(h);
		return 0;
	}
	decompress(h, 0, dest, UOSI_LZW_WORKSEG);
	dosfs_close(h);
	return dest;
}

/* the size of an open file (seek to its end), the position back at the start */
static int file_size(int16_t h, uint32_t *size)
{
	if (dosfs_seek(h, 2, 0, size) != 0)
		return -1;
	if (dosfs_seek(h, 0, 0, 0) != 0)
		return -1;
	return 0;
}

/* C_145A_0006: a file smaller than 64 KB into memory it mallocs; 0 on error (also for an
 * empty file: Turbo C's malloc(0) is NULL) */
char *LOAD_near(char *name)
{
	int16_t h;
	uint32_t size;
	uint16_t done;
	char *buf;

	if (dosfs_open(name, 0, &h) != 0)
		return 0;
	if (file_size(h, &size) != 0 || (size >> 16) != 0 || size == 0) {
		dosfs_close(h);
		return 0;
	}
	buf = (char *)malloc(size);
	if (!buf) {
		dosfs_close(h);
		return 0;
	}
	if (dosfs_read(h, buf, (uint16_t)size, &done) != 0 || done != (uint16_t)size) {
		free(buf);
		dosfs_close(h);
		return 0;
	}
	dosfs_close(h);
	return buf;
}

/* C_1463_0002: a whole file into memory it farmallocs; 0 on error. Only the low 16 bits
 * of the size are read (one DOS read): a file of 64 KB or more is loaded short, and one
 * of exactly 64 KB reads nothing and is still a success. */
char *LOAD_far(char *name)
{
	int16_t h;
	uint32_t size;
	uint16_t done;
	char *buf;

	if (dosfs_open(name, 0, &h) != 0)
		return 0;
	if (file_size(h, &size) != 0) {
		dosfs_close(h);
		return 0;
	}
	buf = (char *)farmalloc(size);
	if (!buf) {
		dosfs_close(h);
		return 0;
	}
	if (dosfs_read(h, buf, (uint16_t)size, &done) != 0 || done != (uint16_t)size) {
		farfree(buf);
		dosfs_close(h);
		return 0;
	}
	dosfs_close(h);
	return buf;
}

/* C_1463_00AE: LOAD_to(name, buf): the file (its size's low 16 bits) into buf; returns
 * that size, 0 on error. Not called by U. */
int LOAD_to(char *name, void *buf)
{
	int16_t h;
	uint32_t size;
	uint16_t done;

	if (dosfs_open(name, 0, &h) != 0)
		return 0;
	if (file_size(h, &size) != 0 || dosfs_read(h, buf, (uint16_t)size, &done) != 0
			|| done != (uint16_t)size) {
		dosfs_close(h);
		return 0;
	}
	dosfs_close(h);
	return (int16)(uint16_t)size;   /* AX, an int to U */
}
