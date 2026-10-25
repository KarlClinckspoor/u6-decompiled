/* The MCGA graphics driver (U6MCGA.DRV, VGA mode 13h) rewritten in C.
 *
 * In DOS the driver is a separate binary that the game loads at run time (LoadFile with
 * IsZFile = 1: the file is LZW compressed, 13796 bytes once unpacked) and calls through a
 * far pointer whose offset is patched with the entry number (gr.h). It is a 27-entry jump
 * table (3 bytes an entry) followed by its data and code; everything is reached as CS:xxxx.
 * The game is built with Turbo C -mm -p: medium model (near data, far code) and the Pascal
 * calling convention, so arguments are pushed left to right and the driver pops them
 * (RETF n). The functions below take the arguments in that same order: the first argument
 * is the one pushed first, as written in the GR_* macro.
 *
 * Full description: docs/port/MCGA_DRIVER.md.
 *
 * Mapping of the GR_* macros of SRC/gr.h (and of the mouse calls of MOUSE.ASM):
 *
 *   GR_INIT(a)                         GRDRV_00(a)            returns struct tGr *
 *   GR_SHUTDOWN()                      GRDRV_03()
 *   GR_SET_COLOR(c)                    GRDRV_06(c)
 *   GR_PUT_PIXEL(x,y)                  GRDRV_09(x,y)
 *   (MOUSE.ASM _MOUSE_SAVE_BG, 0Ch)    GRDRV_0C(surface,saveBuf,posX,posY,shapes,shapeIndex)
 *   (MOUSE.ASM _MOUSE_01B0, 0Fh)       GRDRV_0F(surface,saveBuf,posX,posY,shapes,shapeIndex)
 *   GR_FILL_RECT(x0,y0,x1,y1)          GRDRV_12(x0,y0,x1,y1)
 *   GR_SCROLL_RECT_Y(x0,y0,x1,y1,dy)   GRDRV_15(x0,y0,x1,y1,dy)
 *   GR_DRAW_PIXMAP(p,x,y)              GRDRV_18(p,x,y)
 *   (none; RETF 4)                     GRDRV_1B(a,b)          no-op
 *   GR_PUT_CHAR(ch,x,y)                GRDRV_1E(ch,x,y)
 *   (none; RETF)                       GRDRV_21()             no-op
 *   GR_INIT_PALETTE()                  GRDRV_24()             no-op (EGA/CGA only)
 *   GR_CYCLE_PALETTE(a)                GRDRV_27(a)            no-op (EGA/CGA only), see doc
 *   GR_INIT_MEMORY(seg,size)           GRDRV_2A(seg,size)
 *   GR_PUT_TILE_SCREEN(tile,x,y)       GRDRV_2D(tile,x,y)
 *   (MOUSE.ASM _MOUSE_DRAW, 30h)       GRDRV_30(surface,posX,posY,shapes,shapeIndex)
 *   GR_DEFINE_SPRITE(i,a,b,c,d)        GRDRV_33(i,a,b,c,d)    slot, tile, rotation, pivot x, pivot y
 *   GR_DRAW_SPRITE(i,x,y)              GRDRV_36(i,x,y)
 *   GR_SAVE_SPRITE_BG(i,x,y)           GRDRV_39(i,x,y)
 *   (none; RETF)                       GRDRV_3C()             no-op
 *   GR_ANIMATE_TILES()                 GRDRV_3F()
 *   GR_PUT_TILE(tile,x,y)              GRDRV_42(tile,x,y)
 *   GR_COPY_TO_SCREEN(x0,y0,x1,y1,a,b) GRDRV_45(x0,y0,x1,y1,a,b)
 *   GR_FADE_TO_SCREEN(x0,y0,x1,y1)     GRDRV_48(x0,y0,x1,y1)
 *   GR_SET_ANIM_FRAME(a,b)             GRDRV_4B(a,b)
 *   GR_CODEX_EFFECT()                  GRDRV_4E()
 *
 * For the port's gr.h, e.g.:
 *   #define GR_PUT_TILE(tile,x,y)  GRDRV_42((tile),(x),(y))
 *   #define GR_INIT(a)             ((struct tGr *)GRDRV_00(a))
 *   #define GR_DRAW_PIXMAP(p,x,y)  GRDRV_18((p),(x),(y))
 *
 * The mouse entries, as the port's MOUSE.ASM replacement should call them (it pushed the
 * fields of the tMouse struct, see mouse.h; the surface is the near pointer Mouse.surface):
 *
 *   #define MOUSE_GR_SAVE_BG()    GRDRV_0C(Mouse.surface, Mouse.saveBuf, Mouse.posX, Mouse.posY, Mouse.shapes, Mouse.shapeIndex)
 *   #define MOUSE_GR_RESTORE_BG() GRDRV_0F(Mouse.surface, Mouse.saveBuf, Mouse.posX, Mouse.posY, Mouse.shapes, Mouse.shapeIndex)
 *   #define MOUSE_GR_DRAW()       GRDRV_30(Mouse.surface, Mouse.posX, Mouse.posY, Mouse.shapes, Mouse.shapeIndex)
 *
 *   void GRDRV_0C(const void *surface, void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
 *   void GRDRV_0F(const void *surface, const void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
 *   void GRDRV_30(const void *surface, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
 *
 * Memory: the driver's own segment (its tables, at their DOS offsets) is a 64 KB array
 * here; far pointers the game passes are turned back into segment:offset (dosmem.h,
 * normalised) so that offset arithmetic wraps at 64 KB exactly as in DOS; segment
 * numbers (driver memory, tile cache pages, sprites, the mouse surface's segment) address
 * the DOS arena; A000:0000-FFFF is the driver's 64 KB screen buffer (mcga_screen()).
 */
