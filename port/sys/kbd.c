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

static uint16_t kbd_live(int keepLastOnly)
{
	uint16_t ax = 0;

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
