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
/* Segments DOS_SEG_FOREIGN..FFFFh stand for pointers outside the arena: FP_SEG of such a
 * pointer is DOS_SEG_FOREIGN + n, where n numbers a 64 KB window starting at (or a little
 * below) the pointer; FP_OFF is the offset in that window; MK_FP(DOS_SEG_FOREIGN + n, off)
 * gives the window's base + off. 4096 windows; the arena's F0000h.. range (BIOS ROM in a
 * PC) is not reachable through MK_FP. */
#define DOS_SEG_FOREIGN  0xF000u
/* The arena is allocated this much larger, so that any seg:off (up to 10FFEFh) is inside. */
#define DOS_ARENA_SLACK  0x10010u

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

/* Additions for port/sys. */
/* The largest free block above the far heap, in paragraphs (what DOS function 48h with
 * BX=FFFFh reports): OSI_memory, OSI_copy. */
uint16_t dos_free_paragraphs(void);
/* A pointer to linear address seg*16+off taken as 16-bit offset arithmetic inside the
 * segment, as the 8086 does: dos_mk_fp(seg, (uint16_t)(off + n)). */
#define dos_seg_ptr(seg, off) ((uint8_t *)dos_mk_fp((uint16_t)(seg), (uint16_t)(off)))

#endif
