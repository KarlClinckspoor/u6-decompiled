/*
 * Ultima VI - The False Prophet: U.EXE
 * The project's own macros shared by the DOS build and the native port, kept underneath
 * Exhume's runtime/include/portable.h (int16, uint16, int32, uint32, ...). Not called
 * portable.h, which would shadow Exhume's.
 *
 * Under Turbo C every one is the original type or tokens, so the DOS build compiles to
 * the same bytes (tools/u/ubuild.py checks U.EXE). On a modern host int is 32 bits and
 * long 64 (32 on Windows), so the integers are written with explicit widths (Exhume's portable.h):
 * int16 for Turbo C's int, uint16 for unsigned, int32 for long, uint32 for unsigned long.
 */
#ifndef U6PORT_H
#define U6PORT_H

#include "portable.h"

/*a far pointer made from a long (segment in the high word), as Turbo C converts one*/
#ifndef __TURBOC__
#define PORT_FAR32(l) MK_FP((uint16)((uint32)(l) >> 16), (uint16)(uint32)(l))
#endif

#endif
