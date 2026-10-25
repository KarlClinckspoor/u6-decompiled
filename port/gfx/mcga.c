/* replaces U6MCGA.DRV: the MCGA (VGA 320x200x256) graphics driver (mcga.h).
 *
 * Written as a close transcription of the original's code: the comments give the
 * original's offsets, the driver's variables live in drv[] at their DOS offsets (so that
 * tables indexed with out-of-range arguments read what DOS would read), registers that
 * wrap are uint16_t, and every memory access goes through segment:offset with a 16-bit
 * offset. See docs/port/MCGA_DRIVER.md.
 */
#include "mcga.h"
#include "vgadac.h"
#include "../platform/plat.h"
#include "../sys/dosmem.h"

#include <string.h>

/* ---- The driver's segment (CS) ---- */

static uint8_t drv[0x10000];
static uint8_t vram[0x10000];      /* A000:0000-FFFF */
static struct McgaInfo info;       /* the tGr block (CS:0052 in DOS) */
static const uint8_t *image;       /* mcga_attach_image */
static uint32_t image_size;
static int vdirty;

enum {
	O_SAVEDMODE  = 0x0086,   /* byte: BIOS mode before init (3 in the file) */
	O_TILESEG    = 0x0088,   /* word[0x800]: segment of each tile's pixels, 0 = not loaded */
	O_MASKTYPE   = 0x1088,   /* byte[0x800]: MASKTYPE.VGA part 1, tile format */
	O_BLOCKNUM   = 0x1688,   /* byte, indexed by tile 0x200..0x7ff: objtiles block */
	O_BLOCKOFS   = 0x1E88,   /* word[0xC0]: block position in OBJTILES.VGA, paragraphs */
	O_ROWS       = 0x2008,   /* word[200]: y * 320 */
	O_PAGES_END  = 0x2200,   /* word: segment after the last cache page */
	O_CACHE      = 0x2202,   /* word: segment of the first cache page */
	O_LRU        = 0x2204,   /* word: least recently used page (the next one loaded) */
	O_NAMES      = 0x2208,   /* "maptiles.vga" "objtiles.vga" "tileindx.vga" "masktype.vga" "animmask.vga" */
	O_N_MAPTILES = 0x2208,
	O_N_OBJTILES = 0x2215,
	O_N_TILEINDX = 0x2222,
	O_N_MASKTYPE = 0x222F,
	O_N_ANIMMASK = 0x223C,
	O_SPRSEG     = 0x26C0,   /* word[16]: sprite slot segments (256 bytes each) */
	O_PIVX       = 0x26E0,   /* word: pivot x * 8 (GRDRV_33) */
	O_PIVY       = 0x26E2,
	O_SIN        = 0x26E4,   /* signed byte[128]: 64 * sin(2 pi i / 128) */
	O_LFSR       = 0x28F0,   /* word[]: LFSR taps by bit count - 2 (GRDRV_48) */
	O_COLOR      = 0x2A70,   /* word: color in both bytes (GRDRV_06) */
	O_ANIMSRC    = 0x2C00,   /* word[32]: tile * 2 (GRDRV_3F) */
	O_ANIMDST    = 0x2C40,   /* word[32]: tile * 2 */
	O_ANIMMSK    = 0x2C80,   /* word[32]: offset in ANIMMASK.VGA */
	O_ANIMMASK   = 0x2CC0,   /* word: segment of ANIMMASK.VGA (= O_PAGES_END) */
	O_CODEX      = 0x2D70,   /* word[8]: start angles of GRDRV_4E */
	O_PTRTMP     = 0x2F64    /* byte[0x140]: GRDRV_30 clipped run */
};

static uint8_t cs8(uint16_t o) { return drv[o]; }
static uint16_t cs16(uint16_t o) { return (uint16_t)(drv[o] | drv[(uint16_t)(o + 1)] << 8); }
static void cs16w(uint16_t o, uint16_t v) { drv[o] = (uint8_t)v; drv[(uint16_t)(o + 1)] = (uint8_t)(v >> 8); }

/* ---- Segments and far pointers ---- */

typedef struct { uint8_t *host; uint32_t base; } Seg;   /* host: memory outside the arena */
typedef struct { Seg s; uint16_t off; } Far;

static uint8_t *lin(uint32_t a)
{
	a &= 0xFFFFFu;
	if (a - 0xA0000u < 0x10000u)
		return vram + (a - 0xA0000u);
	return dos_base + a;
}

static Seg seg_of(uint16_t s) { Seg r; r.host = 0; r.base = (uint32_t)s << 4; return r; }
static Seg screen_seg(void) { return seg_of(0xA000); }

static uint8_t *at(Seg s, uint16_t o) { return s.host ? s.host + o : lin(s.base + o); }
static uint8_t rd8(Seg s, uint16_t o) { return *at(s, o); }
static uint16_t rd16(Seg s, uint16_t o) { return (uint16_t)(rd8(s, o) | rd8(s, (uint16_t)(o + 1)) << 8); }

static void wr8(Seg s, uint16_t o, uint8_t v)
{
	uint8_t *p = at(s, o);
	*p = v;
	if ((uintptr_t)p - (uintptr_t)vram < sizeof vram)
		vdirty = 1;
}

static void wr16(Seg s, uint16_t o, uint16_t v)
{
	wr8(s, o, (uint8_t)v);
	wr8(s, (uint16_t)(o + 1), (uint8_t)(v >> 8));
}

/* A far pointer from the game back to segment:offset (normalised, see dosmem.h). A
 * pointer outside the arena is used as is (offsets from it, no 64 KB wrap). */
static Far far_of(const void *p)
{
	Far f;
	long l;

	f.s.host = 0;
	f.s.base = 0;
	f.off = 0;
	if (!p)
		return f;   /* 0000:0000 */
	l = dos_linear(p);
	if (l >= 0) {
		f.s = seg_of((uint16_t)(l >> 4));
		f.off = (uint16_t)(l & 15);
	} else {
		f.s.host = (uint8_t *)(uintptr_t)p;
	}
	return f;
}

/* The far pointer f plus a 32-bit offset, normalised (the mouse shape lookup). */
static Far far_add32(Far f, uint32_t d)
{
	uint32_t l;

	if (f.s.host) {
		f.s.host += d;
		return f;
	}
	l = (f.s.base + f.off + d) & 0xFFFFFFFu;
	f.s = seg_of((uint16_t)(l >> 4));
	f.off = (uint16_t)(l & 15);
	return f;
}

/* rep movsb / movsw: forward, byte after byte, offsets wrapping in their segments */
static void copy(Seg d, uint16_t di, Seg s, uint16_t si, uint16_t n)
{
	while (n--)
		wr8(d, di++, rd8(s, si++));
}

