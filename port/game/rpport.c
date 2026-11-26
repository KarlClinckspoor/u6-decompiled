/* The port's side of Exhume's replay.c (port/include/rpport.h, the bindings): what the
 * bindings call that the port has to supply. Compiled with the game's sources (the game's
 * headers, compat.h), since the SEGS words and the far heap's bounds read the game's own
 * structures (struct tMouse, struct tGr, union tHandler), which hold host pointers here where
 * DOS's hold segment:offset pairs: each pointer is turned back into the segment it names in
 * the DOS arena (FP_SEG, port/sys/dosmem.c), so tools/replay.py's [replay.segments] can pair
 * it with DOS's. */
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "u6.h"
#include "mouse.h"
#include "gr.h"

/* not rpport.h: its declarations are the dump's byte views of the game's variables, which
 * u6.h declares with their own types */
uint32_t u6_rand_seed(void);

#ifndef O_BINARY
#define O_BINARY 0
#endif

unsigned char u6_rp_keystate[1];
int16_t u6_rp_joy[4];
uint32_t u6_rp_clock;

/* the session sets them (u6_rp_set_replay, u6_rp_set_record, below): the recording read as
 * replay.c's REPLAY.IN, and --record's folder */
const char *u6_rp_replay_path;
static const char *u6_rp_out_dir;
const char *session_game_home(void);
long dos_linear(const volatile void *p);
extern uint8_t *dos_base;

/* replay.c's port-only references, none of which this port has (Exhume's runtime does) */
int enhance_on;
char *enhance_names(int on, char *buf, int n)
{
	(void)on;
	if (n > 0)
		buf[0] = 0;
	return buf;
}
int cheat_scripted(void) { return 0; }
int bc_open_host(const char *name, int flags, int mode) { return open(name, flags | O_BINARY, mode); }

/* dump_screen's (rpport.h's RP_DUMP writes PAL and SCRN itself and never calls it) */
const unsigned char *vga_plane(int p) { static unsigned char z[0x10000]; (void)p; return z; }
unsigned char vga_reg_crtc(int i) { (void)i; return 0; }
void vga_get_dac(unsigned char rgb6[768]) { memset(rgb6, 0, 768); }
uint32 port_rand_seed(void) { return u6_rand_seed(); }

/* NULL's stand-ins: DS:0..30h as Turbo C 2.0's C0 leaves it in GAME.EXE (four zero bytes, its
 * copyright and the start of "Null pointer assignment", which sum to 0DFAh, RP_NULL_SUM), and
 * a vector table of zeros. The port has neither; tools/replay.py checks NULL in each build
 * alone, and a cross comparison skips it. */
unsigned char *port_null_copy(int far_table)
{
	static unsigned char ds0[0x31] =
		"\0\0\0\0Turbo-C - Copyright (c) 1988 Borland Intl.\0Nu";
	static unsigned char ivt[0x400];
	return far_table ? ivt : ds0;
}

/* PAL and SCRN as the DOS build's RP_U6_SCREEN reads them: the DAC through ports 3C7h/3C9h and
 * A000:0000. While the game has the VGA in mode 13h (port/gfx/mcga.c, GR_INIT to GR_SHUTDOWN)
 * that is the port's DAC and screen. Before (the first dump, at MOUSE_INIT's first button read,
 * before InitGraphicsData) DOS's VGA is still in the text mode 3 that DOS started in, which the
 * port does not draw, so this writes what that mode holds:
 *   - the DAC the VGA BIOS loads for mode 3: the EGA's 64 colours (bit 0/1/2 blue/green/red at
 *     2/3 intensity, 2Ah; bits 3/4/5 the same at 1/3, 15h) in entries 0..3Fh, the rest 0;
 *   - A000:0000 reads FFh: in text mode the graphics controller maps B800h, and nothing
 *     answers at A000h (DOSBox-X gives FFh, as a real ISA bus does). */
