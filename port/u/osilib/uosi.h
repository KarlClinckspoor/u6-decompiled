/* U.EXE's OSI library (SRC_U/OSILIB) in C: the declarations shared by its files.
 *
 * The files of port/u/osilib are compiled like U's own C (SRC_U/*.c): -std=gnu89
 * -fsigned-char -fwrapv, force-included port/include/compat.h then port/u/uport.h, whose
 * urename.h turns every public name of U, the library's included, into U_<name>. So the
 * functions below are written with their original names (TXT_clear, MOUSE_SERVE, ...)
 * and are U_TXT_clear, U_MOUSE_SERVE, ... in the executable; they never collide with
 * GAME.EXE's routines of the same name (port/sys: OSI_open, MOUSE_INIT, ...). The port's
 * own helpers are named uosi_* (not renamed).
 *
 * Types are U's (SRC_U/u.h: struct tSurface, tText, tMouse as the host compiles them:
 * far pointers are host pointers, every struct packed) and the explicit widths of
 * SRC_U/portable.h, so that the arithmetic wraps as in DOS. A routine U's C sees as
 * returning int (implicit int, no prototype) returns int.
 *
 * What each module does, in DOS and here: docs/port/U_OSILIB.md.
 */
#ifndef UOSI_H
#define UOSI_H

#include <stdint.h>
#include "../../../SRC_U/u.h"   /* struct tSurface, tText, tMouse; DRV(o) is UDRV_o */
#include "../ugfx.h"            /* the driver's entries UDRV_3, 6, 9, 0Ch, 0Fh, 12h */

/* ==== SURFACE.C, SURFSHR.C ==== */
int SURF_alloc(struct tSurface *s);
int SURF_free(struct tSurface *s);
int SURF_share(struct tSurface *src, ...);

/* ==== TXTPORT, TXTCLR, CLREOL, GOTOXY, TXTJUST, TXTCHAR, TXTSCRL, TXTPRINT ==== */
extern struct tText *pText;
int TXT_select(struct tText *t);
int TXT_clear(void);
int TXT_clearEos(void);
void TXT_clrEos(void);
int clreol(void);
int gotoxy(int16 x, int16 y);
int TXT_puts(char *s);
void TXT_putsES(char *s);
void TXT_lineLeft(void);
void TXT_lineRight(void);
void TXT_lineCenter(void);
void TXT_lineFull(void);
int TXT_putc(int16 c);
int TXT_bufc(int16 c);
void TXT_bufcAL(int16 c);
void TXT_putcAL(int16 c);
void TXT_cr(void);
void TXT_newLine(void);
int TXT_scrollUp(int16 n);
int TXT_scrollDown(int16 n);
void TXT_scrollUp1(void);
void TXT_scrollDown1(void);
void TXT_clearLine(int16 x, int16 y);
int TXT_printf(char *fmt, ...);
int TXT_sprintf(char *fmt, ...);
int TXT_sappend(char *fmt, ...);

/* ==== DRAW.ASM ==== */
int DRW_line(struct tSurface *s, int16 x1, int16 y1, int16 x2, int16 y2, int16 color);
int DRW_lineFunc(void (*plot)(struct tSurface *, int16, int16, int16), struct tSurface *s,
	int16 x1, int16 y1, int16 x2, int16 y2, int16 color);
int DRW_getPixel(struct tSurface *s, int16 x, int16 y);

/* ==== MOUSEINI, MOUSESRV, MOUSECUR, MOUSEHIT ==== */
extern struct tMouse *pMouse;
extern int16 nMouseSem;
/* MOUSECUR.ASM's driver table (5 far pointers: draw pointer, save, restore, put char, fill
 * rectangle): main.c writes it as 10 words; the port calls the driver's C directly and
 * only keeps the words. */
extern int16 drvDrawPointer[10];
int MOUSE_INIT(struct tMouse *m);
int MOUSE_NOINIT(struct tMouse *m);
void MOUSE_NOP(void);
int MOUSE_POINTER(void *shapes, int16 index);
int MOUSE_SERVE(void);
int MOUSE_RESET(void);
int MOUSE_SHOW(void);
int MOUSE_HIDE(void);
int MOUSE_DRAW(void);
int MOUSE_SAVE_BG(void);
int MOUSE_RESTORE_BG(void);
int MOUSE_IN_AREA(struct tSurface *s);
int MOUSE_IN_RECT(struct tSurface *s);
int SHAPE_IN_RECT(struct tSurface *s, void *shapes, int16 index, int16 x, int16 y);

