/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0406: character creation (the name, the gypsy and her questions)
 */
#include "u.h"
#include <alloc.h>
#include <conio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/*a drawing surface of the graphics driver (GAME.EXE's struct tSurface)*/
struct tSurface {
	unsigned segment;/*_00*/
	unsigned far *pScanlines;/*_02: offset of each line*/
	int right, left, top, bottom;/*_06,_08,_0a,_0c*/
};

extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface D_5710;/*the screen*/
extern struct tSurface D_5732;/*the back buffer*/
extern struct tSurface D_565F;/*the vellum's area*/
extern struct tSurface D_5702;/*copy of D_565F*/
extern struct tSurface D_5748;
extern struct tSurface D_5980;
extern struct tSurface D_599C;/*the text window of the name question*/
extern struct tSurface D_5AE6;/*the area of the gypsy's pattern*/
extern int D_5AF0;/*= D_5AE6.top*/
extern struct tSurface D_5AFA;/*EGA: the second page (segment a800)*/
extern struct tSurface D_5B08;/*EGA: the screen, for the mouse pointer*/
extern struct tSurface D_6072;/*a 320x56 work surface (the A/B buttons)*/
extern struct tSurface *D_5054;/*the surface the mouse pointer is drawn on*/
extern struct tSurface *D_566D;/*the current text window*/
extern int D_566F, D_5671;/*text cursor x, y*/
extern char D_5677, D_5678;/*text colors*/
extern char D_567D;
extern char D_5046, D_5047;/*mouse buttons*/
extern char D_505E;/*a mouse is present?*/
extern int D_575A;/*set when the player gave up*/
extern int D_59B8;/*the vellum has been shown*/
extern int D_5A2E;

/*the new character*/
extern char D_5A30[];/*name*/
extern int D_5A40, D_5A42, D_5A44;/*strength, dexterity, intelligence*/
extern char D_5A46;/*sex: 'M' or 'F'*/
extern char D_5A4E[];/*"Avatar"*/
extern char D_5A5E;/*'G'*/
extern int D_5A60;/*experience*/
extern int D_5A62;/*level*/
extern int D_5A64;/*hit points*/
extern char D_5A6A;

extern char far *D_5AD2;/*montage image*/
extern char far *D_5AD6;/*vellum image*/
extern char far *D_5ADC;/*woods / gypsy image*/

