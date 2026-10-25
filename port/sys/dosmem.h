/* The DOS memory arena: what gives the game's far pointers a meaning on the host.
 *
 * The game builds far pointers from segments and offsets and does arithmetic on segment
 * numbers (its bump allocator over the far heap, LoadFile's work segments, the driver's
 * memory). The port keeps one block of host memory standing for the PC's first megabyte:
 * linear address L is dos_base + L. farmalloc hands out blocks from it, MK_FP gives
 * dos_base + seg*16 + off, FP_SEG/FP_OFF give a pointer into it back as a normalised
 * segment:offset (offset 0..15). Code that only adds offsets to a pointer works as before.
 *
 * FP_SEG/FP_OFF of a pointer outside the arena (a global, a stack buffer) give the
 * address as an offset from DOS_SEG_FOREIGN; a later MK_FP with that segment gives the
 * pointer back, which is enough for the game's own round trips. Such uses are logged once.
 */
#ifndef DOSMEM_H
#define DOSMEM_H

#include <stdint.h>
#include <stddef.h>

#define DOS_ARENA_SIZE   0x100000u   /* the first megabyte */
#define DOS_HEAP_START   0x10000u    /* where the far heap starts (paragraph 1000h) */
#define DOS_HEAP_END     0xA0000u    /* 640 KB */

extern uint8_t *dos_base;

void dosmem_init(void);
void *dos_mk_fp(uint16_t seg, uint16_t off);
uint16_t dos_fp_seg(const volatile void *p);
uint16_t dos_fp_off(const volatile void *p);
/* Linear address of a pointer into the arena (or -1). */
long dos_linear(const volatile void *p);

/* Borland's far heap over the arena. */
void *dos_farmalloc(unsigned long size);
void dos_farfree(void *p);
unsigned long dos_farcoreleft(void);

#endif