static void fill(Seg d, uint16_t di, uint8_t v, uint16_t n)
{
	while (n--)
		wr8(d, di++, v);
}

static uint16_t rowofs(uint16_t y) { return cs16((uint16_t)(O_ROWS + (uint16_t)(y << 1))); }

/* 24B9: ES:DI = pWorkBuf + y * 176 + x */
static uint16_t work_di(Far w, uint16_t x, uint16_t y)
{
	return (uint16_t)(w.off + (uint16_t)(y * 176u) + x);
}

static void flush(void)
{
	if (vdirty) {
		vdirty = 0;
		plat_video_dirty();
	}
}

/* ---- Callbacks ---- */

static int16_t cb_disk(int16_t n) { return ((McgaDiskConfirmFn *)(void (*)(void))info.pfnDiskConfirm)(n); }
static int16_t cb_load(char *name, void *dest, int16_t z) { return ((McgaLoadFileFn *)(void (*)(void))info.pfnLoadFile)(name, dest, z); }
static int16_t cb_open(char *path) { return ((McgaOpenFn *)(void (*)(void))info.pfnOpen)(path); }
static uint32_t cb_read(int16_t h, uint32_t o, uint32_t n, void *b) { return ((McgaReadFn *)(void (*)(void))info.pfnRead)(h, o, n, b); }
static int16_t cb_close(int16_t h) { return ((McgaCloseFn *)(void (*)(void))info.pfnClose)(h); }

/* ---- The initial image ---- */

/* the data of the unpacked U6MCGA.DRV outside its code (the rest of the image is 0) */
static const int8_t init_sin[128] = {
	0, 3, 6, 9, 12, 16, 19, 22, 24, 27, 30, 33, 36, 38, 41, 43,
	45, 47, 49, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 63, 64, 64,
	64, 64, 64, 63, 63, 62, 61, 60, 59, 58, 56, 55, 53, 51, 49, 47,
	45, 43, 41, 38, 36, 33, 30, 27, 24, 22, 19, 16, 12, 9, 6, 3,
	0, -3, -6, -9, -12, -16, -19, -22, -24, -27, -30, -33, -36, -38, -41, -43,
	-45, -47, -49, -51, -53, -55, -56, -58, -59, -60, -61, -62, -63, -63, -64, -64,
	-64, -64, -64, -63, -63, -62, -61, -60, -59, -58, -56, -55, -53, -51, -49, -47,
	-45, -43, -41, -38, -36, -33, -30, -27, -24, -22, -19, -16, -12, -9, -6, -3
};
/* 28F0: maximal-length Galois taps for 2..16 bits */
static const uint16_t init_lfsr[15] = {
	0x0003, 0x0006, 0x000C, 0x0014, 0x0030, 0x0060, 0x00B8, 0x0110,
	0x0240, 0x0500, 0x0CA0, 0x1B00, 0x3500, 0x6000, 0xB400
};
/* 2C00: the water tile each shore tile 10h..2Fh takes its animated pixels from */
static const uint8_t init_animsrc[32] = {
	0x0B, 0x0B, 0x0D, 0x0D, 0x0F, 0x0F, 0x09, 0x09, 0x0D, 0x0F, 0x0B, 0x09, 0x0B, 0x0D, 0x0F, 0x09,
	0x0D, 0x0F, 0x0F, 0x09, 0x09, 0x0B, 0x0B, 0x0D, 0x09, 0x0B, 0x0F, 0x0D, 0x0D, 0x0F, 0x09, 0x0B
};
static const uint16_t init_codex[8] = { 0x000, 0x03D, 0x081, 0x0BF, 0x104, 0x142, 0x17E, 0x1C3 };
static const char init_names[] = "maptiles.vga\0objtiles.vga\0tileindx.vga\0masktype.vga\0animmask.vga";
static const uint8_t init_header[6] = { 0x00, 0x00, 0x40, 0x01, 0xC8, 0x00 };

static void load_image(void)
{
	int i;

	memset(drv, 0, sizeof drv);
	if (image) {
		memcpy(drv, image, image_size);
		return;
	}
	drv[0x51] = 0x90;
	memcpy(drv + 0x52, init_header, 6);
	drv[0x5C] = 0x48;   /* textFg */
	drv[0x5D] = 0x31;   /* textBg */
	drv[0x5F] = 1;
	drv[0x60] = 1;
	drv[O_SAVEDMODE] = 3;
	drv[0x87] = 0x90;
	for (i = 0; i < 200; i++)
		cs16w((uint16_t)(O_ROWS + 2 * i), (uint16_t)(i * 320));
	memcpy(drv + O_NAMES, init_names, sizeof init_names);
	for (i = 0; i < 128; i++)
		drv[O_SIN + i] = (uint8_t)init_sin[i];
	for (i = 0; i < 15; i++)
		cs16w((uint16_t)(O_LFSR + 2 * i), init_lfsr[i]);
	for (i = 0; i < 32; i++) {
		cs16w((uint16_t)(O_ANIMSRC + 2 * i), (uint16_t)(init_animsrc[i] * 2));
		cs16w((uint16_t)(O_ANIMDST + 2 * i), (uint16_t)((0x10 + i) * 2));
		cs16w((uint16_t)(O_ANIMMSK + 2 * i), (uint16_t)(i * 0x40));
	}
	for (i = 0; i < 8; i++)
		cs16w((uint16_t)(O_CODEX + 2 * i), init_codex[i]);
}

void mcga_attach_image(const void *img, uint32_t size)
{
	image = (const uint8_t *)img;
	image_size = size > 0x10000u ? 0x10000u : size;
}

uint8_t *mcga_screen(void) { return vram; }
uint8_t *mcga_segment(void) { return drv; }

/* The palette int 10h AH=0 loads for mode 13h (IBM VGA BIOS; same in DOSBox). */
static void bios_palette(void)
{
	static const uint8_t ega[16][3] = {
		{0,0,0},{0,0,42},{0,42,0},{0,42,42},{42,0,0},{42,0,42},{42,21,0},{42,42,42},
		{21,21,21},{21,21,63},{21,63,21},{21,63,63},{63,21,21},{63,21,63},{63,63,21},{63,63,63}
	};
	static const uint8_t grey[16] = { 0, 5, 8, 11, 14, 17, 20, 24, 28, 32, 36, 40, 45, 50, 56, 63 };
	static const uint8_t lv[9][5] = {
		{0,16,31,47,63},{31,39,47,55,63},{45,49,54,58,63},
		{0,7,14,21,28},{14,17,21,24,28},{20,22,24,26,28},
		{0,4,8,12,16},{8,10,12,14,16},{11,12,13,15,16}
	};
	/* the 24 hues: indexes into the 5 levels, as r, g, b */
	static const uint8_t hue[24][3] = {
		{0,0,4},{1,0,4},{2,0,4},{3,0,4},{4,0,4},{4,0,3},{4,0,2},{4,0,1},
		{4,0,0},{4,1,0},{4,2,0},{4,3,0},{4,4,0},{3,4,0},{2,4,0},{1,4,0},
		{0,4,0},{0,4,1},{0,4,2},{0,4,3},{0,4,4},{0,3,4},{0,2,4},{0,1,4}
	};
	int i, g, h, c;

	for (i = 0; i < 16; i++)
		for (c = 0; c < 3; c++)
			vga_palette[i * 3 + c] = ega[i][c];
	for (i = 0; i < 16; i++)
		for (c = 0; c < 3; c++)
			vga_palette[(16 + i) * 3 + c] = grey[i];
	for (g = 0; g < 9; g++)
		for (h = 0; h < 24; h++)
			for (c = 0; c < 3; c++)
				vga_palette[(32 + g * 24 + h) * 3 + c] = lv[g][hue[h][c]];
	memset(vga_palette + 248 * 3, 0, 8 * 3);
}