/*the gypsy's table: this module's initialised data*/
/*0692*/char D_0692 = 75;/*karma (75; +3 when Humility is chosen): objlist 0x1bf9*/
/*0693*/int D_0693 = 0;/*the vial being poured gives its color this step*/
/*0695*/int D_0695[8] = {10, 10, 10, 10, -13, -13, -13, -13};/*x offset of each vial (left side +10, right side -13)*/
/*06A5*/int D_06A5 = -20;/*y offset of the gypsy scene*/
/*06A7*/int D_06A7[8] = {239, 14, 231, 103, 228, 5, 15, 219};/*pattern color of each virtue (MCGA; set per video mode by CreateCharacter)*/
/*06B7*/int D_06B7[8][8] = {
	{-1, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f},
	{0x69, -1, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75},
	{0x6a, 0x70, -1, 0x76, 0x77, 0x78, 0x79, 0x7a},
	{0x6b, 0x71, 0x76, -1, 0x7b, 0x7c, 0x7d, 0x7e},
	{0x6c, 0x72, 0x77, 0x7b, -1, 0x7f, 0x80, 0x81},
	{0x6d, 0x73, 0x78, 0x7c, 0x7f, -1, 0x82, 0x83},
	{0x6e, 0x74, 0x79, 0x7d, 0x80, 0x82, -1, 0x84},
	{0x6f, 0x75, 0x7a, 0x7e, 0x81, 0x83, 0x84, -1}
};/*the question (its text number) for each pair of virtues (0 Honesty, 1 Compassion, 2 Valor, 3 Justice, 4 Sacrifice, 5 Honor, 6 Spirituality, 7 Humility)*/
/*0737*/int D_0737[28] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6};/*virtue chosen by answer A, by question - 0x69*/
/*076F*/int D_076F[28] = {1, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7, 4, 5, 6, 7, 5, 6, 7, 6, 7, 7};/*virtue chosen by answer B, by question - 0x69*/
/*07A7*/int D_07A7[8] = {0, 0, 2, 0, 1, 1, 1, 0};/*strength gained per virtue*/
/*07B7*/int D_07B7[8] = {0, 2, 0, 1, 1, 0, 1, 0};/*dexterity gained per virtue*/
/*07C7*/int D_07C7[8] = {2, 0, 0, 1, 0, 1, 1, 0};/*intelligence gained per virtue*/
/*07D7*/int D_07D7 = 1;/*shape base: the arm, the A/B buttons*/
/*07D9*/int D_07D9 = 9;/*shape base: hand and vials (streams of vials 6, 7)*/
/*07DB*/int D_07DB = 66;/*shape base: streams, level marks*/
/*07DD*/int D_07DD[4][3][8] = {
	{
		{58, 79, 100, 124, 40, 126, 32, 52},
		{54, 75, 96, 118, 38, 120, 28, 48},
		{54, 75, 96, 118, 38, 120, 28, 48}
	},
	{
		{60, 81, 102, 127, 20, 95, 34, 54},
		{56, 77, 98, 121, 39, 123, 30, 50},
		{56, 77, 98, 121, 39, 123, 30, 50}
	},
	{
		{64, 83, 104, 129, 41, 131, 36, 56},
		{64, 83, 104, 129, 41, 131, 36, 56},
		{64, 83, 104, 129, 41, 131, 36, 56}
	},
	{
		{0, 3, 6, 9, 22, 25, 28, 31},
		{1, 4, 7, 10, 23, 26, 29, 32},
		{2, 5, 8, 11, 24, 27, 30, 33}
	}
};/*shape of the stream: [kind][level][vial]*/
/*089D*/int D_089D[4][3][8] = {
	{
		{4, 4, 4, 4, 20, 20, 20, 20},
		{4, 4, 4, 4, 19, 19, 19, 19},
		{4, 4, 4, 4, 19, 19, 19, 19}
	},
	{
		{4, 4, 4, 4, 23, 23, 23, 23},
		{3, 3, 3, 3, 23, 23, 23, 23},
		{3, 3, 3, 3, 23, 23, 23, 23}
	},
	{
		{3, 3, 3, 3, 27, 27, 27, 27},
		{3, 3, 3, 3, 27, 27, 27, 27},
		{3, 3, 3, 3, 27, 27, 27, 27}
	},
	{
		{3, 3, 3, 3, 3, 3, 3, 3},
		{3, 3, 3, 3, 3, 3, 3, 3},
		{3, 3, 3, 3, 3, 3, 3, 3}
	}
};/*x of the stream: [kind][level][vial]*/
/*095D*/int D_095D[4][3] = {
	{27, 19, 19},
	{22, 14, 14},
	{14, 14, 14},
	{11, 11, 11}
};/*y of the stream: [kind][level]*/
/*0975*/int D_0975[8] = {2, 5, 8, 11, 24, 27, 30, 33};/*per vial: level marks: shape*/
/*0985*/int D_0985[8] = {93, 93, 93, 93, 172, 172, 172, 172};/*per vial: arm x (steps 4-12)*/
/*0995*/int D_0995[8] = {41, 62, 83, 104, 200, 221, 242, 263};/*per vial: vial x*/
/*09A5*/int D_09A5[8] = {29, 50, 71, 92, 202, 223, 244, 264};/*per vial: hand x, steps 3/13*/
/*09B5*/int D_09B5[8] = {107, 107, 107, 107, 170, 170, 170, 170};/*per vial: hand x, steps 4/12*/
/*09C5*/int D_09C5[8] = {107, 107, 107, 107, 170, 170, 170, 170};/*per vial: hand x, steps 5/11*/
/*09D5*/int D_09D5[8] = {109, 109, 109, 109, 168, 168, 168, 168};/*per vial: hand x, steps 6/10*/
/*09E5*/int D_09E5[8] = {112, 112, 112, 112, 165, 165, 165, 165};/*per vial: hand x, steps 7-9*/
/*09F5*/int D_09F5[8] = {40, 40, 40, 40, 40, 40, 40, 40};/*per vial: hand y, steps 4/12*/
/*0A05*/int D_0A05[8] = {40, 40, 40, 40, 40, 40, 40, 40};/*per vial: hand y, steps 5/11*/
/*0A15*/int D_0A15[8] = {41, 41, 41, 41, 41, 41, 41, 41};/*per vial: hand y, steps 6/10*/
/*0A25*/int D_0A25[8] = {33, 33, 33, 33, 33, 33, 33, 33};/*per vial: hand y, steps 7-9*/
/*0A35*/int D_0A35[8] = {11, 11, 11, 11, 14, 14, 14, 14};/*per vial: vial shape, standing*/
/*0A45*/int D_0A45[8] = {12, 12, 12, 12, 13, 13, 13, 13};/*per vial: vial shape, steps 1-2/14-15*/
/*0A55*/int D_0A55[8] = {9, 9, 9, 9, 17, 17, 17, 17};/*per vial: vial shape, steps 3/13*/
/*0A65*/int D_0A65[8] = {9, 9, 9, 9, 17, 17, 17, 17};/*per vial: vial shape, steps 4/12*/
/*0A75*/int D_0A75[8] = {10, 10, 10, 10, 18, 18, 18, 18};/*per vial: vial shape, steps 5/11*/
/*0A85*/int D_0A85[8] = {16, 16, 16, 16, 46, 46, 46, 46};/*per vial: vial shape, steps 6/10*/
/*0A95*/int D_0A95[8] = {15, 15, 15, 15, 37, 37, 37, 37};/*per vial: vial shape, steps 7-9*/
/*0AA5*/int D_0AA5[8] = {0, 0, 0, 0, 5, 5, 5, 5};/*per vial: arm shape, steps 1/15*/
/*0AB5*/int D_0AB5[8] = {1, 1, 1, 1, 4, 4, 4, 4};/*per vial: arm shape, steps 2-3/13-14*/
/*0AC5*/int D_0AC5[8] = {2, 2, 2, 2, 3, 3, 3, 3};/*per vial: arm shape, steps 4-12*/
/*0AD5*/int D_0AD5[3][8] = {
	{43, 67, 86, 107, 36, 116, 25, 44},
	{44, 68, 87, 108, 35, 115, 24, 43},
	{45, 69, 88, 109, 34, 114, 23, 42}
};/*shape of the liquid in the bowl: [3 - level][vial]*/
/*0B05*/int D_0B05[3][8] = {
	{146, 146, 146, 146, 169, 169, 169, 169},
	{145, 145, 145, 145, 169, 169, 169, 169},
	{148, 148, 148, 148, 170, 170, 170, 170}
};/*x of the liquid in the bowl: [3 - level][vial]*/
/*0B35*/int D_0B35[3] = {50, 55, 64};/*y of the liquid in the bowl: [3 - level]*/

