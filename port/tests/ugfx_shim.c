/* Test shim for port/u/ugfx.c (ugfx_diff.py loads it with ctypes): a stand-alone DOS
 * arena with the dosmem.h calls the driver makes. */
#include "../sys/dosmem.h"
#include "../u/ugfx.h"

static uint8_t arena[0x110000];
uint8_t *dos_base = arena;

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

static int mode_sets;
static void on_mode(void) { mode_sets++; }

uint8_t *shim_arena(void) { return arena; }
void shim_install_hook(void) { ugfx_set_mode_hook(on_mode); }
int shim_mode_sets(void) { return mode_sets; }
int shim_surface_size(void) { return (int)sizeof(struct tSurface); }
int shim_text_size(void) { return (int)sizeof(struct tText); }
