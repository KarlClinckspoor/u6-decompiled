/* replaces U.EXE's MCGADRV.BIN: the MCGA (VGA 320x200x256) graphics driver (ugfx.h).
 *
 * A close transcription of the original's code: the comments give the original's
 * offsets, the driver's variables live in drv[] at their DOS offsets, registers are
 * uint16_t and wrap, comparisons are signed where the original uses JL/JG and unsigned
 * where it uses JA/JB, and every memory access goes through segment:offset with a 16-bit
 * offset. REP MOVSW is done a word at a time (read both bytes, write both), which matters
 * when source and destination overlap. See docs/port/U_DRIVER.md.
 */
#include "ugfx.h"
#include "../sys/dosmem.h"

#include <string.h>

/* ---- The driver's segment (CS) ---- */

static uint8_t drv[0x10000];
static int drv_ready;
static void (*mode_hook)(void);

enum {
	O_SURF     = 0x001C,   /* 03h: copy of the surface (segment, pScanlines, right, left, top, bottom) */
	O_SX       = 0x002A,   /* 06h/09h: clipped x0; 0Fh: source left */
	O_DX       = 0x002C,   /* 0Fh: destination left */
	O_PIXSEG   = 0x002E,   /* 06h: surface segment, 09h: saveBuf segment, 0Fh: source segment */
	O_SCANSEG  = 0x0030,   /* 06h/09h/0Fh/12h: segment of the (source) scanline table */
	O_DSCANSEG = 0x0032,   /* 0Fh: segment of the destination's scanline table */
	O_HEIGHT   = 0x0034,   /* 06h/09h: box height */
	O_ROWS     = 0x0036,   /* 06h/09h: rows left to copy */
	O_X0       = 0x0038,   /* 06h/09h: box x0 */
	O_CW       = 0x003A,   /* 06h/09h: clipped width */
	O_Y0       = 0x003C,   /* 06h/09h: box y0 */
	O_SY       = 0x003E,   /* 06h/09h: clipped y0 */
	O_WIDTH    = 0x0040,   /* 06h/09h: box width (saveBuf's stride) */
	O_X        = 0x0042,   /* 03h: x */
	O_Y        = 0x0044,   /* 03h: y */
	O_TSEG     = 0x0054,   /* 12h: surface segment */
	O_TTEXT    = 0x0056,   /* 12h: BX, the tText's DGROUP offset (not kept here) */
	O_TROWS    = 0x0058,   /* 12h: rows left */
	O_TWIDTH   = 0x005A,   /* 12h: width */
	O_TY2      = 0x005C,   /* 12h: 2 * y, incremented by 2 a row */
	O_TDS      = 0x005E,   /* 12h: DS (not kept here) */
	O_RUN      = 0x0060,   /* 03h: an RLE run unpacked, when clipped at its ends */
	O_XLAT     = 0x01A0    /* byte[256]: identity, 12h's color substitution */
};

static void init(void)
{
	int i;

	if (drv_ready)
		return;
	drv_ready = 1;
	memset(drv, 0, sizeof drv);
	for (i = 0; i < 256; i++)
		drv[O_XLAT + i] = (uint8_t)i;
}

uint8_t *ugfx_segment(void)
{
	init();
	return drv;
}

void ugfx_set_mode_hook(void (*hook)(void))
{
	mode_hook = hook;
}

static uint16_t cs16(uint16_t o) { return (uint16_t)(drv[o] | drv[(uint16_t)(o + 1)] << 8); }
static void cs16w(uint16_t o, uint16_t v) { drv[o] = (uint8_t)v; drv[(uint16_t)(o + 1)] = (uint8_t)(v >> 8); }

/* ---- Segments and far pointers ---- */

typedef struct { uint8_t *host; uint16_t seg; } Seg;   /* host: the driver's segment */
typedef struct { Seg s; uint16_t off; } Far;

static Seg seg_of(uint16_t s) { Seg r; r.host = 0; r.seg = s; return r; }
static Seg cs_seg(void) { Seg r; r.host = drv; r.seg = 0; return r; }

