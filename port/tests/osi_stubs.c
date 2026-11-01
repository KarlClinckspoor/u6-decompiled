/* Test stand-ins for the platform layer and the graphics driver's mouse entries, for the
 * OSILIB tests (osilib_diff.py loads libosi_test.so with ctypes; test_dosfs links it).
 * The clock can be real or simulated (sleeping advances it), the keyboard is a queue, the
 * mouse a settable state, the speaker a log. */
#define _POSIX_C_SOURCE 200809L
#include "../platform/plat.h"
#include "../sys/osi.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int16_t SoundFlag = 1;

/* ---- clock ---- */
static int fake_clock;
static uint32_t fake_ms;
static uint32_t pump_count;
static uint32_t pump_advance_ms = 1;   /* simulated clock: each pump advances it this much */

void stub_set_fake_clock(int on, uint32_t ms) { fake_clock = on; fake_ms = ms; }
uint32_t stub_pump_count(void) { return pump_count; }
void stub_set_pump_advance(uint32_t ms) { pump_advance_ms = ms; }

static uint32_t real_ms(void)
{
	static int have;
	static struct timespec t0;
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	if (!have) {
		have = 1;
		t0 = t;
	}
	return (uint32_t)((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000);
}

uint32_t plat_ms(void) { return fake_clock ? fake_ms : real_ms(); }

static uint32_t sleep_total;
uint32_t stub_sleep_total(void) { return sleep_total; }

void plat_sleep_ms(uint32_t ms)
{
	sleep_total += ms;
	if (fake_clock) {
		fake_ms += ms ? ms : 1;
		return;
	}
	{
		struct timespec t;
		t.tv_sec = ms / 1000;
		t.tv_nsec = (long)(ms % 1000) * 1000000L;
		nanosleep(&t, 0);
	}
}

void plat_pump(void)
{
	pump_count++;
	if (fake_clock)
		fake_ms += pump_advance_ms;
}

/* ---- keyboard ---- */
static uint16_t keyq[256];
static unsigned kq_head, kq_tail;

void stub_key_push(uint16_t k) { keyq[kq_tail++ & 255] = k; }
void stub_key_clear(void) { kq_head = kq_tail = 0; }
int plat_key_available(void) { return kq_head != kq_tail; }
uint16_t plat_key_get(void) { return kq_head != kq_tail ? keyq[kq_head++ & 255] : 0; }
uint16_t plat_key_shift(void) { return 0; }

/* ---- mouse ---- */
static int mouse_present = 1;
static int16_t mx, my;
static uint16_t mb;

void stub_mouse_set(int present, int16_t x, int16_t y, uint16_t b) { mouse_present = present; mx = x; my = y; mb = b; }
int plat_mouse_present(void) { return mouse_present; }
void plat_mouse_state(int16_t *x, int16_t *y, uint16_t *buttons) { *x = mx; *y = my; *buttons = mb; }

/* ---- speaker ---- */
static uint32_t spk_log[65536];
static uint32_t spk_n;
void plat_speaker(uint32_t hz) { if (spk_n < 65536) spk_log[spk_n++] = hz; }
uint32_t stub_speaker_count(void) { return spk_n; }
uint32_t stub_speaker_at(uint32_t i) { return i < spk_n ? spk_log[i] : 0xFFFFFFFFu; }
void stub_speaker_clear(void) { spk_n = 0; }

/* ---- misc ---- */
static int quiet_log;
static uint32_t log_count;
void stub_quiet_log(int q) { quiet_log = q; }
uint32_t stub_log_count(void) { return log_count; }

void plat_log(const char *fmt, ...)
{
	va_list ap;
	log_count++;
	if (quiet_log)
		return;
	va_start(ap, fmt);
	fprintf(stderr, "[log] ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

jmp_buf *stub_fatal_jmp;
char stub_fatal_msg[256];
void plat_fatal(const char *message)
{
	snprintf(stub_fatal_msg, sizeof stub_fatal_msg, "%s", message);
	if (stub_fatal_jmp)
		longjmp(*stub_fatal_jmp, 1);
	fprintf(stderr, "fatal: %s\n", message);
	exit(99);
}

static int quit;
void stub_set_quit(int q) { quit = q; }
int plat_quit_requested(void) { return quit; }
void plat_shutdown(void) {}
int plat_init(const PlatConfig *cfg) { (void)cfg; return 0; }
void plat_apply_config(const PlatConfig *cfg) { (void)cfg; }
void plat_video_source(const uint8_t *pixels, const uint8_t *palette6) { (void)pixels; (void)palette6; }
void plat_video_dirty(void) {}
void plat_set_key_hook(plat_key_hook hook) { (void)hook; }
void plat_set_draw_hook(plat_draw_hook hook) { (void)hook; }

static char home_dir[1024] = ".", base_dir[1024] = ".";
void stub_set_dirs(const char *home, const char *base)
{
	snprintf(home_dir, sizeof home_dir, "%s", home);
	snprintf(base_dir, sizeof base_dir, "%s", base);
}
const char *plat_home_dir(void) { return home_dir; }
const char *plat_base_dir(void) { return base_dir; }
const char *plat_choose_folder(const char *title) { (void)title; return 0; }

/* ---- the graphics driver's mouse entries: a log of calls ---- */
static char grlog[8192];
static void grlogf(const char *fmt, ...)
{
	va_list ap;
	size_t l = strlen(grlog);
	if (l > sizeof grlog - 128)
		return;
	va_start(ap, fmt);
	vsnprintf(grlog + l, sizeof grlog - l, fmt, ap);
	va_end(ap);
}
const char *stub_grlog(void) { return grlog; }
void stub_grlog_clear(void) { grlog[0] = 0; }

void GRDRV_0C(const void *surface, void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	(void)surface; (void)saveBuf; (void)shapes;
	grlogf("0C(%d,%d,%u) ", posX, posY, shapeIndex);
}
void GRDRV_0F(const void *surface, const void *saveBuf, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	(void)surface; (void)saveBuf; (void)shapes;
	grlogf("0F(%d,%d,%u) ", posX, posY, shapeIndex);
}
void GRDRV_30(const void *surface, int16_t posX, int16_t posY, const void *shapes, uint16_t shapeIndex)
{
	(void)surface; (void)shapes;
	grlogf("30(%d,%d,%u) ", posX, posY, shapeIndex);
}