extern int D_5B16;
extern int D_5B18[];/*level of each vial*/
extern int D_5B28;/*number of colors in the pattern*/
extern int D_5B2A[];/*virtues*/
extern int D_5B3A[];/*colors of the pattern*/
extern char D_5B7A[];/*pattern*/

char far *C_1463_0002();/*load a whole file into far memory*/
int C_13F2_01B9(void far *, void far *, int);/*far copy (from, to, size)*/
int C_12EE_0249(char far *, ...);/*printf in the text window*/
char far *C_13BC_000C();/*load an image file*/

/*C_0406_0006*/int WriteNewCharacter() {
	char far *buf;
	int fd;

	buf = C_1463_0002("savegame\\objlist");
	if(buf[0x1c72])
		return 1;
	buf[0x901] = D_5A40;
	buf[0xa01] = D_5A42;
	buf[0xb01] = D_5A44;
	buf[0x13f2] = D_5A44 * 2;
	buf[0x1c71] = D_5A46 == 'F';
	buf[0x1bf9] = D_0692;
	C_13F2_01B9(D_5A30, buf + 0xf00, 0xe);
	buf[0xc02] = D_5A60 % 0x100;
	buf[0xc03] = D_5A60 / 0x100;
	if(D_5A64 > 0xff)
		D_5A64 = 0xff;
	buf[0xe01] = D_5A64;
	buf[0xff2] = D_5A62;
	buf[0x1c72] = D_5A6A + 1;
	fd = C_0000_09F3("savegame\\objlist");
	C_0000_0BCA(fd, 0L, 0x1c73L, buf);
	C_0000_0A32(fd);
	farfree(buf);
	return 0;
}

/*ask the name and the sex of the new character; 0 if the player gave up (escape)*/
/*C_0406_012A*/int AskNameAndSex() {
	struct tSurface win;
	int ok;
	int y;

	win = D_599C;
	switch(D_5044) {
		case 4:
		case 7:
			DRV(0xc)(&win, 3);
			D_5677 = 0;
			D_5678 = 3;
			break;
		case 0xd:
			win.segment = D_5710.segment;
			win.pScanlines = D_5710.pScanlines;
		case 9:
			DRV(0xc)(&win, 0xe);
			D_5677 = 0;
			D_5678 = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&win, 0x31);
			D_5677 = 0x3d;
			D_5678 = 0x31;
			break;
	}
	win.right -= 0x10;
	win.left += 0x10;
	win.top += 0x10;
	win.bottom -= 0x10;
	if(D_5044 == 0xd)
		win.segment = D_5710.segment;
	D_566D = &win;
	C_1288_0002();
	D_567D = 2;
	C_12EE_0249("By what name shalt thou be called?\n\n%P");
	if(D_59B8 == 0 && D_5044 == 0x13)
		C_136D_014D(&D_5702, &D_565F);
	else if(D_5044 != 0xd) {
		DRV(0xf)(&D_5702, &D_565F);
	}
	D_59B8 = 1;
	C_12EA_000A(&D_5710, &win, 0);
	y = D_5671;
	do {
		D_566F = D_566D->left;
		D_5671 = y;
		C_1288_0024();
		D_566F = 0x35;
		if(!C_00E1_028A(D_5A30, 0xd))
			return 0;
		D_566F = D_566D->left;
		C_10E1_000A();
		D_567D = 2;
		C_12EE_0249("%s\n\n%P", D_5A30);
		C_12EE_0249("And art thou Male, or Female?\n\n%P");
		do {
			D_566F = 0x9d;
			C_10E1_000A();
			ok = C_00E1_028A(&D_5A46, 1);
			D_5A46 = toupper(D_5A46);
		} while(D_5A46 != 'M' && D_5A46 != 'F' && ok);
	} while(!ok);
	C_0263_055F();
	C_0263_055F();
	return 1;
}

/*add a color to the gypsy's pattern: the 16 entries of the pattern are shared
between the colors added so far*/
/*C_0406_033B*/AddPatternColor(int c) {
	int j;
	int i;

	D_5AF0 -= 4;
	D_5B3A[D_5B28++] = c;
	for(i = 1; i <= D_5B28; i++)
		for(j = 0; j < 0x10 / D_5B28; j++)
			D_5B7A[j * i] = D_5B3A[i - 1];
}