/* ---- Entries ---- */

/* 00h (2198): GR_INIT */
void *GRDRV_00(int16_t mode)
{
	(void)mode;   /* always mode 13h */
	load_image();
	memset(&info, 0, sizeof info);
	memcpy(info.header, drv + 0x52, 6);
	info.textFg = drv[0x5C];
	info.textBg = drv[0x5D];
	info.xorMode = drv[0x5E];
	info.unknown0d = drv[0x5F];
	info.animOn = drv[0x60];
	info.toBackBuffer = drv[0x61];
	/* int 10h AH=0Fh: the current mode is kept for GR_SHUTDOWN; the port starts from
	 * text mode 3 */
	drv[O_SAVEDMODE] = 3;
	/* int 10h AX=0013h: clears the video memory, default palette */
	memset(vram, 0, sizeof vram);
	bios_palette();
	vdirty = 0;
	plat_video_source(vram, vga_palette);
	plat_video_dirty();
	return &info;
}

/* 03h (21B1): GR_SHUTDOWN: int 10h AH=0, AL = the saved mode. Nothing to do here. */
void GRDRV_03(void)
{
}

/* 06h (2A72): GR_SET_COLOR */
void GRDRV_06(int16_t color)
{
	uint8_t c = (uint8_t)color;
	cs16w(O_COLOR, (uint16_t)(c | c << 8));
}

/* 09h (2A82): GR_PUT_PIXEL */
void GRDRV_09(int16_t x, int16_t y)
{
	Seg es;
	uint16_t di;
	uint8_t c = cs8(O_COLOR);

	if (!info.toBackBuffer) {
		es = screen_seg();
		di = (uint16_t)(rowofs((uint16_t)y) + (uint16_t)x);
	} else {
		Far w = far_of(info.pWorkBuf);
		es = w.s;
		di = work_di(w, (uint16_t)x, (uint16_t)y);
	}
	if (info.xorMode)
		wr8(es, di, (uint8_t)(rd8(es, di) ^ c));
	else
		wr8(es, di, c);
	flush();
}

/* The pointer shape: shapes + dword at shapes + 4 + index * 4 (3316, 347B, 30A4). */
static Far shape_of(Far sh, uint16_t index)
{
	uint16_t bx = (uint16_t)((uint16_t)(index << 2) + 4);
	uint32_t d = rd16(sh.s, (uint16_t)(sh.off + bx)) | (uint32_t)rd16(sh.s, (uint16_t)(sh.off + bx + 2)) << 16;
	return far_add32(sh, d);
}

static uint16_t scanline(Far sc, uint16_t row)
{
	return rd16(sc.s, (uint16_t)(sc.off + (uint16_t)(row << 1)));
}

/* 0Ch (3316) and 0Fh (347B): the rectangle of the pointer shape at posX,posY clipped
 * to the surface; save copies it from the surface into saveBuf, restore back. saveBuf
 * holds the whole shape rectangle (width * height bytes, row after row). */
static void ptr_bg(const void *surface, const void *saveBuf, int16_t posX, int16_t posY,
	const void *shapes, uint16_t shapeIndex, int restore)
{
	const struct McgaSurface *sf = (const struct McgaSurface *)surface;
	Far sh = shape_of(far_of(shapes), shapeIndex);
	Far sc = far_of(sf->pScanlines);
	Far sv = far_of(saveBuf);
	Seg surf = seg_of(sf->segment);
	uint16_t e_r = rd16(sh.s, sh.off), e_l = rd16(sh.s, (uint16_t)(sh.off + 2));
	uint16_t e_u = rd16(sh.s, (uint16_t)(sh.off + 4)), e_d = rd16(sh.s, (uint16_t)(sh.off + 6));
	uint16_t stride, cw, rows, ox, sx, oy, sy, a, off, bx;

	stride = cw = (uint16_t)(e_r + e_l + 1);       /* 2F44, 2F3E */
	rows = (uint16_t)(e_d + e_u + 1);               /* 2F3A */
	ox = sx = (uint16_t)(posX - e_l);               /* 2F3C, 2F2E */
	if ((int16_t)ox > (int16_t)sf->right)
		return;
	a = (uint16_t)(sf->left - sx);
	if ((int16_t)a >= 0) {
		cw = (uint16_t)(cw - a);
		sx = sf->left;
	}
	oy = sy = (uint16_t)(posY - e_u);               /* 2F40, 2F42 */
	if ((int16_t)oy > (int16_t)sf->bottom)
		return;
	a = (uint16_t)(sf->top - sy);
	if ((int16_t)a >= 0) {
		rows = (uint16_t)(rows - a);
		sy = sf->top;
	}
	a = (uint16_t)(posX + e_r);
	if ((int16_t)a < (int16_t)sf->left)
		return;
	a = (uint16_t)(a - sf->right);
	if ((int16_t)a > 0)
		cw = (uint16_t)(cw - a);
	a = (uint16_t)(posY + e_d);
	if ((int16_t)a < (int16_t)sf->top)
		return;
	a = (uint16_t)(a - sf->bottom);
	if ((int16_t)a > 0)
		rows = (uint16_t)(rows - a);
	off = (uint16_t)((uint16_t)((uint16_t)(sy - oy) * stride) + (uint16_t)(sx - ox));
	off = (uint16_t)(off + sv.off);
	bx = (uint16_t)(sy << 1);
	do {
		uint16_t so = (uint16_t)(rd16(sc.s, (uint16_t)(sc.off + bx)) + sx);
		if (restore)
			copy(surf, so, sv.s, off, cw);
		else
			copy(sv.s, off, surf, so, cw);
		bx = (uint16_t)(bx + 2);
		off = (uint16_t)(off + stride);
	} while (--rows);
	flush();
}

