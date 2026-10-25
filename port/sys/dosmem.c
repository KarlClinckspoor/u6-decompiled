/* The DOS arena (dosmem.h): host memory standing for the PC's first megabyte, the far
 * pointer conversions, and Turbo C 2.0's far heap on top of it.
 *
 * Turbo C 2.0's farmalloc puts an 8-byte header in front of each block and returns
 * block + 8 (a pointer seg:0008 once normalised); farcoreleft is the room between the top
 * of the far heap and the end of memory minus those 8 bytes, so farmalloc(farcoreleft())
 * takes exactly everything, as the game does at startup (main.c). This file does the
 * same over DOS_HEAP_START..DOS_HEAP_END, with paragraph aligned blocks.
 */
#include "dosmem.h"
#include "../platform/plat.h"

#include <stdlib.h>
#include <string.h>

uint8_t *dos_base;

void dosmem_init(void)
{
	if (dos_base)
		return;
	dos_base = (uint8_t *)calloc(1, DOS_ARENA_SIZE + DOS_ARENA_SLACK);
	if (!dos_base)
		plat_fatal("Not enough memory for the DOS arena.");
}

/* ---- foreign pointers ---- */

#define FOREIGN_WINDOWS (0x10000u - DOS_SEG_FOREIGN)

static uintptr_t foreign_base[FOREIGN_WINDOWS];
static unsigned foreign_count;
static int foreign_logged, foreign_full_logged;

static int in_arena(uintptr_t a)
{
	uintptr_t b = (uintptr_t)dos_base;
	return dos_base && a >= b && a < b + DOS_ARENA_SIZE + DOS_ARENA_SLACK;
}

/* The window holding a, created when there is none. */
static unsigned foreign_window(uintptr_t a)
{
	unsigned i;

	for (i = 0; i < foreign_count; i++)
		if (a >= foreign_base[i] && a - foreign_base[i] < 0x10000u)
			return i;
	/* the MCGA driver's own data is a host array: FP_OFF of it (decompress's return value)
	 * is expected, so this is only reported with U6_DEBUG set */
	if (!foreign_logged && getenv("U6_DEBUG")) {
		foreign_logged = 1;
		plat_log("dosmem: FP_SEG/FP_OFF of a pointer outside the DOS arena (%p); "
		         "it gets a foreign segment (logged once)", (void *)a);
	}
	if (foreign_count == FOREIGN_WINDOWS) {
		if (!foreign_full_logged) {
			foreign_full_logged = 1;
			plat_log("dosmem: out of foreign segments, reusing the last one");
		}
		foreign_base[FOREIGN_WINDOWS - 1] = a & ~(uintptr_t)15;
		return FOREIGN_WINDOWS - 1;
	}
	foreign_base[foreign_count] = a & ~(uintptr_t)15;
	return foreign_count++;
}

void *dos_mk_fp(uint16_t seg, uint16_t off)
{
	if (!dos_base)
		dosmem_init();
	if (seg >= DOS_SEG_FOREIGN) {
		unsigned w = seg - DOS_SEG_FOREIGN;
		if (w < foreign_count)
			return (void *)(foreign_base[w] + off);
		/* not a segment we handed out: the arena's ROM area, as a PC would see it */
	}
	return dos_base + (uint32_t)seg * 16u + off;
}

uint16_t dos_fp_seg(const volatile void *p)
{
	uintptr_t a = (uintptr_t)p;

	if (!p)
		return 0;
	if (in_arena(a))
		return (uint16_t)((a - (uintptr_t)dos_base) >> 4);
	return (uint16_t)(DOS_SEG_FOREIGN + foreign_window(a));
}

uint16_t dos_fp_off(const volatile void *p)
{
	uintptr_t a = (uintptr_t)p;

	if (!p)
		return 0;
	if (in_arena(a))
		return (uint16_t)((a - (uintptr_t)dos_base) & 15u);
	return (uint16_t)(a - foreign_base[foreign_window(a)]);
}