/* ==== VMODE, VSYNC, PALETTE ==== */
int VID_getMode(void);
int VID_setMode(int16 mode);
int VID_inRetrace(void);
int VID_waitRetrace(void);
int VID_mouseRetrace(void);
int VID_retrace(void);
int PAL_load(char *name);
int PAL_set(int16 index, int16 *rgb);
int PAL_get(int16 index, int16 *rgb);

/* ==== FADE.C ==== */
int FADE_step(int16 *cur, int16 *target, int16 n);
int FADE_in(struct tSurface *src, struct tSurface *dst);
int FADE_out(struct tSurface *s, int16 color);

/* ==== DISSOLVE.ASM ==== */
int DIS_dissolve(struct tSurface *from, struct tSurface *to, int16 speed);
int DIS_dissolveEGA(struct tSurface *from, struct tSurface *to, int16 speed);
int DIS_dissolveCGA(struct tSurface *from, struct tSurface *to, int16 speed);
int DIS_dissolveTDY(struct tSurface *from, struct tSurface *to, int16 speed);
int16 DIS_farcpy(void *from, void *to, int16 n);

/* ==== LZW.ASM (+ LZ.INC), LOADNEAR, LOADFAR ==== */
void *LZW_load(char *name);
char *LOAD_near(char *name);
char *LOAD_far(char *name);
int LOAD_to(char *name, void *buf);

/* ==== OSI_FILE.ASM (pascal in DOS) ==== */
int16 OSI_open(char *name);
int16 OSI_close(int16 fd);
int16 OSI_create(char *name);
int OSI_delete(char *name);
int32 OSI_read(int16 fd, int32 offset, int32 length, void *buf);
int32 OSI_write(int16 fd, int32 offset, int32 length, void *buf);
int OSI_rename(char *oldName, char *newName);
int OSI_copy(char *src, char *dst);
uint32 OSI_fileSize(int16 fd);
uint32 OSI_memory(void);
int OSI_callback(void);

/* ==== TIMER.C ==== */
extern int16 timerInstalled;
extern int32 timerTicks;
extern void (*timerHook)(void);
extern int32 loopsPerTick;
extern int32 delayCount;
int TIM_setCallback(void (*f)(void));
int TIM_resetCallback(void);
int TIM_install(void);
int TIM_remove(void);
int TIM_calibrate(void);
int TIM_delay(int16 ms);

/* ==== Turbo C's conio (port/u/conio.h; not renamed) ==== */
int kbhit(void);
int getch(void);

/* ==== the port's side ==== */

/* What TIM_calibrate gives loopsPerTick (timer.c): 20000, the top of the range ScaleSpeed
 * scales over (intro.c: 1500..20000), what a 386/16 or a 286/20 counted. */
#define UOSI_LOOPS_PER_TICK 20000
extern int32 uosi_loops_per_tick;

/* The timer's ticks due since the last poll: their callbacks run (timerHook, timerTicks).
 * Called by every poll point and wait of the library (MOUSE_SERVE, kbhit, getch, the
 * retrace waits, TIM_delay, the dissolves' waits). */
void uosi_poll(void);
/* A poll point: the screen (A000:0000 with the DAC) is presented, the platform pumped,
 * the timer polled. */
void uosi_pump(void);
/* The screen is A000:0000 in the DOS arena, with the DAC of port/gfx/vgadac.h. */
void uosi_present(void);
/* The BIOS video mode the library sees (0040:0049): 13h until VID_setMode changes it. */
extern int16 uosi_bios_mode;

/* Waits for real time: the vertical retrace (70.086 Hz), the next timer tick, a number of
 * nanoseconds; each pumps the platform and polls the timer while it waits. */
void uosi_wait_frame(void);
void uosi_wait_tick(void);
void uosi_wait_ns(uint64_t ns);

/* Tests: when set, the waits do not wait; the hook is called instead (kind: one of
 * UOSI_WAIT_*), and the timer is not polled. */
enum { UOSI_WAIT_RETRACE = 1, UOSI_WAIT_INRETRACE, UOSI_WAIT_TICK, UOSI_WAIT_NS };
extern void (*uosi_wait_hook)(int kind, uint64_t arg);

/* From port/sys (timer.c, GAME's): the 18.2065 Hz tick count, live (not recorded), and the
 * busy-poll throttle. U's reads are not recorded (U runs before a session's recording;
 * see docs/port/U_OSILIB.md). */
uint32_t osi_ticks_live(void);
void osi_poll_idle(int gotSomething);

/* Turbo C's divide error (dosint.c). */
void dos_divide_error(const char *where);

#endif