int mcga_mode13(void);
const unsigned char *u6_rp_screen(unsigned char pal[768])
{
	static unsigned char text_frame[64000];
	int i;
	if (mcga_mode13()) {
		outportb(0x3C7, 0);
		for (i = 0; i < 768; i++)
			pal[i] = inportb(0x3C9);
		return mcga_screen();
	}
	memset(pal, 0, 768);
	for (i = 0; i < 64; i++) {
		pal[i * 3 + 0] = (unsigned char)((i & 4 ? 0x2A : 0) + (i & 0x20 ? 0x15 : 0));
		pal[i * 3 + 1] = (unsigned char)((i & 2 ? 0x2A : 0) + (i & 0x10 ? 0x15 : 0));
		pal[i * 3 + 2] = (unsigned char)((i & 1 ? 0x2A : 0) + (i & 0x08 ? 0x15 : 0));
	}
	memset(text_frame, 0xFF, sizeof text_frame);
	return text_frame;
}

/* main.c's rp_u6_shutdown (the replay DOS build's, -DREPLAY): the temporary object blocks
 * deleted from SAVEGAME, which the runs compare; the music, the timer and the video mode are
 * the port's exit's */
void u6_port_rp_shutdown(void)
{
	OBJ_DeleteTmpBlocks();
}

/* replay.c's other files (STATE.OUT, RECORD.OUT, TRACE.OUT): in --record's folder, else in the
 * folder the game runs in (the home folder, or a replay's) */
static void home_path(char *out, size_t n, const char *name)
{
	const char *h = u6_rp_out_dir ? u6_rp_out_dir : session_game_home();
	snprintf(out, n, "%s/%s", h && h[0] ? h : ".", name);
}

int u6_rp_open_in(const char *name)
{
	char p[1100];
	if (!strcmp(name, "REPLAY.IN")) {
		if (!u6_rp_replay_path)
			return -1;
		return open(u6_rp_replay_path, O_RDONLY | O_BINARY);
	}
	home_path(p, sizeof p, name);
	return open(p, O_RDONLY | O_BINARY);
}

int u6_rp_open_out(const char *name, int flags)
{
	char p[1100];
	home_path(p, sizeof p, name);
	return open(p, flags | O_BINARY, 0644);
}

static uint16_t seg(const void *p)
{
	return p ? FP_SEG(p) : 0;
}

static uint32_t lin(const void *p)
{
	long l = p ? dos_linear(p) : -1;
	return l < 0 ? 0 : (uint32_t)l;
}

unsigned char *u6_rp_lin(uint32_t l)
{
	return dos_base + l;
}

/* SEGS, rpgame.h's order. DGROUP (the first word) is 0: the port's near data is host memory,
 * no segment of the arena. */
void u6_rp_segs(uint16_t w[30])
{
	w[0] = 0;
	w[1] = seg(FarHeap);
	w[2] = seg(Mem_BP);
	w[3] = seg(GrDriver.p);
	w[4] = seg(MUS_Driver.p);
	w[5] = seg(GrInfo);
	w[6] = GrInfo ? seg(GrInfo->font) : 0;
	w[7] = seg(Mouse.shapes);
	w[8] = seg(Mouse.saveBuf);
	w[9] = seg(PointerShapes);
	w[10] = seg(VGAPalette);
	w[11] = seg(TerrainType);
	w[12] = seg(TileFlag);
	w[13] = seg(BaseTile);
	w[14] = seg(TypeWeight);
	w[15] = seg(TileFlag2);
	w[16] = seg(ScratchBuf);
	w[17] = MapSurface.segment;
	w[18] = seg(Schedule);
	w[19] = seg(NPCMode);
	w[20] = seg(NPCComMode);
	w[21] = seg(SchedIndex);
	w[22] = seg(Leader);
	w[23] = seg(NPCFlag);
	w[24] = seg(OrigShapeType);
	w[25] = seg(Level);
	w[26] = seg(PathData);
	w[27] = seg(ChunkIndexBuf);
	w[28] = seg(TalkBuf);
	w[29] = seg(GrMemory);
}

/* the far heap's sections' bounds, as rpgame.h's RP_U6_HEAPS takes them: FarHeap; the music
 * driver's block (its segment, else the next block); the next block (VGAPalette with VGA, else
 * the font); Mem_BP */
void u6_rp_heap_bounds(uint32_t b[4])
{
	uint32_t end = VGAPalette ? lin(VGAPalette) : (GrInfo ? lin(GrInfo->font) : 0);
	uint16_t ms = seg(MUS_Driver.p);
	b[0] = lin(FarHeap);
	b[1] = ms ? (uint32_t)ms << 4 : end;
	b[2] = end;
	b[3] = lin(Mem_BP);
}

