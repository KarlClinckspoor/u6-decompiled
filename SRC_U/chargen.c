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

extern int16 videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface screen;/*the screen*/
extern struct tSurface backBuffer;/*the back buffer*/
extern struct tSurface textArea;/*the vellum's area*/
extern struct tSurface textAreaCopy;/*copy of textArea*/
extern struct tSurface egaSaved;
extern struct tSurface dialogArea;
extern struct tSurface nameWindow;/*the text window of the name question*/
extern struct tSurface patternArea;/*the area of the gypsy's pattern*/
extern struct tSurface egaPage2;/*EGA: the second page (segment a800)*/
extern struct tSurface egaScreen;/*EGA: the screen, for the mouse pointer*/
extern struct tSurface workArea;/*a 320x56 work surface (the A/B buttons)*/
extern int16 menuDrawn;/*set when the player gave up*/
extern int16 transferFrom;/*here: the vellum has been shown (transfer.c: the game the character comes from)*/
extern int16 portraitIdx;

/*the new character*/
extern char newName[];/*name*/
extern int16 newStr, newDex, newInt;/*strength, dexterity, intelligence*/
extern char newSex;/*sex: 'M' or 'F'*/
extern char newClass[];/*"Avatar"*/
extern char newGood;/*'G'*/
extern int16 newExp;/*experience*/
extern int16 newLevel;/*level*/
extern int16 newHp;/*hit points*/
extern char newPortrait;

extern char far *portraits;/*montage image*/
extern char far *vellum;/*vellum image*/
extern char far *sceneShapes;/*woods / gypsy image*/

