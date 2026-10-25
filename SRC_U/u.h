/*
 * Ultima VI - The False Prophet: U.EXE (main menu, intro, character creation)
 * shared declarations of the decompilation
 */
#ifndef U_H
#define U_H

#include <dos.h>
#include "portable.h"

/*OSI file library (assembly, pascal convention), the same as GAME.EXE's OSI_FILE.ASM*/
int16 pascal OSI_open(char *name);/*OSI_open*/
int16 pascal OSI_close(int16 fd);/*OSI_close*/
int32 pascal OSI_read(int16 fd, int32 offset, int32 length, void far *buf);/*OSI_read*/
int32 pascal OSI_write(int16 fd, int32 offset, int32 length, void far *buf);/*OSI_write*/

/*a surface the graphics driver draws on: the screen, a page or an off-screen buffer
  (GAME.EXE's struct tSurface)*/
struct tSurface {
	uint16 segment;/*_00 segment of the pixels*/
	uint16 far *pScanlines;/*_02 offset of each scan line*/
	int16 right, left, top, bottom;/*_06,_08,_0a,_0c clipping rectangle*/
};

/*a text window of the OSI library: where and how the text routines print (OSI.INC's
  tText); TXT_select makes one current*/
struct tText {
	struct tSurface *surface;/*+00 the surface it prints on*/
	int16 curX, curY;/*+02,+04 the cursor*/
	unsigned char far *font;/*+06 height (word), ..., width of each character at +4*/
	char fgColor;/*+0A*/
	char bgColor;/*+0B*/
	char *buffer;/*+0C string buffer of the formatted output*/
	char *bufPtr;/*+0E where its next character goes*/
	char justify;/*+10 alignment (0 left, 1 right, 2 centred, 3 ...)*/
};
extern struct tText console;/*the console's text window*/
#define textWin console.surface/*the console: the surface it writes on*/
#define textX console.curX/*the console: text cursor x*/
#define textY console.curY/*the console: text cursor y*/
#define textFont console.font/*the console: font*/
#define textFg console.fgColor/*the console: text colours*/
#define textBg console.bgColor
#define textBuf console.buffer/*the console: buffer*/
#define textAlign console.justify/*the console: alignment (0 left, 1 right, 2 centred)*/

/*the mouse as the OSI library keeps it (OSI.INC's tMouse; MOUSE_INIT)*/
struct tMouse {
	char buttonL, buttonR;/*+00,+01 the buttons are down*/
	int16 x, y;/*+02,+04*/
	int16 unused06;
	void far *shapes;/*+08 the pointer shapes*/
	int16 shape;/*+0C the shape shown*/
	struct tSurface *surface;/*+0E the surface it is drawn on*/
	void far *saveBuf;/*+10 the background under it*/
	void (far *onMove)();/*+14 called when it moves or a button changes*/
	char installed;/*+18 a mouse is present*/
};
extern struct tMouse mouseInfo;
#define mouseL mouseInfo.buttonL/*mouse buttons*/
#define mouseR mouseInfo.buttonR
#define mouseX mouseInfo.x/*mouse x, y*/
#define mouseY mouseInfo.y
#define mouseShapes mouseInfo.shapes/*mouse pointer shapes*/
#define mouseShape mouseInfo.shape/*mouse pointer shape*/
#define mouseSurface mouseInfo.surface/*the surface the mouse pointer is drawn on*/
#define mouseSave mouseInfo.saveBuf/*mouse background save area*/
#define mouseInstalled mouseInfo.installed/*a mouse is present?*/

/*the graphics driver (xxxDRV.BIN): a far pointer whose offset selects the entry*/
union tDrv {
	void (far *f)();
	struct { uint16 ofs, seg; } p;
};
extern union tDrv gfxDrv;
#define DRV(o) gfxDrv.p.ofs = (o); (*gfxDrv.f)

/*the music driver (U6xxx.DRV): a far pointer whose offset selects the entry (0 init(port),
  6 play(song), 9 stop, 0x0c channel(ch, a, b)); its entries clean the stack themselves*/
union tMus {
	void (far pascal *f)();
	struct { uint16 ofs, seg; } p;
};
extern union tMus musDrv;
#define MUS(o) musDrv.p.ofs = (o); (*musDrv.f)

#endif
