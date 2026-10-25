/*
 * Ultima VI - The False Prophet: U.EXE (main menu, intro, character creation)
 * shared declarations of the decompilation
 */
#ifndef U_H
#define U_H

#include <dos.h>

/*OSI file library (assembly, pascal convention), the same as GAME.EXE's OSI_FILE.ASM*/
int pascal OSI_open(char *name);/*OSI_open*/
int pascal OSI_close(int fd);/*OSI_close*/
long pascal OSI_read(int fd, long offset, long length, void far *buf);/*OSI_read*/
long pascal OSI_write(int fd, long offset, long length, void far *buf);/*OSI_write*/

/*a surface the graphics driver draws on: the screen, a page or an off-screen buffer
  (GAME.EXE's struct tSurface)*/
struct tSurface {
	unsigned segment;/*_00 segment of the pixels*/
	unsigned far *pScanlines;/*_02 offset of each scan line*/
	int right, left, top, bottom;/*_06,_08,_0a,_0c clipping rectangle*/
};

/*the graphics driver (xxxDRV.BIN): a far pointer whose offset selects the entry*/
union tDrv {
	void (far *f)();
	struct { unsigned ofs, seg; } p;
};
extern union tDrv gfxDrv;
#define DRV(o) gfxDrv.p.ofs = (o); (*gfxDrv.f)

/*the music driver (U6xxx.DRV): a far pointer whose offset selects the entry (0 init(port),
  6 play(song), 9 stop, 0x0c channel(ch, a, b)); its entries clean the stack themselves*/
union tMus {
	void (far pascal *f)();
	struct { unsigned ofs, seg; } p;
};
extern union tMus musDrv;
#define MUS(o) musDrv.p.ofs = (o); (*musDrv.f)

#endif
