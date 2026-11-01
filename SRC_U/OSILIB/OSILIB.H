/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: declarations shared by its C modules (see OSI.INC for the assembly)
 */
#ifndef OSILIB_H
#define OSILIB_H

#include "portable.h"

/*a drawing surface: a bitmap in memory (or the screen) with its clipping window*/
struct tSurface {
	uint16 segment;/*+00 segment of the pixels*/
	uint16 far *scanlines;/*+02 offset of each line*/
	int16 right, left, top, bottom;/*+06 the clipping window (inclusive)*/
};

/*a text window: where and how the text routines print*/
struct tText {
	struct tSurface *surface;/*+00*/
	int16 curX, curY;/*+02 the cursor*/
	unsigned char far *font;/*+06 height (word), ..., width of each character at +4*/
	char fgColor;/*+0A*/
	char bgColor;/*+0B*/
	char *buffer;/*+0C string buffer of the formatted output*/
	char *bufPtr;/*+0E where its next character goes*/
	char justify;/*+10*/
};

#endif