/* replay.c's mode, which the session (port/sys/session.c) chooses before the game starts
 * (replay.c: RP_OFF 0, RP_RECORD 1, RP_REPLAY 2):
 *   - off: no recording (--no-recording, the setting off), and U.EXE's and END.EXE's parts;
 *   - record (--record DIR): RECORD.OUT and STATE.OUT, the dumps, go to DIR, and the stop key
 *     (F9) ends the session;
 *   - the black box (every player's session): RECORD.OUT alone, at the path given, no dumps,
 *     no stop key (replay.c's rp_blackbox);
 *   - replay: REPLAY.IN is the file given (a RECORD.OUT of the port or of the replay DOS
 *     build); with watch set (a replay shown to the player) no dumps are written. */
extern int16 rp_request;
extern int16 rp_blackbox;
extern const char *rp_blackbox_name;
void u6_rp_set_off(void)
{
	rp_request = 0;
	rp_blackbox = 0;
}
void u6_rp_set_record(const char *dir)
{
	u6_rp_out_dir = dir;
	rp_request = 1;
	rp_blackbox = 0;
}
void u6_rp_set_blackbox(const char *file)
{
	rp_blackbox_name = file;
	rp_request = 1;
	rp_blackbox = 1;
}
void u6_rp_set_replay(const char *file, int watch)
{
	u6_rp_replay_path = file;
	rp_request = 2;
	rp_blackbox = (int16)(watch != 0);
}

/* portable.h's FAR_COPY, which replay.c's dump_far copies through: Exhume's runtime resolves
 * its paragraph map there; the port's far pointers are host pointers already */
void port_far_copy(void *dst, const void *src, unsigned n)
{
	memcpy(dst, src, n);
}

/* ---- DGROUP in DOS's layout (the DATA and BSS sections) ----
 *
 * port/game/dgroup_port.c (port/tools/gen_dgroup.py) lists each DOS byte range the port has a
 * counterpart for; this writes them into one image of DS:0..EDF9, every pointer in DOS's form.
 * What the table leaves out stays 0 (exhume.toml masks those ranges, each with its reason),
 * except DS:0..30h, C0's copyright, which is the same constant as the NULL section's stand-in. */
#include "dgs.h"

extern const struct dg_ent u6_dg_ents[];
extern const unsigned u6_dg_nents;
extern const struct dg_fn u6_dg_fns[];
extern const unsigned u6_dg_nfns;
extern const struct dg_fn *const u6_dg_sfns[];
extern const unsigned u6_dg_nsfns[];

#define DG_SIZE 0xEDFAu
static uint8_t dg_img[DG_SIZE];
struct dg_span { const uint8_t *p; uint32_t n; uint16_t off; };
static struct dg_span *dg_spans;
static unsigned dg_nspans;

static void put16(uint8_t *d, uint16_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); }
static void dg_put_handler(uint8_t *d, const void *base, unsigned short entry);
extern unsigned short u6_gr_entry, u6_mus_entry;
const void *mcga_info_ptr(void);

/* the host ranges the image's plain bytes come from, to place a near pointer */
/* the port's writable copies of literals (u6port.h's WRITABLE_STR), noted when the game makes
 * them, with the literal's DOS offset (u6_dg_wstrs) */
extern const struct dg_wstr u6_dg_wstrs[];
#define DG_MAX_WSTR 16
static struct { char *buf; unsigned n; uint16_t off; } dg_wstr[DG_MAX_WSTR];
static unsigned dg_nwstr;

void u6_writable(char *buf, const char *init, unsigned n)
{
	unsigned i;
	for (i = 0; i < dg_nwstr; i++)
		if (dg_wstr[i].buf == buf)
			return;
	for (i = 0; u6_dg_wstrs[i].init; i++)
		if (!strcmp(u6_dg_wstrs[i].init, init) && dg_nwstr < DG_MAX_WSTR) {
			dg_wstr[dg_nwstr].buf = buf;
			dg_wstr[dg_nwstr].n = n;
			dg_wstr[dg_nwstr++].off = u6_dg_wstrs[i].off;
			return;
		}
}