static uint8_t *at(Seg s, uint16_t o) { return s.host ? s.host + o : (uint8_t *)dos_mk_fp(s.seg, o); }
static uint8_t rd8(Seg s, uint16_t o) { return *at(s, o); }
static uint16_t rd16(Seg s, uint16_t o) { return (uint16_t)(rd8(s, o) | rd8(s, (uint16_t)(o + 1)) << 8); }
static void wr8(Seg s, uint16_t o, uint8_t v) { *at(s, o) = v; }

/* A far pointer back to segment:offset (dosmem.h: normalised in the arena). */
static Far far_of(const void *p)
{
	Far f;

	f.s = seg_of(0);
	f.off = 0;
	if (p) {
		f.s.seg = dos_fp_seg(p);
		f.off = dos_fp_off(p);
	}
	return f;
}

/* rep movsw + rep movsb (n >> 1 words, then n & 1 bytes), forward, offsets wrapping */
static void movs(Seg d, uint16_t *pdi, Seg s, uint16_t *psi, uint16_t n)
{
	uint16_t di = *pdi, si = *psi, w;

	for (w = (uint16_t)(n >> 1); w; w--) {
		uint8_t lo = rd8(s, si), hi = rd8(s, (uint16_t)(si + 1));
		wr8(d, di, lo);
		wr8(d, (uint16_t)(di + 1), hi);
		si = (uint16_t)(si + 2);
		di = (uint16_t)(di + 2);
	}
	if (n & 1) {
		wr8(d, di, rd8(s, si));
		si++;
		di++;
	}
	*pdi = di;
	*psi = si;
}

/* rep stosw + rep stosb with AH = AL */
static void stos(Seg d, uint16_t *pdi, uint8_t v, uint16_t n)
{
	uint16_t di = *pdi;

	while (n--)
		wr8(d, di++, v);
	*pdi = di;
}

/* The shape: shapes + the dword at shapes + 4 + index * 4, as a 20-bit linear address
 * plus that dword, normalised (02BA, 051D, 0680). */
static Far shape_of(Far sh, const void *shapes, uint16_t index)
{
	uint16_t bx = (uint16_t)((uint16_t)(index << 2) + 4);
	uint32_t d = rd16(sh.s, (uint16_t)(sh.off + bx)) | (uint32_t)rd16(sh.s, (uint16_t)(sh.off + bx + 2)) << 16;
	Far f;

	if (sh.s.seg >= DOS_SEG_FOREIGN) {
		/* a pointer outside the arena: plain pointer arithmetic */
		const uint8_t *q = (const uint8_t *)shapes + d;
		return far_of(q);
	}
	d += ((uint32_t)sh.s.seg << 4) + sh.off;
	f.s = seg_of((uint16_t)(d >> 4));
	f.off = (uint16_t)(d & 15);
	return f;
}

/* the RLE run of 03h (0377, 0474, 04BE): count pixels in chunks of a count byte
 * (bit 0: 1 = one color byte repeated, 0 = that many literal bytes; count = byte >> 1);
 * stops when the remaining count is exactly 0 */
static void rle(Seg d, uint16_t *pdi, Seg s, uint16_t *psi, uint16_t count)
{
	uint16_t bx = count;

	do {
		uint8_t al = rd8(s, *psi);
		uint16_t n = (uint16_t)(al >> 1);

		*psi = (uint16_t)(*psi + 1);
		if (al & 1) {
			uint8_t c = rd8(s, *psi);
			*psi = (uint16_t)(*psi + 1);
			bx = (uint16_t)(bx - n);
			stos(d, pdi, c, n);
		} else {
			movs(d, pdi, s, psi, n);
			bx = (uint16_t)(bx - n);
		}
	} while (bx != 0);
}

/* ---- 00h (08CE): the scanline offsets, mode 13h ---- */

void UDRV_0(uint16_t *scanlines)
{
	uint16_t si = 0;
	int i;

	init();
	for (i = 0; i < 200; i++) {
		scanlines[i] = si;
		si = (uint16_t)(si + 320);
	}
	/* int 10h AX=0013h: clears the video memory (and resets the DAC: the hook) */
	memset(dos_mk_fp(0xA000, 0), 0, 0x10000);
	if (mode_hook)
		mode_hook();
}

