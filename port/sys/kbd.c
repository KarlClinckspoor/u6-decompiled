/* KBD.ASM: KBD_GetKey.
 *
 * DOS: int 16h AH=1 tests the BIOS keyboard buffer, AH=0 takes a key (scan code in AH,
 * ASCII in AL). keepLastOnly != 0 takes every key in the buffer and keeps the last one
 * (key repeat does not pile up), else at most one key is taken. The word is then turned
 * into the game's key code:
 *   AL = 0, AH = 0              -> 0 (no key)
 *   AL = 0 (extended key)       -> 0x100 + scan code
 *   scan 47h..51h (keypad), except 4Ah (-) and 4Eh (+), and 0Eh (backspace)
 *                               -> 0x100 + scan code (keypad digits become directions)
 *   else                        -> the ASCII code
 *
 * Port: plat_key_get() gives the BIOS words. Each call is a poll point: plat_pump() runs
 * first so that the host's events are in. The game calls it through KEY (Exhume's rp_key),
 * which records its result, and under replay gives the recorded one without calling it.
 */
#include "osi.h"
#include "../platform/plat.h"
#include <stdlib.h>

/* --feed SCRIPT (tests): tools/keyfeed.py's KEYFEED.COM (mode bios) in the port. SCRIPT is
 * "GAP:AX GAP:AX ..." (decimal, hex). A poll (int 16h function 1, here a look at the buffer)
 * says "no key" and counts until it has been asked GAP times since the last key was read, then
 * offers the key, which the next read takes. A session typed this way is the DOS run's, poll
 * for poll; with the script used up the keyboard is the host's. */
static struct { uint16_t gap, ax; } *feed;
static int feed_n, feed_i, feed_cnt;

void kbd_set_feed(const char *script)
{
	const char *p = script;
	int cap = 0;
	while (p && *p) {
		char *e;
		unsigned long g, ax;
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		g = strtoul(p, &e, 10);
		if (*e != ':')
			break;
		ax = strtoul(e + 1, &e, 16);
		if (feed_n == cap) {
			cap = cap ? cap * 2 : 64;
			feed = realloc(feed, (size_t)cap * sizeof *feed);
		}
		feed[feed_n].gap = (uint16_t)g;
		feed[feed_n++].ax = (uint16_t)ax;
		p = e;
	}
}

/* a poll: is a key there? (the feeder's rule while it has keys, else the host's buffer) */
static int key_poll(void)
{
	if (feed_i < feed_n) {
		if (feed_cnt >= feed[feed_i].gap)
			return 1;
		feed_cnt++;
		return 0;
	}
	return plat_key_available();
}

static uint16_t key_read(void)
{
	if (feed_i < feed_n) {
		feed_cnt = 0;
		return feed[feed_i++].ax;
	}
	return plat_key_get();
}

static uint16_t kbd_live(int keepLastOnly)
{
	uint16_t ax = 0;

	if (feed) {
		if (keepLastOnly) {
			while (key_poll())
				ax = key_read();
		} else if (key_poll()) {
			ax = key_read();
		}
		return ax;
	}
	if (keepLastOnly) {
		while (plat_key_available())
			ax = plat_key_get();
	} else if (plat_key_available()) {
		ax = plat_key_get();
	}
	return ax;
}

/* the translation (C_0036..C_0067) */
static int kbd_translate(uint16_t ax)
{
	uint8_t al = (uint8_t)ax, ah = (uint8_t)(ax >> 8);

	if (al == 0) {
		if (ah == 0)
			return 0;
		return 0x100 | ah;
	}
	if (ah >= 0x47 && ah <= 0x51) {
		if (ah == 0x4A || ah == 0x4E)
			return al;
		return 0x100 | ah;
	}
	if (ah == 0x0E)
		return 0x100 | ah;
	return al;
}

int KBD_GetKey(int keepLastOnly)
{
	uint16_t ax;

	plat_pump();
	ax = osi_replaying() ? 0 : kbd_live((uint16_t)keepLastOnly != 0);
	osi_poll_idle(ax != 0);
	return kbd_translate(ax);
}

/* For tests. */
int osi_kbd_translate(uint16_t biosKey)
{
	return kbd_translate(biosKey);
}
