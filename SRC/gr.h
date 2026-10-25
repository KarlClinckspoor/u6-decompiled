/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __GR_H__
#define __GR_H__

struct tGr {
	/*common part*/
	unsigned char header[6];/*_00*/
	void far *font;/*_06*//*character data*/
	unsigned char textFg,textBg;/*text foreground / background color*/
	/* */
	unsigned char xorMode;/*1 during flash/noise effects (storm cloak noise, spell flash); likely XOR drawing (low)*/
	unsigned char unknown0d;/*unused by the game*/
	unsigned char animOn;/*driver tile animation enabled (0 while time is stopped)*/
	unsigned char toBackBuffer;/*1: GR_SET_COLOR/09/12 primitives draw into the map back buffer instead of the screen*/
	void (*pfnDiskConfirm)();/*callback = DISK_confirm*/
	unsigned char reserved14[8];/*unknown*/
	void far *pWorkBuf;/*= ScratchBuf (map back buffer / decompress work)*/
	void far *pDriverMem;/*set by GR_INIT_MEMORY, 0 if not enough memory (tile graphics?)*/
	void (*pfnLoadFile)();/*callback = LoadFileGr*/
	void (*pfnOpen)();/*callback = OSI_open*/
	void (*pfnRead)();/*callback = OSI_read*/
	void (*pfnClose)();/*callback = OSI_close*/
};

struct tPixmap/*size:6*/ {
	int16 width,height;/*_00,_02*/
	unsigned char data[0x1000];/*_04*/
};

/*init driver (video mode or segment + GrDriverOption), returns struct tGr far **/
#define GR_INIT(a)               (GrDriver.iii.ofs = 0x00, (struct tGr far *)(*GrDriver.f)(a))
/*restore video mode before exit/exec*/
#define GR_SHUTDOWN()                (GrDriver.iii.ofs = 0x03, (*GrDriver.f)())
/*drawing color*/
#define GR_SET_COLOR(c)               (GrDriver.iii.ofs = 0x06, (*GrDriver.f)(c))
/*plot point*/
#define GR_PUT_PIXEL(x,y)             (GrDriver.iii.ofs = 0x09, (*GrDriver.f)(x,y))
/*fill rectangle x0,y0,x1,y1*/
#define GR_FILL_RECT(x0,y0,x1,y1)     (GrDriver.iii.ofs = 0x12, (*GrDriver.f)(x0,y0,x1,y1))
/*move rectangle vertically by dy (text scroll)*/
#define GR_SCROLL_RECT_Y(x0,y0,x1,y1,dy)  (GrDriver.iii.ofs = 0x15, (*GrDriver.f)(x0,y0,x1,y1,dy))
/*draw tPixmap at x,y on screen*/
#define GR_DRAW_PIXMAP(p,x,y)           (GrDriver.iii.ofs = 0x18, (*GrDriver.f)(p,x,y))
/*8x8 font char with tGr text colors*/
#define GR_PUT_CHAR(ch,x,y)          (GrDriver.iii.ofs = 0x1e, (*GrDriver.f)(ch,x,y))
/*non-VGA palette setup*/
#define GR_INIT_PALETTE()                (GrDriver.iii.ofs = 0x24, (*GrDriver.f)())
/*non-VGA counterpart of PaletteAnimation*/
#define GR_CYCLE_PALETTE(a)               (GrDriver.iii.ofs = 0x27, (*GrDriver.f)(a))
/*give driver memory (segment, paragraphs); it loads its data, result in tGr._20*/
#define GR_INIT_MEMORY(seg,size)        (GrDriver.iii.ofs = 0x2a, (*GrDriver.f)(seg,size))
/*draw tile directly on screen*/
#define GR_PUT_TILE_SCREEN(tile,x,y)        (GrDriver.iii.ofs = 0x2d, (*GrDriver.f)(tile,x,y))
/*(slot, tile, rotation, pivotX, pivotY) build a rotated sprite*/
#define GR_DEFINE_SPRITE(i,a,b,c,d)       (GrDriver.iii.ofs = 0x33, (*GrDriver.f)(i,a,b,c,d))
/*draw sprite slot at x,y in back buffer (also used to put saved background back)*/
#define GR_DRAW_SPRITE(i,x,y)           (GrDriver.iii.ofs = 0x36, (*GrDriver.f)(i,x,y))
/*copy back buffer area at x,y into sprite slot (to restore later with GR_DRAW_SPRITE) - medium*/
#define GR_SAVE_SPRITE_BG(i,x,y)           (GrDriver.iii.ofs = 0x39, (*GrDriver.f)(i,x,y))
/*low: called every 2nd animation tick, presumably driver-side tile animation*/
#define GR_ANIMATE_TILES()                (GrDriver.iii.ofs = 0x3f, (*GrDriver.f)())
/*draw tile into the back buffer*/
#define GR_PUT_TILE(tile,x,y)        (GrDriver.iii.ofs = 0x42, (*GrDriver.f)(tile,x,y))
/*copy back buffer rect x0,y0,x1,y1 to screen at x,y*/
#define GR_COPY_TO_SCREEN(x0,y0,x1,y1,a,b) (GrDriver.iii.ofs = 0x45, (*GrDriver.f)(x0,y0,x1,y1,a,b))
/*same with fade-in effect (ScreenFade)*/
#define GR_FADE_TO_SCREEN(x0,y0,x1,y1)     (GrDriver.iii.ofs = 0x48, (*GrDriver.f)(x0,y0,x1,y1))

/*make tile a display frame b (tile animation, sundial)*/
#define GR_SET_ANIM_FRAME(a,b)             (GrDriver.iii.ofs = 0x4b, (*GrDriver.f)(a,b))
/*low: only used when the Codex vanishes, some screen transition effect*/
#define GR_CODEX_EFFECT()                (GrDriver.iii.ofs = 0x4e, (*GrDriver.f)())

#endif