static void dg_init(void)
{
	unsigned i;
	if (dg_spans)
		return;
	dg_spans = (struct dg_span *)calloc(u6_dg_nents + DG_MAX_WSTR, sizeof *dg_spans);
	for (i = 0; i < u6_dg_nents; i++) {
		const struct dg_ent *e = &u6_dg_ents[i];
		if (e->kind == K_BYTES) {
			dg_spans[dg_nspans].p = (const uint8_t *)e->p;
			dg_spans[dg_nspans].n = e->len;
			dg_spans[dg_nspans++].off = e->off;
		} else if (e->kind == K_SBYTES) {
			dg_spans[dg_nspans].p = (const uint8_t *)e->ref->p;
			dg_spans[dg_nspans].n = e->ref->n < e->len ? e->ref->n : e->len;
			dg_spans[dg_nspans++].off = e->off;
		}
	}
}

/* a host pointer into the near data as its DOS offset; 0 if it is none of the table's */
static int dg_near_of(const void *p, uint16_t *off)
{
	const uint8_t *q = (const uint8_t *)p;
	unsigned i;
	for (i = 0; i < dg_nwstr; i++)
		if (q >= (const uint8_t *)dg_wstr[i].buf && q < (const uint8_t *)dg_wstr[i].buf + dg_wstr[i].n) {
			*off = (uint16_t)(dg_wstr[i].off + (q - (const uint8_t *)dg_wstr[i].buf));
			return 1;
		}
	for (i = 0; i < dg_nspans; i++)
		if (q >= dg_spans[i].p && q < dg_spans[i].p + dg_spans[i].n) {
			*off = (uint16_t)(dg_spans[i].off + (q - dg_spans[i].p));
			return 1;
		}
	return 0;
}

static void dg_put_near(uint8_t *d, const void *p)
{
	uint16_t off;
	if (!p)
		put16(d, 0);
	else if (dg_near_of(p, &off))
		put16(d, off);
	else
		put16(d, 0xFFFF);              /* a string literal or other host memory */
}

static void dg_put_far(uint8_t *d, const void *p)
{
	long l;
	uint16_t off;
	if (!p) {
		put16(d, 0);
		put16(d + 2, 0);
		return;
	}
	if (p == mcga_info_ptr() && GrDriver.p) {
		/* the tGr block: DOS's is in the graphics driver's segment at offset 52h */
		l = dos_linear(GrDriver.p) + 0x52;
		put16(d, (uint16_t)(0x52 + (dos_linear(GrDriver.p) & 15)));
		put16(d + 2, (uint16_t)(dos_linear(GrDriver.p) >> 4));
		return;
	}
	l = dos_linear(p);
	if (l >= 0) {                       /* the DOS arena: normalised, as FP_SEG and FP_OFF */
		put16(d, (uint16_t)(l & 15));
		put16(d + 2, (uint16_t)(l >> 4));
	} else if (dg_near_of(p, &off)) {   /* the near data: DGROUP, which is segment 0 here */
		put16(d, off);
		put16(d + 2, 0);
	} else {
		put16(d, 0xFFFF);
		put16(d + 2, 0xFFFF);
	}
}

static void dg_put_code(uint8_t *d, const void *f)
{
	unsigned i, t;
	for (i = 0; f && i < u6_dg_nfns; i++)
		if (u6_dg_fns[i].f == f) {
			put16(d, u6_dg_fns[i].off);
			put16(d + 2, u6_dg_fns[i].seg);
			return;
		}
	for (t = 0; f && u6_dg_sfns[t]; t++)
		for (i = 0; i < u6_dg_nsfns[t]; i++)
			if (u6_dg_sfns[t][i].f == f) {
				put16(d, u6_dg_sfns[t][i].off);
				put16(d + 2, u6_dg_sfns[t][i].seg);
				return;
			}
	put16(d, f ? 0xFFFF : 0);
	put16(d + 2, f ? 0xFFFF : 0);
}