/* ---- 03h (02A0): draw a shape ---- */

void UDRV_3(struct tSurface *s, int16_t xArg, int16_t yArg, void *shapes, int16_t index)
{
	Far shs = far_of(shapes), sh, sc;
	Seg es, ds, tmp = cs_seg();
	uint16_t x = (uint16_t)xArg, y = (uint16_t)yArg, si, bx;
	int16_t right, left, top, bottom;

	init();
	/* 02A7: the surface into CS:001C */
	sc = far_of(s->pScanlines);
	cs16w(O_SURF, s->segment);
	cs16w(O_SURF + 2, sc.off);
	cs16w(O_SURF + 4, sc.s.seg);
	cs16w(O_SURF + 6, (uint16_t)s->right);
	cs16w(O_SURF + 8, (uint16_t)s->left);
	cs16w(O_SURF + 10, (uint16_t)s->top);
	cs16w(O_SURF + 12, (uint16_t)s->bottom);
	right = s->right;
	left = s->left;
	top = s->top;
	bottom = s->bottom;
	/* 02D3: index * 4 against the table's first offset (signed) */
	bx = (uint16_t)((uint16_t)index << 2);
	if ((int16_t)bx >= (int16_t)rd16(shs.s, (uint16_t)(shs.off + 4)))
		return;
	sh = shape_of(shs, shapes, (uint16_t)index);
	ds = sh.s;
	si = sh.off;
	es = seg_of(s->segment);
	cs16w(O_X, x);
	cs16w(O_Y, y);
	if (!((int16_t)(uint16_t)(rd16(ds, si) + x) > right
		|| (int16_t)(uint16_t)(x - rd16(ds, (uint16_t)(si + 2))) < left
		|| (int16_t)(uint16_t)(y - rd16(ds, (uint16_t)(si + 4))) < top
		|| (int16_t)(uint16_t)(rd16(ds, (uint16_t)(si + 6)) + y) > bottom)) {
		/* 0342: the box is inside the window: no clipping */
		si = (uint16_t)(si + 8);
		for (;;) {
			uint16_t ax = rd16(ds, si), di, row;

			si = (uint16_t)(si + 2);
			if (ax == 0)
				break;
			di = (uint16_t)(rd16(ds, si) + x); si = (uint16_t)(si + 2);
			row = (uint16_t)(rd16(ds, si) + y); si = (uint16_t)(si + 2);
			di = (uint16_t)(di + rd16(sc.s, (uint16_t)(sc.off + (uint16_t)(row << 1))));
			if (ax & 1)
				rle(es, &di, ds, &si, (uint16_t)(ax >> 1));   /* (a count of 0: see rle) */
			else
				movs(es, &di, ds, &si, (uint16_t)(ax >> 1));
		}
		return;
	}
	/* 03A2: clipped run by run; a run word of 0 or 1 ends the shape */
	si = (uint16_t)(si + 8);
	for (;;) {
		uint16_t ax = rd16(ds, si), cx, dx, row, di;

		si = (uint16_t)(si + 2);
		cx = (uint16_t)(ax >> 1);
		if (cx == 0)
			break;
		dx = (uint16_t)(rd16(ds, si) + x); si = (uint16_t)(si + 2);
		row = (uint16_t)(rd16(ds, si) + y); si = (uint16_t)(si + 2);
		if ((int16_t)row < top || (int16_t)row > bottom || (int16_t)dx > right
			|| (int16_t)(uint16_t)(dx + cx - 1) < left) {
			if (!(ax & 1)) {
				si = (uint16_t)(si + cx);   /* 041A */
			} else {
				/* 04A1: skip the RLE data */
				do {
					uint8_t b = rd8(ds, si);
					uint16_t n = (uint16_t)(b >> 1);

					si = (uint16_t)(si + 2);
					if (!(b & 1))
						si = (uint16_t)(si + n - 1);
					cx = (uint16_t)(cx - n);
				} while (cx != 0);
			}
			continue;
		}
		di = (uint16_t)(rd16(sc.s, (uint16_t)(sc.off + (uint16_t)(row << 1))) + dx);
		if (!(ax & 1)) {
			/* 03EE: literal run, clipped at both ends */
			uint16_t a;

			if (!((int16_t)left <= (int16_t)dx)) {
				a = (uint16_t)(left - dx);
				cx = (uint16_t)(cx - a);
				si = (uint16_t)(si + a);
				di = (uint16_t)(di + a);
				dx = (uint16_t)(dx + a);
			}
			dx = (uint16_t)(dx + cx - 1);
			if ((int16_t)dx > right)
				dx = (uint16_t)(dx - right);
			else
				dx = 0;
			cx = (uint16_t)(cx - dx);
			movs(es, &di, ds, &si, cx);
			si = (uint16_t)(si + dx);
		} else {
			/* 0448: RLE run */
			uint16_t over = (uint16_t)(dx + cx - 1 - right);   /* pixels past right */
			int under = (int16_t)dx < left;                     /* JL after SUB DX,left */

			dx = (uint16_t)(dx - left);
			if (!under && !((int16_t)over > 0)) {
				rle(es, &di, ds, &si, cx);
			} else {
				/* 04B2: unpack into CS:0060, copy the visible part */
				uint16_t t = O_RUN, n = cx;

				rle(tmp, &t, ds, &si, cx);
				t = O_RUN;
				if ((int16_t)dx < 0) {
					n = (uint16_t)(n + dx);
					t = (uint16_t)(t - dx);
					di = (uint16_t)(di - dx);
				}
				if ((int16_t)over > 0)
					n = (uint16_t)(n - over);
				movs(es, &di, tmp, &t, n);
			}
		}
	}
}

