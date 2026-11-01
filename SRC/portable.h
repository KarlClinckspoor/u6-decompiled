/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Types and macros shared by the DOS build and the native port.

	Under Turbo C every one is the original type or tokens, so the DOS build
	compiles to the same bytes (tools/dosbuild.py checks GAME.EXE). On a
	modern host int is 32 bits and long 64 (32 on Windows), so the game's
	integers are written with explicit widths: int16 for Turbo C's int,
	uint16 for unsigned, int32 for long, uint32 for unsigned long.
*/
#ifndef __PORTABLE_H__
#define __PORTABLE_H__

#ifdef __TURBOC__
typedef int int16;
typedef unsigned uint16;
typedef long int32;
typedef unsigned long uint32;
#else
#include <stdint.h>
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
#endif

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
  interpreter keeps string pointers on its value stack*/
#ifdef __TURBOC__
typedef int16 NEARPTR;
typedef int32 NEARLONG;
#define PTR2INT16(p) (int16)p
#else
typedef intptr_t NEARPTR;
typedef int64_t NEARLONG;
#define PTR2INT16(p) (intptr_t)(p)
#endif

/*a call of the music driver's entry point o (MUS_DRV(6)(song)): in DOS a far
  call through MUS_Driver with its offset set to o; the port has the entries
  as C functions MUSDRV_0, MUSDRV_6, MUSDRV_9, MUSDRV_0xc (port/sys/musdrv.c)*/
#ifdef __TURBOC__
#define MUS_DRV(o) MUS_Driver.iii.ofs = o; (*MUS_Driver.f)
#else
#define MUS_DRV(o) MUSDRV_##o
#endif

/*a string literal the game writes into (DOS has no read-only data; a host keeps
  literals read-only): the port gives it a writable copy of its own*/
#ifdef __TURBOC__
#define WRITABLE_STR(s) s
#else
#define WRITABLE_STR(s) ({ static char writable_[] = s; writable_; })
#endif

/*a local the game only partly sets (DOS keeps whatever the stack held in the rest):
  the port zeroes it, so that what reaches a file does not depend on the compiler*/
#ifdef __TURBOC__
#define PORT_ZERO(v)
#else
#define PORT_ZERO(v) memset(&(v), 0, sizeof(v));
#endif

#endif
