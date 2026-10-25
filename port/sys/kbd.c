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
 * Port: plat_key_get() gives the BIOS words. The word KBD_GetKey ends up with (0 when the
 * buffer was empty) is the value read from the outside world: one rp_value(RP_KEY) per
 * call. Each call is a poll point: plat_pump() runs first so that the host's events are
 * in. Under replay the live keyboard is not read (keys pressed then stay for the overlay).
 */
#include "osi.h"
#include "../platform/plat.h"
#include "../replay/replay.h"

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
	uint16_t live, ax;

	plat_pump();
	live = osi_replaying() ? 0 : kbd_live((uint16_t)keepLastOnly != 0);
	ax = (uint16_t)rp_value(RP_KEY, live);
	osi_poll_idle(ax != 0);
	return kbd_translate(ax);
}

/* For tests. */
int osi_kbd_translate(uint16_t biosKey)
{
	return kbd_translate(biosKey);
}
