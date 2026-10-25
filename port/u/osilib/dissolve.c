/* DISSOLVE.ASM: a picture dissolved into another, the pixels copied in a pseudo-random
 * order (docs/port/U_OSILIB.md).
 *
 * The order: a maximal-length linear feedback shift register (Galois, shifting right)
 * with as many bits as x and y need (at least 2): its low bits are x, the bits above
 * (lfsr >> yShift, 16 bits) y; values outside the window are skipped, (0, 0), which the
 * register never gives, is copied last. Both windows are the source's size; the target's
 * window only gives its top left corner.
 *
 * The routine's data (D_4612..D_49E9: the two windows, segments, the two 200-entry
 * scanline tables copied from the surfaces, the register's state, the masks, the table of
 * feedbacks) is kept here as a byte block at the same offsets, because the original reads
 * a row beyond its tables (a window whose top + height is past 200, or a negative top)
 * from the variables next to them; outside the block such reads give 0 in the port.
 *
 * DIS_dissolve (MCGA, 1 byte a pixel) waits for the vertical retrace every
 * count / (7 * speed + 1) pixels; DIS_dissolveCGA (4 pixels a byte) and DIS_dissolveTDY
 * (Tandy: 2 a byte) for the next BIOS tick every speed pixels (int 1Ah). DIS_dissolveEGA
 * goes through the EGA's planes and latches (ports 3C4h/3CEh), which the port does not
 * have: it does nothing (logged once). The register's walk never ends on an empty or 1x1
 * window (no value is ever inside): DOS hangs there, the port stops after a whole period.
 */
#include "uosi.h"
#include "ufar.h"
#include "../../platform/plat.h"

#define BASE 0x4612u
#define END 0x49EAu

static uint8_t blk[END - BASE];
static int blk_ready;

enum {
	O_SRCRIGHT = 0x4612, O_SRCLEFT = 0x4614, O_SRCTOP = 0x4616, O_SRCBOTTOM = 0x4618,
	O_DSTRIGHT = 0x461A, O_DSTLEFT = 0x461C, O_DSTTOP = 0x461E, O_DSTBOTTOM = 0x4620,
	O_SRCSEG = 0x4622, O_DSTSEG = 0x4624, O_SRCLINES = 0x4626, O_DSTLINES = 0x47B6,
	O_WINWIDTH = 0x4946, O_WINHEIGHT = 0x4948, O_LFSR = 0x494A, O_TAPS = 0x494E,
	O_XMASK = 0x4952, O_YSHIFT = 0x4954, O_COUNT = 0x4956, O_PERWAIT = 0x4958,
	O_BITMASK = 0x495A, O_MASK4 = 0x4962, O_TAPTABLE = 0x4966, O_NMASK4 = 0x4966,
	O_MASK2 = 0x496A, O_NMASK2 = 0x496C
};

static const uint32_t taps_init[31] = {
	0x3, 0x6, 0xC, 0x14, 0x30, 0x60, 0xB8, 0x110, 0x240, 0x500, 0xCA0, 0x1B00, 0x3500, 0x6000,
	0xB400, 0x12000, 0x20400, 0x72000, 0x90000, 0x140000, 0x300000, 0x420000, 0xD80000,
	0x1200000, 0x3880000, 0x7200000, 0x9000000, 0x14000000, 0x32800000, 0x48000000, 0xA3000000
};

static uint8_t rb(uint16 off)
{
	return off >= BASE && off < END ? blk[off - BASE] : 0;
}

static void wb(uint16 off, uint8_t v)
{
	if (off >= BASE && off < END)
		blk[off - BASE] = v;
}

static uint16 rw(uint16 off)
{
	return (uint16)(rb(off) | (rb((uint16)(off + 1)) << 8));
}

static void ww(uint16 off, uint16 v)
{
	wb(off, (uint8_t)v);
	wb((uint16)(off + 1), (uint8_t)(v >> 8));
}

static void init(void)
{
	static const uint8_t masks[] = {
		0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,   /* bitMask */
		0xC0, 0x30, 0x0C, 0x03,                           /* mask4 */
		0x3F, 0xCF, 0xF3, 0xFC,                           /* nmask4 (tapTable starts here) */
		0xF0, 0x0F,                                       /* mask2 */
		0x0F, 0xF0                                        /* nmask2 */
	};
	unsigned i;

	if (blk_ready)
		return;
	blk_ready = 1;
	for (i = 0; i < sizeof masks; i++)
		wb((uint16)(O_BITMASK + i), masks[i]);
	for (i = 0; i < 31; i++) {
		ww((uint16)(0x496E + 4 * i), (uint16)taps_init[i]);
		ww((uint16)(0x4970 + 4 * i), (uint16)(taps_init[i] >> 16));
	}
}

/* C_0161 DIS_setup: the segments, 200 words of each scanline table (REP MOVSW from the
 * table's far pointer), the windows */
