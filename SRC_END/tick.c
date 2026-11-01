/*
 * Ultima VI - The False Prophet: END.EXE
 * segment 02AE: the timer callback of the ending (the cycling colours and the stars),
 * the graphic driver's segment, the Hercules graphics mode
 *
 * This module has inline assembly: Turbo C compiled it through the assembler (tcc -B),
 * like U.EXE's main.c (where DriverSegment's lines and InitHercules come from).
 */
#include "end.h"

/*this module's _BSS*/
/*2104*/extern int16 palIndex;/*the colour being moved by the cycling*/
/*2106*/extern uint16 egaPage;/*EGA: the page the stars are drawn on (passed in es)*/
/*2108*/extern int16 palTemp[3];/*the colour being moved*/
/*210E*/extern int16 palSaved[3];/*colour 31, which goes round to 16*/

extern VID_waitRetrace();/*wait for the vertical retrace*/
extern PAL_set(int16, int16 *);/*set a VGA palette entry*/
extern PAL_get(int16, int16 *);/*read a VGA palette entry*/
/*stars.asm: move the stars and draw them, one routine per video mode*/
extern StarsMCGA();
extern StarsEGA();
extern StarsTandy();
extern StarsCGA();

/*0E5E*/int16 herculesCrtc[9] = {0x3500, 0x2d01, 0x2e02, 0x0703, 0x5b04, 0x0205, 0x5706, 0x5707, 0x0309};
/*0E70*/int16 cycleOdd = 0;/*the colours cycle every other tick*/

/*the timer callback (TIM_setCallback): MCGA, every other tick but in the last scenes,
cycles the colours 16..31 by one; then the stars move (the routines of stars.asm return
straight to the timer handler, over this function's popf)*/
/*C_02AE_0001*/EndTick() {
#ifdef __TURBOC__
	asm pushf
	asm cli
#endif
	if(videoMode == 0x13) {
		if((cycleOdd = !cycleOdd) != 0 && scene < 9) {
			VID_waitRetrace();
			PAL_get(31, palSaved);
			for(palIndex = 30; palIndex >= 16; palIndex--) {
				PAL_get(palIndex, palTemp);
				PAL_set(palIndex + 1, palTemp);
			}
			PAL_set(16, palSaved);
		}
	}
	if(videoMode == 0x13) {
		StarsMCGA();
	} else if(videoMode == 0xd) {
		egaPage = screen.segment ^ 0x200;/*the page not shown*/
#ifdef __TURBOC__
		asm mov es,DGROUP:egaPage/*(DGROUP: MASM takes the extrn Turbo C declares at the end for a _BSS-relative one)*/
#endif
		StarsEGA();
	} else if(videoMode == 9) {
		StarsTandy();
	} else {
		StarsCGA();
	}
#ifdef __TURBOC__
	asm popf
#endif
}

/*the segment of the graphic driver (for the OSI library's entries)*/
/*C_02AE_00BA*/int16 DriverSegment() {
	int16 seg;

#ifdef __TURBOC__
	asm mov bx,offset DGROUP:gfxDrv
	asm mov ax,[bx+2]
	asm mov seg,ax
#else
	seg = 0;/*the port's OSI library calls the driver's C directly*/
#endif
	return seg;
}

/*Hercules graphics: 720x348, 4 interleaved banks of 90 bytes per line;
the 200 lines are drawn on 300 (x1.5)*/
/*C_02AE_00D0*/InitHercules() {
#ifdef __TURBOC__
	asm mov dx,3bfh
	asm mov al,1
	asm out dx,al
	asm mov dx,3b8h
	asm xor al,al
	asm out dx,al
	asm sub dl,4
	asm mov cx,9
	asm mov si,offset herculesCrtc
crtc:
	asm lodsw
	asm out dx,ax
	asm loop crtc
	asm add dl,4
	asm mov al,0ah
	asm out dx,al
	/*clear the screen*/
	asm mov ax,0b000h
	asm mov es,ax
	asm mov cx,3fffh
	asm xor ax,ax
	asm xor di,di
	asm rep stosw
	/*the scanline offsets*/
	asm xor si,si
	asm xor dx,dx
	asm xor di,di
line:
	asm mov ax,di
	asm add ax,0fh
	asm xor bx,bx
	asm shr ax,1
	asm rcr bx,1
	asm shr ax,1
	asm rcr bx,1
	asm shr bx,1
	asm mov ah,5ah
	asm mul ah
	asm add bx,ax
	asm shl si,1
	asm add bx,5
	asm mov scanlineOfs[si],bx
	asm sar si,1
	asm inc si
	asm inc di
	asm inc dl
	asm cmp dl,2
	asm jnz next
	asm xor dl,dl
	asm inc di
next:
	asm cmp si,0c8h
	asm jnz line
#endif
}
