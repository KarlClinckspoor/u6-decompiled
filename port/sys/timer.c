/* Time: the timer tick counter (DELAY.ASM's int 1Ch handler), OSI_delay, and the busy
 * delay loops of DELAY2.ASM and the sound routines, timed in real time.
 *
 * DOS: the PIT's channel 0 interrupts 1193182/65536 = 18.2065 times a second (int 8,
 * which calls int 1Ch). OSI_delay_init hooks int 1Ch with a handler that increments the
 * word D_305A; OSI_delay(n) spins until D_305A >= n (unsigned) and then sets it to 0. So
 * it waits for n ticks counted from the end of the previous OSI_delay (a frame limiter),
 * and returns at once if they have already passed. OSI_delay_clean unhooks the handler,
 * which freezes the counter.
 *
 * Port: the tick count is derived from plat_ms() (tick n starts at n*65536/1193182 s
 * after plat_init): OSI_delay remembers the tick at which it last returned, and each pass
 * of its loop reads the counter once. A pass that has to wait pumps the platform and
 * sleeps until the next tick is due. The count is not recorded: nothing the game decides
 * on reads it (the replay DOS build's DELAYRP.ASM keeps it outside DGROUP, and DOS's
 * recordings replay without it), so under replay (Exhume's replay.c, osi_set_replay) it is
 * a virtual clock that each wait advances by one tick, paced to the replay's speed.
 *
 * The busy loops (DELAY_CXAX in SOUND.INC) count CX*AX iterations of "dec word; jnz",
 * calibrated by CpuSpeed: OSI_lowInit counted N iterations of a loop of the same cost
 * during one tick and set CpuSpeed = N*18/750, so an iteration lasts
 * 1 tick * 18 / (750 * CpuSpeed). osi_spin_ns waits that long in real time. These loops
 * read nothing the game decides on, so they are not recorded; under replay they return
 * at once.
 */
#include "osi.h"
#include "dosint.h"
#include "../platform/plat.h"

#define PIT_HZ 1193182u
#define TICK_NS 54925416u          /* 65536 / 1193182 s, in ns */

/* ---- the tick counter ---- */

static uint32_t raw_ticks(void)
{
	return (uint32_t)(((uint64_t)plat_ms() * PIT_HZ) / 65536000u);
}

/* ms (on the plat_ms clock) at which tick t starts */
static uint32_t tick_start_ms(uint32_t t)
{
	return (uint32_t)(((uint64_t)t * 65536000u + PIT_HZ - 1) / PIT_HZ);
}

/* Live, the game's tick count is the host's plus tick_offset, which is 0 until the player
 * takes over a replay: from then on the count goes on from the replay's virtual one instead
 * of jumping (OSI_delay would wait up to an hour otherwise). */
static uint32_t tick_offset;

/* Under replay (osi_set_replay: --replay, --dos-replay) Exhume's replay.c gives every outside
 * value the game decides on, and the tick count is none of them, so it is a virtual clock that
 * advances a tick each time OSI_delay waits for one. A replay reads no live input and the busy
 * loops return at once. At speed 0 it runs as fast as the host can; at speed N each virtual
 * tick is due N times as fast as a real one, counted from when the speed was last set. */
static int replaying;
static uint32_t virt_ticks;
static int replay_speed;
static uint32_t pace_tick, pace_ms;

void osi_set_replay(int on)
{
	if (replaying && !on)
		tick_offset = virt_ticks - raw_ticks();	/* the player took over */
	replaying = on;
}

void osi_set_replay_speed(int speed)
{
	replay_speed = speed < 0 ? 0 : speed;
	pace_tick = virt_ticks;
	pace_ms = plat_ms();
}

int osi_replay_speed(void)
{
	return replay_speed;
}

int osi_replaying(void)
{
	return replaying;
}

uint32_t osi_ticks_live(void)
{
	return raw_ticks() + tick_offset;
}

uint32_t osi_ticks(void)
{
	return replaying ? virt_ticks : raw_ticks() + tick_offset;
}

/* a paced replay: wait (pumping) until virtual tick t is due at the replay's speed */
static void pace_to(uint32_t t)
{
	for (;;) {
		uint32_t now = plat_ms(), due;
		if (!replaying || replay_speed == 0 || plat_quit_requested())
			return;
		due = pace_ms + tick_start_ms(t - pace_tick) / (uint32_t)replay_speed;
		if ((int32_t)(due - now) <= 0)
			return;
		plat_sleep_ms(due - now > 10 ? 10 : due - now);
		plat_pump();
	}
}

static uint32_t last_pump_ms = 0xFFFFFFFFu;

/* plat_pump at most once per host millisecond (busy loops call this very often) */
static void pump_rate_limited(void)
{
	uint32_t now = plat_ms();
	if (now != last_pump_ms) {
		last_pump_ms = now;
		plat_pump();
	}
}

void osi_wait_tick(uint32_t seen)
{
	uint32_t now, next;

	plat_pump();
	if (replaying) {
		virt_ticks = seen + 1;
		pace_to(virt_ticks);
		return;
	}
	now = plat_ms();
	next = tick_start_ms(seen + 1 - tick_offset);
	/* sleep until shortly before the next tick (none if it came during the pump); the
	 * last millisecond is a busy wait */
	if (next > now + 1)
		plat_sleep_ms(next - now - 1);
}

