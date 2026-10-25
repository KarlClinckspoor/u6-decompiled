/* The portability layer, force-included (clang -include) into every game source compiled
 * for the host. The game's C stays as Turbo C saw it; this file and the stand-ins for
 * Borland's headers in this directory (dos.h, alloc.h, process.h) give it a meaning on a
 * 64-bit host:
 *   - the memory-model keywords go away (every pointer is a flat host pointer);
 *   - MK_FP, FP_SEG, FP_OFF, farmalloc and farcoreleft work on the DOS arena
 *     (port/sys/dosmem.h), so the game's segment arithmetic keeps working;
 *   - Borland's extra library names (stricmp, random, max, __abs__, ...) are provided;
 *   - the game's main is renamed so that the port's main can run it;
 *   - every struct the game declares is packed, as Turbo C laid them out (byte alignment).
 */
#ifndef PORT_COMPAT_H
#define PORT_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define far
#define near
#define huge
#define pascal
#define cdecl
#define interrupt
#define _cdecl

/* ---- far pointers and the far heap ---- */
void *dos_mk_fp(uint16_t seg, uint16_t off);
uint16_t dos_fp_seg(const volatile void *p);
uint16_t dos_fp_off(const volatile void *p);
void *dos_farmalloc(unsigned long size);
void dos_farfree(void *p);
unsigned long dos_farcoreleft(void);
#define MK_FP(seg, off) dos_mk_fp((uint16_t)(seg), (uint16_t)(off))
#define FP_SEG(p) dos_fp_seg(p)
#define FP_OFF(p) dos_fp_off(p)
#define farmalloc(n) dos_farmalloc(n)
#define farfree(p) dos_farfree(p)
#define farcoreleft() dos_farcoreleft()

/* ---- Borland's library ---- */
#define stricmp port_stricmp
int port_stricmp(const char *a, const char *b);
#define __abs__(x) abs(x)
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
/* Borland's rand: its own generator, RAND_MAX 7FFFh (port/sys/borland.c) */
#define rand port_rand
#define srand port_srand
int port_rand(void);
void port_srand(unsigned seed);
#define random(num) ((int)(((long)port_rand() * (num)) / 0x8000L))
#define exit port_exit
void port_exit(int code);

/* ---- the music driver's entries (port/sys/musdrv.c, portable.h's MUS_DRV) ---- */
void MUSDRV_0(int port);
void MUSDRV_6(void *song);
void MUSDRV_9(void);
void MUSDRV_0xc(int channel, int a, int b);

/* ---- the game's entry point ---- */
#define main game_main

/* Turbo C packs structs (no -a): byte alignment for every struct the game declares. */
#pragma pack(1)

#endif
