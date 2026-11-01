/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: palette fades
 *
 * FADE_in and FADE_out had inline assembly (Turbo C compiles that through TASM): it is
 * given here as __emit__ bytes, one instruction a line. Two stand-ins reproduce what the
 * inline assembly did to the code around it: the unused register variables make Turbo C
 * save SI and DI (the assembly uses them), and the volatile locals stay on the stack
 * (Turbo C makes no register variables of its own in a function with assembly).
 */
/*the library was compiled through the assembler (tcc -B): this does the same*/
#pragma inline
#include <dos.h>
#include <stdlib.h>
#include "OSILIB.H"

#pragma warn -eff
#pragma warn -def
#pragma warn -par

struct tRGB { int16 r, g, b; };

/*the graphics driver (as in u.h)*/
union tDrv {
	void (far *f)();
	struct { uint16 ofs, seg; } p;
};
extern union tDrv gfxDrv;
#define DRV(o) gfxDrv.p.ofs = (o); (*gfxDrv.f)

/*45E2*/int16 fadeStart = 1;/*1: the next FADE_step starts a fade*/
/*this module's _BSS*/
extern int16 *fadeErr;/*FADE_step: error term of each value*/
extern int16 *fadeDist;/*  its distance to go*/
extern int16 *fadeDir;/*  its direction (+1/-1)*/
extern int16 fadeSteps;/*  the largest distance: the number of steps*/
extern int16 fadeLeft;/*  the steps left*/
extern char *fadeInColors;/*FADE_in: the colors used by the picture*/
extern char *fadeOutColors;/*FADE_out: the same*/

extern DRW_getPixel(struct tSurface *s, int16 x, int16 y);/*DRW_getPixel*/
extern VID_waitRetrace(void);/*wait for the vertical retrace*/
extern PAL_set(int16 index, struct tRGB *rgb);/*PAL_set*/
extern PAL_get(int16 index, struct tRGB *rgb);/*PAL_get*/

/*move each of the n values of cur one step towards target (all of them arrive in the
  same number of steps, as the points of Bresenham's lines): 0 when they have arrived*/
/*C_136D_0004*/FADE_step(int16 *cur, int16 *target, int16 n) {
	register int16 i;
	register int16 d;

	if(fadeStart) {
		fadeErr = malloc(n * 2);
		fadeDist = malloc(n * 2);
		fadeDir = malloc(n * 2);
		fadeSteps = 0;
		for(i = 0; i < n; i++) {
			d = cur[i] - target[i];
			if(d < 0) {
				d = -d;
				fadeDir[i] = 1;
			} else {
				fadeDir[i] = -1;
			}
			fadeDist[i] = d;
			if(d > fadeSteps)
				fadeSteps = d;
		}
		d = fadeSteps / 2;
		for(i = 0; i < n; i++)
			fadeErr[i] = d;
		fadeLeft = fadeSteps;
		fadeStart = 0;
	}
	if(fadeLeft-- == 0) {
		free(fadeErr);
		free(fadeDist);
		free(fadeDir);
		fadeStart = 1;
		return 0;
	}
	for(i = 0; i < n; i++) {
		if((fadeErr[i] += fadeDist[i]) > fadeSteps) {
			fadeErr[i] -= fadeSteps;
			cur[i] += fadeDir[i];
		}
	}
	return 1;
}

/*show the picture src on dst (driver entry 0Fh), its colors fading in from the color
  of dst's top left pixel*/
/*C_136D_014D*/FADE_in(struct tSurface *src, struct tSurface *dst) {
	volatile int16 count;
	volatile int16 i;
	int16 row;
	int16 width;
	int16 height;
	int16 *list;
	struct tRGB *cur;
	struct tRGB *target;
	struct tRGB rgb;
	register int16 si_, di_;

	row; width; height;/*(used by the assembly)*/
	list = calloc(256, 2);
	fadeInColors = calloc(256, 1);
	/*the colors used in src's window*/
	__emit__(0x56);/*push si*/
	__emit__(0x57);/*push di*/
	__emit__(0x8B, 0x5E, 0x06);/*mov bx,src*/
	__emit__(0x8B, 0x47, 0x06);/*mov ax,[bx].right*/
	__emit__(0x2B, 0x47, 0x08);/*sub ax,[bx].left*/
	__emit__(0x40);/*inc ax*/
	__emit__(0x89, 0x46, 0xF0);/*mov width,ax*/
	__emit__(0x8B, 0x47, 0x0C);/*mov ax,[bx].bottom*/
	__emit__(0x2B, 0x47, 0x0A);/*sub ax,[bx].top*/
	__emit__(0x40);/*inc ax*/
	__emit__(0x89, 0x46, 0xF2);/*mov height,ax*/
	__emit__(0xC7, 0x46, 0xEE, 0x00, 0x00);/*mov row,0*/
	__emit__(0x8E, 0x07);/*mov es,[bx].segment*/
	/*L1:*/
	__emit__(0x8B, 0x5E, 0x06);/*mov bx,src*/
	__emit__(0x8B, 0x7F, 0x02);/*mov di,word ptr [bx].scanlines*/
	__emit__(0x8B, 0x77, 0x0A);/*mov si,[bx].top*/
	__emit__(0x03, 0x76, 0xEE);/*add si,row*/
	__emit__(0xD1, 0xE6);/*shl si,1*/
	__emit__(0x1E);/*push ds*/
	__emit__(0x8E, 0x5F, 0x04);/*mov ds,word ptr [bx].scanlines+2*/
	__emit__(0x03, 0xF7);/*add si,di*/
	__emit__(0x8B, 0x34);/*mov si,[si]*/
	__emit__(0x1F);/*pop ds*/
	__emit__(0x03, 0x77, 0x08);/*add si,[bx].left*/
	__emit__(0xB7, 0x00);/*mov bh,0*/
	__emit__(0x8B, 0x3E, &fadeInColors);/*mov di,fadeInColors*/
	__emit__(0xFC);/*cld*/
	__emit__(0x8B, 0x4E, 0xF0);/*mov cx,width*/
	/*L2:*/
	__emit__(0x26, 0xAC);/*lods byte ptr es:[si]*/
	__emit__(0x8A, 0xD8);/*mov bl,al*/
	__emit__(0xC6, 0x01, 0x01);/*mov byte ptr [bx+di],1*/
	__emit__(0xE2, 0xF7);/*loop L2*/
	__emit__(0xFF, 0x46, 0xEE);/*inc row*/
	__emit__(0xFF, 0x4E, 0xF2);/*dec height*/
	__emit__(0x75, 0xCB);/*jnz L1*/
	__emit__(0x5F);/*pop di*/
	__emit__(0x5E);/*pop si*/
	count = 0;
	for(i = 0; i < 256; i++)
		if(fadeInColors[i])
			list[count++] = i;
	free(fadeInColors);
	cur = calloc(count, sizeof(struct tRGB));
	target = calloc(count, sizeof(struct tRGB));
	PAL_get(DRW_getPixel(dst, dst->left, dst->top), &rgb);
	VID_waitRetrace();
	for(i = 0; i < count; i++) {
		PAL_get(list[i], &cur[i]);
		PAL_set(list[i], &rgb);
		target[i] = rgb;
	}
	DRV(0xf)(src, dst);
	while(FADE_step((int16 *)target, (int16 *)cur, count * 3)) {
		VID_waitRetrace();
		for(i = 0; i < count; i++)
			PAL_set(list[i], &target[i]);
	}
	free(target);
	free(cur);
	free(list);
}

