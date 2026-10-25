/* U.EXE's MCGA graphics driver (MCGADRV.BIN, VGA mode 13h) rewritten in C.
 *
 * In DOS, U.EXE loads the driver with LoadZFile("mcgadrv.bin") (LZW compressed, 2632 bytes
 * unpacked) and calls it through a far pointer whose offset selects the entry:
 * DRV(o)(args) in SRC_U/u.h. The image starts with a jump table (entries 0, 3, 6, 9, 0Ch,
 * 0Fh, 12h, 15h, 18h), then its data (CS:001C..029F) and code. U.EXE is cdecl (Turbo C
 * -mm: far code, near data): the caller pushes the arguments right to left and cleans the
 * stack; every entry ends with a plain RETF. Struct pointers are near (DGROUP), shapes and
 * buffers far. Entry 12h is called with registers (OSILIB/TXTCHAR.ASM). The functions
 * below take the arguments in the order of U.EXE's C (the first one is at [BP+6]), so
 * that the port can turn DRV(o)(a, b) into UDRV_##o(a, b).
 *
 * Full description, with the shape and font formats: docs/port/U_DRIVER.md.
 *
 *   DRV(0)(scanlineOfs)                    UDRV_0(scanlines)
 *       fills scanlines[0..199] with y * 320, then sets mode 13h (int 10h AX=0013h:
 *       clears A000:0000-FFFF; see ugfx_set_mode_hook).
 *   DRV(3)(&s, x, y, shapes, index)        UDRV_3(s, x, y, shapes, index)
 *       draws shape `index` of a shape file with its hot spot at (x, y). When the shape's
 *       box (its header) is inside s's clipping window it is drawn without clipping, else
 *       run by run clipped to the window. Nothing when index * 4 >= the first offset of
 *       the file's table (so index == the number of shapes is still accepted: it takes
 *       the first 4 bytes of shape 0 as an offset). Pixel value 0 is drawn like any other.
 *   (MOUSECUR.ASM drvSavePointer, 6)       UDRV_6(s, saveBuf, x, y, shapes, index)
 *       copies the part of shape index's box (at x, y) inside s's window from s into
 *       saveBuf, which holds the whole box (width * height bytes, row after row; the
 *       clipped part at its place in it). No check of index.
 *   (MOUSECUR.ASM drvRestPointer, 9)       UDRV_9(s, saveBuf, x, y, shapes, index)
 *       the same box copied back from saveBuf to s.
 *   DRV(0xc)(&s, color)                    UDRV_0xc(s, color)
 *       fills s's window (left..right, top..bottom) with color. No clipping: the window
 *       is the rectangle.
 *   DRV(0xf)(&src, &dst)                   UDRV_0xf(src, dst)
 *       copies src's window to dst's window, top left corner on top left corner, the
 *       smaller width and the smaller height of the two (unsigned compares).
 *   (TXTCHAR.ASM drvPutChar, 12h)          UDRV_0x12(t, ch, height, width, y2)
 *       registers: AX = ch, BX = t (struct tText *), CX = height (the font's word 0),
 *       DX = width (font[4 + ch]), SI = y2 = 2 * t->curY. Draws character ch of t->font
 *       at (t->curX, t->curY) on t->surface: height rows of width bytes, the font's
 *       pixel values font[2] and font[3] replaced by t->fgColor and t->bgColor (unless
 *       both are equal to them), others drawn as they are; no clipping. Then
 *       t->curX += width, limited to the surface's right.
 *   DRV(0x15)(&screen)                     UDRV_0x15(s)
 *       nothing (a lone RETF: the EGA driver's page flip, probably).
 *   DRV(0x18)(&s, x, y, color)             UDRV_0x18(s, x, y, color)
 *       puts one pixel when (x, y) is inside s's window.
 *
 * Coordinates and window bounds are compared signed, offsets wrap at 64 KB in their
 * segment, as in DOS. A count of 0 where the original loops with DEC/JNZ means 65536 (the
 * heights of 6, 9, 0Ch, 0Fh, 12h): bad windows (right < left, bottom < top) make huge
 * copies, as in DOS.
 *
 * Memory: the pixels are surface->segment:offset in the DOS arena (port/sys/dosmem.h;
 * the screen is A000:0000); far pointers (pScanlines, shapes, saveBuf, the font) are
 * turned into segment:offset with dos_fp_seg/dos_fp_off so that their offset arithmetic
 * wraps at 64 KB as in DOS (use pointers into the arena for exact results; a pointer
 * outside it gets a foreign segment, dosmem.h). The driver's own segment (its variables
 * and buffers, at their DOS offsets) is a 64 KB array: ugfx_segment().
 */
#ifndef UGFX_H
#define UGFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* U's structs as the host compiles them ("far" empty, packed, unsigned = uint16); SRC_U/u.h
 * defines the same ones (define U_STRUCTS_DEFINED there when both are included). */
#ifndef U_STRUCTS_DEFINED
#define U_STRUCTS_DEFINED
#pragma pack(push, 1)
struct tSurface {
	uint16_t segment;        /* +00 segment of the pixels */
	uint16_t *pScanlines;    /* +02 offset of each line in that segment */
	int16_t right, left, top, bottom;   /* the clipping window, inclusive */
};
struct tText {
	struct tSurface *surface;   /* +00 */
	int16_t curX, curY;         /* +02 the cursor */
	unsigned char *font;        /* +06 height (word), colors (2 bytes), widths, offsets, glyphs */
	char fgColor;               /* +0A */
	char bgColor;               /* +0B */
	char *buffer;               /* +0C */
	char *bufPtr;               /* +0E */
	char justify;               /* +10 */
};
#pragma pack(pop)
#endif

void UDRV_0(uint16_t *scanlines);
void UDRV_3(struct tSurface *s, int16_t x, int16_t y, void *shapes, int16_t index);
void UDRV_6(struct tSurface *s, void *saveBuf, int16_t x, int16_t y, void *shapes, int16_t index);
void UDRV_9(struct tSurface *s, void *saveBuf, int16_t x, int16_t y, void *shapes, int16_t index);
void UDRV_0xc(struct tSurface *s, int16_t color);
void UDRV_0xf(struct tSurface *src, struct tSurface *dst);
void UDRV_0x12(struct tText *t, int16_t ch, int16_t height, int16_t width, int16_t y2);
void UDRV_0x15(struct tSurface *s);
void UDRV_0x18(struct tSurface *s, int16_t x, int16_t y, int16_t color);

/* Called by UDRV_0 after it has cleared A000:0000-FFFF (what int 10h AX=0013h does to the
 * video memory): the port resets the DAC to the BIOS palette there, if it wants. */
void ugfx_set_mode_hook(void (*hook)(void));
/* The driver's segment (64 KB; CS:0060 the clipped run buffer, CS:01A0 the color table of
 * entry 12h), for tests and debugging. */
uint8_t *ugfx_segment(void);

#ifdef __cplusplus
}
#endif

#endif
