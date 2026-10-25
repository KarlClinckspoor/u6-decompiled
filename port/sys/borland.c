/* replaces SEG_155X.ASM's data and the parts of Borland's C library the game uses that
 * the host's library lacks or does differently. */
#include <ctype.h>
#include <stdint.h>

/* SEG_155X.ASM: D_07CC, a byte in the data segment */
unsigned char PartyListTop = 0;

int port_stricmp(const char *a, const char *b)
{
	for (;; a++, b++) {
		int ca = toupper((unsigned char)*a), cb = toupper((unsigned char)*b);
		if (ca != cb || !ca)
			return ca - cb;
	}
}

/* Borland's rand: a 32-bit LCG, multiplier 22695477, increment 1, result bits 16..30 */
static uint32_t rand_seed = 1;

int port_rand(void)
{
	rand_seed = rand_seed * 22695477u + 1u;
	return (int)((rand_seed >> 16) & 0x7fff);
}

void port_srand(unsigned seed)
{
	rand_seed = seed & 0xffff;
}