/* ---- 06h (0510) and 09h (0673): the box of a shape, clipped, from/to saveBuf ---- */

static void box(struct tSurface *s, void *saveBuf, int16_t xArg, int16_t yArg,
	void *shapes, int16_t index, int restore)
{
	Far sh = shape_of(far_of(shapes), shapes, (uint16_t)index);
	Far sc = far_of(s->pScanlines), sv = far_of(saveBuf);
	Seg surf = seg_of(s->segment);
	uint16_t x = (uint16_t)xArg, y = (uint16_t)yArg, ax, bx, di;
	uint16_t e_r = rd16(sh.s, sh.off), e_l = rd16(sh.s, (uint16_t)(sh.off + 2));
	uint16_t e_u = rd16(sh.s, (uint16_t)(sh.off + 4)), e_d = rd16(sh.s, (uint16_t)(sh.off + 6));

	ax = (uint16_t)(e_r + e_l + 1);
	cs16w(O_WIDTH, ax);
	cs16w(O_CW, ax);
	ax = (uint16_t)(e_d + e_u + 1);
	cs16w(O_HEIGHT, ax);
	cs16w(O_ROWS, ax);
	ax = (uint16_t)(x - e_l);
	if ((int16_t)ax > s->right)
		return;
	cs16w(O_X0, ax);
	cs16w(O_SX, ax);
	if (!(s->left < (int16_t)cs16(O_SX))) {
		cs16w(O_CW, (uint16_t)(cs16(O_CW) - (uint16_t)((uint16_t)s->left - cs16(O_SX))));
		cs16w(O_SX, (uint16_t)s->left);
	}
	ax = (uint16_t)(y - e_u);
	if ((int16_t)ax > s->bottom)
		return;
	cs16w(O_Y0, ax);
	cs16w(O_SY, ax);
	if (!(s->top < (int16_t)cs16(O_SY))) {
		cs16w(O_ROWS, (uint16_t)(cs16(O_ROWS) - (uint16_t)((uint16_t)s->top - cs16(O_SY))));
		cs16w(O_SY, (uint16_t)s->top);
	}
	ax = (uint16_t)(x + e_r);
	if ((int16_t)ax < s->left)
		return;
	if ((int16_t)ax > s->right)
		cs16w(O_CW, (uint16_t)(cs16(O_CW) - (uint16_t)(ax - (uint16_t)s->right)));
	ax = (uint16_t)(y + e_d);
	if ((int16_t)ax < s->top)
		return;
	if ((int16_t)ax > s->bottom)
		cs16w(O_ROWS, (uint16_t)(cs16(O_ROWS) - (uint16_t)(ax - (uint16_t)s->bottom)));
	/* 0602: where the window's part starts in saveBuf */
	di = (uint16_t)((uint16_t)(cs16(O_SY) - cs16(O_Y0)) * cs16(O_WIDTH));
	di = (uint16_t)(di + (uint16_t)(cs16(O_SX) - cs16(O_X0)));
	di = (uint16_t)(di + sv.off);
	cs16w(O_SCANSEG, sc.s.seg);
	cs16w(O_PIXSEG, restore ? sv.s.seg : s->segment);
	bx = (uint16_t)((uint16_t)(cs16(O_SY) << 1) + sc.off);
	do {
		uint16_t so = (uint16_t)(rd16(sc.s, bx) + cs16(O_SX));

		if (restore)
			movs(surf, &so, sv.s, &di, cs16(O_CW));
		else
			movs(sv.s, &di, surf, &so, cs16(O_CW));
		bx = (uint16_t)(bx + 2);
		di = (uint16_t)(di - cs16(O_CW) + cs16(O_WIDTH));
		cs16w(O_ROWS, (uint16_t)(cs16(O_ROWS) - 1));
	} while (cs16(O_ROWS) != 0);
}

