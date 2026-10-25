/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0C17: the introduction's night scene (the twinkling stars, the hands,
 * the moongate rising, the stones)
 */
#include "u.h"
#include <alloc.h>
#include <stdlib.h>

/*a drawing surface of the graphics driver (14 bytes)*/

extern int16 videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface screen;/*the screen*/
extern struct tSurface backBuffer;/*the page the introduction draws on*/
extern int16 menuDrawn;
extern int16 escaped;/*set when the player escapes the introduction*/
extern void far *sceneShapes;/*the shapes of the introduction (intro.*)*/
extern struct tSurface skySurface;/*twinkle: the surface the stars are drawn on*/
extern struct tSurface workArea;/*an off-screen buffer*/
extern int16 twinklePal;/*twinkle: palette index (a global: the twinkle runs from the timer)*/
extern char twinkleEntry[];/*twinkle: a palette entry*/
extern char twinkleEntry2[];/*twinkle: a palette entry*/
extern int16 starIndex;/*twinkle: star index*/
extern int16 starPixel;/*twinkle: colour of the star's pixel*/
extern int16 starLit;/*twinkle: colour of a lit star*/
extern char far *starPosData;/*starpos.dat*/
extern int16 twinkleFirst;/*twinkle: first star of the group that twinkles*/
extern int16 starCount;/*number of stars*/
extern int16 twinkleRandom;/*twinkle: random number (a shift register)*/
extern int16 starColor[];/*colour of each star*/
extern int16 starX[];/*x of each star*/
extern int16 starY[];/*y of each star*/
extern int16 hands[];/*the hands: x0,y0, x1,y1*/
extern int16 shapeDest[];/*where the moving shapes go*/
extern int16 shapePos[];/*the moving shapes: x,y each*/

