/* replaces nothing: a digest of the game's state for the replay checkpoints.
 * Compiled like the game's own sources (compat.h, u6.h): it reads the game's globals. It
 * covers the world and the party (objects, NPCs, the clock, the map position) and the
 * random generator, which is what a replay that went another way changes first. */
#include "u6.h"

extern uint32_t port_rand_state(void);

static uint64_t h;

static void mix(const void *p, size_t n)
{
	const uint8_t *b = (const uint8_t *)p;
	size_t i;
	for (i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3ull;	/* FNV-1a 64 */
	}
}

uint64_t port_game_digest(void)
{
	h = 0xcbf29ce484222325ull;
	mix(ObjPos, 3 * 0xC00);
	mix(ObjShapeType, 2 * 0xC00);
	mix(ObjStatus, 0xC00);
	mix(Amount, 2 * 0xC00);
	mix(Link, 2 * 0xC00);
	mix(NPCStatus, 0x100);
	mix(HitPoints, 0x100);
	mix(STREN, 0x100);
	mix(DEXTE, 0x100);
	mix(INTEL, 0x100);
	mix(MAGIC, 0x100);
	mix(ExpPoints, 2 * 0x100);
	mix(MovePts, 0x100);
	mix(Party, 0x11);
	mix(&PartySize, 1);
	mix(&MapX, 2);
	mix(&MapY, 2);
	mix(&MapZ, 2);
	mix(&Time_M, 1);
	mix(&Time_H, 1);
	mix(&Date_D, 1);
	mix(&Date_M, 1);
	mix(&Date_Y, 2);
	mix(&KARMA, 1);
	mix(&Active, 1);
	if (NPCMode)
		mix(NPCMode, 0x100);
	if (NPCFlag)
		mix(NPCFlag, 0x100);
	{
		uint32_t r = port_rand_state();
		mix(&r, 4);
	}
	return h;
}