/*fade the colors used in the window of s to the palette's color "color"*/
/*C_136D_0346*/FADE_out(struct tSurface *s, int16 color) {
	volatile int16 count;
	volatile int16 i;
	int16 row;
	int16 width;
	int16 height;
	int16 *list;
	struct tRGB *cur;
	struct tRGB *target;
	struct tRGB rgb;
	register int16 si_, di_;

	row; width; height;/*(used by the assembly)*/
	list = calloc(256, 2);
	fadeOutColors = calloc(256, 1);
	/*the colors used in s's window (its scanline table taken as near: a bug?)*/
	__emit__(0x56);/*push si*/
	__emit__(0x57);/*push di*/
	__emit__(0x8B, 0x5E, 0x06);/*mov bx,s*/
	__emit__(0x8B, 0x47, 0x06);/*mov ax,[bx].right*/
	__emit__(0x2B, 0x47, 0x08);/*sub ax,[bx].left*/
	__emit__(0x40);/*inc ax*/
	__emit__(0x89, 0x46, 0xF0);/*mov width,ax*/
	__emit__(0x8B, 0x47, 0x0C);/*mov ax,[bx].bottom*/
	__emit__(0x2B, 0x47, 0x0A);/*sub ax,[bx].top*/
	__emit__(0x40);/*inc ax*/
	__emit__(0x89, 0x46, 0xF2);/*mov height,ax*/
	__emit__(0xC7, 0x46, 0xEE, 0x00, 0x00);/*mov row,0*/
	__emit__(0x8E, 0x07);/*mov es,[bx].segment*/
	/*L1:*/
	__emit__(0x8B, 0x5E, 0x06);/*mov bx,s*/
	__emit__(0x8B, 0x7F, 0x02);/*mov di,word ptr [bx].scanlines*/
	__emit__(0x8B, 0x77, 0x0A);/*mov si,[bx].top*/
	__emit__(0x03, 0x76, 0xEE);/*add si,row*/
	__emit__(0xD1, 0xE6);/*shl si,1*/
	__emit__(0x03, 0xF7);/*add si,di*/
	__emit__(0x8B, 0x34);/*mov si,[si]*/
	__emit__(0x03, 0x77, 0x08);/*add si,[bx].left*/
	__emit__(0xB7, 0x00);/*mov bh,0*/
	__emit__(0x8B, 0x3E, &fadeOutColors);/*mov di,fadeOutColors*/
	__emit__(0xFC);/*cld*/
	__emit__(0x8B, 0x4E, 0xF0);/*mov cx,width*/
	/*L2:*/
	__emit__(0x26, 0xAC);/*lods byte ptr es:[si]*/
	__emit__(0x8A, 0xD8);/*mov bl,al*/
	__emit__(0xC6, 0x01, 0x01);/*mov byte ptr [bx+di],1*/
	__emit__(0xE2, 0xF7);/*loop L2*/
	__emit__(0xFF, 0x46, 0xEE);/*inc row*/
	__emit__(0xFF, 0x4E, 0xF2);/*dec height*/
	__emit__(0x75, 0xD0);/*jnz L1*/
	__emit__(0x5F);/*pop di*/
	__emit__(0x5E);/*pop si*/
	count = 0;
	for(i = 0; i < 256; i++)
		if(fadeOutColors[i])
			list[count++] = i;
	free(fadeOutColors);
	cur = calloc(count, sizeof(struct tRGB));
	target = calloc(count, sizeof(struct tRGB));
	PAL_get(color, &rgb);
	VID_waitRetrace();
	for(i = 0; i < count; i++) {
		PAL_get(list[i], &cur[i]);
		target[i] = rgb;
	}
	while(FADE_step((int16 *)cur, (int16 *)target, count * 3)) {
		VID_waitRetrace();
		for(i = 0; i < count; i++)
			PAL_set(list[i], &cur[i]);
	}
	free(target);
	free(cur);
	free(list);
}