/* 0Ch: save the background under the mouse pointer */
void GRDRV_0C(const void *surface, void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	ptr_bg(surface, saveBuf, posX, posY, shapes, shapeIndex, 0);
}

/* 0Fh: put it back */
void GRDRV_0F(const void *surface, const void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	ptr_bg(surface, saveBuf, posX, posY, shapes, shapeIndex, 1);
}

/* 12h (2ADF): GR_FILL_RECT, corners in any order */
void GRDRV_12(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	uint16_t ax = (uint16_t)x0, bx = (uint16_t)y0, cx = (uint16_t)x1, dx = (uint16_t)y1, t, di, step;
	uint8_t c = cs8(O_COLOR);
	Seg es;

	if (ax > cx) { t = ax; ax = cx; cx = t; }
	if (bx > dx) { t = bx; bx = dx; dx = t; }
	cx = (uint16_t)(cx - ax + 1);
	dx = (uint16_t)(dx - bx + 1);
	if (!info.toBackBuffer) {
		es = screen_seg();
		di = rowofs(bx);
		step = (uint16_t)(320 - cx);
	} else {
		Far w = far_of(info.pWorkBuf);
		es = w.s;
		di = work_di(w, 0, bx);
		step = (uint16_t)(176 - cx);
	}
	di = (uint16_t)(di + ax);
	do {
		if (!info.xorMode) {
			fill(es, di, c, cx);
		} else {
			/* XOR of a width of 1 runs its word loop 65536 times (LOOP with CX = 0), which
			 * XORs the whole segment twice: no change */
			uint16_t i;
			for (i = 0; i < cx; i++)
				wr8(es, (uint16_t)(di + i), (uint8_t)(rd8(es, (uint16_t)(di + i)) ^ c));
		}
		di = (uint16_t)(di + cx + step);
	} while (--dx);
	flush();
}

/* 15h (2B8D): GR_SCROLL_RECT_Y: dy < 0 moves the rectangle's content up by -dy lines
 * (the last -dy lines keep their pixels); dy >= 0 does nothing */
void GRDRV_15(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t dy)
{
	uint16_t ax = (uint16_t)x0, bx = (uint16_t)y0, cx = (uint16_t)x1, dx = (uint16_t)y1, t, di, si, step;
	Seg es = screen_seg();

	if (ax > cx) { t = ax; ax = cx; cx = t; }
	if (bx > dx) { t = bx; bx = dx; dx = t; }
	di = (uint16_t)(rowofs(bx) + ax);
	si = di;
	cx = (uint16_t)(cx - ax + 1);
	dx = (uint16_t)(dx - bx + 1);
	step = (uint16_t)(320 - cx);
	bx = (uint16_t)dy;
	if (bx == 0 || (int16_t)bx > 0)
		return;   /* dy > 0: 2BD2 adds [2008h + dy] to SI (not dy * 2) and returns */
	bx = (uint16_t)-bx;
	dx = (uint16_t)(dx - bx);
	si = (uint16_t)(si + rowofs(bx));
	do {   /* height == -dy: DX = 0, 65536 lines */
		copy(es, di, es, si, cx);
		di = (uint16_t)(di + cx + step);
		si = (uint16_t)(si + cx + step);
	} while (--dx);
	flush();
}

/* 18h (21C0): GR_DRAW_PIXMAP: width, height words then the pixels; no clipping */
void GRDRV_18(const void *pixmap, int16_t x, int16_t y)
{
	Far p = far_of(pixmap);
	Seg es = screen_seg();
	uint16_t di = (uint16_t)(rowofs((uint16_t)y) + (uint16_t)x);
	uint16_t si = p.off, w, h;

	w = rd16(p.s, si); si = (uint16_t)(si + 2);
	h = rd16(p.s, si); si = (uint16_t)(si + 2);
	do {
		copy(es, di, p.s, si, w);
		di = (uint16_t)(di + 320);
		si = (uint16_t)(si + w);
	} while (--h);
	flush();
}

void GRDRV_1B(int16_t a, int16_t b) { (void)a; (void)b; }   /* 35E0: RETF 4 */

/* 1Eh (2A10): GR_PUT_CHAR: 8x8, bit 7 first, textFg for 1 bits, textBg for 0 bits */
void GRDRV_1E(int16_t ch, int16_t x, int16_t y)
{
	Far f = far_of(info.font);
	Seg es = screen_seg();
	uint16_t di = (uint16_t)(rowofs((uint16_t)y) + (uint16_t)x);
	uint16_t si = (uint16_t)(f.off + (uint16_t)((uint16_t)ch << 3));
	int r, b;

	for (r = 0; r < 8; r++) {
		uint8_t bits = rd8(f.s, si++);
		for (b = 0; b < 8; b++) {
			wr8(es, di++, (bits & 0x80) ? info.textFg : info.textBg);
			bits = (uint8_t)(bits << 1);
		}
		di = (uint16_t)(di + 0x138);
	}
	flush();
}

void GRDRV_21(void) { }                  /* 35E3: RETF */
void GRDRV_24(void) { }                  /* 2D65: RETF */
void GRDRV_27(int16_t a) { (void)a; }    /* 2D66: RETF, does not pop its argument */

/* 2Ah (25A2): GR_INIT_MEMORY: loads the map tiles, the tile index and mask types, makes
 * the rest a cache of 2 KB pages for the object tiles, 16 sprite slots and the shore
 * animation masks */
int16_t GRDRV_2A(uint16_t seg, uint16_t size)
{
	uint16_t ax, bx, n, pg, last, i;
	int16_t r;

	if (size < 0x3000)
		return 0;
	info.pDriverMem = dos_mk_fp(seg, 0);   /* CS:0074 = seg (offset 0) */
	cs16w(0x74, seg);
	cb_load((char *)drv + O_N_MAPTILES, dos_mk_fp(seg, 0), 1);
	cb_load((char *)drv + O_N_TILEINDX, drv + O_TILESEG, 0);
	cb_load((char *)drv + O_N_MASKTYPE, drv + O_MASKTYPE, 1);
	ax = (uint16_t)(cs16(O_TILESEG + 0x200 * 2) + seg);
	cs16w(O_LRU, ax);
	cs16w(O_CACHE, ax);
	for (i = 0; i < 0x200; i++)
		cs16w((uint16_t)(O_TILESEG + 2 * i), (uint16_t)(cs16((uint16_t)(O_TILESEG + 2 * i)) + seg));
	memset(drv + O_TILESEG + 0x400, 0, 0xC00);
	n = (uint16_t)((uint16_t)((uint16_t)(size + seg) - ax) >> 7);
	n = (uint16_t)(n - 3);
	bx = (uint16_t)((uint16_t)(n << 7) + ax);
	cs16w(O_PAGES_END, bx);
	cs16w(O_ANIMMASK, bx);
	bx = (uint16_t)(bx + 0x80);
	for (i = 0; i < 16; i++) {
		cs16w((uint16_t)(O_SPRSEG + 2 * i), bx);
		bx = (uint16_t)(bx + 0x10);
	}
	/* the pages: a circular list, header paragraph = prev, next, first tile, tile count */
	last = ax;
	do {
		Seg es = seg_of(ax);
		last = ax;
		ax = (uint16_t)(ax - 0x80);
		wr16(es, 0, ax);
		ax = (uint16_t)(ax + 0x100);
		wr16(es, 2, ax);
		wr16(es, 4, 0);
		wr16(es, 6, 0);
	} while (--n);
	pg = cs16(O_CACHE);
	wr16(seg_of(last), 2, pg);
	wr16(seg_of(pg), 0, last);
	r = cb_load((char *)drv + O_N_ANIMMASK, dos_mk_fp(cs16(O_ANIMMASK), 0), 1);
	flush();
	return r;
}

