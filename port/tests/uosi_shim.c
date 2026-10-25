/* Test shim for port/u/osilib (U.EXE's OSI library in C), loaded by uosi_diff.py with
 * ctypes. Built into one shared library with the library (compiled with U's
 * force-includes), port/u/ugfx.c, the port/sys files it uses, port/gfx/vgadac.c,
 * port/replay/replay.c and osi_stubs.c (the platform: a settable clock, key queue,
 * mouse). This file is compiled as the port's own C: U's names are written U_*. */
#include "../sys/dosmem.h"
#include "../platform/plat.h"

#include <stdint.h>
#include <string.h>

extern void (*uosi_wait_hook)(int kind, uint64_t arg);
extern int16_t uosi_bios_mode;

/* the arena, with the far heap's first 128 KB taken (10000h..2FFFFh), so that the
 * library's allocations land at 30000h and above, where the test's emulated machine has
 * no code */
uint8_t *shim_init(void)
{
	static int done;

	dosmem_init();
	if (!done) {
		done = 1;
		dos_farmalloc(0x20000 - 8);
	}
	return dos_base;
}

/* the waits: each one calls back the test (kind: 1 VID_waitRetrace, 2 VID_inRetrace,
 * 3 a timer tick, 4 nanoseconds) instead of waiting */
static void (*wait_cb)(int kind, uint64_t arg);

static void hook(int kind, uint64_t arg)
{
	if (wait_cb)
		wait_cb(kind, arg);
}

void shim_set_wait_cb(void (*cb)(int kind, uint64_t arg))
{
	wait_cb = cb;
	uosi_wait_hook = cb ? hook : 0;
}

void shim_waits_real(void)
{
	wait_cb = 0;
	uosi_wait_hook = 0;
}

long shim_linear(const void *p)
{
	return dos_linear(p);
}

void *shim_mk_fp(uint16_t seg, uint16_t off)
{
	return dos_mk_fp(seg, off);
}

unsigned long shim_farcoreleft(void)
{
	return dos_farcoreleft();
}
