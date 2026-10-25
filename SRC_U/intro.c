/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 06E9: the introduction (the computer in the bedroom, the storm,
 * the moongate in the circle of stones)
 */
#include "u.h"
#include <stdlib.h>
#include <ctype.h>
#include <alloc.h>
#include <conio.h>
#include <dos.h>

/*a drawing surface, as GAME.EXE's tSurface*/

/*---- other modules ----*/
extern SetPalette();
extern RestorePalette();
extern SavePalette();
extern PlayMusic();
extern ShowPassage();/*start the music/sound piece n*/
extern DisplayPassage();/*called every frame while a piece plays*/
extern AskForDisk();/*AskForDisk(n)*/
extern LoadPalette();/*LoadPalette(n)*/
extern SURF_alloc();/*(struct tSurface *): give the surface its own pixel buffer*/
extern SURF_free();/*(struct tSurface *): free it*/
extern SURF_share();/*(dst, src, 0): copy a surface*/
extern MOUSE_SERVE();/*poll the mouse (mouseL, mouseR)*/
extern VID_inRetrace();/*wait for the vertical retrace?*/
extern VID_waitRetrace();/*wait for the vertical retrace*/
extern FADE_in();/*(&screen, &back buffer): fade in (MCGA)*/
extern FADE_out();/*(&back buffer, 0): fade out (MCGA)*/
void far *LZW_load(char *);/*load a shapes file*/
extern DIS_dissolve();/*(dst, src, speed): dissolve, MCGA*/
extern DIS_dissolveEGA();/*(dst, src, speed): dissolve, EGA*/
extern DIS_dissolveCGA();/*(dst, src, speed): dissolve, CGA/Hercules*/
extern DIS_dissolveTDY();/*(dst, src, speed): dissolve, Tandy*/
extern PAL_set();/*(i, int rgb[3]): set palette entry i*/
extern PAL_get();/*(i, int rgb[3]): get palette entry i*/
extern TIM_setCallback();/*(void (far *)()): install a timer tick handler*/
extern TIM_resetCallback();/*remove it*/
extern TIM_install();
extern TIM_remove();
extern TIM_calibrate();

/*---- data ----*/
extern int videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern char mouseL, mouseR;/*mouse buttons*/
extern int escaped;/*1: Escape was pressed: leave the introduction*/
extern unsigned long loopsPerTick;/*speed of the computer (loop count, see Delay)*/
extern struct tSurface screen;/*the back buffer*/
extern struct tSurface backBuffer;/*the screen*/
extern int musicCard;/*music on*/
extern struct tSurface egaSaved;/*EGA: the page the noise is drawn into*/
extern void far *songData;/*the music driver's song*/
/*the music driver (as GAME.EXE's MUS_Driver): its entries take the pascal convention*/

/*the scenes are drawn in layers, each with its own copy of the screen surface
 (its own clipping rectangle)*/
extern struct tSurface skySurface, skyCopy, lowerSurface;
extern struct tSurface introPage2;/*EGA: the second page*/
extern struct tSurface workArea;/*320x32 work surface*/

/*part 1: the computer's monitor*/
extern struct tSurface monitor;/*the monitor's screen*/
extern int monitorNoisy;/*1: the monitor showed noise: redraw its frame*/
extern void far *intro1Shapes;/*shapes of intro1*/
extern int monitorScroll;/*how far the monitor's picture went out of the screen on the left*/
extern int script;/*script: the current one*/
extern int scriptPos;/*script: position in it*/
extern int loopPos;/*script: position of the loop*/
extern int loopCount;/*script: loop count*/
extern int chosenPicture;/*script: the picture chosen by command 0x87*/

/*part 2: the storm*/
extern struct tSurface *cloudSurface;/*clouds: the surface*/
extern void far *cloudShapes;/*clouds: the shapes*/
extern int cloudCount;/*clouds: their number*/
extern int cloudY[], cloudShape[], cloudX[];/*clouds: x, y, shape of each*/
extern int bankX, bankY;/*clouds: x, y of the big cloud bank*/
extern int cloudSpeed;/*clouds: speed*/
extern struct tSurface *rainSurface;/*rain: the surface*/
extern int rainDelay;/*rain: frames between the moves of a drop*/
extern int rainMax;/*rain: the maximum number of drops - 1*/
extern int rainCount;/*rain: the number of drops*/
extern int rainChance;/*rain: 1 in n chances of a new drop every frame*/
extern char rainShape[];/*rain: shape of each drop*/
extern int rainX[], rainY[];/*rain: x, y of each drop*/
extern int rainWait[];/*rain: frames before the next move of each drop*/
extern void far *intro2Shapes;/*shapes of intro2*/

/*part 3: the moongate*/
extern int palEntry;/*palette entry*/
extern int palColors[3], palColors2[3];/*palette colors*/