/*draw the gypsy's pattern in D_5AE6's area: noise (taken from this very code)
through the colors of the pattern*/
/*C_0406_038C*/DrawPattern() {
	char far *dst;
	char far *src;
	struct tSurface s;
	int x;
	int y;

	src = MK_FP(_CS, rand() % 0x800);
	switch(D_5044) {
		case 4:
		case 7:
			/*CGA, Hercules: 4 pixels a byte*/
			for(y = D_5AE6.top; y <= D_5AE6.bottom; y++) {
				dst = MK_FP(D_5AE6.segment, D_5AE6.pScanlines[y]);
				for(x = D_5AE6.left / 4; x <= D_5AE6.right / 4; x++, src += 4)
					dst[x] =
						(D_5B7A[*src & 0xf] & 3) +
						((D_5B7A[(*src + 1) & 0xf] & 3) << 2) +
						((D_5B7A[(*src + 2) & 0xf] & 3) << 4) +
						((D_5B7A[(*src + 3) & 0xf] & 3) << 6);
			}
			break;
		case 9:
			/*Tandy (back buffer), EGA (D_5748): 2 pixels a byte*/
			s = D_5AE6;
			s.segment = D_5732.segment;
			s.pScanlines = D_5732.pScanlines;
			goto twoPixels;
		case 0xd:
			s = D_5AE6;
			s.segment = D_5748.segment;
			s.pScanlines = D_5748.pScanlines;
		twoPixels:
			for(y = D_5AE6.top; y <= D_5AE6.bottom; y++) {
				dst = MK_FP(s.segment, s.pScanlines[y]);
				for(x = D_5AE6.left / 2; x <= D_5AE6.right / 2; x++, src += 2)
					dst[x] =
						(D_5B7A[*src & 0xf] & 0xf) +
						((D_5B7A[(*src + 1) & 0xf] & 0xf) << 4);
			}
			if(D_5044 == 0xd) {
				D_5AE6.segment = D_5710.segment;
				D_5AE6.pScanlines = D_5710.pScanlines;
				DRV(0xf)(&s, &D_5AE6);
			}
			break;
		case 0x13:
			/*a pixel a byte*/
			for(y = D_5AE6.top; y <= D_5AE6.bottom; y++) {
				dst = MK_FP(D_5AE6.segment, D_5AE6.pScanlines[y]);
				for(x = D_5AE6.left; x <= D_5AE6.right; x++) {
					dst[x] = D_5B7A[*src & 0xf];
					src++;
				}
			}
			break;
	}
}

/*draw the stream poured from vial n (the gypsy's frame shape) at x, y*/
/*C_0406_05D3*/DrawPouring(int n, int shape, int x, int y) {
	int ofs;
	int kind;
	int level;

	switch(shape) {
		case 9: case 0x11: kind = 0; break;
		case 0xa: case 0x12: kind = 1; break;
		case 0x10: case 0x2e: kind = 2; break;
		case 0xb: case 0xc: case 0xd: case 0xe: kind = 3; break;
		case 0xf: case 0x25: return;
	}
	level = D_5B18[n] - 1;
	ofs = (n > 5 && kind != 3)?D_07D9:D_07DB;
	if(level > -1 && level < 3) {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_5732,
			D_089D[kind][level][n] + x + D_0695[n],
			D_095D[kind][level] + y + D_06A5,
			D_5ADC,
			D_07DD[kind][level][n] + ofs
		);
	}
}

/*draw the gypsy's table with her hand at step of pouring vial n (redraw=1: the
pouring itself is not taken into account); 0 when the step is the last one*/
/*C_0406_06F8*/int DrawGypsyStep(int n, int step, int redraw) {
	int done;
	int y0;
	int shape;
	int x;
	int y;
	int i;

	y0 = 0x56;
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	done = 0;
	D_0693 = 0;
	switch(step %= 0x10) {
		case 0:
			x = D_0995[n];
			y = y0;
			shape = D_0A35[n];
			break;
		case 1: case 15:
			DRV(3)(&D_5732, D_0695[n] + D_0995[n] - 6, y0 + D_06A5 - 0x2d, D_5ADC, D_0AA5[n] + D_07D7);
			x = D_0995[n];
			y = y0;
			shape = D_0A45[n];
			break;
		case 2: case 14:
			DRV(3)(&D_5732, D_0695[n] + D_0995[n] - 6, y0 + D_06A5 - 0x2d, D_5ADC, D_0AB5[n] + D_07D7);
			x = D_0995[n];
			y = y0 - 0x25;
			shape = D_0A45[n];
			break;
		case 3: case 13:
			DRV(3)(&D_5732, D_0695[n] + D_0995[n] - 6, y0 + D_06A5 - 0x2d, D_5ADC, D_0AB5[n] + D_07D7);
			x = D_09A5[n];
			y = y0 - 0x29;
			shape = D_0A55[n];
			break;
		case 4: case 12:
			DRV(3)(&D_5732, D_0985[n], D_06A5 + 0x29, D_5ADC, D_0AC5[n] + D_07D7);
			x = D_09B5[n];
			y = D_09F5[n];
			shape = D_0A65[n];
			break;
		case 5: case 11:
			DRV(3)(&D_5732, D_0985[n], D_06A5 + 0x29, D_5ADC, D_0AC5[n] + D_07D7);
			x = D_09C5[n];
			y = D_0A05[n];
			shape = D_0A75[n];
			if(step == 0xb && redraw == 0)
				D_5B18[n] --;
			if(D_5B18[n] == 2 && step == 0xb)
				done = 1;
			if(D_5B18[n] == 3)
				D_0693 = 1;
			break;
		case 6: case 10:
			DRV(3)(&D_5732, D_0985[n], D_06A5 + 0x29, D_5ADC, D_0AC5[n] + D_07D7);
			x = D_09D5[n];
			y = D_0A15[n];
			shape = D_0A85[n];
			if((D_5B18[n] == 1 || D_5B18[n] == 2) && step == 0xa)
				done = 1;
			if(D_5B18[n] == 2)
				D_0693 = 1;
			if(D_5B18[n] == 3)
				done = 1;
			break;
		case 7: case 9:
			DRV(3)(&D_5732, D_0985[n], D_06A5 + 0x29, D_5ADC, D_0AC5[n] + D_07D7);
			x = D_09E5[n];
			y = D_0A25[n];
			shape = D_0A95[n];
			if(D_5B18[n] == 1 && step == 9)
				done = 1;
			if(D_5B18[n] == 2)
				done = 1;
			if(D_5B18[n] == 3)
				done = 1;
			break;
		case 8:
			DRV(3)(&D_5732, D_0985[n], D_06A5 + 0x29, D_5ADC, D_0AC5[n] + D_07D7);
			x = D_09E5[n];
			y = D_0A25[n];
			shape = D_0A95[n];
			if(D_5B18[n] == 1)
				D_0693 = 1;
			if(D_5B18[n] == 2)
				done = 1;
			if(D_5B18[n] == 3)
				done = 1;
			break;
	}
	if((step > 3 && step < 9) || (step > 8 && step < 0xd))
		x -= D_0695[n];
	if(!done) {
		DrawPouring(n, shape, x, y);
		DRV(3)(&D_5732, D_0695[n] + x, D_06A5 + y, D_5ADC, D_07D9 + shape);
		for(i = 0; i < 8; i++) {
			if(i == n)
				continue;
			if(D_5B18[i] > 0) {
				DRV(3)(&D_5732, D_0695[i] + D_0995[i] + 3, D_06A5 + 0x61, D_5ADC, D_0975[i] + D_07DB + D_5B18[i] - 3);
			}
			DRV(3)(&D_5732, D_0695[i] + D_0995[i], y0 + D_06A5, D_5ADC, D_0A35[i] + D_07D9);
		}
		if(step < 9 && D_0693 && redraw == 0) {
			AddPatternColor(D_06A7[n]);
			D_5B16 ++;
		}
		if(D_5B16)
			DrawPattern();
		DRV(3)(&D_5732, 0x8e, D_06A5 + 0x4c, D_5ADC, D_07D9 + 7);
		if(D_0693) {
			DRV(3)(&D_5732,
				D_0B05[3 - D_5B18[n]][n],
				D_0B35[3 - D_5B18[n]] + D_06A5,
				D_5ADC,
				D_0AD5[3 - D_5B18[n]][n] + ((n > 5)?D_07D9:D_07DB)
			);
		}
	}
	return !done;
}

