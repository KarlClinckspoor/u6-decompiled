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

/* port/sys/portmain.c sets it from --dos-replay: the recording, read as replay.c's REPLAY.IN */
const char *u6_rp_replay_path;
const char *session_home(void);
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

/* main.c's rp_u6_shutdown (the replay DOS build's, -DREPLAY): the temporary object blocks
 * deleted from SAVEGAME, which the runs compare; the music, the timer and the video mode are
 * the port's exit's */
void u6_port_rp_shutdown(void)
{
	OBJ_DeleteTmpBlocks();
}

static void home_path(char *out, size_t n, const char *name)
{
	const char *h = session_home();
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

/* RECORDED for the port's own C (port/sys/mouse.c), where the port's older recorder's
 * rp_value macro (port/replay/replay.h) takes the name */
uint32_t u6_rp_recorded(int id, uint32_t v)
{
	return rp_value(id, v);
}

/* replay.c's mode, which the port's main chooses before the game starts (replay.c: RP_OFF 0,
 * RP_REPLAY 2): off unless --dos-replay names a recording made by the replay DOS build */
extern int16 rp_request;
void u6_rp_set_off(void)
{
	rp_request = 0;
}
void u6_rp_set_dos_replay(const char *path)
{
	u6_rp_replay_path = path;
	rp_request = 2;
}

/* portable.h's FAR_COPY, which replay.c's dump_far copies through: Exhume's runtime resolves
 * its paragraph map there; the port's far pointers are host pointers already */
void port_far_copy(void *dst, const void *src, unsigned n)
{
	memcpy(dst, src, n);
}