/* evict the tiles of the page at segment pg (2272, 23D3) */
static void evict(uint16_t pg)
{
	Seg p = seg_of(pg);
	uint16_t cx = rd16(p, 6), o, cache;

	if (!cx)
		return;
	o = (uint16_t)(rd16(p, 4) << 1);
	cache = cs16(O_CACHE);
	do {
		if (cs16((uint16_t)(o + O_TILESEG)) >= cache)
			cs16w((uint16_t)(o + O_TILESEG), 0);
		o = (uint16_t)(o + 2);
	} while (--cx);
}

/* 2249: make the tile's pixels resident, return their segment (offset 0). Object tiles
 * are read from OBJTILES.VGA a block (0x7F0 bytes) at a time into the least recently
 * used page, after evicting that page's tiles. */
static uint16_t tile_load(uint16_t bx)
{
	uint16_t tile = bx & 0x7FF, dx, cx, page;

	dx = cs16((uint16_t)(O_TILESEG + tile * 2));
	cx = cs16(O_CACHE);
	page = (uint16_t)(((uint16_t)(dx - cx) & 0xFF80) + cx);
	if (dx == 0) {
		uint16_t lru = cs16(O_LRU), blk, h, first, cnt;
		uint8_t ah;
		uint32_t pos;

		evict(lru);
		cb_disk(5);
		h = (uint16_t)cb_open((char *)drv + O_N_OBJTILES);
		blk = cs16((uint16_t)(O_BLOCKOFS + 2 * cs8((uint16_t)(O_BLOCKNUM + tile))));
		pos = (uint32_t)blk << 4;
		cb_read((int16_t)h, pos, 0x7F0, dos_mk_fp((uint16_t)(cs16(O_LRU) + 1), 0));
		cb_close((int16_t)h);
		/* the block's tiles: the run of tiles around this one with the same block number */
		ah = cs8((uint16_t)(O_BLOCKNUM + tile));
		first = tile;
		do {
			first--;
			if ((int16_t)first < 0x200)
				break;
		} while (cs8((uint16_t)(O_BLOCKNUM + first)) == ah);
		first++;
		lru = cs16(O_LRU);
		wr16(seg_of(lru), 4, first);
		cnt = 0;
		dx = (uint16_t)(lru + 1);
		for (;;) {
			cnt++;
			if (cs16((uint16_t)(O_TILESEG + (uint16_t)(first << 1))) == 0)
				cs16w((uint16_t)(O_TILESEG + (uint16_t)(first << 1)), dx);
			if (!(cs8((uint16_t)(O_MASKTYPE + first)) & 8))
				dx = (uint16_t)(dx + 0x10);
			else
				dx = (uint16_t)(dx + rd8(seg_of(dx), 0));   /* compressed: size in paragraphs */
			first++;
			if (first >= 0x800)
				break;
			if (cs8((uint16_t)(O_BLOCKNUM + first)) != ah)
				break;
		}
		wr16(seg_of(lru), 6, cnt);
		dx = cs16((uint16_t)(O_TILESEG + tile * 2));
		cx = cs16(O_CACHE);
		page = (uint16_t)(((uint16_t)(dx - cx) & 0xFF80) + cx);
	}
	/* 2372: a cached tile's page becomes the most recently used */
	if (dx >= cx) {
		uint16_t head = cs16(O_LRU);
		Seg hd = seg_of(head);
		if (page == head) {
			cs16w(O_LRU, rd16(hd, 2));
		} else if (page != 0 && page != rd16(hd, 0)) {
			Seg p = seg_of(page), es;
			uint16_t prev = rd16(p, 0), next, hp;
			es = seg_of(prev);
			next = rd16(p, 2);
			wr16(es, 2, next);
			es = seg_of(next);
			wr16(es, 0, prev);
			wr16(p, 2, head);
			hp = rd16(hd, 0);
			wr16(p, 0, hp);
			wr16(seg_of(hp), 2, page);
			wr16(hd, 0, page);
		}
	}
	return dx;
}

/* 23C6: the tile as 16x16 bytes (0FFh = transparent) in the least recently used page,
 * whose tiles are evicted; returns that segment */
static uint16_t tile_raw(uint16_t bx)
{
	uint16_t ts = tile_load(bx), head, es;
	Seg s, d;

	head = cs16(O_LRU);
	evict(head);
	wr16(seg_of(head), 6, 0);
	es = (uint16_t)(head + 1);
	s = seg_of(ts);
	d = seg_of(es);
	if (!(cs8((uint16_t)(O_MASKTYPE + bx)) & 2)) {
		copy(d, 0, s, 0, 0x100);
	} else {
		uint16_t si = 1, di = 0, ax;
		uint8_t n;
		fill(d, 0, 0xFF, 0x100);
		for (;;) {
			ax = rd16(s, si); si = (uint16_t)(si + 2);
			di = (uint16_t)(di + ax);
			while ((int16_t)ax >= 16) {
				di = (uint16_t)(di - 160);
				ax = (uint16_t)(ax - 176);
			}
			n = rd8(s, si++);
			if (!n)
				break;
			di &= 0xFF;
			copy(d, di, s, si, n);
			di = (uint16_t)(di + n);
			si = (uint16_t)(si + n);
		}
	}
	return es;
}

/* draw 16x16 from src:si at dst:di, rows `stride` apart; transparent: skip 0FFh */
static void blit16(Seg d, uint16_t di, Seg s, uint16_t si, uint16_t stride, int transparent)
{
	int r, c;

	for (r = 0; r < 16; r++) {
		for (c = 0; c < 16; c++) {
			uint8_t v = rd8(s, si++);
			if (!transparent || v != 0xFF)
				wr8(d, di, v);
			di++;
		}
		di = (uint16_t)(di + stride - 16);
	}
}