long dos_linear(const volatile void *p)
{
	uintptr_t a = (uintptr_t)p;

	if (!in_arena(a))
		return -1;
	return (long)(a - (uintptr_t)dos_base);
}

/* ---- far heap ---- */

#define HEAP_HDR 8u
#define MAX_BLOCKS 512

typedef struct {
	uint32_t start, size;   /* linear address of the header, size with header */
	int used;
} Block;

static Block blocks[MAX_BLOCKS];   /* in address order, contiguous from DOS_HEAP_START */
static int nblocks;
static uint32_t heap_top = DOS_HEAP_START;

static void put32(uint32_t lin, uint32_t v)
{
	dos_base[lin] = (uint8_t)v;
	dos_base[lin + 1] = (uint8_t)(v >> 8);
	dos_base[lin + 2] = (uint8_t)(v >> 16);
	dos_base[lin + 3] = (uint8_t)(v >> 24);
}

void *dos_farmalloc(unsigned long size)
{
	uint32_t need;
	int i;

	if (!dos_base)
		dosmem_init();
	if (size == 0 || size > DOS_HEAP_END)
		return 0;
	need = ((uint32_t)size + HEAP_HDR + 15u) & ~15u;
	/* first fit in the freed blocks */
	for (i = 0; i < nblocks; i++) {
		if (blocks[i].used || blocks[i].size < need)
			continue;
		if (blocks[i].size > need && nblocks < MAX_BLOCKS) {
			memmove(&blocks[i + 2], &blocks[i + 1], (size_t)(nblocks - i - 1) * sizeof(Block));
			blocks[i + 1].start = blocks[i].start + need;
			blocks[i + 1].size = blocks[i].size - need;
			blocks[i + 1].used = 0;
			blocks[i].size = need;
			nblocks++;
		}
		blocks[i].used = 1;
		put32(blocks[i].start, blocks[i].size | 1u);
		return dos_base + blocks[i].start + HEAP_HDR;
	}
	if (need > DOS_HEAP_END - heap_top || nblocks == MAX_BLOCKS)
		return 0;
	blocks[nblocks].start = heap_top;
	blocks[nblocks].size = need;
	blocks[nblocks].used = 1;
	nblocks++;
	put32(heap_top, need | 1u);
	heap_top += need;
	return dos_base + heap_top - need + HEAP_HDR;
}

void dos_farfree(void *p)
{
	long lin;
	int i;

	if (!p)
		return;
	lin = dos_linear(p) - (long)HEAP_HDR;
	for (i = 0; i < nblocks; i++)
		if (blocks[i].used && (long)blocks[i].start == lin)
			break;
	if (i == nblocks) {
		plat_log("dosmem: farfree of a pointer that farmalloc did not return (%p)", p);
		return;
	}
	blocks[i].used = 0;
	put32(blocks[i].start, blocks[i].size);
	/* merge with the free neighbours */
	if (i + 1 < nblocks && !blocks[i + 1].used) {
		blocks[i].size += blocks[i + 1].size;
		memmove(&blocks[i + 1], &blocks[i + 2], (size_t)(nblocks - i - 2) * sizeof(Block));
		nblocks--;
	}
	if (i > 0 && !blocks[i - 1].used) {
		blocks[i - 1].size += blocks[i].size;
		memmove(&blocks[i], &blocks[i + 1], (size_t)(nblocks - i - 1) * sizeof(Block));
		nblocks--;
		i--;
	}
	/* a free block at the top gives its memory back */
	if (i == nblocks - 1 && !blocks[i].used) {
		heap_top = blocks[i].start;
		nblocks--;
	}
}

unsigned long dos_farcoreleft(void)
{
	uint32_t room = DOS_HEAP_END - heap_top;

	return room > HEAP_HDR ? room - HEAP_HDR : 0;
}

uint16_t dos_free_paragraphs(void)
{
	return (uint16_t)((DOS_HEAP_END - heap_top) >> 4);
}