/*draw the mouse pointer on the back buffer (EGA: on the page shown)*/
/*C_0406_0D3D*/DrawMouseOnBackBuffer() {
	if(D_5044 == 0xd) {
		D_5054 = &D_5B08;
		D_5B08.segment = D_5710.segment;
		C_1339_005C();
		C_1339_0038();
	} else {
		D_5054 = &D_5732;
		C_1339_005C();
		C_1339_0038();
		D_5054 = &D_5710;
	}
}

/*shuffle the n ints of tab*/
/*C_0406_0D73*/Shuffle(int *tab, int n) {
	int i;
	int k;
	int j;
	int tmp;

	k = rand() % (n * n) + n;
	while(k) {
		i = rand() % n;
		j = rand() % n;
		tmp = tab[i];
		tab[i] = tab[j];
		tab[j] = tmp;
		k --;
	}
}

/*animate the gypsy's table until a key or a mouse click: the key (upper case),
'A' / 'B' for a click on a card, 1 for a click elsewhere*/
/*C_0406_0DDE*/char WaitForAnswer(int mouse) {
	int key;
	struct tSurface cardA;
	struct tSurface cardB;
	int got;
	int first;

	got = 0;
	key = 0;
	first = 1;
	cardA = cardB = D_5732;
	cardA.right = 0x125;
	cardA.left = 0x116;
	cardA.top = 0xae;
	cardA.bottom = 0xbc;
	cardB.right = 0x136;
	cardB.left = 0x127;
	cardB.top = 0xae;
	cardB.bottom = 0xbc;
	if(D_5044 == 0xd) {
		D_565F.segment = D_5732.segment = D_5710.segment;
		D_5710.segment = D_5AFA.segment;
		C_09A0_21C5();
		D_5710.segment = D_565F.segment;
	}
	while(!got) {
		if(D_5044 == 0xd) {
			D_5732.segment = D_5710.segment;
			DRV(0xf)(&D_5AFA, &D_5710);
		} else {
			DRV(3)(&D_5732, 0, D_06A5, D_5ADC, 0);
		}
		C_132C_0008();
		DrawGypsyStep(0, 0, 1);
		C_132C_0008();
		if(D_5044 != 0xd)
			C_09A0_21C5();
		C_132C_0008();
		if(mouse)
			DrawMouseOnBackBuffer();
		if(D_5044 == 0xd) {
			DRV(0x15)(&D_5710);
			if(first) {
				first = 0;
				DRV(0xf)(&D_5732, &D_5710);
			}
		} else {
			DRV(0xf)(&D_5732, &D_5710);
		}
		C_132C_0008();
		if(D_5046 || D_5047) {
			while(D_5046 || D_5047)
				C_132C_0008();
			if(C_1344_00A7(&cardA))
				key = 'A';
			else if(C_1344_00A7(&cardB))
				key = 'B';
			else
				key = 1;
			got = 1;
		}
		if(kbhit()) {
			key = toupper(getch());
			got = 1;
		}
	}
	return key;
}

/*ask question q: the virtue chosen*/
/*C_0406_0F99*/int AskQuestion(int q) {
	char key;

	if(D_505E) {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_6072, 0x10f, 0x29, D_5ADC, D_07D7 + 6);
		DRV(3)(&D_6072, 0x120, 0x29, D_5ADC, D_07D7 + 7);
	}
	do {
		key = WaitForAnswer(1);
	} while(key != 'A' && key != 'B');
	if(key == 'A')
		return D_0737[q - 0x69];
	return D_076F[q - 0x69];
}

