/* TIMER.C: the timer tick (int 1Ch) with a callback, and the calibrated delay loop
 * (docs/port/U_OSILIB.md, "Time").
 *
 * DOS: TIM_install hooks int 1Ch; its handler calls timerHook (set by TIM_setCallback:
 * the moongate's light, the stars twinkling), counts timerTicks and chains to the old
 * handler, 18.2065 times a second (the PIT's 1193182 Hz / 65536).
 * Port: there are no interrupts. The ticks come from port/sys/timer.c's clock
 * (osi_ticks_live: tick n starts at n * 65536 / 1193182 s), and uosi_poll, called at every
 * poll point and wait of the library (MOUSE_SERVE, kbhit, getch, the retrace waits,
 * TIM_delay, the dissolves' and PAL_load's waits), runs the handler for the next tick
 * that has come since the last poll (one a poll, so that the count goes up one at a time
 * as U's exact waits need): the callback, then timerTicks++. After a long stall
 * (the window moved, a breakpoint) at most UOSI_MAX_CATCHUP ticks are run.
 *
 * TIM_calibrate counted the iterations of a busy loop during one tick (loopsPerTick); U
 * scales its animations with it (intro.c ScaleSpeed, over 1500..20000; Delay's loops; the
 * first frames of the bedroom: loopsPerTick / 400). The port gives it a fixed value,
 * uosi_loops_per_tick (20000, see uosi.h), after the same wait for the third tick as the
 * original, so that the program runs the same on every machine as on a fast one of 1990.
 * TIM_delay's loop is the same loop as the calibration's: n iterations last
 * n / loopsPerTick ticks, which the port waits in real time.
 */
#include "uosi.h"
#include "../../platform/plat.h"

#define TICK_NS 54925416u          /* 65536 / 1193182 s */
#define UOSI_MAX_CATCHUP 18u       /* a second of ticks */

int16 timerInstalled = 0;          /* D_49FC */
int32 timerTicks;                  /* D_682A */
void (*timerHook)(void);           /* D_682E */
void (*oldInt1C)(void);            /* D_6832 (no vector in the port) */
int32 loopsPerTick;                /* D_6836 */
int32 delayCount;                  /* D_683A */

int32 uosi_loops_per_tick = UOSI_LOOPS_PER_TICK;

static uint32_t polled;            /* the tick the handler last ran for */
static int in_poll;

/* C_1482_000E */
static void TIM_nop(void)
{
}

/* the previous int 1Ch handler: BIOS's is an IRET */
static void old_int1c(void)
{
}

/* C_1482_002A: one tick */
static void TIM_handler(void)
{
	timerHook();
	timerTicks++;
	oldInt1C();
}

void uosi_poll(void)
{
	uint32_t t;

	if (!timerInstalled || in_poll || uosi_wait_hook)
		return;
	t = osi_ticks_live();
	if ((uint32_t)(t - polled) > UOSI_MAX_CATCHUP)
		polled = t - UOSI_MAX_CATCHUP;
	/* one tick a poll: U waits for exact counts (TIM_calibrate: until timerTicks == 2),
	 * which in DOS go up one at a time; ticks that came together run at the next polls */
	in_poll = 1;
	if (polled != t) {
		polled++;
		TIM_handler();
	}
	in_poll = 0;
}

/* C_1482_000F */
int TIM_setCallback(void (*f)(void))
{
	timerHook = f;
	return 0;
}

/* C_1482_001F */
int TIM_resetCallback(void)
{
	timerHook = TIM_nop;
	return 0;
}

/* C_1482_0055: (again: only the count is reset) */
int TIM_install(void)
{
	if (timerInstalled == 0) {
		timerInstalled = 1;
		oldInt1C = old_int1c;
		timerHook = TIM_nop;
		polled = osi_ticks_live();
	}
	timerTicks = 0;
	return 0;
}

/* C_1482_009C */
int TIM_remove(void)
{
	if (timerInstalled)
		timerInstalled = 0;
	return 0;
}

/* ms (on the plat_ms clock) at which tick t starts */
static uint32_t tick_start_ms(uint32_t t)
{
	return (uint32_t)(((uint64_t)t * 65536000u + 1193182u - 1) / 1193182u);
}

/* until the next tick of the clock (whether or not the handler is installed) */
void uosi_wait_tick(void)
{
	uint32_t t, due, now;

	if (uosi_wait_hook) {
		uosi_wait_hook(UOSI_WAIT_TICK, 0);
		return;
	}
	uint32_t p0 = polled;
	int inst = timerInstalled && !in_poll;

	t = osi_ticks_live();
	due = tick_start_ms(t + 1);
	uosi_present();
	for (;;) {
		plat_pump();
		uosi_poll();
		/* installed: until the handler has run once more (exactly once: U waits for
		 * exact counts); else until the clock's next tick */
		if (inst ? polled != p0 : osi_ticks_live() != t)
			break;
		now = plat_ms();
		if ((int32_t)(due - now) > 1)
			plat_sleep_ms(due - now - 1);
	}
	osi_poll_idle(1);
}

void uosi_wait_ns(uint64_t ns)
{
	uint32_t start, now, left;
	uint64_t ms = (ns + 500000u) / 1000000u;

	if (uosi_wait_hook) {
		uosi_wait_hook(UOSI_WAIT_NS, ns);
		return;
	}
	start = plat_ms();
	uosi_present();
	for (;;) {
		plat_pump();
		uosi_poll();
		now = plat_ms();
		if ((uint64_t)(uint32_t)(now - start) >= ms)
			break;
		left = (uint32_t)(ms - (uint32_t)(now - start));
		/* not past the next tick, so that the callbacks run on time */
		if (timerInstalled) {
			uint32_t next = tick_start_ms(osi_ticks_live() + 1) - now;
			if ((int32_t)next > 0 && next < left)
				left = next;
		}
		if (left > 1)
			plat_sleep_ms(left - 1);
	}
	osi_poll_idle(1);
}

/* C_1482_00BE: TIM_install, the third tick waited for, TIM_remove (also when the timer
 * was installed before: then its callback ran during the wait, and it is removed). DOS
 * counted the loops during the third tick; the port sets uosi_loops_per_tick. */
int TIM_calibrate(void)
{
	loopsPerTick = 0;
	TIM_install();
	while (timerTicks != 2)
		uosi_wait_tick();
	while (timerTicks != 3)
		uosi_wait_tick();
	loopsPerTick = uosi_loops_per_tick;
	TIM_remove();
	return 0;
}

/* C_1482_0113: n = (ms / 55) * loopsPerTick iterations (16-bit signed division, 32-bit
 * product: a negative ms counts up to 2^32 + n, hours in DOS); they last
 * n / loopsPerTick ticks. */
int TIM_delay(int16 ms)
{
	uint32 n = (uint32)(int32)(ms / 55) * (uint32)loopsPerTick;
	uint64_t ns;

	delayCount = 0;
	if (n && loopsPerTick) {
		ns = (uint64_t)n * TICK_NS / (uint32)loopsPerTick;
		uosi_wait_ns(ns);
	}
	delayCount = (int32)n;
	return 0;
}