/*---- this module's initialised data ----*/
/*0FE4*/int randomPictures[9] = {5,6,7,13,14,24,25,31,32};/*the pictures command 0x87 chooses from*/
/*0FF6*/int monitorX[54] = {/*x of each picture of the monitor (in its rectangle)*/
	0,0,0,0,0,31,31,31,0,0,0,0,0,31,31,0,9,9,
	9,12,12,12,0,4,31,31,4,0,4,4,4,31,31,0,0,6,
	6,8,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
/*1062*/int monitorY[54] = {/*y of each picture of the monitor ([0x22] moved by command 0x83)*/
	0,0,0,0,0,2,2,2,0,0,0,0,0,2,2,0,7,7,
	7,3,3,3,0,2,2,2,0,0,0,0,1,2,2,0,0,3,
	8,29,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
/*the 7 scripts of the monitor (see RunScript): below 0x80 a picture to draw;
 0x80 n: repeat n times up to 0x81; 0x81: end of the repeated part and of a frame;
 0x82: noise, end of a frame; 0x83: scroll; 0x84 n, n pictures: one at random;
 0x85: next script; 0x86: draw the picture chosen by 0x87; 0x87: choose one of randomPictures;
 0x88: blink 0x25/0x26; 0x89: 0x32-0x34 at random; 0x8a: end of a frame; 0x8b: glow*/
/*10CE*/unsigned char scripts[7][0x50] = {
	{0x82,0x82,0x80,0x03,0x02,0x8a,0x02,0x8a,0x01,0x8a,0x01,0x8a,0x00,0x8a,0x00,0x8a,0x01,0x8a,0x01,0x81,0x85},
	{0x82,0x82,0x80,0x28,0x03,0x8b,0x81,0x85},
	{0x82,0x82,0x80,0x04,0x04,0x81,0x80,0x04,0x08,0x81,0x80,0x04,0x09,0x81,0x80,0x04,0x0a,0x81,0x80,0x04,0x0b,0x81,0x80,0x04,0x0c,0x81,0x85},
	{0x82,0x82,0x87,0x80,0x46,0x0f,0x86,0x84,0x09,0x10,0x10,0x10,0x11,0x11,0x11,0x12,0x12,0x12,0x81,0x85},
	{0x82,0x82,0x80,0x32,0x83,0x81,0x85},
	{0x82,0x82,0x80,0x05,0x27,0x8a,0x28,0x8a,0x29,0x81,0x80,0x06,0x2a,0x8a,0x2a,0x8a,0x2b,0x8a,0x2b,0x8a,0x2c,0x8a,0x2c,0x8a,0x2d,0x8a,0x2d,0x81,0x80,0x0a,0x2e,0x8a,0x2f,0x8a,0x30,0x8a,0x84,0x09,0x2e,0x2e,0x2e,0x2f,0x2f,0x2f,0x30,0x30,0x30,0x8a,0x2e,0x8a,0x2f,0x8a,0x30,0x81,0x85},
	{0x82,0x82,0x80,0x55,0x16,0x17,0x84,0x0c,0x13,0x13,0x13,0x13,0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,0x88,0x81,0x80,0x0f,0x16,0x84,0x02,0x1a,0x1b,0x89,0x88,0x81,0x80,0x03,0x16,0x1a,0x89,0x88,0x81,0x80,0x03,0x16,0x1c,0x88,0x81,0x80,0x03,0x16,0x1d,0x88,0x81,0x80,0x03,0x16,0x1e,0x88,0x81,0x80,0x03,0x16,0x23,0x88,0x81,0x80,0x03,0x16,0x24,0x88,0x81,0x80,0x32,0x16,0x88,0x81,0x85},
};
/*12FE*/int cycleTicks = 0;/*palette cycling: tick counter*/
/*1300*/int blinkCount = 0;/*script: counter of command 0x88 (blinking)*/
/*1302*/int prevRandom = -1;/*script: the previous random choice of command 0x84*/
/*1304*/int picture = -1;/*script: the picture to draw, -1 none*/
/*1306*/int scrollCount = 14;/*script: counter of command 0x83 (scrolling)*/
/*1308*/int gateLight = 0;/*moongate light: 1 every other tick*/

int PollInput();
int ScaleSpeed(int, int);

/*the computer's monitor shows noise ("snow"): random bits, read from this code
 segment, become pixels*/
/*C_06E9_000E*/MonitorNoise() {
	unsigned char far *dst;
	unsigned char far *src;
	struct tSurface s;
	int x, y;

	switch(videoMode) {
		case 4: case 7:/*CGA, Hercules: 4 pixels a byte*/
			s = monitor;
			s.segment = backBuffer.segment;
			s.pScanlines = backBuffer.pScanlines;
			src = MK_FP(_CS, rand() & 0xe00);
			for(y = s.top; y <= s.bottom; y++) {
				dst = MK_FP(s.segment, s.pScanlines[y]);
				for(x = s.left / 4; x <= s.right / 4; x++) {
					dst[x] = 0;
					if(*src++ & 1)
						dst[x] = 3;
					if(*src++ & 1)
						dst[x] |= 0xc;
					if(*src++ & 1)
						dst[x] |= 0x30;
					if(*src++ & 1)
						dst[x] |= 0xc0;
				}
			}
		break;
		case 9:/*Tandy: 2 pixels a byte*/
			s = monitor;
			s.segment = backBuffer.segment;
			s.pScanlines = backBuffer.pScanlines;
		goto two;
		case 0xd:/*EGA: in the other page, then copied to the screen*/
			s = monitor;
			s.segment = egaSaved.segment;
			s.pScanlines = egaSaved.pScanlines;
			s.left = 0xe5;
			s.top = 0x32;
			s.right = 0x11d;
			s.bottom = 0x56;
		two:
			src = MK_FP(_CS, rand() & 0xe00);
			for(y = s.top; y <= s.bottom; y++) {
				dst = MK_FP(s.segment, s.pScanlines[y]);
				for(x = s.left / 2; x <= s.right / 2; x++) {
					dst[x] = 0;
					if(*src++ & 1)
						dst[x] = 0xf;
					if(*src++ & 1)
						dst[x] |= 0xf0;
				}
			}
			DRV(0xf)(&s, &monitor);
		break;
		case 0x13:/*MCGA: a pixel a byte*/
			src = MK_FP(_CS, rand() & 0xe00);
			for(y = monitor.top; y <= monitor.bottom; y++) {
				dst = MK_FP(monitor.segment, monitor.pScanlines[y]);
				for(x = monitor.left; x <= monitor.right; x++)
					dst[x] = (*src++ & 1)?0xf:0;
			}
		break;
	}
	monitorNoisy = 1;
}

/*MCGA: every 4th call, rotate the palette entries 0xe0-0xe5 (the monitor's glow)*/
/*C_06E9_0262*/CyclePalette() {
	int rgb[3];
	int first[3];
	int i;

	if(videoMode == 0x13 && cycleTicks++ == 3) {
		cycleTicks = 0;
		VID_waitRetrace();
		PAL_get(0xe5, first);
		for(i = 0xe4; i >= 0xe0; i--) {
			PAL_get(i, rgb);
			PAL_set(i + 1, rgb);
		}
		PAL_set(0xe0, first);
	}
}

/*the monitor plays script n from its start*/
/*C_06E9_02D5*/SetScript(int n) {
	scriptPos = 0;
	script = n;
}

/*draw picture n of the monitor (shape n + 0x10 of intro1) at its place*/
/*C_06E9_02E6*/DrawMonitorShape(int n) {
	if(videoMode == 0xd)
		monitor.segment = screen.segment;
	DRV(3)(&monitor, monitorX[n] + monitor.left - monitorScroll, monitorY[n] + monitor.top, intro1Shapes, n + 0x10);
}

/*PollInput, and Escape leaves the introduction*/
/*C_06E9_033A*/int CheckInput() {
	int k;

	k = PollInput();
	if(k == 0x1b)
		escaped = 1;
	return k;
}

/*0 nothing; 1 a mouse click; else the key pressed. A digit key (0-6, mod 7)
 starts that script of the monitor, and counts as nothing*/
/*C_06E9_0351*/int PollInput() {
	char c[2];

	MOUSE_SERVE();
	if(mouseL || mouseR) {
		/*wait for the release*/
		while(mouseL || mouseR)
			MOUSE_SERVE();
		return 1;
	}
	if(!kbhit())
		return 0;
	if(isdigit(c[0] = getch())) {
		SetScript(atoi(c) % 7);/*(c[1] is not set: the atoi of one digit)*/
		return 0;
	}
	return c[0];
}

/*one frame of the monitor: run its script up to the end of the frame (0x81, 0x82,
 0x8a). A byte below 0x80 draws that picture*/
/*C_06E9_03C3*/RunScript() {
	int c;
	int done;
	int n, i;

	monitorNoisy = done = 0;
	while(!done) {
		c = scripts[script][scriptPos];
		scriptPos ++;
		if(c < 0x80) {
			picture = c;
		} else switch(c) {
			case 0x83:/*scroll pictures 0x22 up the screen, draw 0x21*/
				if(scrollCount-- == 0)
					scrollCount = 0xe;
				monitorY[0x22] = 0x15 - scrollCount;
				DrawMonitorShape(0x22);
				monitorY[0x22] = 0x24 - scrollCount;
				DrawMonitorShape(0x22);
				picture = 0x21;
			break;
			case 0x85:/*go on with the next script*/
				scriptPos = 0;
				script = (script + 1) % 7;
			break;
			case 0x82:/*noise, end of the frame*/
				MonitorNoise();
				picture = -1;
			goto stop;
			case 0x80:/*n: repeat n times what follows up to 0x81 (n >= 10: fewer times on a slow computer)*/
				loopCount = scripts[script][scriptPos];
				scriptPos ++;
				if(loopCount >= 10)
					loopCount = ScaleSpeed(loopCount, loopCount / 4);
				loopPos = scriptPos;
			break;
			case 0x81:/*end of the repeated part; end of the frame*/
				if(loopCount) {
					loopCount --;
					scriptPos = loopPos;
				}
			case 0x8a:/*end of the frame*/
			stop:
				done = 1;
			break;
			case 0x84:/*n, n pictures: draw one of them at random, not the one of last time*/
				n = scripts[script][scriptPos];
				scriptPos ++;
				do
					i = rand() % n + scriptPos;
				while(prevRandom == i);
				prevRandom = i;
				picture = scripts[script][i];
				scriptPos += n;
			break;
			case 0x87:/*choose a picture at random*/
				chosenPicture = randomPictures[rand() % 9];
			break;
			case 0x86:/*draw it*/
				picture = chosenPicture;
			break;
			case 0x8b:/*the glow*/
				CyclePalette();
			break;
			case 0x89:/*picture 0x32-0x34 at random*/
				picture = rand() % 3 + 0x32;
			break;
			case 0x88:/*blinking picture 0x25/0x26*/
				blinkCount = (blinkCount + 1) & 0xff;
				if(blinkCount & 4)
					picture = ((blinkCount & 0x10) >> 4) + 0x25;
			break;
		}
		if(picture > -1)
			DrawMonitorShape(picture);
	}
}

/*the timer tick handler of part 3, MCGA: every other tick, rotate the palette
 entries 0x90-0x9f (the moongate's light)*/
/*C_06E9_05BC*/FlashMoongate() {
	if(videoMode == 0x13) {
		if(!gateLight) {
			VID_waitRetrace();
			PAL_get(0x9f, palColors2);
			for(palEntry = 0x9e; palEntry >= 0x90; palEntry--) {
				PAL_get(palEntry, palColors);
				PAL_set(palEntry + 1, palColors);
			}
			PAL_set(0x90, palColors2);
		}
		gateLight = !gateLight;
	}
}

/*a random x, y in the clipping rectangle of s*/
/*C_06E9_0630*/int RandomX(struct tSurface *s) {
	return rand() % (s->right - s->left + 1) + s->left;
}

/*C_06E9_064E*/int RandomY(struct tSurface *s) {
	return rand() % (s->bottom - s->top + 1) + s->top;
}

/*n clouds (shapes 2, 3), at random left of the surface; the cloud bank (shape 0)*/
/*C_06E9_066C*/InitClouds(struct tSurface *s, int n, void far *shapes) {
	int i;

	cloudSurface = s;
	cloudShapes = shapes;
	cloudCount = n;
	for(i = 0; i < cloudCount; i++) {
		cloudY[i] = rand() % (cloudSurface->right - cloudSurface->left) + cloudSurface->left - 0x104;
		cloudShape[i] = rand() % 0x1e;
		cloudX[i] = (rand() & 1) + 2;
		cloudY[i] -= 0x8c;
	}
	bankX = -400;
	bankY = -5;
}

/*draw the clouds and move them right; one out on the right comes back on the left*/
/*C_06E9_06FD*/MoveClouds() {
	int i;

	for(i = 0; i < cloudCount; i++) {
		DRV(3)(cloudSurface, cloudY[i], cloudShape[i], cloudShapes, cloudX[i]);
		if(cloudY[i] > cloudSurface->right) {
			cloudY[i] = rand() % (cloudSurface->right - cloudSurface->left) + cloudSurface->left - 0x140;
			cloudShape[i] = rand() % 0x1e;
			cloudX[i] = (rand() & 1) + 2;
			cloudY[i] -= 0x8c;
		}
		cloudY[i] += cloudSpeed;
	}
	/*the cloud bank, 320 wide, scrolls at half speed*/
	DRV(3)(cloudSurface, bankX, bankY, cloudShapes, 0);
	DRV(3)(cloudSurface, bankX - 0x140, bankY, cloudShapes, 0);
	bankX += cloudSpeed >> 1;
	if(bankX == 0x140)
		bankX = 0;
}

/*at most n - 1 drops of rain, moving every delay + 1 frames*/
/*C_06E9_081A*/InitRain(struct tSurface *s, int n, int delay) {
	rainMax = n - 1;
	rainCount = 0;
	rainDelay = delay;
	rainSurface = s;
}

/*maybe a new drop at the top; move the drops down (and right), one out at the
 bottom comes back at the top; draw them (shapes 4-7 of intro2)*/
/*C_06E9_0838*/MoveRain() {
	int i;

	if(rainCount != rainMax && rand() % rainChance == 0) {
		rainShape[rainCount] = (rand() & 3) + 4;
		rainX[rainCount] = rand() % (rainSurface->right - rainSurface->left + 0x15) + rainSurface->left - 10;
		rainY[rainCount] = rand() % 9 + rainSurface->top - 4;
		rainWait[rainCount] = rainDelay;
		DRV(3)(rainSurface, rainX[rainCount], rainY[rainCount], intro2Shapes, rainShape[rainCount]);
		rainCount ++;
	}
	for(i = 0; i < rainCount; i++) {
		if(rainWait[i]) {
			rainWait[i] --;
		} else {
			rainWait[i] = rainDelay;
			rainX[i] += 2;
			rainY[i] += 10;
			if(rainY[i] > rainSurface->bottom) {
				rainY[i] = rand() % 9 + rainSurface->top - 4;
				rainX[i] = rand() % (rainSurface->right - rainSurface->left + 0x15) + rainSurface->left - 10;
				rainShape[i] = (rand() & 3) + 4;
			}
		}
		DRV(3)(rainSurface, rainX[i], rainY[i], intro2Shapes, rainShape[i]);
	}
}

/*the clock radio of the bedroom shows the time of the computer's clock: 12-hour,
 digits (shapes 2-11) at x, x + 4, x + 10, x + 14; shape 12: no tens of hours*/
/*C_06E9_09DC*/DrawClock(struct tSurface *s, void far *shapes, int x) {
	int hours[24] = {12,1,2,3,4,5,6,7,8,9,10,11,12,1,2,3,4,5,6,7,8,9,10,11};
	struct time t;

	gettime(&t);
	t.ti_hour = hours[t.ti_hour];
	if(t.ti_hour < 10) {
		DRV(3)(s, x, 0x14, shapes, 0xc);
	} else {
		DRV(3)(s, x, 0x14, shapes, t.ti_hour / 10 + 2);
	}
	DRV(3)(s, x + 4, 0x14, shapes, t.ti_hour % 10 + 2);
	DRV(3)(s, x + 0xa, 0x14, shapes, t.ti_min / 10 + 2);
	DRV(3)(s, x + 0xe, 0x14, shapes, t.ti_min % 10 + 2);
}

/*from b toward a, by how fast the computer is (loopsPerTick from 1500 to 20000): b on
 a slow computer, a on a fast one. Not MCGA: b counts twice; EGA: twice the result*/
/*C_06E9_0AF0*/int ScaleSpeed(int a, int b) {
	unsigned d;
	unsigned speed;
	int mul;

	mul = 1;
	if(videoMode == 0xd)
		mul ++;
	if(videoMode != 0x13)
		b *= 2;
	speed = loopsPerTick;
	if(loopsPerTick > 20000)
		speed = 20000;
	if(loopsPerTick < 1500)
		speed = 1500;
	speed -= 1500;
	d = speed / (18500 / (abs(a - b) + 1));
	if(abs(a - b) < d)
		d = abs(a - b);
	if(a > b)
		return (b + d) * mul;
	else
		return (b - d) * mul;
}

/*the introduction. Every part ends at a key or a click (Escape: all of it)*/
/*C_06E9_0BA6*/Introduction() {
	void far *shapes;/*the shapes of the current part*/
	int allocated;/*1: skyCopy has its own buffer*/
	int flashColor[3];/*MCGA, the storm: the color of the lightning (part 3: a color saved)*/
	int sky0[3];/*MCGA, the storm: the colors of the sky*/
	int sky1[3];
	int sky2[3];
	int frames;/*the storm: frame count*/
	int scene;/*the storm: what happens*/
	int x;
	int started;/*the storm: 0 before the first frame*/
	int flashTime;/*the storm: frames left of the lightning*/
	int flashShape;
	int face;/*the storm: the face in the clouds (shape - 0xb)*/
	int prevFace;
	int flashX;
	int flashY;
	int speed;/*steps of the moves, by the speed of the computer*/
	int speed2;
	int speed3;
	int i;
	int done;

	allocated = started = 0;
	skySurface = skyCopy = lowerSurface = backBuffer;
	/*a 320x32 work surface*/
	workArea = screen;
	workArea.left = workArea.top = 0;
	workArea.right = 319;
	workArea.bottom = 31;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = 0xa600;
	LoadPalette(1);
	SavePalette();
	TIM_calibrate();
	TIM_install();
	CheckInput();
	if(escaped)
		goto quit;
	/*---- part 1: the bedroom ----*/
	AskForDisk(1);
	PlayMusic(2);
	switch(videoMode) {
		case 4: shapes = intro1Shapes = LZW_load("intro1.cga"); break;
		case 7: shapes = intro1Shapes = LZW_load("intro1.hrc"); break;
		case 9: shapes = intro1Shapes = LZW_load("intro1.tga"); break;
		case 0xd: shapes = intro1Shapes = LZW_load("intro1.ega"); break;
		case 0x13: shapes = intro1Shapes = LZW_load("intro_1.shp"); break;
	}
	DRV(0xc)(&backBuffer, 0);
	DRV(3)(&backBuffer, 0, 0, shapes, 0x45);/*the title*/
	if(musicCard) {
		MUS(6)(songData);/*play*/
	}
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	for(i = 0; i < 50; i++) {
		CheckInput();
		if(escaped)
			goto end;
		if(videoMode == 0x13)
			VID_waitRetrace();
		else
			Delay(0x50);
	}
	DRV(0xc)(&backBuffer, 0);
	DRV(3)(&backBuffer, 0, 0, shapes, 0x46);/*"presents"?*/
	if(videoMode == 0x13) {
		FADE_out(&screen, 0);
		DRV(0xc)(&screen, 0);
		RestorePalette();
	}
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		skySurface.segment = skyCopy.segment = lowerSurface.segment = backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	for(i = 0; i < 50; i++) {
		CheckInput();
		if(escaped)
			goto end;
		if(videoMode == 0x13)
			VID_waitRetrace();
		else
			Delay(0x50);
	}
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	DRV(0xc)(&backBuffer, 0);
	RestorePalette();
	CheckInput();
	if(escaped)
		goto end;
	/*the bedroom at night: the monitor runs its scripts*/
	monitor = backBuffer;
	monitor.left = 0xe5;
	monitor.top = 0x32;
	monitor.right = 0x11d;
	monitor.bottom = 0x56;
	SetScript(2);
	ShowPassage(0);
	DRV(3)(&skySurface, 0, 0, shapes, 0);
	DRV(3)(&lowerSurface, 0, 0x3f, shapes, 0xd);
	DrawClock(&backBuffer, shapes, 0xdd);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		skySurface.segment = lowerSurface.segment = backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	CheckInput();
	if(escaped)
		goto end;
	monitorScroll = 0;
	/*some frames without music, then until a key; then until a key again*/
	for(i = 0; i < loopsPerTick / 400; i++) {
		DRV(3)(&skySurface, 0, 0, shapes, 0);
		DRV(3)(&lowerSurface, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(monitorNoisy) {
			DRV(3)(&lowerSurface, 0x8f, 0x5b, shapes, 0xe);
			monitorNoisy = 0;
		}
		DrawClock(&backBuffer, shapes, 0xdd);
		ShowFrame();
		if(CheckInput())
			break;
	}
	if(escaped)
		goto end;
	do {
		DRV(3)(&skySurface, 0, 0, shapes, 0);
		DRV(3)(&lowerSurface, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(monitorNoisy) {
			DRV(3)(&lowerSurface, 0x8f, 0x5b, shapes, 0xe);
			monitorNoisy = 0;
		}
		DisplayPassage();
		DrawClock(&backBuffer, shapes, 0xdd);
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	while(kbhit())
		getch();
	ShowPassage(1);
	do {
		DRV(3)(&skySurface, 0, 0, shapes, 0);
		DRV(3)(&lowerSurface, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(monitorNoisy) {
			DRV(3)(&lowerSurface, 0x8f, 0x5b, shapes, 0xe);
			monitorNoisy = 0;
		}
		DisplayPassage();
		DrawClock(&backBuffer, shapes, 0xdd);
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	/*the view scrolls right to the window, 320 pixels: the monitor goes out on the
	 left, the window (shape 0xf) comes in*/
	ShowPassage(2);
	speed = ScaleSpeed(2, 10);
	/*(done is set before the last frame: the one with i at its end)*/
	for(i = 0, done = 0; !done && (done = (i == 320), 1); ) {
		if(i >= 0x87) {
			x = (i - 0x87) * 10 / 0x24 + (320 - i) + 0x4d;
			DRV(3)(&skyCopy, x, 0, shapes, 0xf);
		}
		DRV(3)(&skySurface, skySurface.left - i, 0, shapes, 0);
		DRV(3)(&skyCopy, skyCopy.left + 320 - i, 0, shapes, 1);
		DRV(3)(&lowerSurface, lowerSurface.left - i * 2, 0x3f, shapes, 0xd);
		DisplayPassage();
		if(monitor.right > 0 && i < 300)
			RunScript();
		monitor.right -= speed;
		monitor.left -= speed;
		if(monitor.left < 0) {
			monitorScroll -= monitor.left;
			monitor.left = 0;
		}
		DrawClock(&backBuffer, shapes, 0xdd - i);
		if(videoMode == 0x13)
			VID_waitRetrace();
		ShowFrame();
		if(CheckInput())
			i = 320 - speed;
		if(escaped)
			goto end;
		/*up to 320, the last step stops on it*/
		i += speed;
		i += (320 - i) * (i >= 320);
	}
	farfree(shapes);
	/*---- part 2: the storm ----*/
	AskForDisk(1);
	switch(videoMode) {
		case 4: shapes = intro2Shapes = LZW_load("intro2.cga"); break;
		case 7: shapes = intro2Shapes = LZW_load("intro2.hrc"); break;
		case 9: shapes = intro2Shapes = LZW_load("intro2.tga"); break;
		case 0xd: shapes = intro2Shapes = LZW_load("intro2.ega"); break;
		case 0x13: shapes = intro2Shapes = LZW_load("intro_2.shp"); break;
	}
	while(!CheckInput());
	if(escaped)
		goto end;
	skySurface.right = 0x101;
	skySurface.left = 0x39;
	skySurface.top = 0;
	skySurface.bottom = 0xb0;
	skyCopy.top = 0;
	skyCopy.bottom = 0x2e;
	speed3 = ScaleSpeed(1, 5);
	speed2 = ScaleSpeed(4, 12);
	cloudSpeed = ScaleSpeed(2, 10);
	rainChance = ScaleSpeed(4, 1);
	/*the storm*/
	InitRain(&backBuffer, 150, 0);
	InitClouds(&backBuffer, 5, shapes);
	ShowPassage(3);
	done = i = 0;
	frames = done;
	flashTime = face = scene = done;
	while(!done) {
		DRV(3)(&backBuffer, 0, 0, shapes, 1);
		MoveClouds();
		/*lightning*/
		if(rand() % 6 == 0 && frames >= 60 && flashTime == 0 && started != 0) {
			flashTime = (rand() & 3) + 1;
			flashShape = rand() & 7;
			flashX = RandomX(&skyCopy);
			flashY = RandomY(&skyCopy);
		}
		if(flashTime) {
			DRV(3)(&backBuffer, rand() % 3 + flashX - 2, rand() % 3 + flashY - 2, shapes, flashShape + 0xb);
		}
		if(videoMode == 0x13 && started) {
			VID_inRetrace();
			/*the sky lit by the lightning*/
			if((flashTime || scene == 3) && (rand() % 3 || scene == 3)) {
				PAL_set(0x58, flashColor);
				PAL_set(0x5a, flashColor);
				PAL_set(0x5c, flashColor);
			} else {
				PAL_set(0x58, sky0);
				PAL_set(0x5a, sky1);
				PAL_set(0x5c, sky2);
			}
		}
		DRV(3)(&backBuffer, 0, 0x4c, shapes, 10);
		DRV(3)(&backBuffer, 0, 0, shapes, 8);
		prevFace = face;
		while(face == prevFace)
			face = rand() % 5 + 8;
		if(scene == 3 || (scene == 4 && i < 0xa0)) {
			DRV(3)(&backBuffer, 0x9e, 0x72, shapes, face + 0xb);
		}
		MoveRain();
		if(scene < 5) {
			if((flashTime && (rand() & 1)) || scene == 3) {
				DRV(3)(&backBuffer, skySurface.left - i, skySurface.top, shapes, 0x1b);
			} else {
				DRV(3)(&backBuffer, skySurface.left - i, skySurface.top, shapes, 0x1a);
			}
		}
		/*0-3: shape 0x1c (3: the face too); 4: it leaves on the left and the
		 two halves 0x18, 0x19 come in from the right; 5: they stay; 6: they part;
		 7: the end*/
		switch(scene) {
			case 1: case 2: case 3:
				DRV(3)(&backBuffer, 0, 0, shapes, 0x1c);
			goto music;
			case 0:
				DRV(3)(&backBuffer, 0, 0, shapes, 0x1c);
			break;
			case 4:
				DRV(3)(&backBuffer, 0 - i, 0, shapes, 0x1c);
				DRV(3)(&backBuffer, 320 - i, 0, shapes, 0x18);
				DRV(3)(&backBuffer, 320 - i + 0xfd, 0, shapes, 0x19);
				i += speed2;
				if(i > 320) {
					scene = 5;
					i = 0;
				}
			goto music;
			case 5:
				DRV(3)(&backBuffer, 0, 0, shapes, 0x18);
				DRV(3)(&backBuffer, 0xfd, 0, shapes, 0x19);
			goto music;
			case 6:
				DRV(3)(&backBuffer, 0 - i, 0, shapes, 0x18);
				DRV(3)(&backBuffer, i + 0xfd, 0, shapes, 0x19);
				i += speed3;
				if(i > 0x4b) {
					ShowPassage(8);
					i = 0;
					scene = 7;
				}
			break;
			case 7:
			music:
				DisplayPassage();
			break;
		}
		if(!started) {
			started ++;
			if(videoMode == 0x13) {
				VID_waitRetrace();
				LoadPalette(2);
				PAL_get(0x58, sky0);
				PAL_get(0x5a, sky1);
				PAL_get(0x5c, sky2);
				PAL_get(0x75, flashColor);
			}
		}
		ShowFrame();
		/*a key: the next scene*/
		if(CheckInput()) {
			switch(scene) {
				case 6:
					ShowPassage(8);
				case 0: case 4:
					scene ++;
					i = 0;
				break;
				case 1:
					ShowPassage(4);
				goto next;
				case 2:
					ShowPassage(5);
				goto next;
				case 3:
					ShowPassage(6);
				case 5:
				next:
					scene ++;
				break;
				default:
					done = 1;
			}
		}
		if(escaped)
			goto end;
		if(flashTime)
			flashTime --;
		if(frames != 1000)
			frames ++;
		else
			frames = 900;
		if(frames >= 40 && scene == 0)
			scene = 1;/*the music begins*/
	}
	CheckInput();
	if(escaped)
		goto end;
	farfree(shapes);
	/*---- part 3: the moongate ----*/
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	DRV(0xc)(&backBuffer, 0);
	AskForDisk(2);
	switch(videoMode) {
		case 4: shapes = LZW_load("intro3.cga"); break;
		case 7: shapes = LZW_load("intro3.hrc"); break;
		case 9: shapes = LZW_load("intro3.tga"); break;
		case 0xd:
			shapes = LZW_load("intro3.ega");
			DRV(0xc)(&screen, 0);
			DRV(0x15)(&screen);
			DRV(0xc)(&screen, 0);
			DRV(0x15)(&screen);
		break;
		case 0x13: shapes = LZW_load("intro_3.shp"); break;
	}
	LoadPalette(3);
	CheckInput();
	if(escaped)
		goto end;
	ShowPassage(9);
	DRV(3)(&backBuffer, 0, 0, shapes, 0);
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	CheckInput();
	if(escaped)
		goto end;
	TIM_setCallback(FlashMoongate);/*the moongate's light*/
	for(i = 0; i < 3; i++) {
		DRV(3)(&backBuffer, 0, 0, shapes, 0);
		ShowFrame();
		if(CheckInput())
			break;
	}
	if(escaped)
		goto end;
	do {
		DRV(3)(&backBuffer, 0, 0, shapes, 0);
		DisplayPassage();
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	if(videoMode == 0xd) {
		screen.bottom = 0x46;
		DRV(0xc)(&screen, 0);
		DRV(0x15)(&screen);
		DRV(0xc)(&screen, 0);
		DRV(0x15)(&screen);
		screen.bottom = 199;
	} else {
		backBuffer.bottom = 100;
		DRV(0xc)(&backBuffer, 0);
		backBuffer.bottom = 199;
	}
	/*clipped to the upper half*/
	skySurface = backBuffer;
	skySurface.bottom = 0x65;
	ShowPassage(10);
	speed = ScaleSpeed(4, 10);
	for(i = 199, done = 0; !done && (done = (i == 0x54), 1); ) {
		DRV(3)(&backBuffer, 0, 0, shapes, 0);
		DRV(3)(&backBuffer, 0xbd, i, shapes, 1);
		DRV(3)(&backBuffer, 0x96, 0x64, shapes, 3);
		DisplayPassage();
		ShowFrame();
		if(CheckInput())
			i = speed + 0x54;
		if(escaped)
			goto end;
		i -= speed;
		i += (0x54 - i) * (i <= 0x54);
	}
	ShowPassage(0xb);
	while(!CheckInput());
	speed = ScaleSpeed(2, 8);
	for(i = 0x65, done = 0; !done && (done = (i == 6), 1); ) {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&backBuffer, 0xbd, 0x54, shapes, 1);
		DRV(3)(&skySurface, 0x7c, i, shapes, 2);
		DisplayPassage();
		ShowFrame();
		if(CheckInput())
			i = speed + 6;
		if(escaped)
			goto end;
		i -= speed;
		i += (6 - i) * (i <= 6);
	}
	speed = ScaleSpeed(4, 10);
	for(i = 0x54, done = 0; !done && (done = (i == 0xcc), 1); ) {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&backBuffer, 0xbd, i, shapes, 1);
		DRV(3)(&skySurface, 0x7c, 5, shapes, 2);
		DisplayPassage();
		ShowFrame();
		if(CheckInput())
			i = 0xcc - speed;
		i += speed;
		i += (0xcc - i) * (i >= 0xcc);
	}
	while(!CheckInput());
	if(escaped)
		goto end;
	speed = ScaleSpeed(2, 8);
	for(i = 0, done = 0; !done && (done = (i == 0x3f), 1); ) {
		DRV(3)(&backBuffer, 0, 0 - i, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66 - i, shapes, 4);
		DRV(3)(&skySurface, 0x7c, 5 - i, shapes, 2);
		backBuffer.top = 199 - i;
		DRV(0xc)(&backBuffer, 0);
		backBuffer.top = 0;
		DRV(3)(&backBuffer, 0x9b, 200 - i * 2, shapes, 6);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(escaped)
			goto end;
		i += speed;
		i += (0x3f - i) * (i >= 0x3f);
	}
	ShowPassage(0xc);
	do {
		DRV(3)(&backBuffer, 0, -0x3f, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x27, shapes, 4);
		DRV(3)(&skySurface, 0x7c, -0x3a, shapes, 2);
		backBuffer.top = 0x87;
		DRV(0xc)(&backBuffer, 0);
		backBuffer.top = 0;
		DRV(3)(&backBuffer, 0x9b, 0x4a, shapes, 6);
		DisplayPassage();
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	DRV(0xc)(&backBuffer, 0);
	speed = ScaleSpeed(2, 8);
	for(i = 0x3f, done = 0; !done && (done = (i == 0), 1); ) {
		DRV(3)(&backBuffer, 0, 0 - i, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66 - i, shapes, 4);
		DRV(3)(&skySurface, 0x7c, 5 - i, shapes, 2);
		backBuffer.top = 199 - i;
		DRV(0xc)(&backBuffer, 0);
		backBuffer.top = 0;
		DRV(3)(&backBuffer, 0x9b, 200 - i * 2, shapes, 6);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(escaped)
			goto end;
		i -= speed;
		i += (0 - i) * (i <= 0);
	}
	ShowPassage(0xd);
	do {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&skySurface, 0x7c, 5, shapes, 2);
		DisplayPassage();
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	ShowPassage(0xe);
	do {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&skySurface, (rand() & 1) + 0x7c, (rand() & 1) + 5, shapes, 2);
		DisplayPassage();
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	ShowPassage(0xf);
	if(videoMode == 0x13) {
		PAL_get(0x19, sky0);
		PAL_get(0x19, flashColor);
		flashColor[0] = flashColor[1] = flashColor[2] = 0;
		PAL_set(0x19, flashColor);
	}
	do {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&skySurface, (rand() & 1) + 0x7c, (rand() & 3) + 5, shapes, 2);
		DisplayPassage();
		ShowFrame();
	} while(!CheckInput());
	if(escaped)
		goto end;
	/*skyCopy: a copy of the rectangle of skySurface, at 0,0*/
	skyCopy.left = skyCopy.right = 0;
	skyCopy.bottom = skySurface.bottom - skySurface.top;
	skyCopy.right = skySurface.right - skySurface.left;
	if(videoMode != 0xd) {
		SURF_alloc(&skyCopy);
		allocated = 1;
	} else {
		allocated = 0;
	}
	speed = ScaleSpeed(1, 3);
	for(i = 0, done = 0; !done && (done = (i == 0x3b), 1); ) {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&skySurface, (rand() & 1) + 0x7c, (rand() & 3) + 5, shapes, 2);
		DRV(3)(&backBuffer, -2, 0x12 - i, shapes, i / 3 + 7);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(escaped)
			goto end;
		i += speed;
		i += (0x3b - i) * (i >= 0x3b);
	}
	DRV(3)(&backBuffer, 0, 0, shapes, 5);
	DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
	DRV(3)(&skySurface, 0x7c, 5, shapes, 2);
	if(videoMode == 0xd) {
		introPage2 = screen;
		introPage2.segment = 0xa600;
		DRV(0xf)(&screen, &introPage2);
	} else {
		DRV(0xf)(&skySurface, &skyCopy);
	}
	CheckInput();
	if(escaped)
		goto end;
	DRV(3)(&backBuffer, -2, -0x2a, shapes, 0x1a);
	ShowFrame();
	CheckInput();
	if(escaped)
		goto end;
	/*the dissolve*/
	switch(videoMode) {
		case 4: case 7:
			SURF_share(&screen, &skySurface, 0);
			speed = ScaleSpeed(200, 300);
			DIS_dissolveCGA(&skyCopy, &skySurface, speed);
		goto show;
		case 9:
			SURF_share(&screen, &skySurface, 0);
			speed = ScaleSpeed(200, 300);
			DIS_dissolveTDY(&skyCopy, &skySurface, speed);
		goto show;
		case 0xd:
			screen.segment ^= 0x200;
			speed = ScaleSpeed(200, 300);
			DIS_dissolveEGA(&introPage2, &screen, speed);
			screen.segment ^= 0x200;
		break;
		case 0x13:
			SURF_share(&screen, &skySurface, 0);
			DIS_dissolve(&skyCopy, &skySurface, 0x1e);
		show:
			SURF_share(&backBuffer, &skySurface, 0);
		break;
	}
	CheckInput();
	if(escaped)
		goto end;
	speed = ScaleSpeed(2, 8);
	for(i = 5, done = 0; !done && (done = (i == 0x65), 1); ) {
		DRV(3)(&backBuffer, 0, 0, shapes, 5);
		DRV(3)(&backBuffer, 0x5e, 0x66, shapes, 4);
		DRV(3)(&skySurface, 0x7c, i, shapes, 2);
		ShowFrame();
		if(CheckInput())
			i = 0x65 - speed;
		if(escaped)
			goto end;
		i += speed;
		i += (0x65 - i) * (i >= 0x65);
	}
	if(videoMode == 0x13)
		PAL_set(0x19, sky0);
	DRV(0xc)(&backBuffer, 0);
	while(!CheckInput()) {
		DRV(3)(&backBuffer, 0, 0, shapes, 0);
		DRV(3)(&backBuffer, 0x96, 0x64, shapes, 3);
		ShowFrame();
	}
end:/*cleanup*/
	if(allocated) {
		SURF_free(&skyCopy);
		allocated = 0;
	}
	farfree(shapes);
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	DRV(0xc)(&backBuffer, 0);
quit:
	AskForDisk(2);
	if(videoMode == 0x13)
		TIM_resetCallback();
	TIM_remove();
	if(videoMode != 0xd)
		SURF_free(&workArea);
	SetPalette();
	PlayMusic(0);
}

/*the frame drawn in the back buffer goes to the screen (EGA: page flip)*/
/*C_06E9_2AF6*/ShowFrame() {
	if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		skySurface.segment = skyCopy.segment = lowerSurface.segment = backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
}

/*wait n times loopsPerTick / 400 loops*/
/*C_06E9_2B34*/Delay(int n) {
	int i;
	int dummy;/*(never set)*/

	while(n--)
		for(i = 0; i < loopsPerTick / 400; i++)
			dummy += n;
}
