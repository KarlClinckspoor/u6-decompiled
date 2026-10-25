/* The platform layer: everything the port needs from the host.
 *
 *   SRC/ (the game's C, shared with the DOS build)
 *     |  calls the DOS-side API: OSI_*, MOUSE_*, GR_*, decompress, ...
 *   port/sys, port/gfx (C written for the port, replacing the assembly and the drivers)
 *     |  plat_* calls only; no OS or SDL headers
 *   plat.h
 *     |
 *   backend: platform/sdl2/plat_sdl2.c (window, input, audio) or platform/headless.c
 *
 * Threads: the game runs on the main thread. It never blocks inside the backend: every
 * place the DOS game waited (delays, retrace, polling the keyboard) calls plat_pump(),
 * which handles the host's events, runs the overlay and presents the screen. So the order
 * of everything the game reads is fixed by the game's own code, which makes record and
 * replay exact (port/replay).
 */
#ifndef PORT_PLAT_H
#define PORT_PLAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- System ---- */

typedef struct PlatConfig {
	int scale;          /* window size as a multiple of 320x200 (1..8) */
	int fullscreen;
	int aspect;         /* 1: show the 200 lines as 240 (4:3) */
	int integer_scale;  /* 1: whole multiples only */
	int hidden;         /* no window: replays and tests */
	int audio;          /* 0: no audio device */
	int volume;         /* 0..100 */
	const char *title;
} PlatConfig;

/* 0 on success. With cfg->hidden the headless backend is used whatever was linked. */
int plat_init(const PlatConfig *cfg);
void plat_shutdown(void);
/* Applies display settings (scale, fullscreen, aspect, integer scaling) at once. */
void plat_apply_config(const PlatConfig *cfg);
/* Shows the message (a message box when there is a window) and ends the process. */
void plat_fatal(const char *message);
void plat_log(const char *fmt, ...);
/* 1 once the user asked to close the window. */
int plat_quit_requested(void);

/* ---- Time ---- */

/* Milliseconds since plat_init, from a monotonic clock. */
uint32_t plat_ms(void);
/* Milliseconds since start, counting the pauses too. */
uint32_t plat_real_ms(void);
void plat_sleep_ms(uint32_t ms);
/* Handles window and input events, lets the overlay run, presents the screen when it
 * changed. Called from every wait and poll of the game. */
void plat_pump(void);
/* While paused (the overlay is open), plat_ms() stands still, so the game's clock does
 * not run on behind the overlay. */
void plat_clock_pause(int paused);

/* ---- Video: 320x200, one byte a pixel ---- */

#define PLAT_W 320
#define PLAT_H 200

/* The frame to show: 320x200 pixels and 256 palette entries as 6-bit VGA r,g,b. The
 * backend keeps the pointers and shows what they hold at each present. */
void plat_video_source(const uint8_t *pixels, const uint8_t *palette6);
/* The game's screen changed: present at the next pump (at most once per host frame). */
void plat_video_dirty(void);
/* What the next present would show (the frame with the overlay drawn): screenshots. */
int plat_video_capture(uint8_t *frame, uint8_t *palette6);

/* ---- Keyboard ----
 * Keys are delivered as the BIOS keyboard buffer held them (int 16h): scan code in the
 * high byte, ASCII (or 0) in the low byte. */
int plat_key_available(void);
uint16_t plat_key_get(void);      /* 0 when none */
uint16_t plat_key_shift(void);
/* Puts a key in the buffer as if typed (tests: --keys). */
void plat_push_key(uint16_t bios_key);    /* BIOS shift flags: 1 right shift, 2 left shift, 4 ctrl, 8 alt */

/* ---- Mouse: position in the game's 320x200 pixels ---- */

int plat_mouse_present(void);
void plat_mouse_state(int16_t *x, int16_t *y, uint16_t *buttons); /* buttons: 1 left, 2 right */

/* ---- Sound: the PC speaker ---- */

/* Square wave at hz from now on; 0 silences it. Timed by the host's audio clock. */
void plat_speaker(uint32_t hz);

/* ---- Hooks the port provides to the backend ---- */

/* Called by plat_pump for each key before it goes to the game; returns 1 to swallow it
 * (the overlay's F11, its keys while open). sym is a PLAT_K_* code or ASCII. */
#define PLAT_K_F11 0x5700
typedef int (*plat_key_hook)(uint16_t bios_key, int down);
void plat_set_key_hook(plat_key_hook hook);
/* Called before each present with the 320x200 frame to draw over (the overlay). It may
 * draw into frame; palette is the game's. */
typedef void (*plat_draw_hook)(uint8_t *frame, uint8_t *palette6);
void plat_set_draw_hook(plat_draw_hook hook);
/* Called at the end of every plat_pump (not re-entered): the overlay runs its modal loop
 * from here, calling plat_pump itself, which pauses the game. */
typedef void (*plat_pump_hook)(void);
void plat_set_pump_hook(plat_pump_hook hook);
/* The mouse as the host has it now (the overlay, which bypasses the game's replayed input). */
void plat_mouse_raw(int16_t *x, int16_t *y, uint16_t *buttons);

/* ---- Files ---- */

/* The directory the port keeps its settings, saved games and recordings in (created). */
const char *plat_home_dir(void);
/* The folder the program's executable is in. */
const char *plat_base_dir(void);
/* Lets the user pick a folder; returns a static path or 0. */
const char *plat_choose_folder(const char *title);

#ifdef __cplusplus
}
#endif

#endif
