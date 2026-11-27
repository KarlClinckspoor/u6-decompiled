/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	The project's own macros shared by the DOS build and the native port, kept
	underneath Exhume's runtime/include/portable.h (which this file includes, and
	which defines int16, uint16, int32, uint32, NEARPTR, STACK_JUNK_SET, ...).
	It is called u6port.h because a project header named portable.h would shadow
	Exhume's. Every shared source includes this file, never Exhume's directly.

	Under Turbo C every one is the original type or tokens, so the DOS build
	compiles to the same bytes (tools/dosbuild.py checks GAME.EXE). On a
	modern host int is 32 bits and long 64 (32 on Windows), so the game's
	integers are written with explicit widths (Exhume's portable.h): int16 for
	Turbo C's int, uint16 for unsigned, int32 for long, uint32 for unsigned long.
*/
#ifndef __U6PORT_H__
#define __U6PORT_H__

#include "portable.h"

/*CON_printf's variable arguments: Turbo C takes their address with "...";
  each is a 16-bit word (a near pointer for %s)*/
#ifdef __TURBOC__
#define VA_LIST void **
#define VA_START(ap, last) ap = ...
#define VA_INT(ap) *(int16 *)ap++
#define VA_CHAR(ap) *(char *)ap++
#define VA_STR(ap) (char *)*ap++
#else
#include <stdarg.h>
#define VA_LIST va_list
#define VA_START(ap, last) va_start(ap, last)
#define VA_INT(ap) ((int16)va_arg(ap, int))
#define VA_CHAR(ap) ((char)va_arg(ap, int))
#define VA_STR(ap) va_arg(ap, char *)
#endif

/*a pointer advanced by one byte through a cast lvalue (Turbo C accepts
  "((char far *)p)++", ISO C does not)*/
#ifdef __TURBOC__
#define CHARPTR_POSTINC(p) (((char far *)p)++)
#else
#define CHARPTR_POSTINC(p) ((*(char **)&(p))++)
#endif

/*a near pointer kept in an integer (16 bits in DOS): the conversation
  interpreter keeps string pointers on its value stack (NEARPTR itself is Exhume's)*/
#ifdef __TURBOC__
typedef int32 NEARLONG;
#define PTR2INT16(p) (int16)p
#else
typedef int64_t NEARLONG;
#define PTR2INT16(p) (intptr_t)(p)
#endif

/*a far pointer as Turbo C's (int32) of it gives it: the segment in the high word, the offset in
  the low one (on the host, the pointer's place in the DOS arena: FP_SEG, FP_OFF)*/
#ifdef __TURBOC__
#define FARPTR32(p) ((int32)(p))
#else
#define FARPTR32(p) ((int32)((uint32)FP_SEG(p) << 16 | FP_OFF(p)))
#endif

/*a call of the music driver's entry point o (MUS_DRV(6)(song)): in DOS a far
  call through MUS_Driver with its offset set to o; the port has the entries
  as C functions MUSDRV_0, MUSDRV_6, MUSDRV_9, MUSDRV_0xc (port/sys/musdrv.c)*/
#ifdef __TURBOC__
#define MUS_DRV(o) MUS_Driver.iii.ofs = o; (*MUS_Driver.f)
#else
/*the entry number is kept as DOS keeps it in MUS_Driver's offset word, for the replay dumps*/
extern unsigned short u6_mus_entry;
#define MUS_DRV(o) (u6_mus_entry = (o), MUSDRV_##o)
#endif

/*a string literal the game writes into (DOS has no read-only data; a host keeps
  literals read-only): the port gives it a writable copy of its own. Exhume's
  WRITABLE_STR is a block-scope compound literal, which dies with its block; this one
  is static, so the pointer may outlive the block (talk.c keeps it in VarStr[]), hence
  the project's definition replaces Exhume's on the host (the same `s` under Turbo C)*/
#ifdef __TURBOC__
#define WRITABLE_STR(s) s
#else
#undef WRITABLE_STR
/*u6_writable (port/game/rpport.c) notes the copy, for the replay dumps' DATA section, which
  writes it at the literal's DOS offset*/
void u6_writable(char *buf, const char *init, unsigned n);
#define WRITABLE_STR(s) ({ static char writable_[] = s; u6_writable(writable_, s, sizeof writable_); writable_; })
#endif

/*FOUNTAIN_WISH(i): objcmd.c's UseFountain compares the wish with FountainWishes[0..9], a table
  of 5 (docs/CODE_REVIEW.md, CR-008). DOS reads past it the next words, UnboardDirX/Y, as near
  pointers: entries 5 and 6 are DS:0100 and DS:0101, zero bytes of Turbo C's run-time data (in
  every dump), so an empty wish matches entry 5; 7 to 9 are DS:FF00 and DS:FFFF, the stack, whose
  text a typed wish matches only by chance. The host's table is 5 native pointers followed by
  unrelated memory: past it the port gives "", which entries 5 and 6 are in DOS. The original
  tokens under Turbo C, so the gate is unchanged*/
#ifdef __TURBOC__
#define FOUNTAIN_WISH(i) FountainWishes[i]
#else
#define FOUNTAIN_WISH(i) ((i) < 5 ? FountainWishes[i] : "")
#endif

/*STACK_JUNK_FILL(a, v): Exhume's STACK_JUNK_SET (portable.h) for every element of a local
  array the original may read before it sets it: nothing under Turbo C, so GAME.EXE's bytes
  are the same (the gate proves it); in the replay build and the port each element starts as
  v, so both read the same value where DOS read what its stack held. Written as a statement
  after the declarations*/
#if defined(__TURBOC__) && !defined(REPLAY)
#define STACK_JUNK_FILL(a, v)
#else
#define STACK_JUNK_FILL(a, v) { int stack_junk_i_; \
	for(stack_junk_i_ = 0; stack_junk_i_ < (int)(sizeof(a) / sizeof((a)[0])); stack_junk_i_ ++) \
		(a)[stack_junk_i_] = (v); }
#endif

/*U6_FIX(f): whether the port's fix f of the original's faults is on (port/sys/fixes.h,
  docs/port/FIXES.md; FIX_VORTEX_CUBE ... the critical findings of docs/CODE_REVIEW.md). Used
  only in lines under #ifndef __TURBOC__, so that neither DOS build sees a fix (GAME.EXE's bytes,
  the gate, and the replay build's code size, which every recording depends on, are unchanged);
  off, the port runs the original lines as DOS does*/
#ifndef __TURBOC__
#include "../port/sys/fixes.h"
#define U6_FIX(f) ((enhance_on >> (f)) & 1u)
#endif

#endif