/* compressed tile: skip the size byte, then (displacement word, count byte, pixels)
 * until a count of 0; displacements are in a 176-wide buffer, `rowfix` (stride - 176)
 * converts them to the destination's stride */
static void blit_packed(Seg d, uint16_t di, Seg s, uint16_t rowfix, int convert)
{
	uint16_t si = 1, ax;
	uint8_t n;

	for (;;) {
		ax = rd16(s, si); si = (uint16_t)(si + 2);
		di = (uint16_t)(di + ax);
		if (convert) {
			while ((int16_t)ax >= 16) {
				di = (uint16_t)(di + rowfix);
				ax = (uint16_t)(ax - 176);
			}
		}
		n = rd8(s, si++);
		if (!n)
			break;
		copy(d, di, s, si, n);
		di = (uint16_t)(di + n);
		si = (uint16_t)(si + n);
	}
}

/* 2Dh (2458): GR_PUT_TILE_SCREEN */
void GRDRV_2D(int16_t tile, int16_t x, int16_t y)
{
	uint8_t type = cs8((uint16_t)(O_MASKTYPE + (uint16_t)tile)) & 3;
	Seg s = seg_of(tile_load((uint16_t)tile));
	Seg es = screen_seg();
	uint16_t di = (uint16_t)(rowofs((uint16_t)y) + (uint16_t)x);

	if (type == 0)
		blit16(es, di, s, 0, 320, 0);
	else if (type == 1)
		blit16(es, di, s, 0, 320, 1);
	else
		blit_packed(es, di, s, 0x90, 1);
	flush();
}

/* 2764: slot check; CLC before the JNZ makes it always succeed */

/* 33h (2779): GR_DEFINE_SPRITE: the tile rotated by rot (128 = full turn) around
 * (pivotX/16, pivotY/16), into the 16x16 slot */
void GRDRV_33(int16_t slot, int16_t tile, int16_t rot, int16_t pivotX, int16_t pivotY)
{
	uint16_t r, es, di = 0;
	int8_t sn, cs, u, v;
	Seg src, dst;

	cs16w(O_PIVX, (uint16_t)((uint16_t)pivotX << 3));
	cs16w(O_PIVY, (uint16_t)((uint16_t)pivotY << 3));
	if (slot >= 16)
		return;   /* a negative slot goes on and reads [26C0h + slot * 2] */
	src = seg_of(tile_raw((uint16_t)tile));
	es = cs16((uint16_t)(O_SPRSEG + (uint16_t)((uint16_t)slot << 1)));
	dst = seg_of(es);
	r = (uint16_t)rot & 0x7F;
	sn = (int8_t)cs8((uint16_t)(O_SIN + r));
	cs = (int8_t)cs8((uint16_t)(O_SIN + ((r + 0x20) & 0x7F)));
	for (v = -15; v < 16; v = (int8_t)(v + 2)) {
		for (u = -15; u < 16; u = (int8_t)(u + 2)) {
			uint16_t sx = (uint16_t)((int16_t)(u * cs) - (int16_t)(v * sn) + cs16(O_PIVX));
			uint16_t sy = (uint16_t)((int16_t)(u * sn) + (int16_t)(v * cs) + cs16(O_PIVY));
			uint8_t bh = (uint8_t)((uint16_t)(sx << 1) >> 8), bl = (uint8_t)((uint16_t)(sy << 1) >> 8);
			uint8_t al = 0xFF;
			if (((bh | bl) & 0xF0) == 0)
				al = rd8(src, (uint16_t)((bl << 4) + bh));
			wr8(dst, di++, al);
		}
	}
}

/* 36h (2846): GR_DRAW_SPRITE: slot centred at x,y in the back buffer, 0FFh transparent */
void GRDRV_36(int16_t slot, int16_t x, int16_t y)
{
	Far w;
	uint16_t di;

	if ((uint16_t)slot >= 16)
		return;
	w = far_of(info.pWorkBuf);
	di = (uint16_t)(work_di(w, (uint16_t)x, (uint16_t)y) - 0x588);
	blit16(w.s, di, seg_of(cs16((uint16_t)(O_SPRSEG + slot * 2))), 0, 176, 1);
}

/* 39h (2897): GR_SAVE_SPRITE_BG: the 16x16 centred at x,y in the back buffer into the
 * slot, unless its offset (pWorkBuf's offset included) is negative or >= 6EA0h */
void GRDRV_39(int16_t slot, int16_t x, int16_t y)
{
	Far w;
	uint16_t si;
	Seg d;
	int r;

	if ((uint16_t)slot >= 16)
		return;
	w = far_of(info.pWorkBuf);
	si = (uint16_t)(work_di(w, (uint16_t)x, (uint16_t)y) - 0x588);
	if ((int16_t)si < 0 || si >= 0x6EA0)
		return;
	d = seg_of(cs16((uint16_t)(O_SPRSEG + slot * 2)));
	for (r = 0; r < 16; r++) {
		copy(d, (uint16_t)(r * 16), w.s, si, 16);
		si = (uint16_t)(si + 176);
	}
}

void GRDRV_3C(void) { }   /* 28EC: RETF */

/* 3Fh (2D2F): GR_ANIMATE_TILES: copy the water pixels of tiles 9/B/D/F into the shore
 * tiles 10h..2Fh through ANIMMASK.VGA's masks (copy, skip, copy, ... byte counts, 0 ends) */
void GRDRV_3F(void)
{
	Seg mk = seg_of(cs16(O_ANIMMASK));
	int k;

	for (k = 0; k < 32; k++) {
		Seg s = seg_of(cs16((uint16_t)(cs16((uint16_t)(O_ANIMSRC + 2 * k)) + O_TILESEG)));
		Seg d = seg_of(cs16((uint16_t)(cs16((uint16_t)(O_ANIMDST + 2 * k)) + O_TILESEG)));
		uint16_t bx = cs16((uint16_t)(O_ANIMMSK + 2 * k)), si = 0, di = 0;
		uint8_t n;

		n = rd8(mk, bx++);
		copy(d, di, s, si, n);
		si = (uint16_t)(si + n);
		di = (uint16_t)(di + n);
		for (;;) {
			n = rd8(mk, bx++);
			if (!n)
				break;
			si = (uint16_t)(si + n);
			di = (uint16_t)(di + n);
			n = rd8(mk, bx++);
			if (!n)
				break;
			copy(d, di, s, si, n);
			si = (uint16_t)(si + n);
			di = (uint16_t)(di + n);
		}
	}
	flush();
}

/* 42h (24D9): GR_PUT_TILE into the back buffer at x,y. Tile bits 11-12: 0 normal; else
 * only the tile's black (0) pixels, drawn in 0Bh (800h), 0Ch (1000h) or 09h (1800h). */
