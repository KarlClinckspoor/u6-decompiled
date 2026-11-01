/* Far memory as the 8086 addresses it, for the files of port/u/osilib.
 *
 * Pixels are segment:offset in the DOS arena (dosmem.h; the screen is A000:0000), with
 * 16-bit offsets that wrap in their segment. Far pointers (scanline tables, shapes,
 * strings, buffers) are host pointers: one into the arena is taken back as its normalised
 * segment:offset so that adding to its offset wraps as in DOS; any other (U's globals, the
 * host heap) is used as it is. */
#ifndef UFAR_H
#define UFAR_H

#include <stdint.h>
#include "../../sys/dosmem.h"

/* the byte at segment:offset */
static inline uint8_t *u_pix(uint16_t seg, uint16_t off)
{
	return dos_seg_ptr(seg, off);
}

/* the byte at far pointer p plus add, the offset wrapping in p's segment */
static inline uint8_t *u_far(const void *p, uint16_t add)
{
	long lin = dos_linear(p);

	if (lin >= 0)
		return dos_seg_ptr((uint16_t)(lin >> 4), (uint16_t)((lin & 15) + add));
	return (uint8_t *)p + add;
}

static inline uint16_t u_far16(const void *p, uint16_t add)
{
	return (uint16_t)(*u_far(p, add) | (*u_far(p, (uint16_t)(add + 1)) << 8));
}

static inline void u_far16w(void *p, uint16_t add, uint16_t v)
{
	*u_far(p, add) = (uint8_t)v;
	*u_far(p, (uint16_t)(add + 1)) = (uint8_t)(v >> 8);
}

/* the offset of scanline y of a surface: word y of its table (LDS DI,pScanlines;
 * SHL y,1; ADD DI,y; MOV DI,[DI]) */
static inline uint16_t u_scan(const struct tSurface *s, int16_t y)
{
	return u_far16(s->pScanlines, (uint16_t)((uint16_t)y << 1));
}

/* a far pointer plus a 32-bit offset, normalised (MOUSEHIT.ASM's shape header: the
 * linear address seg * 16 + off + add, its segment cut to 16 bits) */
static inline uint8_t *u_far_add32(const void *p, uint32_t add)
{
	long lin = dos_linear(p);

	if (lin >= 0) {
		uint32_t l = (uint32_t)lin + add;
		return (uint8_t *)dos_mk_fp((uint16_t)(l >> 4), (uint16_t)(l & 15));
	}
	return (uint8_t *)p + add;
}

#endif
