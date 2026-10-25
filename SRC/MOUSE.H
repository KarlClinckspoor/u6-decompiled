/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __MOUSE_H__
#define __MOUSE_H__

/*a graphic structure?*/
struct tSurface {
	unsigned segment;/*_00*/
	unsigned far *pScanlines;/*_02*/
	unsigned right,left,top,bottom;/*f_06,f_08,f_0a,f_0c*/
};

struct tMouse {
	unsigned char buttonL,buttonR;/*button 1 & 2?*/
	int posX,posY;/*x, y?*/
	unsigned unused06;/*never used*/
	void far *shapes;/*far pointer to the pointer shape table*/
	unsigned shapeIndex;/*current pointer shape (MOUSE_POINTER 2nd arg)*/
	struct tSurface *surface;/*surface the pointer is drawn on (Screen or map back buffer)*/
	void far *saveBuf;/*0x400 bytes background save area*/
	void (far *onMove)();/*far callback after each position/button change (CON_UpdateMousePointer)*/
	unsigned char installed;/*mouse driver present*/
};

/*MOUSE_0000*/extern cdecl MOUSE_INIT(struct tMouse *);/*register the tMouse struct, int 33h reset, set installed flag*/
/*MOUSE_0056*/extern cdecl MOUSE_POINTER(void far */*graphic data?*/, unsigned);
/*MOUSE_009F*/extern cdecl MOUSE_SERVE();
/*MOUSE_0135*/extern cdecl MOUSE_SHOW();
/*MOUSE_0146*/extern cdecl MOUSE_HIDE();
/*MOUSE_015C*/extern cdecl MOUSE_DRAW();/*graphic driver fn 0x30: draw the pointer shape on Mouse.f_0e surface*/
/*MOUSE_0183*/extern cdecl MOUSE_SAVE_BG();/*graphic driver fn 0x0C: save the background under the pointer into Mouse.f_10*/
/*MOUSE_01F0*/extern int cdecl MOUSE_IN_AREA(struct tSurface *);

#define ShowMouse if(MouseOn) MOUSE_SHOW();
#define HideMouse if(MouseOn) MOUSE_HIDE();
#define ServeMouse if(MouseOn) MOUSE_SERVE();

#endif
