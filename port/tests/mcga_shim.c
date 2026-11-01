/* Test shim for port/gfx/mcga.c (mcga_diff.py loads it with ctypes): a stand-alone DOS
 * arena (dosmem.h) and the two platform calls the driver makes. */
#include "../sys/dosmem.h"
#include "../platform/plat.h"
#include "../gfx/mcga.h"

static uint8_t arena[0x110000];
uint8_t *dos_base;

void dosmem_init(void) { dos_base = arena; }

void *dos_mk_fp(uint16_t seg, uint16_t off) { return dos_base + ((uint32_t)seg << 4) + off; }

long dos_linear(const volatile void *p)
{
	uintptr_t a = (uintptr_t)p, b = (uintptr_t)dos_base;
	if (a >= b && a < b + sizeof arena)
		return (long)(a - b);
	return -1;
}

uint16_t dos_fp_seg(const volatile void *p) { return (uint16_t)(dos_linear(p) >> 4); }
uint16_t dos_fp_off(const volatile void *p) { return (uint16_t)(dos_linear(p) & 15); }

static int dirty_count;
static const uint8_t *src_pixels, *src_palette;

void plat_video_source(const uint8_t *pixels, const uint8_t *palette6)
{
	src_pixels = pixels;
	src_palette = palette6;
}

void plat_video_dirty(void) { dirty_count++; }

uint8_t *shim_arena(void) { return arena; }
int shim_dirty_count(void) { return dirty_count; }
const uint8_t *shim_video_pixels(void) { return src_pixels; }
const uint8_t *shim_video_palette(void) { return src_palette; }
int shim_info_size(void) { return (int)sizeof(struct McgaInfo); }
int shim_surface_size(void) { return (int)sizeof(struct McgaSurface); }
