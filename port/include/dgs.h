/* dgs.h: a translation unit's static near data, as port/game/dgroup_port.c (written by
 * port/tools/gen_dgroup.py) finds it: the statics of a game source (port/game/tu_*.c, which
 * compile it) or of a port file (its port/game/dgs_*.inc), each with its size on the host. */
#ifndef U6PORT_DGS_H
#define U6PORT_DGS_H
#include <stdint.h>
/* 16 bytes however it is packed: the game's sources pack every struct (compat.h), the port's
 * files do not, and the tables cross from one to the other */
struct u6_dgs {
	const void *p;
	uint32_t n;
	uint32_t pad;
};

/* port/game/dgroup_port.c's entries: a DOS byte range of DGROUP and where the port keeps it.
 * len is the DOS bytes; for a static (K_S*, through ref), the most DOS has room for. */
enum { K_BYTES, K_FAR, K_NEAR, K_CODE, K_RAND, K_SBYTES, K_SFAR, K_SNEAR, K_HGR, K_HMUS };
struct dg_ent {
	uint16_t off, len;
	uint8_t kind;
	const void *p;
	const struct u6_dgs *ref;
};
/* a string literal the game writes into (WRITABLE_STR): its text and DOS offset */
struct dg_wstr {
	const char *init;
	uint16_t off;
};
/* a function a far code pointer may name: its offset in RGAME.MAP, and its segment counted
 * from DGROUP's paragraph */
struct dg_fn {
	const void *f;
	uint16_t seg, off;
};
#endif