/*the gypsy's table: this module's initialised data*/
/*0692*/char avatarKarma = 75;/*karma (75; +3 when Humility is chosen): objlist 0x1bf9*/
/*0693*/int16 pourColor = 0;/*the vial being poured gives its color this step*/
/*0695*/int16 vialOffsetX[8] = {10, 10, 10, 10, -13, -13, -13, -13};/*x offset of each vial (left side +10, right side -13)*/
/*06A5*/int16 sceneY = -20;/*y offset of the gypsy scene*/
/*06A7*/int16 virtueColor[8] = {239, 14, 231, 103, 228, 5, 15, 219};/*pattern color of each virtue (MCGA; set per video mode by CreateCharacter)*/
/*06B7*/int16 questionOf[8][8] = {
	{-1, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f},
	{0x69, -1, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75},
	{0x6a, 0x70, -1, 0x76, 0x77, 0x78, 0x79, 0x7a},
	{0x6b, 0x71, 0x76, -1, 0x7b, 0x7c, 0x7d, 0x7e},
	{0x6c, 0x72, 0x77, 0x7b, -1, 0x7f, 0x80, 0x81},
	{0x6d, 0x73, 0x78, 0x7c, 0x7f, -1, 0x82, 0x83},
	{0x6e, 0x74, 0x79, 0x7d, 0x80, 0x82, -1, 0x84},
	{0x6f, 0x75, 0x7a, 0x7e, 0x81, 0x83, 0x84, -1}
};/*the question (its text number) for each pair of virtues (0 Honesty, 1 Compassion, 2 Valor, 3 Justice, 4 Sacrifice, 5 Honor, 6 Spirituality, 7 Humility)*/
/*0737*/int16 answerAVirtue[28] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6};/*virtue chosen by answer A, by question - 0x69*/
/*076F*/int16 answerBVirtue[28] = {1, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7, 4, 5, 6, 7, 5, 6, 7, 6, 7, 7};/*virtue chosen by answer B, by question - 0x69*/
/*07A7*/int16 strGain[8] = {0, 0, 2, 0, 1, 1, 1, 0};/*strength gained per virtue*/
/*07B7*/int16 dexGain[8] = {0, 2, 0, 1, 1, 0, 1, 0};/*dexterity gained per virtue*/
/*07C7*/int16 intGain[8] = {2, 0, 0, 1, 0, 1, 1, 0};/*intelligence gained per virtue*/
/*07D7*/int16 shapeArm = 1;/*shape base: the arm, the A/B buttons*/
/*07D9*/int16 shapeVial = 9;/*shape base: hand and vials (streams of vials 6, 7)*/
/*07DB*/int16 shapeStream = 66;/*shape base: streams, level marks*/
/*07DD*/int16 markShape[4][3][8] = {
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
/*089D*/int16 markX[4][3][8] = {
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
/*095D*/int16 markY[4][3] = {
	{27, 19, 19},
	{22, 14, 14},
	{14, 14, 14},
	{11, 11, 11}
};/*y of the stream: [kind][level]*/
/*0975*/int16 vialMarkShape[8] = {2, 5, 8, 11, 24, 27, 30, 33};/*per vial: level marks: shape*/
/*0985*/int16 armX[8] = {93, 93, 93, 93, 172, 172, 172, 172};/*per vial: arm x (steps 4-12)*/
/*0995*/int16 vialX[8] = {41, 62, 83, 104, 200, 221, 242, 263};/*per vial: vial x*/
/*09A5*/int16 handX3[8] = {29, 50, 71, 92, 202, 223, 244, 264};/*per vial: hand x, steps 3/13*/
/*09B5*/int16 handX4[8] = {107, 107, 107, 107, 170, 170, 170, 170};/*per vial: hand x, steps 4/12*/
/*09C5*/int16 handX5[8] = {107, 107, 107, 107, 170, 170, 170, 170};/*per vial: hand x, steps 5/11*/
/*09D5*/int16 handX6[8] = {109, 109, 109, 109, 168, 168, 168, 168};/*per vial: hand x, steps 6/10*/
/*09E5*/int16 handX7[8] = {112, 112, 112, 112, 165, 165, 165, 165};/*per vial: hand x, steps 7-9*/
/*09F5*/int16 handY4[8] = {40, 40, 40, 40, 40, 40, 40, 40};/*per vial: hand y, steps 4/12*/
/*0A05*/int16 handY5[8] = {40, 40, 40, 40, 40, 40, 40, 40};/*per vial: hand y, steps 5/11*/
/*0A15*/int16 handY6[8] = {41, 41, 41, 41, 41, 41, 41, 41};/*per vial: hand y, steps 6/10*/
/*0A25*/int16 handY7[8] = {33, 33, 33, 33, 33, 33, 33, 33};/*per vial: hand y, steps 7-9*/
/*0A35*/int16 vialStanding[8] = {11, 11, 11, 11, 14, 14, 14, 14};/*per vial: vial shape, standing*/
/*0A45*/int16 vialShape1[8] = {12, 12, 12, 12, 13, 13, 13, 13};/*per vial: vial shape, steps 1-2/14-15*/
/*0A55*/int16 vialShape3[8] = {9, 9, 9, 9, 17, 17, 17, 17};/*per vial: vial shape, steps 3/13*/
/*0A65*/int16 vialShape4[8] = {9, 9, 9, 9, 17, 17, 17, 17};/*per vial: vial shape, steps 4/12*/
/*0A75*/int16 vialShape5[8] = {10, 10, 10, 10, 18, 18, 18, 18};/*per vial: vial shape, steps 5/11*/
/*0A85*/int16 vialShape6[8] = {16, 16, 16, 16, 46, 46, 46, 46};/*per vial: vial shape, steps 6/10*/
/*0A95*/int16 vialShape7[8] = {15, 15, 15, 15, 37, 37, 37, 37};/*per vial: vial shape, steps 7-9*/
/*0AA5*/int16 armShape1[8] = {0, 0, 0, 0, 5, 5, 5, 5};/*per vial: arm shape, steps 1/15*/
/*0AB5*/int16 armShape2[8] = {1, 1, 1, 1, 4, 4, 4, 4};/*per vial: arm shape, steps 2-3/13-14*/
/*0AC5*/int16 armShape4[8] = {2, 2, 2, 2, 3, 3, 3, 3};/*per vial: arm shape, steps 4-12*/
/*0AD5*/int16 streamShape[3][8] = {
	{43, 67, 86, 107, 36, 116, 25, 44},
	{44, 68, 87, 108, 35, 115, 24, 43},
	{45, 69, 88, 109, 34, 114, 23, 42}
};/*shape of the liquid in the bowl: [3 - level][vial]*/
/*0B05*/int16 streamX[3][8] = {
	{146, 146, 146, 146, 169, 169, 169, 169},
	{145, 145, 145, 145, 169, 169, 169, 169},
	{148, 148, 148, 148, 170, 170, 170, 170}
};/*x of the liquid in the bowl: [3 - level][vial]*/
/*0B35*/int16 streamY[3] = {50, 55, 64};/*y of the liquid in the bowl: [3 - level]*/

extern int16 patternAdded;
extern int16 vialLevel[];/*level of each vial*/
extern int16 patternColorCount;/*number of colors in the pattern*/
extern int16 chosenVirtues[];/*virtues*/
extern int16 patternColors[];/*colors of the pattern*/
extern char pattern[];/*pattern*/

char far *LOAD_far();/*load a whole file into far memory*/
int16 DIS_farcpy(void far *, void far *, int16);/*far copy (from, to, size)*/
int16 TXT_sprintf(char far *, ...);/*printf in the text window*/
char far *LZW_load();/*load an image file*/

/*C_0406_0006*/int16 WriteNewCharacter() {
	char far *buf;
	int16 fd;

	buf = LOAD_far("savegame\\objlist");
	if(buf[0x1c72])
		return 1;
	buf[0x901] = newStr;
	buf[0xa01] = newDex;
	buf[0xb01] = newInt;
	buf[0x13f2] = newInt * 2;
	buf[0x1c71] = newSex == 'F';
	buf[0x1bf9] = avatarKarma;
	DIS_farcpy(newName, buf + 0xf00, 0xe);
	buf[0xc02] = newExp % 0x100;
	buf[0xc03] = newExp / 0x100;
	if(newHp > 0xff)
		newHp = 0xff;
	buf[0xe01] = newHp;
	buf[0xff2] = newLevel;
	buf[0x1c72] = newPortrait + 1;
	fd = OSI_open("savegame\\objlist");
	OSI_write(fd, 0L, 0x1c73L, buf);
	OSI_close(fd);
	farfree(buf);
	return 0;
}

/*ask the name and the sex of the new character; 0 if the player gave up (escape)*/
/*C_0406_012A*/int16 AskNameAndSex() {
	struct tSurface win;
	int16 ok;
	int16 y;

	win = nameWindow;
	switch(videoMode) {
		case 4:
		case 7:
			DRV(0xc)(&win, 3);
			textFg = 0;
			textBg = 3;
			break;
		case 0xd:
			win.segment = screen.segment;
			win.pScanlines = screen.pScanlines;
		case 9:
			DRV(0xc)(&win, 0xe);
			textFg = 0;
			textBg = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&win, 0x31);
			textFg = 0x3d;
			textBg = 0x31;
			break;
	}
	win.right -= 0x10;
	win.left += 0x10;
	win.top += 0x10;
	win.bottom -= 0x10;
	if(videoMode == 0xd)
		win.segment = screen.segment;
	textWin = &win;
	TXT_clear();
	textAlign = 2;
	TXT_sprintf("By what name shalt thou be called?\n\n%P");
	if(transferFrom == 0 && videoMode == 0x13)
		FADE_in(&textAreaCopy, &textArea);
	else if(videoMode != 0xd) {
		DRV(0xf)(&textAreaCopy, &textArea);
	}
	transferFrom = 1;
	SURF_share(&screen, &win, 0);
	y = textY;
	do {
		textX = textWin->left;
		textY = y;
		TXT_clearEos();
		textX = 0x35;
		if(!InputString(newName, 0xd))
			return 0;
		textX = textWin->left;
		clreol();
		textAlign = 2;
		TXT_sprintf("%s\n\n%P", newName);
		TXT_sprintf("And art thou Male, or Female?\n\n%P");
		do {
			textX = 0x9d;
			clreol();
			ok = InputString(&newSex, 1);
			newSex = toupper(newSex);
		} while(newSex != 'M' && newSex != 'F' && ok);
	} while(!ok);
	ToggleSex();
	ToggleSex();
	return 1;
}

/*add a color to the gypsy's pattern: the 16 entries of the pattern are shared
between the colors added so far*/
/*C_0406_033B*/AddPatternColor(int16 c) {
	int16 j;
	int16 i;

	patternArea.top -= 4;
	patternColors[patternColorCount++] = c;
	for(i = 1; i <= patternColorCount; i++)
		for(j = 0; j < 0x10 / patternColorCount; j++)
			pattern[j * i] = patternColors[i - 1];
}

/*draw the gypsy's pattern in patternArea's area: noise (taken from this very code)
through the colors of the pattern*/
/*C_0406_038C*/DrawPattern() {
	char far *dst;
	char far *src;
	struct tSurface s;
	int16 x;
	int16 y;

	src = MK_FP(_CS, rand() % 0x800);
	switch(videoMode) {
		case 4:
		case 7:
			/*CGA, Hercules: 4 pixels a byte*/
			for(y = patternArea.top; y <= patternArea.bottom; y++) {
				dst = MK_FP(patternArea.segment, patternArea.pScanlines[y]);
				for(x = patternArea.left / 4; x <= patternArea.right / 4; x++, src += 4)
					dst[x] =
						(pattern[*src & 0xf] & 3) +
						((pattern[(*src + 1) & 0xf] & 3) << 2) +
						((pattern[(*src + 2) & 0xf] & 3) << 4) +
						((pattern[(*src + 3) & 0xf] & 3) << 6);
			}
			break;
		case 9:
			/*Tandy (back buffer), EGA (egaSaved): 2 pixels a byte*/
			s = patternArea;
			s.segment = backBuffer.segment;
			s.pScanlines = backBuffer.pScanlines;
			goto twoPixels;
		case 0xd:
			s = patternArea;
			s.segment = egaSaved.segment;
			s.pScanlines = egaSaved.pScanlines;
		twoPixels:
			for(y = patternArea.top; y <= patternArea.bottom; y++) {
				dst = MK_FP(s.segment, s.pScanlines[y]);
				for(x = patternArea.left / 2; x <= patternArea.right / 2; x++, src += 2)
					dst[x] =
						(pattern[*src & 0xf] & 0xf) +
						((pattern[(*src + 1) & 0xf] & 0xf) << 4);
			}
			if(videoMode == 0xd) {
				patternArea.segment = screen.segment;
				patternArea.pScanlines = screen.pScanlines;
				DRV(0xf)(&s, &patternArea);
			}
			break;
		case 0x13:
			/*a pixel a byte*/
			for(y = patternArea.top; y <= patternArea.bottom; y++) {
				dst = MK_FP(patternArea.segment, patternArea.pScanlines[y]);
				for(x = patternArea.left; x <= patternArea.right; x++) {
					dst[x] = pattern[*src & 0xf];
					src++;
				}
			}
			break;
	}
}

/*draw the stream poured from vial n (the gypsy's frame shape) at x, y*/
/*C_0406_05D3*/DrawPouring(int16 n, int16 shape, int16 x, int16 y) {
	int16 ofs;
	int16 kind;
	int16 level;

	switch(shape) {
		case 9: case 0x11: kind = 0; break;
		case 0xa: case 0x12: kind = 1; break;
		case 0x10: case 0x2e: kind = 2; break;
		case 0xb: case 0xc: case 0xd: case 0xe: kind = 3; break;
		case 0xf: case 0x25: return;
	}
	level = vialLevel[n] - 1;
	ofs = (n > 5 && kind != 3)?shapeVial:shapeStream;
	if(level > -1 && level < 3) {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&backBuffer,
			markX[kind][level][n] + x + vialOffsetX[n],
			markY[kind][level] + y + sceneY,
			sceneShapes,
			markShape[kind][level][n] + ofs
		);
	}
}

