/* Exhume's record and replay hooks, as the port has them for now.
 *
 * The shared sources call KEY(a) and RECORDED(id, x) (SRC/hookgame.h, Exhume's portable.h),
 * which outside the DOS build are rp_key and rp_value of Exhume's runtime/replay/replay.c.
 * The port does not link that file yet (PLAN-exact-port.md, Phase 2): its own recording sits
 * under the OSI library (port/replay), where the key read and the other reads are already
 * recorded. So here each hook reads the world as the DOS build does and nothing more.
 * Phase 2 replaces this file with replay.c. */
#include <stdint.h>
#include "osi.h"

int rp_key(int a)
{
	return KBD_GetKey(a);
}

uint32_t rp_value(int id, uint32_t v)
{
	(void)id;
	return v;
}