extern SetPalette();
extern RestorePalette();
extern SavePalette();
extern PlayMusic(int16 song);
extern int16 CheckInput();/*nonzero when a key (or the mouse) was pressed*/
extern int16 ScaleSpeed(int16 a, int16 b);/*a random number between the two*/
extern ShowFrame();/*waits for the next frame*/
extern Delay(int16 n);/*waits n ticks*/
extern ShowPassage(int16 n);/*writes text n*/
extern DisplayPassage();
extern LoadPalette(int16 n);
extern SURF_alloc(struct tSurface *);/*gives a surface an off-screen buffer*/
extern SURF_free(struct tSurface *);/*frees it*/
extern VID_waitRetrace();
extern FADE_step(int16 *, int16 *, int16);/*moves n coordinates one step towards their targets: 0 when all are there*/
extern FADE_in(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern FADE_out(struct tSurface *, int16);/*MCGA: palette fade out*/
extern void far *LZW_load(char *);/*loads a file in memory*/
extern DIS_dissolve(struct tSurface *, struct tSurface *, int16);/*MCGA: dissolve*/
extern DIS_dissolveEGA(struct tSurface *, struct tSurface *, int16);/*EGA: dissolve*/
extern DIS_dissolveCGA(struct tSurface *, struct tSurface *, int16);/*CGA, Hercules: dissolve*/
extern DIS_dissolveTDY(struct tSurface *, struct tSurface *, int16);/*Tandy: dissolve*/
extern PAL_set(int16, char *);/*MCGA: sets a palette entry*/
extern PAL_get(int16, char *);/*MCGA: gets a palette entry*/
extern TIM_setCallback(void (far *)());/*installs a timer routine*/
extern TIM_resetCallback();
extern TIM_install();
extern TIM_remove();
extern TIM_calibrate();

/*40C4*/int16 handsRest[4] = {-8, 0x4d, 0xb6, 0x4f};/*where the hands come to rest: x0,y0, x1,y1*/
/*40CC*/int16 twinkleToggle = 0;/*twinkle: toggles every tick (MCGA palette cycling every other tick)*/
/*40CE*/uint16 twinkleTicks = 0;/*twinkle: tick counter*/

KeyPressed();

/*a driver entry that returns something (gfxDrv.p.ofs selects it)*/
#define DRVF(type) (*(type (far *)())gfxDrv.f)

/*draws shape n of the introduction*/
#define SHAPE(x, y, n) DRV(3)(&backBuffer, x, y, sceneShapes, n)

/*the pixel of star i in MCGA video memory*/
#define STAR_PIXEL(i) *(char far *)((int32)(starY[i] * 320) + starX[i] + 0xa0000000L)

/*shows the page that was drawn*/
#define SHOW_PAGE() \
	if(videoMode == 0x13) { \
		FADE_in(&backBuffer, &screen); \
	} else if(videoMode == 0xd) { \
		DRV(0x15)(&screen); \
		backBuffer.segment = screen.segment; \
	} else { \
		DRV(0xf)(&backBuffer, &screen); \
	}

/*the hands tremble: each moves sideways at random, up to n-1 pixels*/
#define TREMBLE(n) \
	if(rand() % 3) \
		left += dleft; \
	if(abs(left) == (n)) { \
		dleft = -dleft; \
		left += dleft; \
	} \
	if(rand() % 3) \
		right += dright; \
	if(abs(right) == (n)) { \
		dright = -dright; \
		right += dright; \
	}

/*draws the two hands (shapes 3 and 4), moved by x0 and x1, at height y*/
/*C_0C17_0000*/DrawHands(int16 x0, int16 x1, int16 y) {
	if(videoMode == 0xd)
		backBuffer = screen;
	SHAPE(x0 - 8, y + 0x4d, 3);
	SHAPE(x1 + 0xb6, y + 0x4f, 4);
}

/*the timer routine of the night sky: every few ticks a group of stars (from twinkleFirst,
 1 to 5 of them) is lit, then put out; on MCGA the palette entries 0xe8-0xef cycle*/
/*C_0C17_0072*/void far Twinkle() {
	if(videoMode == 0x13) {
		/*cycle the palette entries 0xe8-0xef, every other tick*/
		if(twinkleToggle == 0) {
			VID_waitRetrace();
			PAL_get(0xef, twinkleEntry2);
			for(twinklePal = 0xee; twinklePal >= 0xe8; twinklePal--) {
				PAL_get(twinklePal, twinkleEntry);
				PAL_set(twinklePal + 1, twinkleEntry);
			}
			PAL_set(0xe8, twinkleEntry2);
		}
		twinkleToggle = !twinkleToggle;
		if(!(twinkleTicks & 3)) {
			/*put out the stars that are lit*/
			for(starIndex = twinkleFirst; starIndex <= twinkleRandom % 5 + twinkleFirst; starIndex++)
				if(STAR_PIXEL(starIndex) == 0xf)
					STAR_PIXEL(starIndex) = starColor[starIndex];
		}
		twinkleTicks++;
		if(twinkleTicks % 20 == 0) {
			/*light another group of stars*/
			twinkleFirst = (twinkleFirst + 7) % starCount;
			if(twinkleRandom & 1) {
				twinkleRandom >>= 1;
				twinkleRandom ^= 0xb8;
			} else {
				twinkleRandom >>= 1;
			}
			for(starIndex = twinkleFirst; starIndex <= twinkleRandom % 5 + twinkleFirst; starIndex++)
				if(STAR_PIXEL(starIndex) == starColor[starIndex])
					STAR_PIXEL(starIndex) = 0xf;
		}
	} else {
		starLit = 0xf;
		if(videoMode < 9)
			starLit = 3;
		if(videoMode == 0xd) {
			gfxDrv.p.ofs = 0x1e;
			if(DRVF(int32)(0))
				return;
		}
		if(!(twinkleTicks & 3)) {
			for(starIndex = twinkleFirst; starIndex <= twinkleRandom % 5 + twinkleFirst; starIndex++) {
				gfxDrv.p.ofs = 0x1b;
				starPixel = DRVF(int16)(&skySurface, starX[starIndex], starY[starIndex]);
				if(starPixel == starLit) {
					DRV(0x18)(&skySurface, starX[starIndex], starY[starIndex], starColor[starIndex]);
				}
			}
		}
		if(++twinkleTicks % 8 == 0) {
			twinkleFirst = (twinkleFirst + 7) % starCount;
			if(twinkleRandom & 1) {
				twinkleRandom >>= 1;
				twinkleRandom ^= 0xb8;
			} else {
				twinkleRandom >>= 1;
			}
			for(starIndex = twinkleFirst; starIndex <= twinkleRandom % 5 + twinkleFirst; starIndex++) {
				gfxDrv.p.ofs = 0x1b;
				starPixel = DRVF(int16)(&skySurface, starX[starIndex], starY[starIndex]);
				if(starColor[starIndex] == starPixel && starPixel != 0) {
					DRV(0x18)(&skySurface, starX[starIndex], starY[starIndex], starLit);
				}
			}
		}
	}
}

/*waits n ticks, or until a key is pressed*/
/*C_0C17_03CA*/WaitTicks(int16 n) {
	int16 i;

	for(i = n; i; i--) {
		Delay(1);
		if(KeyPressed())
			break;
	}
}

/*draws a small glowing spot of 9 pixels at 0x57-0x59,0x55-0x58*/
/*C_0C17_03EF*/DrawGlow() {
	int16 c;

	if(videoMode == 0x13)
		c = 0xad;
	else if(videoMode == 0xd || videoMode == 9)
		c = 6;
	else
		c = 1;
	DRV(0x18)(&backBuffer, 0x58, 0x55, c);
	DRV(0x18)(&backBuffer, 0x57, 0x56, c);
	DRV(0x18)(&backBuffer, 0x58, 0x56, c);
	DRV(0x18)(&backBuffer, 0x59, 0x56, c);
	DRV(0x18)(&backBuffer, 0x57, 0x57, c);
	DRV(0x18)(&backBuffer, 0x58, 0x57, c);
	DRV(0x18)(&backBuffer, 0x59, 0x57, c);
	DRV(0x18)(&backBuffer, 0x57, 0x58, c);
	DRV(0x18)(&backBuffer, 0x58, 0x58, c);
}

/*waits until no key is pressed*/
/*C_0C17_04FE*/FlushKeys() {
	while(KeyPressed());
}

/*the introduction: the night sky with its twinkling stars, then the scene with the hands and
 the stones; texts 0xc8-0xdb (ShowPassage) are told along it. A key goes on to the
 next part, Escape (escaped) leaves it.
 (the locals' order and which ones are declared after which matter for the stack
 layout and the si/di choice: found by trying)*/
/*C_0C17_0508*/Intro() {
	int16 frames;/*a random duration*/
	int16 speed;/*a random speed of the current animation*/
	int16 done;
	int16 a;
	int16 b;
	int16 tmp;
	int16 ofs;/*horizontal offset of shapes 8, 0xb/0xa; also a dissolve parameter*/
	int16 jitter;
	int16 left;/*the hands' trembling offsets and their directions*/
	int16 right;
	int16 dleft;
	int16 dright;
	int16 i;

	escaped = 0;
	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd) {
		SURF_alloc(&workArea);
	} else {
		workArea.segment = 0xa600;
		backBuffer = screen;
	}
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	DRV(0xc)(&backBuffer, 0);
	TIM_calibrate();
	frames = ScaleSpeed(0xf, 0x28);
	switch(videoMode) {
		case 4: sceneShapes = LZW_load("intro.cga"); break;
		case 7: sceneShapes = LZW_load("intro.hrc"); break;
		case 9: sceneShapes = LZW_load("intro.tga"); break;
		case 0xd: sceneShapes = LZW_load("intro.ega"); break;
		case 0x13: sceneShapes = LZW_load("intro.shp"); break;
	}
	/*the stars: 200 x, 200 y, 200 colours (16 bits each)*/
	starPosData = LZW_load("starpos.dat");
	for(i = 0; i < 0x190; i += 2) {
		starX[i / 2] = (starPosData[i] & 0xff) + ((starPosData[i + 1] & 0xff) << 8);
		starY[i / 2] = (starPosData[i + 0x190] & 0xff) + ((starPosData[i + 0x191] & 0xff) << 8);
		starColor[i / 2] = (starPosData[i + 0x320] & 0xff) + ((starPosData[i + 0x321] & 0xff) << 8);
	}
	LoadPalette(7);
	SavePalette();
	PlayMusic(3);
	SHAPE(0, 0, 6);
	SHAPE(0, 0x70, 5);
	SHOW_PAGE();
	/*shuffle the stars*/
	starCount = 0xc1;
	for(i = 0; i < 0x200; i++) {
		a = rand() % starCount;
		b = rand() % starCount;
		tmp = starX[a];
		starX[a] = starX[b];
		starX[b] = tmp;
		tmp = starY[a];
		starY[a] = starY[b];
		starY[b] = tmp;
		tmp = starColor[a];
		starColor[a] = starColor[b];
		starColor[b] = tmp;
	}
	twinkleRandom = rand() % 0xff + 1;
	twinkleFirst = 0;
	skySurface = screen;
	/*light the first group of stars*/
	if(videoMode == 0x13) {
		for(i = twinkleFirst; i <= twinkleRandom % 5 + twinkleFirst; i++)
			STAR_PIXEL(i) = 0xf;
	} else {
		if(videoMode == 0xd)
			screen.segment ^= 0x200;
		for(i = 0; i < 200; i++) {
			gfxDrv.p.ofs = 0x1b;
			starColor[i] = DRVF(int16)(&screen, starX[i], starY[i]);
		}
		for(i = twinkleFirst; i <= twinkleRandom % 5 + twinkleFirst; i++) {
			DRV(0x18)(&screen, starX[i], starY[i], 0xf);
		}
		if(videoMode == 0xd)
			screen.segment ^= 0x200;
	}
	TIM_install();
	TIM_setCallback(Twinkle);
	ShowPassage(0xc8);
	WaitTicks(10);
	if(videoMode == 0xd)
		screen.segment ^= 0x200;
	DisplayPassage();
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	}
	FlushKeys();
	while(!CheckInput());
	if(escaped)
		goto end;
	ShowPassage(0xc9);
	DisplayPassage();
	if(videoMode != 0xd) {
		DRV(0xf)(&backBuffer, &screen);
	} else {
		screen.segment ^= 0x200;
	}
	FlushKeys();
	while(!CheckInput());
	if(escaped)
		goto end;
	/*the hands come in from the sides*/
	hands[0] = -0x54;
	hands[1] = 0x6d;
	hands[2] = 0x140;
	hands[3] = 0xc8;
	if(videoMode == 0xd)
		backBuffer = screen;
	speed = ScaleSpeed(1, 4);
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(FADE_step(hands, handsRest, 4));
			done = 1;
		}
		if(escaped)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !FADE_step(hands, handsRest, 4);
		SHAPE(0, 0, 6);
		SHAPE(hands[0], hands[1], 3);
		SHAPE(hands[2], hands[3], 4);
		SHAPE(0, 0x70, 5);
		ShowFrame();
	} while(!done);
	ShowPassage(0xca);
	DisplayPassage();
	ShowFrame();
	SHAPE(0, 0, 6);
	DrawHands(0, 0, 0);
	SHAPE(0, 0x70, 5);
	ShowPassage(0xcb);
	DisplayPassage();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	ShowFrame();
	SHAPE(0, 0, 6);
	DrawHands(0, 0, 0);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	ShowPassage(0xcc);
	DisplayPassage();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	ShowFrame();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	/*the moongate rises*/
	ShowPassage(0xcd);
	speed = ScaleSpeed(2, 5);
	FlushKeys();
	ofs = 0x46;
	i = 0x8d;
	done = 0;
	/*(the loop condition sets done when the end is reached: one more frame is drawn)*/
	while(!done && (done = i == 0x1e, 1)) {
		jitter = rand() % 8 + ofs - 3;
		if(i > 0x3c || ofs == 0)
			jitter = ofs;
		SHAPE(0, 0, 6);
		DrawHands(0, 0, 4);
		SHAPE(jitter + 0x90, i, 8);
		SHAPE(jitter + 0xde, i + 0xe, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		ShowFrame();
		if(ofs) {
			ofs -= speed;
			if(ofs < 0)
				ofs = 0;
		}
		if(KeyPressed())
			i = speed + 0x1e;
		if(escaped)
			goto end;
		i -= speed;
		/*not past 0x1e*/
		i += (0x1e - i) * (i <= 0x1e);
	}
	left = right = 0;
	dleft = dright = 1;
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DisplayPassage();
		ShowFrame();
		TREMBLE(3);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xce);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DisplayPassage();
		ShowFrame();
		TREMBLE(3);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xcf);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DisplayPassage();
		ShowFrame();
		TREMBLE(3);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xd0);
	speed = ScaleSpeed(2, 5);
	FlushKeys();
	i = 0x86;
	done = 0;
	while(!done && (done = i == 0x3a, 1)) {
		SHAPE(0, 0, 6);
		SHAPE(0x78, i, 7);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DrawGlow();
		DisplayPassage();
		ShowFrame();
		TREMBLE(3);
		if(KeyPressed())
			i = speed + 0x3a;
		if(escaped)
			goto end;
		i -= speed;
		i += (0x3a - i) * (i <= 0x3a);
	}
	while(!KeyPressed());
	if(escaped)
		goto end;
	SHAPE(0, 0, 6);
	SHAPE(0x78, 0x3a, 7);
	DrawHands(left, right, 4);
	SHAPE(0x90, 0x1e, 8);
	SHAPE(0xde, 0x2c, 0xb);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	DrawGlow();
	ShowFrame();
	switch(videoMode) {
		case 4:
		case 7:
			ofs = ScaleSpeed(0xfa, 0x12c);
			DRV(0xc)(&backBuffer, 0);
			DIS_dissolveCGA(&backBuffer, &screen, ofs);
			break;
		case 9:
			ofs = ScaleSpeed(0x12c, 0x1f4);
			DRV(0xc)(&backBuffer, 0);
			DIS_dissolveTDY(&backBuffer, &screen, ofs);
			break;
		case 0xd:
			backBuffer.segment = 0xa600;
			DRV(0xc)(&backBuffer, 0);
			screen.segment ^= 0x200;
			ofs = ScaleSpeed(0xc8, 0x12c);
			DIS_dissolveEGA(&backBuffer, &screen, ofs);
			DRV(0x15)(&screen);
			backBuffer = screen;
			DRV(0xc)(&backBuffer, 0);
			break;
		case 0x13:
			DRV(0xc)(&backBuffer, 0);
			DIS_dissolve(&backBuffer, &screen, 0);
			break;
	}
	ShowPassage(0xd1);
	DisplayPassage();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	RestorePalette();
	ShowPassage(0xd2);
	DisplayPassage();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	RestorePalette();
	ShowPassage(0xd3);
	DisplayPassage();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	RestorePalette();
	ShowPassage(0xd4);
	DisplayPassage();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(escaped)
		goto end;
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		backBuffer = screen;
	}
	RestorePalette();
	SHAPE(0, 0, 6);
	SHAPE(0x78, 0x3a, 7);
	DrawHands(0, 0, 4);
	SHAPE(0x90, 0x1e, 8);
	SHAPE(0xde, 0x2c, 0xb);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	DrawGlow();
	switch(videoMode) {
		case 4:
		case 7:
			ofs = ScaleSpeed(0xc8, 0x12c);
			DIS_dissolveCGA(&backBuffer, &screen, ofs);
			break;
		case 9:
			ofs = ScaleSpeed(0x12c, 0x1f4);
			DIS_dissolveTDY(&backBuffer, &screen, ofs);
			break;
		case 0xd:
			screen.segment ^= 0x200;
			ofs = ScaleSpeed(0xc8, 0x12c);
			DIS_dissolveEGA(&backBuffer, &screen, ofs);
			screen.segment ^= 0x200;
			break;
		case 0x13:
			DIS_dissolve(&backBuffer, &screen, 0);
			break;
	}
	FlushKeys();
	for(i = 0; i < frames / 2; i++) {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DrawGlow();
		ShowFrame();
		TREMBLE(3);
		if(KeyPressed())
			break;
		if(escaped)
			goto end;
	}
	SHAPE(0, 0, 6);
	SHAPE(0x78, 0x3a, 7);
	DrawHands(0, 0, 4);
	SHAPE(0x90, 0x1e, 8);
	SHAPE(0xde, 0x2c, 0xa);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	DrawGlow();
	for(i = 0; i <= frames / 2; i++)
		ShowFrame();
	ShowPassage(0xd5);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xa);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DisplayPassage();
		ShowFrame();
		TREMBLE(3);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	speed = ScaleSpeed(4, 0xa);
	FlushKeys();
	ofs = 0;
	i = 0x1e;
	done = 0;
	while(!done && (done = i == 0x8d, 1)) {
		ofs += rand() % 9 - 3;
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(ofs + 0x90, i, 8);
		SHAPE(ofs + 0xde, i + 0xe, 0xa);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		ShowFrame();
		TREMBLE(4);
		if(KeyPressed())
			i = 0x8d - speed;
		if(escaped)
			goto end;
		i += speed;
		i += (0x8d - i) * (i >= 0x8d);
	}
	shapePos[0] = -0x20;
	shapePos[1] = 0x7a;
	shapePos[2] = 0x44;
	shapePos[3] = 0x7a;
	shapePos[4] = 0xa8;
	shapePos[5] = 0xca;
	shapeDest[0] = 0x63;
	shapeDest[1] = 0x28;
	shapeDest[2] = 0xb7;
	shapeDest[3] = 0x35;
	shapeDest[4] = 0xf0;
	shapeDest[5] = 0x3e;
	speed = ScaleSpeed(3, 9);
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(FADE_step(shapePos, shapeDest, 6));
			done = 1;
		}
		if(escaped)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !FADE_step(shapePos, shapeDest, 6);
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(shapePos[0], shapePos[1], 0);
		SHAPE(shapePos[2], shapePos[3], 2);
		SHAPE(shapePos[4], shapePos[5], 1);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		ShowFrame();
		TREMBLE(4);
	} while(!done);
	ShowPassage(0xd6);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0x63, 0x28, 0);
		SHAPE(0xb7, 0x35, 2);
		SHAPE(0xf0, 0x3e, 1);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	shapePos[0] = 0x63;
	shapePos[1] = 0x28;
	shapePos[2] = 0xb7;
	shapePos[3] = 0x35;
	shapePos[4] = 0xf0;
	shapePos[5] = 0x3e;
	shapePos[6] = 0x31;
	shapePos[7] = 0x44;
	shapeDest[0] = 0x12;
	shapeDest[1] = 0x8e;
	shapeDest[2] = 0x12;
	shapeDest[3] = 0x8e;
	shapeDest[4] = 0x76;
	shapeDest[5] = 0x7a;
	shapeDest[6] = 0x31;
	shapeDest[7] = 0x8a;
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(FADE_step(shapePos, shapeDest, 8));
			done = 1;
		}
		if(escaped)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !FADE_step(shapePos, shapeDest, 8);
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(shapePos[0], shapePos[1], 0);
		SHAPE(shapePos[2], shapePos[3], 2);
		SHAPE(shapePos[4], shapePos[5], 1);
		SHAPE(shapePos[6], shapePos[7], 9);
		SHAPE(0, 0x70, 5);
		ShowFrame();
		TREMBLE(4);
	} while(!done);
	ShowPassage(0xd7);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xd8);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xd9);
	speed = ScaleSpeed(1, 3);
	FlushKeys();
	i = 0x3a;
	done = 0;
	while(!done && (done = i == 0x86, 1)) {
		SHAPE(0, 0, 6);
		SHAPE(0x78, i, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
		if(KeyPressed())
			i = 0x86 - speed;
		if(escaped)
			goto end;
		i += speed;
		i += (0x86 - i) * (i >= 0x86);
	}
	ShowPassage(0xda);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	ShowPassage(0xdb);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		DisplayPassage();
		ShowFrame();
		TREMBLE(4);
	} while(!KeyPressed());
	if(escaped)
		goto end;
	FlushKeys();
	for(i = 0; i < frames / 2; i++) {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		ShowFrame();
		TREMBLE(4);
		if(KeyPressed())
			break;
		if(escaped)
			break;
	}
end:
	TIM_remove();
	TIM_resetCallback();
	farfree(sceneShapes);
	farfree(starPosData);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	DRV(0xc)(&backBuffer, 0);
	SetPalette();
	if(videoMode != 0xd)
		SURF_free(&workArea);
	menuDrawn = escaped = 0;
	PlayMusic(0);
}

/*whether a key was pressed; also updates the surface the twinkling stars are drawn
 on (skySurface: the screen, the visible EGA page)*/
/*C_0C17_2FBB*/KeyPressed() {
	int16 ret;

	skySurface = screen;
	if(videoMode == 0xd)
		skySurface.segment ^= 0x200;
	ret = CheckInput();
	return ret;
}