void GRDRV_42(int16_t tile, int16_t x, int16_t y)
{
	uint16_t t = (uint16_t)tile & 0x7FF, fl = (uint16_t)tile & 0x7800, di;
	uint8_t type = cs8((uint16_t)(O_MASKTYPE + t)) & 3;
	Seg s = seg_of(tile_load(t));
	Far w = far_of(info.pWorkBuf);

	di = work_di(w, (uint16_t)x, (uint16_t)y);
	if (!(fl & 0x1800)) {
		if (type == 0)
			blit16(w.s, di, s, 0, 176, 0);
		else if (type == 1)
			blit16(w.s, di, s, 0, 176, 1);
		else
			blit_packed(w.s, di, s, 0, 0);
	} else {
		uint8_t color;
		Seg r;
		int i;
		fl &= 0x1800;
		color = fl == 0x800 ? 0x0B : fl == 0x1000 ? 0x0C : 0x09;
		r = seg_of(tile_raw(t));
		for (i = 0; i < 256; i++)
			wr8(r, (uint16_t)i, rd8(r, (uint16_t)i) ? 0xFF : color);
		blit16(w.s, di, r, 0, 176, 1);
	}
	flush();
}

/* 45h (290E): GR_COPY_TO_SCREEN: back buffer x0..x1, y0..y1 to the screen at dstX,dstY;
 * x0 and dstX are rounded down to even, x1 up to odd (word copies) */
void GRDRV_45(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t dstX, int16_t dstY)
{
	Far w = far_of(info.pWorkBuf);
	uint16_t ax = (uint16_t)x0 & 0xFFFE;
	uint16_t si = work_di(w, ax, (uint16_t)y0);
	uint16_t cx = (uint16_t)x1 | 1;
	uint16_t di = (uint16_t)(rowofs((uint16_t)dstY) + ((uint16_t)dstX & 0xFFFE));
	uint16_t rows = (uint16_t)((uint16_t)y1 - (uint16_t)y0 + 1);
	uint16_t width, words;
	Seg es = screen_seg();

	width = (uint16_t)(cx - ax + 1);
	words = (uint16_t)(width >> 1);
	do {
		copy(es, di, w.s, si, (uint16_t)(words * 2));
		si = (uint16_t)(si + words * 2 + (uint16_t)(176 - width));
		di = (uint16_t)(di + words * 2 + (uint16_t)(320 - width));
	} while (--rows);
	flush();
}

/* 2971: one pixel x,y from the back buffer to the screen */
static void fade_pixel(Far w, uint16_t x, uint16_t y)
{
	wr8(screen_seg(), (uint16_t)(rowofs(y) + x), rd8(w.s, work_di(w, x, y)));
}

/* 48h (29A1): GR_FADE_TO_SCREEN: the rectangle's pixels copied from the back buffer to
 * the screen (same coordinates) in the order of a maximal-length LFSR */
void GRDRV_48(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	Far w = far_of(info.pWorkBuf);
	uint16_t cw = (uint16_t)((uint16_t)x1 - (uint16_t)x0 + 1);
	uint16_t h = (uint16_t)((uint16_t)y1 - (uint16_t)y0 + 1);
	uint16_t ax = (uint16_t)(cw * h - 1u), bits = 0, taps, si = 1;
	uint32_t guard = 0;

	while (ax) {
		ax >>= 1;
		bits++;
	}
	taps = cs16((uint16_t)(O_LFSR + (uint16_t)((uint16_t)(bits - 2) << 1)));
	if (cw != 0) {   /* 0: DIV by zero in DOS */
		do {
			uint16_t q = (uint16_t)(si / cw), r = (uint16_t)(si % cw);
			if ((int16_t)q < (int16_t)h)
				fade_pixel(w, (uint16_t)(r + (uint16_t)x0), (uint16_t)(q + (uint16_t)y0));
			if (si & 1)
				si = (uint16_t)((si >> 1) ^ taps);
			else
				si >>= 1;
		} while (si != 1 && ++guard < 0x10000u);   /* no return to 1: DOS hangs */
	}
	fade_pixel(w, (uint16_t)x0, (uint16_t)y0);
	flush();
}

/* 4Bh (2CC2): GR_SET_ANIM_FRAME: tile shows frame's pixels (frame must be < 200h) */
void GRDRV_4B(int16_t tile, int16_t frame)
{
	uint16_t b = (uint16_t)frame, a = (uint16_t)tile;
	uint8_t cl, ch;
	uint16_t seg;

	if (b >= 0x200)
		return;
	cl = cs8((uint16_t)(O_MASKTYPE + b));
	seg = cs16((uint16_t)(O_TILESEG + b * 2));
	ch = cs8((uint16_t)(O_MASKTYPE + a)) & 0x0C;
	cl &= 3;
	drv[(uint16_t)(O_MASKTYPE + a)] = (uint8_t)(ch | cl);
	cs16w((uint16_t)(O_TILESEG + (uint16_t)(a << 1)), seg);
}

/* 2E00 / 2E28: triangle wave, 128 at 0, -128 at 100h */
static uint16_t tri(uint16_t a)
{
	a &= 0x1FF;
	if ((int16_t)a < 0x100)
		return (uint16_t)(0x80 - a);
	return (uint16_t)(a - 0x180);
}

/* 4Eh (2D80): GR_CODEX_EFFECT: a swirl of the map window toward its centre (58h,58h):
 * 8 arms, pixels read on each arm are written back 7 radius steps further in */
void GRDRV_4E(void)
{
	uint16_t ang[8], pos[64];
	uint16_t angOff = 0, R = 0x58;
	Seg es = screen_seg();
	Far w = far_of(info.pWorkBuf);

	do {
		uint16_t rlim = R > 0x4C ? 0x4C : R, iter = 12;
		do {
			uint16_t roww = 0, rowr = 1, r = 0;
			int k;
			for (k = 7; k >= 0; k--)
				ang[k] = (uint16_t)(cs16((uint16_t)(O_CODEX + 2 * k)) - angOff);
			for (k = 0; k < 64; k++)
				pos[k] = 0xFFFF;
			do {
				for (k = 7; k >= 0; k--) {
					uint16_t a = ang[k]++, val, d;
					int16_t py = (int16_t)((int16_t)(uint16_t)((int16_t)tri((uint16_t)(a - 0x80)) * (int16_t)r) >> 6);
					int16_t px = (int16_t)((int16_t)(uint16_t)((int16_t)tri(a) * (int16_t)r) >> 6);
					int16_t lo = (int16_t)(0x58 - rlim), hi = (int16_t)(rlim + 0x58), cx;
					py = (int16_t)(py + 0x58);
					px = (int16_t)(px + 0x58);
					cx = lo;
					if (px < lo || px >= hi || py < lo || py >= hi) {
						val = 0;
					} else {
						uint16_t bx = (uint16_t)(rowofs((uint16_t)py) + (uint16_t)px) & 0xFFFE;
						pos[roww * 8 + k] = bx;
						if (px >= 0x10 && px < 0xA0) {
							cx = py;
							if (py >= 0x10 && py < 0xA0) {
								val = rd16(es, bx);
								goto store;
							}
						}
						/* outside 16..9Fh: the back buffer, but on row 58h - rlim unless
						 * only py is outside */
						val = rd16(w.s, work_di(w, (uint16_t)px, (uint16_t)cx));
					}
				store:
					d = pos[rowr * 8 + k];
					if (d != 0xFFFF)
						wr16(es, d, val);
				}
				rowr = (rowr + 1) & 7;
				roww = (roww + 1) & 7;
				r++;
			} while (r < rlim);
			angOff = (uint16_t)(angOff + 8);
		} while (--iter);
		R = (uint16_t)(R - 4);
	} while (R);
	flush();
}

