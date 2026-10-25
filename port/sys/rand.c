/* RAND.ASM: the random generator.
 *
 * The state is a 32-bit number held in two words, D_308A:D_3088 (high:low), initially
 * 0:1. Each draw multiplies it by 65539 (RANDU): C_33C5_000C computes 3*state with
 * add/rcl/add/adc and adds the old low word to the high word. OSI_rand(a, b) draws, then
 * returns min + high % (max - min + 1), unsigned, with min/max = the smaller/larger of a
 * and b; when the range is the whole 64K (a = 0, b = FFFFh) it returns min. OSI_randomize
 * sets the low word to seed | 1 and keeps the high word. OSI_timeSeed builds a 12-bit
 * seed from the DOS time. The code below follows the instructions one by one.
 */
#include "osi.h"
#include "dosint.h"
#include "../replay/replay.h"

static uint16_t seed_lo = 1;   /* D_3088 */
static uint16_t seed_hi = 0;   /* D_308A */

/* C_33C5_000C */
static void rand_step(void)
{
	uint32_t t;
	uint16_t cx = seed_lo, bx = cx, ax = seed_hi, c;

	t = (uint32_t)cx + cx;                  /* add cx,cx */
	c = (uint16_t)(t >> 16);
	cx = (uint16_t)t;
	ax = (uint16_t)((ax << 1) | c);          /* rcl ax,1 (its carry out is not used) */
	t = (uint32_t)cx + bx;                  /* add cx,bx */
	c = (uint16_t)(t >> 16);
	cx = (uint16_t)t;
	ax = (uint16_t)(ax + seed_hi + c);       /* adc ax,[308A] */
	ax = (uint16_t)(ax + bx);                /* add ax,bx */
	seed_lo = cx;
	seed_hi = ax;
}

int OSI_rand(int a, int b)
{
	uint16_t bx = (uint16_t)b, cx = (uint16_t)a, dx = 0, t;

	rand_step();
	if (bx < cx) {
		t = bx;
		bx = cx;
		cx = t;
	}
	bx = (uint16_t)(bx - cx + 1);
	if (bx != 0)
		dx = (uint16_t)(seed_hi % bx);
	return (int16_t)(uint16_t)(dx + cx);
}

int OSI_Rand(int a, int b)
{
	return OSI_rand(a, b);
}

void OSI_randomize(int seed)
{
	seed_lo = (uint16_t)((uint16_t)seed | 1u);
}

/* int 21h 2Ch gives CH hour, CL minute, DH second, DL 1/100 s; the shifts are 8-bit */
uint16_t osi_seed_from_time(uint8_t hour, uint8_t minute, uint8_t second, uint8_t hundredths)
{
	uint8_t ch = (uint8_t)(hour << 1), cl = (uint8_t)(minute << 2), dh = (uint8_t)(second << 3);
	uint16_t ax = (uint16_t)(((uint16_t)dh << 8) | hundredths);

	ax = (uint16_t)(ax + (((uint16_t)ch << 8) | cl));
	return (uint16_t)((ax ^ 0x91EBu) & 0x0FFFu);
}

/* The seed is what the game reads from the outside world: it goes through the replay. */
int OSI_timeSeed(void)
{
	uint8_t h, m, s, hs;

	dos_time_live(&h, &m, &s, &hs);
	return (int16_t)(uint16_t)rp_value(RP_SEED, osi_seed_from_time(h, m, s, hs));
}

/* For tests: the generator's state. */

void osi_rand_state(uint16_t *lo, uint16_t *hi)
{
	*lo = seed_lo;
	*hi = seed_hi;
}

void osi_rand_set_state(uint16_t lo, uint16_t hi)
{
	seed_lo = lo;
	seed_hi = hi;
}
