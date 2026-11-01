/* SURFACE.C, SURFSHR.C: surfaces with a bitmap of their own in far memory, and surfaces
 * sharing one (docs/port/U_OSILIB.md).
 *
 * The far heap is the port's Turbo C far heap over the DOS arena (dosmem.h, through
 * compat.h's farmalloc/farfree); farcalloc, which compat.h does not have, is farmalloc
 * and a clear, as Turbo C's. */
#include "uosi.h"
#include "ufar.h"

#include <stdarg.h>
#include <string.h>

/* Turbo C's farcalloc(n, size): n * size (unsigned long) bytes, cleared */
static void *u_farcalloc(uint32 n, uint32 size)
{
	uint32 bytes = n * size;
	void *p = farmalloc(bytes);

	if (p)
		memset(p, 0, bytes);
	return p;
}

/* C_1277_000C: allocate the pixels of the window (width * height bytes, one block) and a
 * table of height scanline offsets. On failure the original prints a message and waits
 * for a key, then goes on with the NULL pointer (segment 0). The table is indexed from 0:
 * U's callers allocate surfaces whose top is 0. */
int SURF_alloc(struct tSurface *s)
{
	uint8_t *p;
	uint16 ofs;
	int16 height, width, i;

	height = (int16)(s->bottom - s->top + 1);
	width = (int16)(s->right - s->left + 1);
	p = (uint8_t *)u_farcalloc(1, (uint32)((int32)height * width));
	if (p == 0) {
		TXT_printf(" << No far memory! >> ");
		getch();
	}
	s->segment = FP_SEG(p);
	ofs = FP_OFF(p);
	s->pScanlines = (uint16 *)farmalloc((uint32)((int32)height * 2));
	if (s->pScanlines == 0) {
		TXT_printf(" << No ytable memory! >> ");
		getch();
	}
	if (s->pScanlines == 0)
		return 0;   /* the original writes the table at 0000:0000 (the interrupt vectors) */
	for (i = 0; i < height; i++) {
		s->pScanlines[i] = ofs;
		ofs = (uint16)(ofs + width);
	}
	return 0;
}

/* C_1277_00D8: the pixels are found again from the segment and the first scanline */
int SURF_free(struct tSurface *s)
{
	void *p;

	p = MK_FP(s->segment, s->pScanlines[0]);
	farfree(s->pScanlines);
	farfree(p);
	return 0;
}

/* C_12EA_000A: SURF_share(src, s1, s2, ..., 0): each s gets src's bitmap (segment and
 * scanline table; its window stays). U ends the list with a plain int 0: on a 64-bit host
 * it is read back as a pointer whose upper half may be garbage, so a pointer whose low 32
 * bits are 0 ends the list (as dosint.c's execl does). */
int SURF_share(struct tSurface *src, ...)
{
	va_list ap;
	struct tSurface *s;

	va_start(ap, src);
	for (;;) {
		s = va_arg(ap, struct tSurface *);
		if (((uintptr_t)s & 0xFFFFFFFFu) == 0)
			break;
		s->segment = src->segment;
		s->pScanlines = src->pScanlines;
	}
	va_end(ap);
	return 0;
}