void UDRV_6(struct tSurface *s, void *saveBuf, int16_t x, int16_t y, void *shapes, int16_t index)
{
	init();
	box(s, saveBuf, x, y, shapes, index, 0);
}

void UDRV_9(struct tSurface *s, void *saveBuf, int16_t x, int16_t y, void *shapes, int16_t index)
{
	init();
	box(s, saveBuf, x, y, shapes, index, 1);
}

/* ---- 0Ch (07D6): fill the surface's window ---- */

void UDRV_0xc(struct tSurface *s, int16_t color)
{
	Far sc = far_of(s->pScanlines);
	Seg es = seg_of(s->segment);
	uint8_t c = (uint8_t)color;
	uint16_t bp = (uint16_t)s->left;
	uint16_t dx = (uint16_t)((uint16_t)s->right - bp + 1);
	uint16_t rows = (uint16_t)((uint16_t)s->bottom - (uint16_t)s->top + 1);
	uint16_t bx = (uint16_t)((uint16_t)((uint16_t)s->top << 1) + sc.off);

	init();
	do {
		uint16_t di = (uint16_t)(rd16(sc.s, bx) + bp);

		stos(es, &di, c, dx);
		bx = (uint16_t)(bx + 2);
	} while (--rows);
}

/* ---- 0Fh (081E): copy src's window to dst's window ---- */

void UDRV_0xf(struct tSurface *src, struct tSurface *dst)
{
	Far ss = far_of(src->pScanlines), ds = far_of(dst->pScanlines);
	Seg es = seg_of(dst->segment), from = seg_of(src->segment);
	uint16_t w, h, cx, ax, bx, bp;

	init();
	cs16w(O_PIXSEG, src->segment);
	cs16w(O_SX, (uint16_t)src->left);
	cs16w(O_DX, (uint16_t)dst->left);
	ax = (uint16_t)((uint16_t)dst->right - (uint16_t)dst->left + 1);
	w = (uint16_t)((uint16_t)src->right - (uint16_t)src->left + 1);
	if (w > ax)   /* JNA: unsigned */
		w = ax;
	cx = (uint16_t)((uint16_t)dst->bottom - (uint16_t)dst->top + 1);
	h = (uint16_t)((uint16_t)src->bottom - (uint16_t)src->top + 1);
	if (h > cx)
		h = cx;
	cs16w(O_SCANSEG, ss.s.seg);
	cs16w(O_DSCANSEG, ds.s.seg);
	bx = (uint16_t)((uint16_t)((uint16_t)src->top << 1) + ss.off);
	bp = (uint16_t)((uint16_t)((uint16_t)dst->top << 1) + ds.off);
	do {
		uint16_t si = (uint16_t)(rd16(ss.s, bx) + cs16(O_SX));
		uint16_t di = (uint16_t)(rd16(ds.s, bp) + cs16(O_DX));

		movs(es, &di, from, &si, w);
		bp = (uint16_t)(bp + 2);
		bx = (uint16_t)(bx + 2);
	} while (--h);
}