/* 30h (30A4): draw the mouse pointer shape on the surface, hot spot at posX,posY.
 * Shape: extents right, left, up, down (words), then runs: word n2 (pixels * 2, +1 if
 * RLE), word dx, word dy, data; a word 0 ends (clipped path: n2 >> 1 == 0 ends).
 * RLE data: byte b: b >> 1 pixels, literal (b even) or one color byte (b odd). */
static void ptr_rle(Seg d, uint16_t di, Seg s, uint16_t *psi, uint16_t count)
{
	uint16_t si = *psi;

	do {
		uint8_t b = rd8(s, si++);
		uint16_t n = (uint16_t)(b >> 1);
		count = (uint16_t)(count - n);
		if (b & 1) {
			uint8_t c = rd8(s, si++);
			fill(d, di, c, n);
		} else {
			copy(d, di, s, si, n);
			si = (uint16_t)(si + n);
		}
		di = (uint16_t)(di + n);
	} while (count != 0);
	*psi = si;
}

void GRDRV_30(const void *surface, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	const struct McgaSurface *sf = (const struct McgaSurface *)surface;
	Far shs = far_of(shapes), sh, sc = far_of(sf->pScanlines);
	Seg surf = seg_of(sf->segment), s, tmp;
	int16_t right = (int16_t)sf->right, left = (int16_t)sf->left;
	int16_t top = (int16_t)sf->top, bottom = (int16_t)sf->bottom;
	uint16_t x = (uint16_t)posX, y = (uint16_t)posY, si;

	if ((int16_t)(uint16_t)(shapeIndex << 2) >= (int16_t)rd16(shs.s, (uint16_t)(shs.off + 4)))
		return;
	sh = shape_of(shs, shapeIndex);
	s = sh.s;
	si = sh.off;
	tmp.host = drv;
	tmp.base = 0;
	if (!((int16_t)(rd16(s, si) + x) > right
		|| (int16_t)(x - rd16(s, (uint16_t)(si + 2))) < left
		|| (int16_t)(y - rd16(s, (uint16_t)(si + 4))) < top
		|| (int16_t)(rd16(s, (uint16_t)(si + 6)) + y) > bottom)) {
		/* 3146: all inside */
		si = (uint16_t)(si + 8);
		for (;;) {
			uint16_t n2 = rd16(s, si), di, row;
			si = (uint16_t)(si + 2);
			if (!n2)
				break;
			di = (uint16_t)(rd16(s, si) + x); si = (uint16_t)(si + 2);
			row = (uint16_t)(rd16(s, si) + y); si = (uint16_t)(si + 2);
			di = (uint16_t)(di + scanline(sc, row));
			if (n2 & 1) {
				ptr_rle(surf, di, s, &si, (uint16_t)(n2 >> 1));
			} else {
				copy(surf, di, s, si, (uint16_t)(n2 >> 1));
				si = (uint16_t)(si + (n2 >> 1));
			}
		}
	} else {
		/* 31A6: clipped to the surface */
		si = (uint16_t)(si + 8);
		for (;;) {
			uint16_t n2 = rd16(s, si), cx, dx, row, di;
			si = (uint16_t)(si + 2);
			cx = (uint16_t)(n2 >> 1);
			if (!cx)
				break;
			dx = (uint16_t)(rd16(s, si) + x); si = (uint16_t)(si + 2);
			row = (uint16_t)(rd16(s, si) + y); si = (uint16_t)(si + 2);
			if (!(n2 & 1)) {
				uint16_t a, d;
				if ((int16_t)row < top || (int16_t)row > bottom || (int16_t)dx > right
					|| (int16_t)(uint16_t)(dx + cx - 1) < left) {
					si = (uint16_t)(si + cx);
					continue;
				}
				di = (uint16_t)(scanline(sc, row) + dx);
				a = (uint16_t)(left - dx);
				if ((int16_t)a > 0) {
					cx = (uint16_t)(cx - a);
					si = (uint16_t)(si + a);
					di = (uint16_t)(di + a);
					dx = (uint16_t)(dx + a);
				}
				d = (uint16_t)(dx + cx - 1 - right);
				if ((int16_t)d <= 0)
					d = 0;
				cx = (uint16_t)(cx - d);
				copy(surf, di, s, si, cx);
				si = (uint16_t)(si + cx + d);
			} else {
				uint16_t a, d;
				if ((int16_t)row < top || (int16_t)row > bottom || (int16_t)dx > right
					|| (int16_t)(uint16_t)(dx + cx - 1) < left) {
					/* 32A7: skip the run's data */
					do {
						uint8_t b = rd8(s, si);
						uint16_t n = (uint16_t)(b >> 1);
						si = (uint16_t)(si + 2);
						if (!(b & 1))
							si = (uint16_t)(si + n - 1);
						cx = (uint16_t)(cx - n);
					} while (cx != 0);
					continue;
				}
				di = (uint16_t)(scanline(sc, row) + dx);
				a = (uint16_t)(dx + cx - 1 - right);
				d = (uint16_t)(dx - left);
				if ((int16_t)d >= 0 && (int16_t)a <= 0) {
					ptr_rle(surf, di, s, &si, cx);
				} else {
					/* 32B8: unpack into CS:2F64, copy the visible part */
					uint16_t ts = O_PTRTMP, n = cx;
					ptr_rle(tmp, O_PTRTMP, s, &si, cx);
					if ((int16_t)d < 0) {
						n = (uint16_t)(n + d);
						ts = (uint16_t)(ts - d);
						di = (uint16_t)(di - d);
					}
					if ((int16_t)a > 0)
						n = (uint16_t)(n - a);
					copy(surf, di, tmp, ts, n);
				}
			}
		}
	}
	flush();
}