static void setup(struct tSurface *from, struct tSurface *to)
{
	uint16 i;

	init();
	ww(O_SRCSEG, from->segment);
	ww(O_DSTSEG, to->segment);
	for (i = 0; i < 200; i++)
		ww((uint16)(O_SRCLINES + 2 * i), u_far16(from->pScanlines, (uint16)(2 * i)));
	for (i = 0; i < 200; i++)
		ww((uint16)(O_DSTLINES + 2 * i), u_far16(to->pScanlines, (uint16)(2 * i)));
	ww(O_SRCRIGHT, (uint16)from->right);
	ww(O_SRCLEFT, (uint16)from->left);
	ww(O_SRCTOP, (uint16)from->top);
	ww(O_SRCBOTTOM, (uint16)from->bottom);
	ww(O_DSTRIGHT, (uint16)to->right);
	ww(O_DSTLEFT, (uint16)to->left);
	ww(O_DSTTOP, (uint16)to->top);
	ww(O_DSTBOTTOM, (uint16)to->bottom);
}

/* the register: as many bits as x and y need; CX = its number of bits */
static uint16 make_register(void)
{
	uint16 ax, dx, cx = 2;
	uint32_t lfsr = 3;

	ax = (uint16)(rw(O_SRCRIGHT) - rw(O_SRCLEFT) + 1);
	ww(O_WINWIDTH, ax);
	dx = (uint16)(rw(O_SRCBOTTOM) - rw(O_SRCTOP) + 1);
	ww(O_WINHEIGHT, dx);
	ww(O_SRCTOP, (uint16)(rw(O_SRCTOP) << 1));
	ww(O_DSTTOP, (uint16)(rw(O_DSTTOP) << 1));
	ww(O_XMASK, 1);
	ww(O_YSHIFT, 1);
	while ((ax >>= 1) != 0) {
		lfsr = (lfsr << 1) | 1;
		ww(O_XMASK, (uint16)((rw(O_XMASK) << 1) | 1));
		ww(O_YSHIFT, (uint16)(rw(O_YSHIFT) + 1));
		cx++;
	}
	while ((dx >>= 1) != 0) {
		lfsr = (lfsr << 1) | 1;
		cx++;
	}
	ww(O_LFSR, (uint16)lfsr);
	ww((uint16)(O_LFSR + 2), (uint16)(lfsr >> 16));
	/* count = width * height - 1 (MUL: the low word) */
	ww(O_COUNT, (uint16)((uint16)(rw(O_WINHEIGHT) * rw(O_WINWIDTH)) - 1));
	return cx;
}

static void load_taps(uint16 cx)
{
	uint16 bx = (uint16)(cx << 2);

	ww(O_TAPS, rw((uint16)(O_TAPTABLE + bx)));
	ww((uint16)(O_TAPS + 2), rw((uint16)(O_TAPTABLE + bx + 2)));
}

/* the next value of the register inside the window: (x, y); 0 when a whole period went
 * by without one (the original loops for ever) */
static int next_pixel(uint16 cx, uint16 *x, uint16 *y)
{
	uint32_t lfsr = (uint32_t)rw(O_LFSR) | ((uint32_t)rw((uint16)(O_LFSR + 2)) << 16);
	uint32_t taps = (uint32_t)rw(O_TAPS) | ((uint32_t)rw((uint16)(O_TAPS + 2)) << 16);
	uint64_t steps = 0, period = cx >= 32 ? 0x100000000ull : (1ull << cx);

	for (;;) {
		int carry = (int)(lfsr & 1);
		lfsr >>= 1;
		if (carry)
			lfsr ^= taps;
		ww(O_LFSR, (uint16)lfsr);
		ww((uint16)(O_LFSR + 2), (uint16)(lfsr >> 16));
		*x = (uint16)(lfsr & rw(O_XMASK));
		*y = (uint16)(lfsr >> (rw(O_YSHIFT) & 31));
		if (rw(O_YSHIFT) > 31)
			*y = 0;
		if (*x < rw(O_WINWIDTH) && *y < rw(O_WINHEIGHT))
			return 1;
		if (++steps > period) {
			plat_log("U: DIS_dissolve: no pixel of the window is ever reached (the DOS program hangs here)");
			return 0;
		}
	}
}

/* the scanline offset of row y (0..) of the source / target, from the tables copied */
static uint16 src_row(uint16 y)
{
	return rw((uint16)(O_SRCLINES + rw(O_SRCTOP) + (uint16)(y << 1)));
}

static uint16 dst_row(uint16 y)
{
	return rw((uint16)(O_DSTLINES + rw(O_DSTTOP) + (uint16)(y << 1)));
}

typedef void (*pixel_fn)(uint16 x, uint16 y);

/* the walk: count pixels, a wait every perWait of them (BP, DEC/JNZ: 0 is 65536), then
 * (0, 0) */