/* ---- 12h (08F0): a character of text (register call, see ugfx.h) ---- */

void UDRV_0x12(struct tText *t, int16_t ch, int16_t height, int16_t width, int16_t y2)
{
	struct tSurface *s = t->surface;
	Far sc = far_of(s->pScanlines), f = far_of(t->font);
	Seg es = seg_of(s->segment), tmp = cs_seg();
	uint16_t bp = (uint16_t)ch, si, w = (uint16_t)width;
	uint8_t fg = (uint8_t)t->fgColor, bg = (uint8_t)t->bgColor;
	uint8_t f2 = rd8(f.s, (uint16_t)(f.off + 2)), f3 = rd8(f.s, (uint16_t)(f.off + 3));

	init();
	cs16w(O_TROWS, (uint16_t)height);
	cs16w(O_TWIDTH, w);
	cs16w(O_TY2, (uint16_t)y2);
	cs16w(O_SCANSEG, sc.s.seg);
	cs16w(O_TSEG, s->segment);
	/* 091E: the glyph: font + (font[104h + ch] | font[204h + ch] << 8) */
	si = (uint16_t)(rd8(f.s, (uint16_t)(bp + f.off + 0x104))
		| rd8(f.s, (uint16_t)(bp + f.off + 0x204)) << 8);
	si = (uint16_t)(si + f.off);
	if (fg != f2 || bg != f3) {
		/* 0941: through the table, font[2] -> fg, font[3] -> bg (bg wins if equal) */
		drv[O_XLAT + f2] = fg;
		drv[O_XLAT + f3] = bg;
		do {
			uint16_t di = rd16(sc.s, (uint16_t)(sc.off + cs16(O_TY2)));
			uint16_t n = w;

			di = (uint16_t)(di + (uint16_t)t->curX);
			while (n) {
				uint8_t b = rd8(f.s, si++);
				wr8(es, di++, rd8(tmp, (uint16_t)(O_XLAT + b)));
				n--;
			}
			cs16w(O_TY2, (uint16_t)(cs16(O_TY2) + 2));
			cs16w(O_TROWS, (uint16_t)(cs16(O_TROWS) - 1));
		} while (cs16(O_TROWS) != 0);
		drv[O_XLAT + f2] = f2;
		drv[O_XLAT + f3] = f3;
	} else {
		/* 09B8: copied as it is */
		do {
			uint16_t di = rd16(sc.s, (uint16_t)(sc.off + cs16(O_TY2)));

			di = (uint16_t)(di + (uint16_t)t->curX);
			movs(es, &di, f.s, &si, w);
			cs16w(O_TY2, (uint16_t)(cs16(O_TY2) + 2));
			cs16w(O_TROWS, (uint16_t)(cs16(O_TROWS) - 1));
		} while (cs16(O_TROWS) != 0);
	}
	/* 09F5: advance the cursor, not past the right of the window */
	t->curX = (int16_t)(uint16_t)((uint16_t)t->curX + w);
	if (t->curX > t->surface->right)
		t->curX = t->surface->right;
}

/* ---- 15h (08CD): nothing ---- */

void UDRV_0x15(struct tSurface *s)
{
	(void)s;
}

/* ---- 18h (0A0F): one pixel, if inside the window ---- */

void UDRV_0x18(struct tSurface *s, int16_t x, int16_t y, int16_t color)
{
	Far sc;

	if (y > s->bottom || y < s->top || x < s->left || x > s->right)
		return;
	sc = far_of(s->pScanlines);
	wr8(seg_of(s->segment),
		(uint16_t)((uint16_t)x + rd16(sc.s, (uint16_t)((uint16_t)((uint16_t)y << 1) + sc.off))),
		(uint8_t)color);
}