/*the virtue v was chosen: its attribute points, and the gypsy pours its vial*/
/*C_0406_1040*/ChooseVirtue(int v) {
	int count;
	int step;
	int i;

	D_5A40 += D_07A7[v];
	D_5A42 += D_07B7[v];
	D_5A44 += D_07C7[v];
	if(v == 7)
		D_0692 += 3;
	count = C_06E9_0AF0(3, 0);
	C_132C_0008();
	if(D_5044 == 0xd) {
		D_565F.segment = D_5732.segment = D_5710.segment;
		DRV(0xf)(&D_5AFA, &D_5710);
		D_5710.segment = D_5AFA.segment;
		C_09A0_21C5();
		D_5710.segment = D_565F.segment;
	} else {
		DRV(3)(&D_5732, 0, D_06A5, D_5ADC, 0);
	}
	C_132C_0008();
	for(step = 0; step < 0x11; step++) {
		if(!DrawGypsyStep(v, step, 0))
			continue;
		C_132C_0008();
		if(D_5044 != 0xd)
			C_09A0_21C5();
		C_132C_0008();
		DrawMouseOnBackBuffer();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
		C_132C_0008();
		for(i = 0; i < count; i++) {
			if(D_5044 == 0xd) {
				D_5732.segment = D_5710.segment;
				DRV(0xf)(&D_5AFA, &D_5710);
			} else {
				DRV(3)(&D_5732, 0, D_06A5, D_5ADC, 0);
			}
			C_132C_0008();
			DrawGypsyStep(v, step, 1);
			C_132C_0008();
			if(D_5044 != 0xd)
				C_09A0_21C5();
			C_132C_0008();
			DrawMouseOnBackBuffer();
			if(D_5044 != 0xd) {
				DRV(0xf)(&D_5732, &D_5710);
			} else {
				DRV(0x15)(&D_5710);
			}
			C_132C_0008();
		}
		if(D_5044 == 0xd) {
			D_5732.segment = D_5710.segment;
			DRV(0xf)(&D_5AFA, &D_5710);
		} else {
			DRV(3)(&D_5732, 0, D_06A5, D_5ADC, 0);
		}
		C_132C_0008();
	}
}