static void walk(uint16 cx, pixel_fn pixel, int retrace)
{
	uint16 x, y, bp = rw(O_PERWAIT);

	load_taps(cx);
	for (;;) {
		if (!next_pixel(cx, &x, &y))
			return;
		pixel(x, y);
		bp = (uint16)(bp - 1);
		if (bp == 0) {
			if (retrace)
				VID_waitRetrace();
			else
				uosi_wait_tick();
			bp = rw(O_PERWAIT);
		}
		ww(O_COUNT, (uint16)(rw(O_COUNT) - 1));
		if (rw(O_COUNT) == 0)
			break;
	}
	pixel(0, 0);
}

/* ---- MCGA ---- */

static void pixel_mcga(uint16 x, uint16 y)
{
	uint8_t c = *u_pix(rw(O_SRCSEG), (uint16)(src_row(y) + rw(O_SRCLEFT) + x));

	*u_pix(rw(O_DSTSEG), (uint16)(dst_row(y) + rw(O_DSTLEFT) + x)) = c;
}

/* C_13F2_000C */
int DIS_dissolve(struct tSurface *from, struct tSurface *to, int16 speed)
{
	uint16 cx, per;

	per = (uint16)(((uint16)speed << 3) - (uint16)speed + 1);
	setup(from, to);
	cx = make_register();
	if (per == 0)
		dos_divide_error("DIS_dissolve");
	ww(O_PERWAIT, (uint16)(rw(O_COUNT) / per));
	walk(cx, pixel_mcga, 1);
	return 0;
}

/* ---- CGA: 4 pixels a byte ---- */

static void pixel_cga(uint16 x, uint16 y)
{
	uint16 si = (uint16)(x + rw(O_SRCLEFT));
	uint16 bx = (uint16)(src_row(y) + (uint16)(si >> 2));
	uint8_t ch = (uint8_t)(*u_pix(rw(O_SRCSEG), bx) & rb((uint16)(O_MASK4 + (si & 3))));
	uint8_t cl;

	si = (uint16)(x + rw(O_DSTLEFT));
	bx = (uint16)(dst_row(y) + (uint16)(si >> 2));
	cl = (uint8_t)(rb((uint16)(O_NMASK4 + (si & 3))) & *u_pix(rw(O_DSTSEG), bx));
	*u_pix(rw(O_DSTSEG), bx) = (uint8_t)(cl | ch);
}

/* C_13F2_037A */
int DIS_dissolveCGA(struct tSurface *from, struct tSurface *to, int16 speed)
{
	uint16 cx;

	init();
	ww(O_PERWAIT, (uint16)speed);
	setup(from, to);
	cx = make_register();
	walk(cx, pixel_cga, 0);
	return 0;
}

/* ---- Tandy: 2 pixels a byte ---- */

static void pixel_tdy(uint16 x, uint16 y)
{
	uint16 si = (uint16)(x + rw(O_SRCLEFT));
	uint16 bx = (uint16)(src_row(y) + (uint16)(si >> 1));
	uint8_t cl = (uint8_t)(*u_pix(rw(O_SRCSEG), bx) & rb((uint16)(O_MASK2 + (si & 1))));
	uint8_t ch;

	si = (uint16)(x + rw(O_DSTLEFT));
	bx = (uint16)(dst_row(y) + (uint16)(si >> 1));
	ch = (uint8_t)(*u_pix(rw(O_DSTSEG), bx) & rb((uint16)(O_NMASK2 + (si & 1))));
	*u_pix(rw(O_DSTSEG), bx) = (uint8_t)(cl | ch);
}

/* C_13F2_0504 */
int DIS_dissolveTDY(struct tSurface *from, struct tSurface *to, int16 speed)
{
	uint16 cx;

	init();
	ww(O_PERWAIT, (uint16)speed);
	setup(from, to);
	cx = make_register();
	walk(cx, pixel_tdy, 0);
	return 0;
}

/* ---- EGA ---- */

/* C_13F2_01D5: not ported (planar memory through the EGA's ports) */
int DIS_dissolveEGA(struct tSurface *from, struct tSurface *to, int16 speed)
{
	static int logged;

	(void)from;
	(void)to;
	(void)speed;
	if (!logged) {
		logged = 1;
		plat_log("U: DIS_dissolveEGA: the EGA is not ported");
	}
	return 0;
}

/* C_13F2_01B9: DIS_farcpy(from, to, n): REP MOVSW n / 2 words, then the odd byte */
int16 DIS_farcpy(void *from, void *to, int16 n)
{
	uint16 cx = (uint16)n, si = 0, di = 0, w;

	for (w = (uint16)(cx >> 1); w; w--) {
		uint8_t lo = *u_far(from, si), hi = *u_far(from, (uint16)(si + 1));
		*u_far(to, di) = lo;
		*u_far(to, (uint16)(di + 1)) = hi;
		si = (uint16)(si + 2);
		di = (uint16)(di + 2);
	}
	if (cx & 1)
		*u_far(to, di) = *u_far(from, si);
	return 0;
}
