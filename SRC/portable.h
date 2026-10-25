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

#endif
