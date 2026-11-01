/* stars.asm (SRC_END) in C, for the port: the starfield of the ending.
 *
 * 64 stars (starX, starY, starColor; 0..112 x 0..93, on the screen at x + 6Ah) fly out
 * from the centre (56, 47): each tick a star is erased (if it still has its colour), moved
 * away from the centre by 1 + distance/16 on each axis, and drawn again where the screen
 * is black; one that leaves the field is reborn at a random place.
 *
 * The port's screen is MCGA's only, so StarsMCGA is the one ported; the others say so.
 * (The original's routines leave the stack unbalanced and return straight into the
 * timer's interrupt handler: that does not change what they draw.) */
#include "end.h"
#include "../../sys/dosmem.h"
#include "../../platform/plat.h"

/* a 16-bit LFSR (shr, xor 0B400h on a carry) */
static uint16 lfsr(uint16 *seed)
{
	uint16 c = (uint16)(*seed & 1);
	*seed >>= 1;
	if (c)
		*seed ^= 0xB400;
	return (uint16)(*seed & 0x7f);
}

/* RandX: a random x, 0..112 */
static uint8_t rand_x(void)
{
	uint16 a = lfsr(&starSeedX);
	return (uint8_t)(a > 112 ? a % 113 : a);
}

/* RandY: a random y, 0..93 */
static uint8_t rand_y(void)
{
	uint16 a = lfsr(&starSeedY);
	return (uint8_t)(a > 93 ? a % 94 : a);
}

/* MOVE_STAR: away from the centre (the signs are those of the 8-bit differences) */
static void move_star(int i)
{
	uint8_t dx = (uint8_t)(starX[i] - 56), dy = (uint8_t)(starY[i] - 47);
	int nx = dx & 0x80, ny = dy & 0x80;
	if (nx)
		dx = (uint8_t)-dx;
	if (ny)
		dy = (uint8_t)-dy;
	dx = (uint8_t)((dx >> 4) + 1);
	dy = (uint8_t)((dy >> 4) + 1);
	starY[i] = (char)(ny ? (uint8_t)starY[i] - dy : (uint8_t)starY[i] + dy);
	starX[i] = (char)(nx ? (uint8_t)starX[i] - dx : (uint8_t)starX[i] + dx);
}

StarsMCGA()
{
	uint8_t *scr = dos_mk_fp(0xA000, 0);
	int i;

	for (i = 63; i >= 0; i--) {
		uint8_t x = (uint8_t)starX[i], y = (uint8_t)starY[i], c = (uint8_t)starColor[i];
		uint16 at = (uint16)(scanlineOfs[y] + x + 0x6A);
		if (scr[at] == c)
			scr[at] = 0;
		move_star(i);
		x = (uint8_t)starX[i];
		y = (uint8_t)starY[i];
		if (y >= 94 || x >= 113) {
			starX[i] = (char)(x = rand_x());
			starY[i] = (char)(y = rand_y());
		}
		at = (uint16)(scanlineOfs[y] + x + 0x6A);
		if (scr[at] == 0)
			scr[at] = (uint8_t)starColor[i];
	}
	return 0;
}

static void unsupported(void)
{
	static int said;
	if (!said++)
		plat_log("END.EXE: the stars of the EGA, CGA and Tandy screens are not ported");
}

StarsEGA() { unsupported(); return 0; }
StarsCGA() { unsupported(); return 0; }
StarsTandy() { unsupported(); return 0; }