#ifndef MCGA_H
#define MCGA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game packs its structs (Turbo C byte alignment, compat.h); these must match. */
#pragma pack(push, 1)

/* struct tSurface of SRC/mouse.h as the host compiles it (the mouse entries read it). */
struct McgaSurface {
	uint16_t segment;
	uint16_t *pScanlines;   /* scanline offsets within segment */
	uint16_t right, left, top, bottom;
};

/* struct tGr of SRC/gr.h as the host compiles it ("far" empty): the block GRDRV_00
 * returns. The callbacks are declared as in gr.h; the driver calls them as: */
typedef int16_t McgaDiskConfirmFn(int16_t disk);                      /* DISK_confirm */
typedef int16_t McgaLoadFileFn(char *name, void *dest, int16_t isZ);  /* LoadFileGr */
typedef int16_t McgaOpenFn(char *path);                               /* OSI_open */
typedef uint32_t McgaReadFn(int16_t handle, uint32_t offset, uint32_t length, void *buf); /* OSI_read */
typedef int16_t McgaCloseFn(int16_t handle);                          /* OSI_close */

struct McgaInfo {
	uint8_t header[6];          /* 00 00 40 01 C8 00: words 0, 320, 200; not read */
	void *font;                 /* 8x8 font, 8 bytes a character (U6.CH) */
	uint8_t textFg, textBg;     /* GRDRV_1E colors (initially 48h, 31h) */
	uint8_t xorMode;            /* GRDRV_09/12 XOR the color in */
	uint8_t unknown0d;          /* 1; not read by this driver */
	uint8_t animOn;             /* 1; not read by this driver */
	uint8_t toBackBuffer;       /* GRDRV_09/12 draw into pWorkBuf (176 wide) */
	void (*pfnDiskConfirm)();
	uint8_t reserved14[8];
	void *pWorkBuf;             /* the 176x176 back buffer (ScratchBuf) */
	void *pDriverMem;           /* set by GRDRV_2A on success */
	void (*pfnLoadFile)();
	void (*pfnOpen)();
	void (*pfnRead)();
	void (*pfnClose)();
};

#pragma pack(pop)

/* Entry 00h: GR_INIT. Sets mode 13h (clears the 64 KB screen, loads the BIOS default
 * palette into vga_palette), resets the driver to the state of a freshly loaded image and
 * returns the struct tGr. The argument (13h for MCGA) is not used. */
void *GRDRV_00(int16_t mode);
void GRDRV_03(void);
void GRDRV_06(int16_t color);
void GRDRV_09(int16_t x, int16_t y);
void GRDRV_0C(const void *surface, void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
void GRDRV_0F(const void *surface, const void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
void GRDRV_12(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void GRDRV_15(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t dy);
void GRDRV_18(const void *pixmap, int16_t x, int16_t y);
void GRDRV_1B(int16_t a, int16_t b);
void GRDRV_1E(int16_t ch, int16_t x, int16_t y);
void GRDRV_21(void);
void GRDRV_24(void);
void GRDRV_27(int16_t a);
/* Returns AX as the driver leaves it: 0 when size < 3000h paragraphs, else the value the
 * last LoadFileGr call returned (the game ignores it and tests tGr.pDriverMem). */
int16_t GRDRV_2A(uint16_t seg, uint16_t size);
void GRDRV_2D(int16_t tile, int16_t x, int16_t y);
void GRDRV_30(const void *surface, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex);
void GRDRV_33(int16_t slot, int16_t tile, int16_t rot, int16_t pivotX, int16_t pivotY);
void GRDRV_36(int16_t slot, int16_t x, int16_t y);
void GRDRV_39(int16_t slot, int16_t x, int16_t y);
void GRDRV_3C(void);
void GRDRV_3F(void);
void GRDRV_42(int16_t tile, int16_t x, int16_t y);
void GRDRV_45(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t dstX, int16_t dstY);
void GRDRV_48(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void GRDRV_4B(int16_t tile, int16_t frame);
void GRDRV_4E(void);

/* The screen: A000:0000-FFFF (the first 64000 bytes are the 320x200 picture). */
uint8_t *mcga_screen(void);
/* The driver's segment (64 KB, the tables at their DOS offsets); for tests and debugging. */
uint8_t *mcga_segment(void);
/* Optional, before GRDRV_00: the unpacked U6MCGA.DRV (13796 bytes; GrDriver.p in the
 * port). GRDRV_00 then starts from these bytes instead of the built-in copy of the data,
 * so that even reads of the original's code bytes (tables indexed out of range by bad
 * arguments) give the same values as in DOS. */
void mcga_attach_image(const void *image, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
