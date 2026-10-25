/*
 * Ultima VI - The False Prophet: U.EXE (main menu, intro, character creation)
 * shared declarations of the decompilation
 */
#ifndef U_H
#define U_H

#include <dos.h>

/*OSI file library (assembly, pascal convention), the same as GAME.EXE's OSI_FILE.ASM*/
int pascal C_0000_09F3(char *name);/*OSI_open*/
int pascal C_0000_0A32(int fd);/*OSI_close*/
long pascal C_0000_0AE4(int fd, long offset, long length, void far *buf);/*OSI_read*/
long pascal C_0000_0BCA(int fd, long offset, long length, void far *buf);/*OSI_write*/

/*the graphics driver (xxxDRV.BIN): a far pointer whose offset selects the entry*/
union tDrv {
	void (far *f)();
	struct { unsigned ofs, seg; } p;
};
extern union tDrv D_4A34;
#define DRV(o) D_4A34.p.ofs = (o); (*D_4A34.f)

#endif
