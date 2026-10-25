/* Helpers for osilib_diff.py (ctypes): batch calls into port/sys so that the Python side
 * only compares arrays. Built into libosi_test.so with the port/sys files, osi_stubs.c,
 * port/gfx/vgadac.c and port/replay/replay.c (see run_osilib_tests.sh). */
#include "../sys/osi.h"
#include "../sys/dosfs.h"
#include "../sys/dosmem.h"
#include "../replay/replay.h"

#include <string.h>

/* ---- RAND: per call the result and the state words after it ---- */
void t_rand_batch(const uint16_t *ab, int n, uint16_t *out)
{
	int i;
	for (i = 0; i < n; i++) {
		uint16_t lo, hi;
		out[3 * i] = (uint16_t)OSI_rand((int16_t)ab[2 * i], (int16_t)ab[2 * i + 1]);
		osi_rand_state(&lo, &hi);
		out[3 * i + 1] = lo;
		out[3 * i + 2] = hi;
	}
}

uint16_t t_seed_from_time(int h, int m, int s, int hs)
{
	return osi_seed_from_time((uint8_t)h, (uint8_t)m, (uint8_t)s, (uint8_t)hs);
}

/* ---- decompress: unpack the file at offset to destSeg:destOff with workSeg ---- */
uint8_t *t_arena(void)
{
	dosmem_init();
	return dos_base;
}

int t_lzw(const char *dosName, uint32_t offset, uint16_t destSeg, uint16_t destOff, uint16_t workSeg,
          uint32_t *posOut)
{
	int16_t h;
	int r;

	dosmem_init();
	if (dosfs_open(dosName, 2, &h) != 0)
		return -1000;
	r = decompress(h, offset, dos_mk_fp(destSeg, destOff), workSeg);
	dosfs_seek(h, 1, 0, posOut);
	dosfs_close(h);
	return r;
}

/* ---- sound: a trace of the speaker's programming ---- */
static uint32_t trace[1 << 20];
static uint32_t ntrace;

static void tracer(int what, uint16_t value)
{
	if (ntrace < sizeof trace / sizeof trace[0])
		trace[ntrace++] = ((uint32_t)what << 16) | value;
}

void t_trace_start(void)
{
	ntrace = 0;
	osi_sound_trace = tracer;
	osi_loop_count = 0;
}

uint32_t t_trace_count(void) { return ntrace; }
const uint32_t *t_trace(void) { return trace; }
uint64_t t_loops(void) { return osi_loop_count; }
void t_set_cpu_speed(uint16_t s) { CpuSpeed = s; }
void t_set_rp_mode(int m) { rp_mode = m; }