/*draw the gypsy's table with her hand at step of pouring vial n (redraw=1: the
pouring itself is not taken into account); 0 when the step is the last one*/
/*C_0406_06F8*/int16 DrawGypsyStep(int16 n, int16 step, int16 redraw) {
	int16 done;
	int16 y0;
	int16 shape;
	int16 x;
	int16 y;
	int16 i;

	y0 = 0x56;
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	done = 0;
	pourColor = 0;
	switch(step %= 0x10) {
		case 0:
			x = vialX[n];
			y = y0;
			shape = vialStanding[n];
			break;
		case 1: case 15:
			DRV(3)(&backBuffer, vialOffsetX[n] + vialX[n] - 6, y0 + sceneY - 0x2d, sceneShapes, armShape1[n] + shapeArm);
			x = vialX[n];
			y = y0;
			shape = vialShape1[n];
			break;
		case 2: case 14:
			DRV(3)(&backBuffer, vialOffsetX[n] + vialX[n] - 6, y0 + sceneY - 0x2d, sceneShapes, armShape2[n] + shapeArm);
			x = vialX[n];
			y = y0 - 0x25;
			shape = vialShape1[n];
			break;
		case 3: case 13:
			DRV(3)(&backBuffer, vialOffsetX[n] + vialX[n] - 6, y0 + sceneY - 0x2d, sceneShapes, armShape2[n] + shapeArm);
			x = handX3[n];
			y = y0 - 0x29;
			shape = vialShape3[n];
			break;
		case 4: case 12:
			DRV(3)(&backBuffer, armX[n], sceneY + 0x29, sceneShapes, armShape4[n] + shapeArm);
			x = handX4[n];
			y = handY4[n];
			shape = vialShape4[n];
			break;
		case 5: case 11:
			DRV(3)(&backBuffer, armX[n], sceneY + 0x29, sceneShapes, armShape4[n] + shapeArm);
			x = handX5[n];
			y = handY5[n];
			shape = vialShape5[n];
			if(step == 0xb && redraw == 0)
				vialLevel[n] --;
			if(vialLevel[n] == 2 && step == 0xb)
				done = 1;
			if(vialLevel[n] == 3)
				pourColor = 1;
			break;
		case 6: case 10:
			DRV(3)(&backBuffer, armX[n], sceneY + 0x29, sceneShapes, armShape4[n] + shapeArm);
			x = handX6[n];
			y = handY6[n];
			shape = vialShape6[n];
			if((vialLevel[n] == 1 || vialLevel[n] == 2) && step == 0xa)
				done = 1;
			if(vialLevel[n] == 2)
				pourColor = 1;
			if(vialLevel[n] == 3)
				done = 1;
			break;
		case 7: case 9:
			DRV(3)(&backBuffer, armX[n], sceneY + 0x29, sceneShapes, armShape4[n] + shapeArm);
			x = handX7[n];
			y = handY7[n];
			shape = vialShape7[n];
			if(vialLevel[n] == 1 && step == 9)
				done = 1;
			if(vialLevel[n] == 2)
				done = 1;
			if(vialLevel[n] == 3)
				done = 1;
			break;
		case 8:
			DRV(3)(&backBuffer, armX[n], sceneY + 0x29, sceneShapes, armShape4[n] + shapeArm);
			x = handX7[n];
			y = handY7[n];
			shape = vialShape7[n];
			if(vialLevel[n] == 1)
				pourColor = 1;
			if(vialLevel[n] == 2)
				done = 1;
			if(vialLevel[n] == 3)
				done = 1;
			break;
	}
	if((step > 3 && step < 9) || (step > 8 && step < 0xd))
		x -= vialOffsetX[n];
	if(!done) {
		DrawPouring(n, shape, x, y);
		DRV(3)(&backBuffer, vialOffsetX[n] + x, sceneY + y, sceneShapes, shapeVial + shape);
		for(i = 0; i < 8; i++) {
			if(i == n)
				continue;
			if(vialLevel[i] > 0) {
				DRV(3)(&backBuffer, vialOffsetX[i] + vialX[i] + 3, sceneY + 0x61, sceneShapes, vialMarkShape[i] + shapeStream + vialLevel[i] - 3);
			}
			DRV(3)(&backBuffer, vialOffsetX[i] + vialX[i], y0 + sceneY, sceneShapes, vialStanding[i] + shapeVial);
		}
		if(step < 9 && pourColor && redraw == 0) {
			AddPatternColor(virtueColor[n]);
			patternAdded ++;
		}
		if(patternAdded)
			DrawPattern();
		DRV(3)(&backBuffer, 0x8e, sceneY + 0x4c, sceneShapes, shapeVial + 7);
		if(pourColor) {
			DRV(3)(&backBuffer,
				streamX[3 - vialLevel[n]][n],
				streamY[3 - vialLevel[n]] + sceneY,
				sceneShapes,
				streamShape[3 - vialLevel[n]][n] + ((n > 5)?shapeVial:shapeStream)
			);
		}
	}
	return !done;
}

