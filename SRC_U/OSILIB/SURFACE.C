/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: allocating a surface in far memory
 */
#include <alloc.h>
#include <conio.h>
#include <dos.h>
#include "OSILIB.H"

extern TXT_printf(char far *fmt, ...);/*TXT_printf*/

/*allocate the pixels of a surface (the size of its window) and its scanline table*/
/*C_1277_000C*/SURF_alloc(struct tSurface *s) {
	char huge *p;
	uint16 ofs;
	int16 height;
	int16 width;
	register int16 i;

	height = s->bottom - s->top + 1;
	width = s->right - s->left + 1;
	p = farcalloc(1, (int32)height * width);
	if(p == 0) {
		TXT_printf(" << No far memory! >> ");
		getch();
	}
	s->segment = FP_SEG(p);
	ofs = FP_OFF(p);
	s->scanlines = farmalloc((int32)height * 2);
	if(s->scanlines == 0) {
		TXT_printf(" << No ytable memory! >> ");
		getch();
	}
	for(i = 0; i < height; i++) {
		s->scanlines[i] = ofs;
		ofs += width;
	}
}

/*free them*/
/*C_1277_00D8*/SURF_free(struct tSurface *s) {
	void far *p;

	p = MK_FP(s->segment, s->scanlines[0]);
	farfree(s->scanlines);
	farfree(p);
}
