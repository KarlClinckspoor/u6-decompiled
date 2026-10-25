/*
 * Ultima VI - The False Prophet: U.EXE
 * Types and macros shared by the DOS build and the native port.
 *
 * Under Turbo C every one is the original type or tokens, so the DOS build compiles to
 * the same bytes (tools/u/ubuild.py checks U.EXE). On a modern host int is 32 bits and
 * long 64 (32 on Windows), so the integers are written with explicit widths: int16 for
 * Turbo C's int, uint16 for unsigned, int32 for long, uint32 for unsigned long.
 */
#ifndef PORTABLE_H
#define PORTABLE_H

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

/*a far pointer made from a long (segment in the high word), as Turbo C converts one*/
#ifndef __TURBOC__
#define PORT_FAR32(l) MK_FP((uint16)((uint32)(l) >> 16), (uint16)(uint32)(l))
#endif

#endif