/*create a new character: the name and the sex on the vellum, the woods, the
gypsy and her questions, then write the character into savegame\objlist;
1 if the player gave up at the name, else what WriteNewCharacter said*/
/*C_0406_1269*/int CreateCharacter() {
	int virtue;
	int speed;
	int done;
	int ret;
	struct tSurface save;
	int i;
	int q;

	if(D_5044 == 0xd) {
		D_5732.segment = D_5710.segment ^ 0x200;
		DRV(0xf)(&D_5732, &D_5710);
		D_5710.segment ^= 0x200;
		D_5732.segment = D_5710.segment;
		D_565F.segment = D_5710.segment;
		D_5B08 = D_5710;
	}
	D_599C = D_5732;
	D_5980 = D_5710;
	D_599C.right = D_5980.right = 0x124;
	D_599C.left = D_5980.left = 0x1c;
	D_599C.top = D_5980.top = 0x57;
	D_599C.bottom = D_5980.bottom = 0xbd;
	D_5702 = D_565F;
	C_12EA_000A(&D_5732, &D_5702, 0);
	if(D_5044 != 0xd) {
		DRV(0xc)(&D_5702, 0);
	}
	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.segment = D_5710.segment;
	save.right = D_565F.right - D_565F.left;
	save.bottom = D_565F.bottom - D_565F.top;
	save.left = save.top = 0;
	C_1277_000C(&save);
	/*the vellum*/
	C_0BC1_0079(2);
	switch(D_5044) {
		case 4:
			D_5AD6 = C_13BC_000C("vellum1.cga");
			D_06A7[3] = D_06A7[1] = D_06A7[6] = 3;
			D_06A7[0] = D_06A7[2] = D_06A7[4] = D_06A7[5] = 2;
			D_06A7[7] = 1;
			break;
		case 7:
			D_5AD6 = C_13BC_000C("vellum1.hrc");
			D_06A7[3] = D_06A7[1] = D_06A7[6] = 3;
			D_06A7[0] = D_06A7[2] = D_06A7[4] = D_06A7[5] = 2;
			D_06A7[7] = 1;
			break;
		case 9:
			D_5AD6 = C_13BC_000C("vellum1.tga");
			D_06A7[0] = 9;
			D_06A7[2] = 4;
			D_06A7[3] = 0xa;
			D_06A7[4] = 0xc;
			D_06A7[7] = 7;
			break;
		case 0xd:
			D_5AD6 = C_13BC_000C("vellum1.ega");
			D_06A7[0] = 9;
			D_06A7[2] = 4;
			D_06A7[3] = 0xa;
			D_06A7[4] = 0xc;
			D_06A7[7] = 7;
			break;
		case 0x13:
			D_5AD6 = C_13BC_000C("vellum1.shp");
			break;
	}
	C_0BC1_0079(2);
	switch(D_5044) {
		case 4: D_5AD2 = C_13BC_000C("montage.cga"); break;
		case 7: D_5AD2 = C_13BC_000C("montage.hrc"); break;
		case 9: D_5AD2 = C_13BC_000C("montage.tga"); break;
		case 0xd: D_5AD2 = C_13BC_000C("montage.ega"); break;
		case 0x13: D_5AD2 = C_13BC_000C("montage.shp"); break;
	}
	DRV(0xf)(&D_565F, &save);
	if(D_5044 == 0x13)
		C_136D_0346(&D_565F, 0);
	if(D_5044 == 0xd)
		D_5702.segment = D_565F.segment = D_5710.segment;
	DRV(0xc)(&D_565F, 0);
	C_00E1_04F9();
	DRV(3)(&D_5702, D_5702.left, D_5702.top, D_5AD6, 0);
	D_5A2E = D_59B8 = 0;
	C_1339_0000();
	C_00E1_178A(4);
	D_575A = 0;
	do {
		C_1339_0016();
		if(!AskNameAndSex()) {
			D_575A = 1;
			break;
		}
		C_1339_0000();
	} while(!C_0263_0987(0));
	if(D_575A) {
		/*given up*/
		if(D_5044 == 0xd) {
			D_565F.segment = D_5710.segment;
			D_565F.pScanlines = D_5710.pScanlines;
		}
		farfree(D_5AD2);
		farfree(D_5AD6);
		if(D_5044 == 0x13)
			C_136D_0346(&D_565F, 0);
		DRV(0xc)(&D_565F, 0);
		C_00E1_04F9();
		if(D_5044 == 0x13) {
			C_136D_014D(&save, &D_565F);
		} else if(D_5044 == 0xd) {
			DRV(0xf)(&save, &D_565F);
			DRV(0x15)(&D_5710);
		} else {
			DRV(0xf)(&save, &D_565F);
		}
		C_1277_00D8(&save);
		if(D_5044 != 0xd)
			C_1277_00D8(&D_6072);
		C_00E1_178A(0);
		return 1;
	}
	C_1339_0016();
	farfree(D_5AD2);
	farfree(D_5AD6);
	C_1277_00D8(&save);
	C_1482_00BE();
	/*the woods*/
	C_0BC1_0079(2);
	switch(D_5044) {
		case 4: D_5ADC = C_13BC_000C("woods.cga"); break;
		case 7: D_5ADC = C_13BC_000C("woods.hrc"); break;
		case 9: D_5ADC = C_13BC_000C("woods.tga"); break;
		case 0xd: D_5ADC = C_13BC_000C("woods.ega"); break;
		case 0x13: D_5ADC = C_13BC_000C("woods.shp"); break;
	}
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	if(D_5044 != 0xd) {
		DRV(0xc)(&D_5732, 0);
	}
	if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	}
	C_0BC1_03D2(4);
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	if(D_5044 == 0xd)
		D_6072.segment = 0xa600;
	C_09A0_000B(100);
	do {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_5732, 0, 0, D_5ADC, 0);
		C_09A0_21C5();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
	} while(!C_06E9_0351());
	speed = C_06E9_0AF0(2, 10);
	i = 0;
	done = 0;
	/*the woods scroll by; one more round once i reaches 0xbf (the constant
	1 after the comma is what gives the original's mov al,1/or al,al)*/
	while(!done && (done = i == 0xbf, 1)) {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_5732, D_5732.left - i, 0, D_5ADC, 0);
		DRV(3)(&D_5732, D_5732.left + 0x140 - i, 0, D_5ADC, 1);
		if(D_5044 == 0x13)
			C_1368_0015();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
		if(C_06E9_0351())
			i = 0xbf - speed;
		i += speed;
		i += (0xbf - i) * (i >= 0xbf);
	}
	C_09A0_000B(0x65);
	do {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_5732, D_5732.left - 0xbf, 0, D_5ADC, 0);
		DRV(3)(&D_5732, D_5732.left + 0x81, 0, D_5ADC, 1);
		C_09A0_21C5();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
	} while(!C_06E9_0351());
	C_09A0_000B(0x66);
	do {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DRV(3)(&D_5732, D_5732.left - 0xbf, 0, D_5ADC, 0);
		DRV(3)(&D_5732, D_5732.left + 0x81, 0, D_5ADC, 1);
		C_09A0_21C5();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
	} while(!C_06E9_0351());
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, D_5732.left - 0xbf, 0, D_5ADC, 0);
	DRV(3)(&D_5732, D_5732.left + 0x81, 0, D_5ADC, 1);
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	farfree(D_5ADC);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	D_575A = 0;
	DRV(0xc)(&D_5710, 0);
	if(D_5044 != 0xd) {
		DRV(0xc)(&D_5732, 0);
	} else {
		DRV(0x15)(&D_5710);
		DRV(0xc)(&D_5710, 0);
	}
	/*the gypsy*/
	C_0BC1_0079(2);
	C_0BC1_03D2(5);
	switch(D_5044) {
		case 4: D_5ADC = C_13BC_000C("gypsy.cga"); break;
		case 7: D_5ADC = C_13BC_000C("gypsy.hrc"); break;
		case 9: D_5ADC = C_13BC_000C("gypsy.tga"); break;
		case 0xd: D_5ADC = C_13BC_000C("gypsy.ega"); break;
		case 0x13: D_5ADC = C_13BC_000C("gypsy.shp"); break;
	}
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	strcpy(D_5A4E, "Avatar");
	D_5A5E = 'G';
	D_5A40 = D_5A42 = D_5A44 = 0xf;
	for(i = 0; i < 8; i++) {
		D_5B2A[i] = i;
		D_5B18[i] = 3;
		D_5B7A[i] = 0xff;
	}
	D_5B16 = 0;
	D_5B28 = 0;
	D_5AE6.right = 0xac;
	D_5AE6.left = 0x93;
	D_5AE6.bottom = D_5AE6.top = D_06A5 + 0x7f;
	C_12EA_000A(&D_5732, &D_5AE6, 0);
	DRV(3)(&D_5732, 0, D_06A5, D_5ADC, 0);
	if(D_5044 == 0xd) {
		D_5AFA = D_5710;
		D_5AFA.segment = 0xa800;
		DRV(0xf)(&D_5710, &D_5AFA);
	}
	DrawGypsyStep(0, 0, 1);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	C_09A0_000B(0x67);
	WaitForAnswer(0);
	C_09A0_000B(0x68);
	WaitForAnswer(0);
	C_1339_0000();
	/*the questions: the 8 virtues in pairs, then the 4 winners, then the 2 last*/
	Shuffle(D_5B2A, 8);
	for(i = 0; i < 4; i++) {
		q = D_06B7[D_5B2A[i * 2]][D_5B2A[i * 2 + 1]];
		C_09A0_000B(q);
		virtue = AskQuestion(q);
		ChooseVirtue(virtue);
		D_5B2A[i] = virtue;
	}
	Shuffle(D_5B2A, 4);
	for(i = 0; i < 2; i++) {
		q = D_06B7[D_5B2A[i * 2]][D_5B2A[i * 2 + 1]];
		C_09A0_000B(q);
		virtue = AskQuestion(q);
		ChooseVirtue(virtue);
		D_5B2A[i] = virtue;
	}
	q = D_06B7[D_5B2A[0]][D_5B2A[1]];
	C_09A0_000B(q);
	virtue = AskQuestion(q);
	ChooseVirtue(virtue);
	C_1339_0016();
	C_09A0_000B(0x85);
	WaitForAnswer(0);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	}
	/*the gypsy's pattern*/
	C_0BC1_03D2(6);
	D_5AE6.right = 0xcb;
	D_5AE6.left = 0x74;
	D_5AE6.top = 0x23;
	D_5AE6.bottom = 0x7f;
	DrawPattern();
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc6);
	C_09A0_000B(0x86);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	do {
		if(D_5044 == 0xd)
			D_5732.segment = D_5710.segment;
		DrawPattern();
		DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc6);
		C_09A0_21C5();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
		}
	} while(!C_06E9_0351());
	DrawPattern();
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc6);
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0x75);
	switch(D_5044) {
		case 4:
		case 7:
			DRV(0xc)(&D_5732, 1);
			ret = C_06E9_0AF0(600, 800);
			C_13F2_037A(&D_5732, &D_5710, ret);
			break;
		case 9:
			DRV(0xc)(&D_5732, 0xb);
			ret = C_06E9_0AF0(800, 1200);
			C_13F2_0504(&D_5732, &D_5710, ret);
			break;
		case 0xd:
			D_5732.segment = D_5710.segment;
			DRV(0xc)(&D_5710, 0xb);
			D_5710.segment ^= 0x200;
			ret = C_06E9_0AF0(300, 400);
			C_13F2_01D5(&D_5732, &D_5710, ret);
			D_5710.segment ^= 0x200;
			DRV(0x15)(&D_5710);
			break;
		case 0x13:
			DRV(0xc)(&D_5710, 0x75);
			break;
	}
	C_0BC1_03D2(6);
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc7);
	C_09A0_000B(0x87);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		D_5710.segment ^= 0x200;
		ret = C_06E9_0AF0(300, 400);
		C_13F2_01D5(&D_5732, &D_5710, ret);
		D_5710.segment ^= 0x200;
		DRV(0x15)(&D_5710);
	} else if(D_5044 == 9) {
		ret = C_06E9_0AF0(400, 600);
		C_13F2_0504(&D_5732, &D_5710, ret);
	} else {
		ret = C_06E9_0AF0(300, 400);
		C_13F2_037A(&D_5732, &D_5710, ret);
	}
	C_1482_0113(2000);
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	C_09A0_21C5();
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	C_09A0_000B(0x88);
	while(!C_06E9_0351());
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc7);
	C_09A0_21C5();
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	C_09A0_000B(0x89);
	while(!C_06E9_0351());
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc7);
	C_09A0_21C5();
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	while(!C_06E9_0351());
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	DRV(3)(&D_5732, 0, 0, D_5ADC, 0xc7);
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		DRV(0x15)(&D_5710);
	}
	farfree(D_5ADC);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	if(D_5044 != 0xd) {
		DRV(0xc)(&D_5732, 0);
	} else {
		DRV(0x15)(&D_5710);
		DRV(0xc)(&D_5710, 0);
		D_5732 = D_5710;
	}
	C_00E1_04F9();
	/*the new character*/
	C_0BC1_0079(8);
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	D_5A60 = 0x172;
	D_5A64 = 0x5a;
	D_5A62 = 3;
	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.segment = 0xa600;
	ret = 0;
	if(WriteNewCharacter()) {
		/*the disk already holds a character*/
		C_09A0_000B(0x3e6);
		C_09A0_21C5();
		ret = 1;
		if(D_5044 == 0xd) {
			DRV(0x15)(&D_5710);
			D_5732 = D_5710;
		} else {
			DRV(0xf)(&D_5732, &D_5710);
		}
		while(!C_06E9_0351());
		DRV(0xc)(&D_5732, 0);
		if(D_5044 == 0xd) {
			DRV(0x15)(&D_5710);
			D_5732 = D_5710;
			DRV(0xc)(&D_5732, 0);
		} else {
			DRV(0xf)(&D_5732, &D_5710);
		}
	}
	C_0BC1_0079(2);
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	C_00E1_178A(0);
	return ret;
}