/*draw the mouse pointer on the back buffer (EGA: on the page shown)*/
/*C_0406_0D3D*/DrawMouseOnBackBuffer() {
	if(videoMode == 0xd) {
		mouseSurface = &egaScreen;
		egaScreen.segment = screen.segment;
		MOUSE_SAVE_BG();
		MOUSE_DRAW();
	} else {
		mouseSurface = &backBuffer;
		MOUSE_SAVE_BG();
		MOUSE_DRAW();
		mouseSurface = &screen;
	}
}

/*shuffle the n ints of tab*/
/*C_0406_0D73*/Shuffle(int16 *tab, int16 n) {
	int16 i;
	int16 k;
	int16 j;
	int16 tmp;

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
/*C_0406_0DDE*/char WaitForAnswer(int16 mouse) {
	int16 key;
	struct tSurface cardA;
	struct tSurface cardB;
	int16 got;
	int16 first;

	got = 0;
	key = 0;
	first = 1;
	cardA = cardB = backBuffer;
	cardA.right = 0x125;
	cardA.left = 0x116;
	cardA.top = 0xae;
	cardA.bottom = 0xbc;
	cardB.right = 0x136;
	cardB.left = 0x127;
	cardB.top = 0xae;
	cardB.bottom = 0xbc;
	if(videoMode == 0xd) {
		textArea.segment = backBuffer.segment = screen.segment;
		screen.segment = egaPage2.segment;
		DisplayPassage();
		screen.segment = textArea.segment;
	}
	while(!got) {
		if(videoMode == 0xd) {
			backBuffer.segment = screen.segment;
			DRV(0xf)(&egaPage2, &screen);
		} else {
			DRV(3)(&backBuffer, 0, sceneY, sceneShapes, 0);
		}
		MOUSE_SERVE();
		DrawGypsyStep(0, 0, 1);
		MOUSE_SERVE();
		if(videoMode != 0xd)
			DisplayPassage();
		MOUSE_SERVE();
		if(mouse)
			DrawMouseOnBackBuffer();
		if(videoMode == 0xd) {
			DRV(0x15)(&screen);
			if(first) {
				first = 0;
				DRV(0xf)(&backBuffer, &screen);
			}
		} else {
			DRV(0xf)(&backBuffer, &screen);
		}
		MOUSE_SERVE();
		if(mouseL || mouseR) {
			while(mouseL || mouseR)
				MOUSE_SERVE();
			if(MOUSE_IN_RECT(&cardA))
				key = 'A';
			else if(MOUSE_IN_RECT(&cardB))
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
/*C_0406_0F99*/int16 AskQuestion(int16 q) {
	char key;

	if(mouseInstalled) {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&workArea, 0x10f, 0x29, sceneShapes, shapeArm + 6);
		DRV(3)(&workArea, 0x120, 0x29, sceneShapes, shapeArm + 7);
	}
	do {
		key = WaitForAnswer(1);
	} while(key != 'A' && key != 'B');
	if(key == 'A')
		return answerAVirtue[q - 0x69];
	return answerBVirtue[q - 0x69];
}

/*the virtue v was chosen: its attribute points, and the gypsy pours its vial*/
/*C_0406_1040*/ChooseVirtue(int16 v) {
	int16 count;
	int16 step;
	int16 i;

	newStr += strGain[v];
	newDex += dexGain[v];
	newInt += intGain[v];
	if(v == 7)
		avatarKarma += 3;
	count = ScaleSpeed(3, 0);
	MOUSE_SERVE();
	if(videoMode == 0xd) {
		textArea.segment = backBuffer.segment = screen.segment;
		DRV(0xf)(&egaPage2, &screen);
		screen.segment = egaPage2.segment;
		DisplayPassage();
		screen.segment = textArea.segment;
	} else {
		DRV(3)(&backBuffer, 0, sceneY, sceneShapes, 0);
	}
	MOUSE_SERVE();
	for(step = 0; step < 0x11; step++) {
		if(!DrawGypsyStep(v, step, 0))
			continue;
		MOUSE_SERVE();
		if(videoMode != 0xd)
			DisplayPassage();
		MOUSE_SERVE();
		DrawMouseOnBackBuffer();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
		MOUSE_SERVE();
		for(i = 0; i < count; i++) {
			if(videoMode == 0xd) {
				backBuffer.segment = screen.segment;
				DRV(0xf)(&egaPage2, &screen);
			} else {
				DRV(3)(&backBuffer, 0, sceneY, sceneShapes, 0);
			}
			MOUSE_SERVE();
			DrawGypsyStep(v, step, 1);
			MOUSE_SERVE();
			if(videoMode != 0xd)
				DisplayPassage();
			MOUSE_SERVE();
			DrawMouseOnBackBuffer();
			if(videoMode != 0xd) {
				DRV(0xf)(&backBuffer, &screen);
			} else {
				DRV(0x15)(&screen);
			}
			MOUSE_SERVE();
		}
		if(videoMode == 0xd) {
			backBuffer.segment = screen.segment;
			DRV(0xf)(&egaPage2, &screen);
		} else {
			DRV(3)(&backBuffer, 0, sceneY, sceneShapes, 0);
		}
		MOUSE_SERVE();
	}
}

/*create a new character: the name and the sex on the vellum, the woods, the
gypsy and her questions, then write the character into savegame\objlist;
1 if the player gave up at the name, else what WriteNewCharacter said*/
/*C_0406_1269*/int16 CreateCharacter() {
	int16 virtue;
	int16 speed;
	int16 done;
	int16 ret;
	struct tSurface save;
	int16 i;
	int16 q;

	if(videoMode == 0xd) {
		backBuffer.segment = screen.segment ^ 0x200;
		DRV(0xf)(&backBuffer, &screen);
		screen.segment ^= 0x200;
		backBuffer.segment = screen.segment;
		textArea.segment = screen.segment;
		egaScreen = screen;
	}
	nameWindow = backBuffer;
	dialogArea = screen;
	nameWindow.right = dialogArea.right = 0x124;
	nameWindow.left = dialogArea.left = 0x1c;
	nameWindow.top = dialogArea.top = 0x57;
	nameWindow.bottom = dialogArea.bottom = 0xbd;
	textAreaCopy = textArea;
	SURF_share(&backBuffer, &textAreaCopy, 0);
	if(videoMode != 0xd) {
		DRV(0xc)(&textAreaCopy, 0);
	}
	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = screen.segment;
	save.right = textArea.right - textArea.left;
	save.bottom = textArea.bottom - textArea.top;
	save.left = save.top = 0;
	SURF_alloc(&save);
	/*the vellum*/
	AskForDisk(2);
	switch(videoMode) {
		case 4:
			vellum = LZW_load("vellum1.cga");
			virtueColor[3] = virtueColor[1] = virtueColor[6] = 3;
			virtueColor[0] = virtueColor[2] = virtueColor[4] = virtueColor[5] = 2;
			virtueColor[7] = 1;
			break;
		case 7:
			vellum = LZW_load("vellum1.hrc");
			virtueColor[3] = virtueColor[1] = virtueColor[6] = 3;
			virtueColor[0] = virtueColor[2] = virtueColor[4] = virtueColor[5] = 2;
			virtueColor[7] = 1;
			break;
		case 9:
			vellum = LZW_load("vellum1.tga");
			virtueColor[0] = 9;
			virtueColor[2] = 4;
			virtueColor[3] = 0xa;
			virtueColor[4] = 0xc;
			virtueColor[7] = 7;
			break;
		case 0xd:
			vellum = LZW_load("vellum1.ega");
			virtueColor[0] = 9;
			virtueColor[2] = 4;
			virtueColor[3] = 0xa;
			virtueColor[4] = 0xc;
			virtueColor[7] = 7;
			break;
		case 0x13:
			vellum = LZW_load("vellum1.shp");
			break;
	}
	AskForDisk(2);
	switch(videoMode) {
		case 4: portraits = LZW_load("montage.cga"); break;
		case 7: portraits = LZW_load("montage.hrc"); break;
		case 9: portraits = LZW_load("montage.tga"); break;
		case 0xd: portraits = LZW_load("montage.ega"); break;
		case 0x13: portraits = LZW_load("montage.shp"); break;
	}
	DRV(0xf)(&textArea, &save);
	if(videoMode == 0x13)
		FADE_out(&textArea, 0);
	if(videoMode == 0xd)
		textAreaCopy.segment = textArea.segment = screen.segment;
	DRV(0xc)(&textArea, 0);
	SetPalette();
	DRV(3)(&textAreaCopy, textAreaCopy.left, textAreaCopy.top, vellum, 0);
	portraitIdx = transferFrom = 0;
	MOUSE_SHOW();
	PlayMusic(4);
	menuDrawn = 0;
	do {
		MOUSE_HIDE();
		if(!AskNameAndSex()) {
			menuDrawn = 1;
			break;
		}
		MOUSE_SHOW();
	} while(!PortraitScreen(0));
	if(menuDrawn) {
		/*given up*/
		if(videoMode == 0xd) {
			textArea.segment = screen.segment;
			textArea.pScanlines = screen.pScanlines;
		}
		farfree(portraits);
		farfree(vellum);
		if(videoMode == 0x13)
			FADE_out(&textArea, 0);
		DRV(0xc)(&textArea, 0);
		SetPalette();
		if(videoMode == 0x13) {
			FADE_in(&save, &textArea);
		} else if(videoMode == 0xd) {
			DRV(0xf)(&save, &textArea);
			DRV(0x15)(&screen);
		} else {
			DRV(0xf)(&save, &textArea);
		}
		SURF_free(&save);
		if(videoMode != 0xd)
			SURF_free(&workArea);
		PlayMusic(0);
		return 1;
	}
	MOUSE_HIDE();
	farfree(portraits);
	farfree(vellum);
	SURF_free(&save);
	TIM_calibrate();
	/*the woods*/
	AskForDisk(2);
	switch(videoMode) {
		case 4: sceneShapes = LZW_load("woods.cga"); break;
		case 7: sceneShapes = LZW_load("woods.hrc"); break;
		case 9: sceneShapes = LZW_load("woods.tga"); break;
		case 0xd: sceneShapes = LZW_load("woods.ega"); break;
		case 0x13: sceneShapes = LZW_load("woods.shp"); break;
	}
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	if(videoMode != 0xd) {
		DRV(0xc)(&backBuffer, 0);
	}
	if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	}
	LoadPalette(4);
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	if(videoMode == 0xd)
		workArea.segment = 0xa600;
	ShowPassage(100);
	do {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&backBuffer, 0, 0, sceneShapes, 0);
		DisplayPassage();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
	} while(!PollInput());
	speed = ScaleSpeed(2, 10);
	i = 0;
	done = 0;
	/*the woods scroll by; one more round once i reaches 0xbf (the constant
	1 after the comma is what gives the original's mov al,1/or al,al)*/
	while(!done && (done = i == 0xbf, 1)) {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&backBuffer, backBuffer.left - i, 0, sceneShapes, 0);
		DRV(3)(&backBuffer, backBuffer.left + 0x140 - i, 0, sceneShapes, 1);
		if(videoMode == 0x13)
			VID_waitRetrace();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
		if(PollInput())
			i = 0xbf - speed;
		i += speed;
		i += (0xbf - i) * (i >= 0xbf);
	}
	ShowPassage(0x65);
	do {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&backBuffer, backBuffer.left - 0xbf, 0, sceneShapes, 0);
		DRV(3)(&backBuffer, backBuffer.left + 0x81, 0, sceneShapes, 1);
		DisplayPassage();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
	} while(!PollInput());
	ShowPassage(0x66);
	do {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DRV(3)(&backBuffer, backBuffer.left - 0xbf, 0, sceneShapes, 0);
		DRV(3)(&backBuffer, backBuffer.left + 0x81, 0, sceneShapes, 1);
		DisplayPassage();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
	} while(!PollInput());
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, backBuffer.left - 0xbf, 0, sceneShapes, 0);
	DRV(3)(&backBuffer, backBuffer.left + 0x81, 0, sceneShapes, 1);
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	farfree(sceneShapes);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	menuDrawn = 0;
	DRV(0xc)(&screen, 0);
	if(videoMode != 0xd) {
		DRV(0xc)(&backBuffer, 0);
	} else {
		DRV(0x15)(&screen);
		DRV(0xc)(&screen, 0);
	}
	/*the gypsy*/
	AskForDisk(2);
	LoadPalette(5);
	switch(videoMode) {
		case 4: sceneShapes = LZW_load("gypsy.cga"); break;
		case 7: sceneShapes = LZW_load("gypsy.hrc"); break;
		case 9: sceneShapes = LZW_load("gypsy.tga"); break;
		case 0xd: sceneShapes = LZW_load("gypsy.ega"); break;
		case 0x13: sceneShapes = LZW_load("gypsy.shp"); break;
	}
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	strcpy(newClass, "Avatar");
	newGood = 'G';
	newStr = newDex = newInt = 0xf;
	for(i = 0; i < 8; i++) {
		chosenVirtues[i] = i;
		vialLevel[i] = 3;
		pattern[i] = 0xff;
	}
	patternAdded = 0;
	patternColorCount = 0;
	patternArea.right = 0xac;
	patternArea.left = 0x93;
	patternArea.bottom = patternArea.top = sceneY + 0x7f;
	SURF_share(&backBuffer, &patternArea, 0);
	DRV(3)(&backBuffer, 0, sceneY, sceneShapes, 0);
	if(videoMode == 0xd) {
		egaPage2 = screen;
		egaPage2.segment = 0xa800;
		DRV(0xf)(&screen, &egaPage2);
	}
	DrawGypsyStep(0, 0, 1);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	ShowPassage(0x67);
	WaitForAnswer(0);
	ShowPassage(0x68);
	WaitForAnswer(0);
	MOUSE_SHOW();
	/*the questions: the 8 virtues in pairs, then the 4 winners, then the 2 last*/
	Shuffle(chosenVirtues, 8);
	for(i = 0; i < 4; i++) {
		q = questionOf[chosenVirtues[i * 2]][chosenVirtues[i * 2 + 1]];
		ShowPassage(q);
		virtue = AskQuestion(q);
		ChooseVirtue(virtue);
		chosenVirtues[i] = virtue;
	}
	Shuffle(chosenVirtues, 4);
	for(i = 0; i < 2; i++) {
		q = questionOf[chosenVirtues[i * 2]][chosenVirtues[i * 2 + 1]];
		ShowPassage(q);
		virtue = AskQuestion(q);
		ChooseVirtue(virtue);
		chosenVirtues[i] = virtue;
	}
	q = questionOf[chosenVirtues[0]][chosenVirtues[1]];
	ShowPassage(q);
	virtue = AskQuestion(q);
	ChooseVirtue(virtue);
	MOUSE_HIDE();
	ShowPassage(0x85);
	WaitForAnswer(0);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	}
	/*the gypsy's pattern*/
	LoadPalette(6);
	patternArea.right = 0xcb;
	patternArea.left = 0x74;
	patternArea.top = 0x23;
	patternArea.bottom = 0x7f;
	DrawPattern();
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc6);
	ShowPassage(0x86);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	do {
		if(videoMode == 0xd)
			backBuffer.segment = screen.segment;
		DrawPattern();
		DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc6);
		DisplayPassage();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
		}
	} while(!PollInput());
	DrawPattern();
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc6);
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	if(videoMode == 0x13)
		FADE_out(&screen, 0x75);
	switch(videoMode) {
		case 4:
		case 7:
			DRV(0xc)(&backBuffer, 1);
			ret = ScaleSpeed(600, 800);
			DIS_dissolveCGA(&backBuffer, &screen, ret);
			break;
		case 9:
			DRV(0xc)(&backBuffer, 0xb);
			ret = ScaleSpeed(800, 1200);
			DIS_dissolveTDY(&backBuffer, &screen, ret);
			break;
		case 0xd:
			backBuffer.segment = screen.segment;
			DRV(0xc)(&screen, 0xb);
			screen.segment ^= 0x200;
			ret = ScaleSpeed(300, 400);
			DIS_dissolveEGA(&backBuffer, &screen, ret);
			screen.segment ^= 0x200;
			DRV(0x15)(&screen);
			break;
		case 0x13:
			DRV(0xc)(&screen, 0x75);
			break;
	}
	LoadPalette(6);
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc7);
	ShowPassage(0x87);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		screen.segment ^= 0x200;
		ret = ScaleSpeed(300, 400);
		DIS_dissolveEGA(&backBuffer, &screen, ret);
		screen.segment ^= 0x200;
		DRV(0x15)(&screen);
	} else if(videoMode == 9) {
		ret = ScaleSpeed(400, 600);
		DIS_dissolveTDY(&backBuffer, &screen, ret);
	} else {
		ret = ScaleSpeed(300, 400);
		DIS_dissolveCGA(&backBuffer, &screen, ret);
	}
	TIM_delay(2000);
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DisplayPassage();
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	ShowPassage(0x88);
	while(!PollInput());
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc7);
	DisplayPassage();
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	ShowPassage(0x89);
	while(!PollInput());
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc7);
	DisplayPassage();
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	while(!PollInput());
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	DRV(3)(&backBuffer, 0, 0, sceneShapes, 0xc7);
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		DRV(0x15)(&screen);
	}
	farfree(sceneShapes);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	if(videoMode != 0xd) {
		DRV(0xc)(&backBuffer, 0);
	} else {
		DRV(0x15)(&screen);
		DRV(0xc)(&screen, 0);
		backBuffer = screen;
	}
	SetPalette();
	/*the new character*/
	AskForDisk(8);
	if(videoMode != 0xd)
		SURF_free(&workArea);
	newExp = 0x172;
	newHp = 0x5a;
	newLevel = 3;
	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = 0xa600;
	ret = 0;
	if(WriteNewCharacter()) {
		/*the disk already holds a character*/
		ShowPassage(0x3e6);
		DisplayPassage();
		ret = 1;
		if(videoMode == 0xd) {
			DRV(0x15)(&screen);
			backBuffer = screen;
		} else {
			DRV(0xf)(&backBuffer, &screen);
		}
		while(!PollInput());
		DRV(0xc)(&backBuffer, 0);
		if(videoMode == 0xd) {
			DRV(0x15)(&screen);
			backBuffer = screen;
			DRV(0xc)(&backBuffer, 0);
		} else {
			DRV(0xf)(&backBuffer, &screen);
		}
	}
	AskForDisk(2);
	if(videoMode != 0xd)
		SURF_free(&workArea);
	PlayMusic(0);
	return ret;
}