const uint8_t *u6_rp_dgroup(void)
{
	unsigned i, k, n;
	dg_init();
	memset(dg_img, 0, sizeof dg_img);
	memcpy(dg_img, port_null_copy(0), 0x31);
	/* a writable literal: its text until the game has made the copy (DOS's is in place from the
	 * start), then the copy */
	for (i = 0; u6_dg_wstrs[i].init; i++)
		memcpy(dg_img + u6_dg_wstrs[i].off, u6_dg_wstrs[i].init, strlen(u6_dg_wstrs[i].init) + 1);
	for (i = 0; i < dg_nwstr; i++)
		memcpy(dg_img + dg_wstr[i].off, dg_wstr[i].buf, dg_wstr[i].n);
	for (i = 0; i < u6_dg_nents; i++) {
		const struct dg_ent *e = &u6_dg_ents[i];
		uint8_t *d = dg_img + e->off;
		switch (e->kind) {
		case K_BYTES:
			memcpy(d, e->p, e->len);
			break;
		case K_FAR:
			dg_put_far(d, *(void *const *)e->p);
			break;
		case K_NEAR:
			dg_put_near(d, *(void *const *)e->p);
			break;
		case K_CODE:
			dg_put_code(d, *(void *const *)e->p);
			break;
		case K_RAND: {
			uint32_t s = u6_rand_seed();
			put16(d, (uint16_t)s);
			put16(d + 2, (uint16_t)(s >> 16));
			break;
		}
		case K_SBYTES:
			memcpy(d, e->ref->p, e->ref->n < e->len ? e->ref->n : e->len);
			break;
		case K_HGR:
			dg_put_handler(d, *(void *const *)e->p, u6_gr_entry);
			break;
		case K_HMUS:
			dg_put_handler(d, *(void *const *)e->p, u6_mus_entry);
			break;
		case K_SNEAR:
		case K_SFAR:
			n = e->ref->n / sizeof(void *);
			for (k = 0; k < n && (k + 1) * (e->kind == K_SNEAR ? 2u : 4u) <= e->len; k++) {
				const void *p = ((void *const *)e->ref->p)[k];
				if (e->kind == K_SNEAR)
					dg_put_near(d + 2 * k, p);
				else
					dg_put_far(d + 4 * k, p);
			}
			break;
		}
	}
	return dg_img;
}

/* ---- the graphics driver's state, and the drivers' entry numbers ---- */

/* the entry number of the last call of each driver, which DOS keeps in GrDriver's and
 * MUS_Driver's offset word (gr.h's and u6port.h's port branches, port/sys/mouse.c set them) */
unsigned short u6_gr_entry, u6_mus_entry;

const uint8_t *mcga_drv_mem(void);
const void *mcga_info_ptr(void);
void mcga_dos_info(uint8_t *d, void (*put_far)(uint8_t *, const void *),
                   void (*put_code)(uint8_t *, const void *));

/* a union tHandler: the entry number and the driver's segment */
static void dg_put_handler(uint8_t *d, const void *base, unsigned short entry)
{
	long l = base ? dos_linear(base) : -1;
	put16(d, entry);
	put16(d + 2, l >= 0 ? (uint16_t)(l >> 4) : 0);
}

/* GDRV, rpgame.h's far heap section from FarHeap to the music driver: the arena's bytes, with
 * the graphics driver's segment as DOS's driver keeps it: its variables (mcga.c's drv[], at
 * their DOS offsets) and the tGr block in DOS's layout at CS:0052 */
static uint8_t *gdrv_buf;
static uint32_t gdrv_cap;
const uint8_t *u6_rp_gdrv(uint32_t a, uint32_t b)
{
	uint32_t n = b > a ? b - a : 0;
	long dl;
	if (n > gdrv_cap) {
		gdrv_buf = (uint8_t *)realloc(gdrv_buf, n);
		gdrv_cap = n;
	}
	if (!n)
		return gdrv_buf;
	memcpy(gdrv_buf, dos_base + a, n);
	dl = GrDriver.p ? dos_linear(GrDriver.p) : -1;
	if (dl >= (long)a && (uint32_t)dl < b) {
		uint32_t o = (uint32_t)dl - a, m = b - (uint32_t)dl;
		if (m > 0x10000u)
			m = 0x10000u;
		memcpy(gdrv_buf + o, mcga_drv_mem(), m);
		if (o + 0x52 + 0x34 <= n)
			mcga_dos_info(gdrv_buf + o + 0x52, dg_put_far, dg_put_code);
	}
	return gdrv_buf;
}
