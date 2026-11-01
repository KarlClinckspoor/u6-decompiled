/* Turbo C's kbhit() and getch() for U's C (port/u/conio.h), over the platform's keyboard
 * (plat.h: BIOS keyboard words, scan code high, ASCII low).
 *
 * Turbo C: kbhit is DOS function 0Bh (input status: AL FFh or 0, CBW: -1 when a character
 * is there), getch DOS function 07h (a character, no echo, no Ctrl-C check; AH = 0). DOS takes the BIOS word (int 16h AH=0) and gives its ASCII
 * byte; for a key without one (ASCII 0; E0h, the enhanced keyboard's grey keys, which int
 * 16h AH=0 turns into 0) it gives 0 and keeps the scan code for the next call: the next
 * getch returns it at once, and kbhit says a character is there.
 *
 * Both are poll points: the screen presented, the platform pumped, the timer polled. A
 * kbhit that finds nothing goes through GAME's busy-poll throttle (port/sys/timer.c:
 * after 200 empty polls in a row each sleeps 1 ms). getch waits for a key, polling every
 * millisecond. These names are not renamed: GAME.EXE's port has no
 * kbhit or getch. U's keys are read live (not recorded). */
#include "uosi.h"
#include "../../platform/plat.h"

static int pending = -1;   /* the scan code of an extended key, for the next getch */

/* a BIOS word from the platform's buffer, or 0 */
static uint16_t take_key(void)
{
	if (!plat_key_available())
		return 0;
	return plat_key_get();
}

static uint16_t ahead;   /* a word taken by kbhit, for getch */

int kbhit(void)
{
	uosi_pump();
	if (pending >= 0 || ahead) {
		osi_poll_idle(1);
		return -1;
	}
	ahead = take_key();
	osi_poll_idle(ahead != 0);
	return ahead != 0 ? -1 : 0;
}

int getch(void)
{
	uint16_t w;
	uint8_t al;

	if (pending >= 0) {
		w = (uint16_t)pending;
		pending = -1;
		return w;
	}
	for (;;) {
		if (ahead) {
			w = ahead;
			ahead = 0;
			break;
		}
		uosi_pump();
		w = take_key();
		if (w)
			break;
		plat_sleep_ms(1);
	}
	osi_poll_idle(1);
	al = (uint8_t)w;
	if (al == 0xE0 && (w >> 8) != 0)
		al = 0;
	if (al == 0) {
		pending = (uint8_t)(w >> 8);
		return 0;
	}
	return al;
}
