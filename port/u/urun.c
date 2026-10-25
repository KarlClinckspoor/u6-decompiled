/* U.EXE in the port: what its two small assembly modules did (lzw.asm's LoadZFile,
 * criterr.asm's DOS handlers), the code segment its noise effects read (_CS), and
 * u_run, which runs U's main.
 *
 * This file is compiled as the port's own C (not with uport.h), so U's renamed names are
 * written out: U_LoadZFile is U's LoadZFile, and so on. */
#include "urun.h"
#include "../sys/dosfs.h"
#include "../sys/dosmem.h"
#include "../sys/osi.h"
#include "../platform/plat.h"
#include "../gfx/mcga.h"
#include "../gfx/vgadac.h"
#include "ugfx.h"

#include <stdlib.h>
#include <string.h>

int U_main(int argc, char **argv);

/* ---- lzw.asm ---- */

/* LoadZFile(name): loads an LZW-compressed driver file into far memory (the graphics and
 * the music drivers): the drivers are C in the port, so the bytes are only loaded */
void *U_LoadZFile(char *name)
{
	int16_t fd;
	uint8_t hdr[4];
	uint16_t got = 0;
	uint32_t size;
	void *dest, *work;

	if (dosfs_open(name, 0, &fd) != 0)
		return 0;
	if (dosfs_read(fd, hdr, 4, &got) != 0 || got != 4) {
		dosfs_close(fd);
		return 0;
	}
	size = hdr[0] | hdr[1] << 8 | (uint32_t)hdr[2] << 16 | (uint32_t)hdr[3] << 24;
	dest = dos_farmalloc(size + 16);
	work = dos_farmalloc(0x3400);
	if (dest && work)
		decompress(fd, 0, dest, (uint16_t)(dos_fp_seg(work) + 1));
	if (work)
		dos_farfree(work);
	dosfs_close(fd);
	return dest;
}

/* ---- criterr.asm ---- */

/* set to 1 by the int 24h handler (U's disk code reads it); the port's files never fail
 * that way */
int16_t U_critError;

/* installs the int 24h (critical error) and int 23h (Ctrl-Break) handlers: nothing to do */
int U_SetDosHandlers(void)
{
	return 0;
}

/* ---- _CS: U.EXE's code ---- */

static uint16_t image_seg;

uint16_t u_code_segment(uint16_t frame)
{
	return (uint16_t)(image_seg + frame);
}

/* U.EXE's image (its code and initialised data, as DOS loads it, relocations aside): the
 * noise of the monitor and of the gypsy's pattern is bytes of the running code. Without
 * U.EXE, random bytes. */
static void load_image(void)
{
	int16_t fd;
	uint8_t hdr[0x1c];
	uint16_t got = 0;
	uint32_t size = 103332, hsize = 0x1a00;
	uint8_t *p;

	if (dosfs_open("U.EXE", 0, &fd) == 0) {
		if (dosfs_read(fd, hdr, sizeof hdr, &got) == 0 && got == sizeof hdr && hdr[0] == 'M' && hdr[1] == 'Z') {
			uint32_t pages = hdr[4] | hdr[5] << 8, last = hdr[2] | hdr[3] << 8;
			hsize = (uint32_t)(hdr[8] | hdr[9] << 8) * 16;
			size = pages * 512 - (last ? 512 - last : 0) - hsize;
		}
	} else
		fd = -1;
	p = dos_farmalloc(size + 16);
	if (!p) {
		if (fd >= 0)
			dosfs_close(fd);
		return;
	}
	image_seg = (uint16_t)(dos_fp_seg(p) + (dos_fp_off(p) + 15) / 16);
	p = dos_mk_fp(image_seg, 0);
	memset(p, 0, size);
	if (fd >= 0) {
		uint32_t done = 0;
		dosfs_seek(fd, 0, (int32_t)hsize, 0);
		while (done < size) {
			uint16_t n = (uint16_t)(size - done > 0x8000 ? 0x8000 : size - done);
			got = 0;
			if (dosfs_read(fd, p + done, n, &got) != 0 || !got)
				break;
			done += got;
		}
		dosfs_close(fd);
	} else {
		uint32_t i, x = 0x12345678;
		for (i = 0; i < size; i++) {
			x = x * 1103515245u + 12345u;
			p[i] = (uint8_t)(x >> 16);
		}
	}
}

/* MCGADRV.BIN's entry 0 sets mode 13h: the screen is A000:0000 of the arena, with the
 * BIOS's palette */
static void mode_set(void)
{
	mcga_bios_palette();
	plat_video_source(dos_mk_fp(0xA000, 0), vga_palette);
	plat_video_dirty();
}

int u_run(int argc, char **argv)
{
	ugfx_set_mode_hook(mode_set);
	load_image();
	return U_main(argc, argv);
}