/* ---- DELAY.ASM ---- */

static int delay_installed;
static uint32_t delay_base;     /* tick at which D_305A was last 0 */
static uint16_t delay_frozen;   /* D_305A once the handler is removed */

void OSI_delay_init(void)
{
	delay_base = osi_ticks();
	delay_frozen = 0;
	delay_installed = 1;
}

void OSI_delay_clean(void)
{
	if (delay_installed)
		delay_frozen = (uint16_t)(osi_ticks() - delay_base);
	delay_installed = 0;
}

void OSI_delay(int ticks)
{
	uint16_t want = (uint16_t)ticks;
	uint32_t t;

	if (!delay_installed) {
		/* nothing increments D_305A: the DOS game hangs here when it is too small */
		if (delay_frozen >= want) {
			delay_frozen = 0;
			return;
		}
		plat_log("OSI_delay(%u) without the timer handler: the DOS game hangs here", want);
		for (;;) {
			plat_pump();
			if (plat_quit_requested())
				dos_exit(0);
			if (!osi_replaying())
				plat_sleep_ms(20);
		}
	}
	for (;;) {
		t = osi_ticks();
		if ((uint16_t)(t - delay_base) >= want)
			break;
		osi_wait_tick(t);
	}
	delay_base = t;
	osi_poll_idle(1);
}

/* ---- busy loops ---- */

uint16_t osi_cpu_speed = 1000;

uint64_t osi_loop_count;

uint64_t osi_loop_ns(uint64_t iterations)
{
	uint64_t div = 750u * (uint64_t)(CpuSpeed ? CpuSpeed : 1);
	return (iterations / div) * (TICK_NS * 18ull) + (iterations % div) * (TICK_NS * 18ull) / div;
}

void osi_spin_loops(uint64_t iterations)
{
	osi_loop_count += iterations;
	osi_spin_ns(osi_loop_ns(iterations));
}

/* The real time the original's busy loops have reached, on the plat_ms clock, in ns. A
 * run of short delays (a noise, a waved note: steps of 50..100 us) accumulates here and
 * is waited for when it is ahead of the clock; after a pause the clock starts again from
 * now. */
static uint64_t spin_clock_ns;

#define SPIN_RESYNC_NS 20000000u   /* behind by more than 20 ms: start again from now */

void osi_spin_ns(uint64_t ns)
{
	uint64_t now;

	if (ns == 0 || osi_replaying())
		return;
	now = (uint64_t)plat_ms() * 1000000u;
	if (spin_clock_ns + SPIN_RESYNC_NS < now)
		spin_clock_ns = now;
	spin_clock_ns += ns;
	for (;;) {
		now = (uint64_t)plat_ms() * 1000000u;
		if (now >= spin_clock_ns)
			break;
		if (spin_clock_ns - now > 3000000u)
			plat_sleep_ms((uint32_t)((spin_clock_ns - now) / 1000000u) - 2);
		pump_rate_limited();
	}
	osi_poll_idle(1);
}

/* DELAY_CXAX: CX outer by AX inner iterations, 0 standing for 65536 */
uint64_t osi_cxax_loops(uint16_t cx, uint16_t ax)
{
	return (uint64_t)(cx ? cx : 0x10000u) * (ax ? ax : 0x10000u);
}

/* D_3066: the shift table of OSI_sDelay, indexed in bytes (the index is used as a byte
 * offset into a table of words, so odd indexes read across two entries). */
static const uint8_t sdelay_table[34] = {
	0, 0, 0, 0, 1, 0, 1, 0, 2, 0, 2, 0, 2, 0, 3, 0, 3, 0,
	3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 4, 0, 4, 0, 4, 0
};

void OSI_sDelay(int speedIndex, int count)
{
	uint16_t ax = CpuSpeed, cx;
	uint16_t idx = (uint16_t)speedIndex;

	if (idx < sizeof sdelay_table - 1) {
		cx = (uint16_t)(sdelay_table[idx] | (sdelay_table[idx + 1] << 8));
	} else {
		plat_log("OSI_sDelay: index %u past the table", idx);
		cx = 0;
	}
	if (cx)
		ax = (uint16_t)(ax >> ((cx & 0xFF) & 31));
	osi_spin_loops(osi_cxax_loops((uint16_t)count, ax));
}

/* ---- busy polling ----
 * Some loops of the game poll the keyboard without waiting (main.c: while(!KBD_GetKey(1))).
 * DOS spun at full speed; the port sleeps 1 ms per poll once 200 polls in a row found
 * nothing and nothing waited in between. That only stretches time the game spends
 * spinning anyway. */
static unsigned idle_polls;

void osi_poll_idle(int gotSomething)
{
	if (gotSomething) {
		idle_polls = 0;
		return;
	}
	if (++idle_polls >= 200 && !osi_replaying()) {
		idle_polls = 200;
		plat_sleep_ms(1);
	}
}
