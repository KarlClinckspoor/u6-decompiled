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
extern C_00E1_04F9();
extern C_00E1_052D();
extern C_00E1_0561();
extern C_00E1_178A();
extern C_09A0_000B();/*start the music/sound piece n*/
extern C_09A0_21C5();/*called every frame while a piece plays*/
extern C_0BC1_0079();/*AskForDisk(n)*/
extern C_0BC1_03D2();/*LoadPalette(n)*/
extern C_1277_000C();/*(struct tSurface *): give the surface its own pixel buffer*/
extern C_1277_00D8();/*(struct tSurface *): free it*/
extern C_12EA_000A();/*(dst, src, 0): copy a surface*/
extern C_132C_0008();/*poll the mouse (D_5046, D_5047)*/
extern C_1368_0002();/*wait for the vertical retrace?*/
extern C_1368_0015();/*wait for the vertical retrace*/
extern C_136D_014D();/*(&screen, &back buffer): fade in (MCGA)*/
extern C_136D_0346();/*(&back buffer, 0): fade out (MCGA)*/
void far *C_13BC_000C(char *);/*load a shapes file*/
extern C_13F2_000C();/*(dst, src, speed): dissolve, MCGA*/
extern C_13F2_01D5();/*(dst, src, speed): dissolve, EGA*/
extern C_13F2_037A();/*(dst, src, speed): dissolve, CGA/Hercules*/
extern C_13F2_0504();/*(dst, src, speed): dissolve, Tandy*/
extern C_1475_0099();/*(i, int rgb[3]): set palette entry i*/
extern C_1475_00B8();/*(i, int rgb[3]): get palette entry i*/
extern C_1482_000F();/*(void (far *)()): install a timer tick handler*/
extern C_1482_001F();/*remove it*/
extern C_1482_0055();
extern C_1482_009C();
extern C_1482_00BE();

/*---- data ----*/
extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern char D_5046, D_5047;/*mouse buttons*/
extern int D_58F0;/*1: Escape was pressed: leave the introduction*/
extern unsigned long D_6836;/*speed of the computer (loop count, see Delay)*/
extern struct tSurface D_5710;/*the back buffer*/
extern struct tSurface D_5732;/*the screen*/
extern int D_5746;/*music on*/
extern struct tSurface D_5748;/*EGA: the page the noise is drawn into*/
extern void far *D_5692;/*the music driver's song*/
/*the music driver (as GAME.EXE's MUS_Driver): its entries take the pascal convention*/

/*the scenes are drawn in layers, each with its own copy of the screen surface
 (its own clipping rectangle)*/
extern struct tSurface D_5BAE, D_5BBC, D_5BCA;
extern struct tSurface D_5BD8;/*EGA: the second page*/
extern struct tSurface D_6072;/*320x32 work surface*/

/*part 1: the computer's monitor*/
extern struct tSurface D_5FBA;/*the monitor's screen*/
extern int D_606E;/*1: the monitor showed noise: redraw its frame*/
extern void far *D_6060;/*shapes of intro1*/
extern int D_6066;/*how far the monitor's picture went out of the screen on the left*/
extern int D_5BE8;/*script: the current one*/
extern int D_5BF8;/*script: position in it*/
extern int D_5B9A;/*script: position of the loop*/
extern int D_5B9C;/*script: loop count*/
extern int D_5B9E;/*script: the picture chosen by command 0x87*/

/*part 2: the storm*/
extern struct tSurface *D_5BEE;/*clouds: the surface*/
extern void far *D_5BEA;/*clouds: the shapes*/
extern int D_5BE6;/*clouds: their number*/
extern int D_5E66[], D_5E7A[], D_5D26[];/*clouds: x, y, shape of each*/
extern int D_5BF4, D_5BF6;/*clouds: x, y of the big cloud bank*/
extern int D_605E;/*clouds: speed*/
extern struct tSurface *D_5BF0;/*rain: the surface*/
extern int D_5BF2;/*rain: frames between the moves of a drop*/
extern int D_606C;/*rain: the maximum number of drops - 1*/
extern int D_6070;/*rain: the number of drops*/
extern int D_6064;/*rain: 1 in n chances of a new drop every frame*/
extern char D_5FC8[];/*rain: shape of each drop*/
extern int D_5BFA[], D_5D3A[];/*rain: x, y of each drop*/
extern int D_5E8E[];/*rain: frames before the next move of each drop*/
extern void far *D_6068;/*shapes of intro2*/

/*part 3: the moongate*/
extern int D_5BA0;/*palette entry*/
extern int D_5BA2[3], D_5BA8[3];/*palette colors*/

/*---- this module's initialised data ----*/
/*0FE4*/int D_0FE4[9] = {5,6,7,13,14,24,25,31,32};/*the pictures command 0x87 chooses from*/
/*0FF6*/int D_0FF6[54] = {/*x of each picture of the monitor (in its rectangle)*/
	0,0,0,0,0,31,31,31,0,0,0,0,0,31,31,0,9,9,
	9,12,12,12,0,4,31,31,4,0,4,4,4,31,31,0,0,6,
	6,8,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
/*1062*/int D_1062[54] = {/*y of each picture of the monitor ([0x22] moved by command 0x83)*/
	0,0,0,0,0,2,2,2,0,0,0,0,0,2,2,0,7,7,
	7,3,3,3,0,2,2,2,0,0,0,0,1,2,2,0,0,3,
	8,29,28,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
/*the 7 scripts of the monitor (see RunScript): below 0x80 a picture to draw;
 0x80 n: repeat n times up to 0x81; 0x81: end of the repeated part and of a frame;
 0x82: noise, end of a frame; 0x83: scroll; 0x84 n, n pictures: one at random;
 0x85: next script; 0x86: draw the picture chosen by 0x87; 0x87: choose one of D_0FE4;
 0x88: blink 0x25/0x26; 0x89: 0x32-0x34 at random; 0x8a: end of a frame; 0x8b: glow*/
/*10CE*/unsigned char D_10CE[7][0x50] = {
	{0x82,0x82,0x80,0x03,0x02,0x8a,0x02,0x8a,0x01,0x8a,0x01,0x8a,0x00,0x8a,0x00,0x8a,0x01,0x8a,0x01,0x81,0x85},
	{0x82,0x82,0x80,0x28,0x03,0x8b,0x81,0x85},
	{0x82,0x82,0x80,0x04,0x04,0x81,0x80,0x04,0x08,0x81,0x80,0x04,0x09,0x81,0x80,0x04,0x0a,0x81,0x80,0x04,0x0b,0x81,0x80,0x04,0x0c,0x81,0x85},
	{0x82,0x82,0x87,0x80,0x46,0x0f,0x86,0x84,0x09,0x10,0x10,0x10,0x11,0x11,0x11,0x12,0x12,0x12,0x81,0x85},
	{0x82,0x82,0x80,0x32,0x83,0x81,0x85},
	{0x82,0x82,0x80,0x05,0x27,0x8a,0x28,0x8a,0x29,0x81,0x80,0x06,0x2a,0x8a,0x2a,0x8a,0x2b,0x8a,0x2b,0x8a,0x2c,0x8a,0x2c,0x8a,0x2d,0x8a,0x2d,0x81,0x80,0x0a,0x2e,0x8a,0x2f,0x8a,0x30,0x8a,0x84,0x09,0x2e,0x2e,0x2e,0x2f,0x2f,0x2f,0x30,0x30,0x30,0x8a,0x2e,0x8a,0x2f,0x8a,0x30,0x81,0x85},
	{0x82,0x82,0x80,0x55,0x16,0x17,0x84,0x0c,0x13,0x13,0x13,0x13,0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,0x88,0x81,0x80,0x0f,0x16,0x84,0x02,0x1a,0x1b,0x89,0x88,0x81,0x80,0x03,0x16,0x1a,0x89,0x88,0x81,0x80,0x03,0x16,0x1c,0x88,0x81,0x80,0x03,0x16,0x1d,0x88,0x81,0x80,0x03,0x16,0x1e,0x88,0x81,0x80,0x03,0x16,0x23,0x88,0x81,0x80,0x03,0x16,0x24,0x88,0x81,0x80,0x32,0x16,0x88,0x81,0x85},
};
/*12FE*/int D_12FE = 0;/*palette cycling: tick counter*/
/*1300*/int D_1300 = 0;/*script: counter of command 0x88 (blinking)*/
/*1302*/int D_1302 = -1;/*script: the previous random choice of command 0x84*/
/*1304*/int D_1304 = -1;/*script: the picture to draw, -1 none*/
/*1306*/int D_1306 = 14;/*script: counter of command 0x83 (scrolling)*/
/*1308*/int D_1308 = 0;/*moongate light: 1 every other tick*/

int PollInput();
int ScaleSpeed(int, int);

/*the computer's monitor shows noise ("snow"): random bits, read from this code
 segment, become pixels*/
/*C_06E9_000E*/MonitorNoise() {
	unsigned char far *dst;
	unsigned char far *src;
	struct tSurface s;
	int x, y;

	switch(D_5044) {
		case 4: case 7:/*CGA, Hercules: 4 pixels a byte*/
			s = D_5FBA;
			s.segment = D_5732.segment;
			s.pScanlines = D_5732.pScanlines;
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
			s = D_5FBA;
			s.segment = D_5732.segment;
			s.pScanlines = D_5732.pScanlines;
		goto two;
		case 0xd:/*EGA: in the other page, then copied to the screen*/
			s = D_5FBA;
			s.segment = D_5748.segment;
			s.pScanlines = D_5748.pScanlines;
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
			DRV(0xf)(&s, &D_5FBA);
		break;
		case 0x13:/*MCGA: a pixel a byte*/
			src = MK_FP(_CS, rand() & 0xe00);
			for(y = D_5FBA.top; y <= D_5FBA.bottom; y++) {
				dst = MK_FP(D_5FBA.segment, D_5FBA.pScanlines[y]);
				for(x = D_5FBA.left; x <= D_5FBA.right; x++)
					dst[x] = (*src++ & 1)?0xf:0;
			}
		break;
	}
	D_606E = 1;
}

/*MCGA: every 4th call, rotate the palette entries 0xe0-0xe5 (the monitor's glow)*/
/*C_06E9_0262*/CyclePalette() {
	int rgb[3];
	int first[3];
	int i;

	if(D_5044 == 0x13 && D_12FE++ == 3) {
		D_12FE = 0;
		C_1368_0015();
		C_1475_00B8(0xe5, first);
		for(i = 0xe4; i >= 0xe0; i--) {
			C_1475_00B8(i, rgb);
			C_1475_0099(i + 1, rgb);
		}
		C_1475_0099(0xe0, first);
	}
}

/*the monitor plays script n from its start*/
/*C_06E9_02D5*/SetScript(int n) {
	D_5BF8 = 0;
	D_5BE8 = n;
}

/*draw picture n of the monitor (shape n + 0x10 of intro1) at its place*/
/*C_06E9_02E6*/DrawMonitorShape(int n) {
	if(D_5044 == 0xd)
		D_5FBA.segment = D_5710.segment;
	DRV(3)(&D_5FBA, D_0FF6[n] + D_5FBA.left - D_6066, D_1062[n] + D_5FBA.top, D_6060, n + 0x10);
}

/*PollInput, and Escape leaves the introduction*/
/*C_06E9_033A*/int CheckInput() {
	int k;

	k = PollInput();
	if(k == 0x1b)
		D_58F0 = 1;
	return k;
}

/*0 nothing; 1 a mouse click; else the key pressed. A digit key (0-6, mod 7)
 starts that script of the monitor, and counts as nothing*/
/*C_06E9_0351*/int PollInput() {
	char c[2];

	C_132C_0008();
	if(D_5046 || D_5047) {
		/*wait for the release*/
		while(D_5046 || D_5047)
			C_132C_0008();
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

	D_606E = done = 0;
	while(!done) {
		c = D_10CE[D_5BE8][D_5BF8];
		D_5BF8 ++;
		if(c < 0x80) {
			D_1304 = c;
		} else switch(c) {
			case 0x83:/*scroll pictures 0x22 up the screen, draw 0x21*/
				if(D_1306-- == 0)
					D_1306 = 0xe;
				D_1062[0x22] = 0x15 - D_1306;
				DrawMonitorShape(0x22);
				D_1062[0x22] = 0x24 - D_1306;
				DrawMonitorShape(0x22);
				D_1304 = 0x21;
			break;
			case 0x85:/*go on with the next script*/
				D_5BF8 = 0;
				D_5BE8 = (D_5BE8 + 1) % 7;
			break;
			case 0x82:/*noise, end of the frame*/
				MonitorNoise();
				D_1304 = -1;
			goto stop;
			case 0x80:/*n: repeat n times what follows up to 0x81 (n >= 10: fewer times on a slow computer)*/
				D_5B9C = D_10CE[D_5BE8][D_5BF8];
				D_5BF8 ++;
				if(D_5B9C >= 10)
					D_5B9C = ScaleSpeed(D_5B9C, D_5B9C / 4);
				D_5B9A = D_5BF8;
			break;
			case 0x81:/*end of the repeated part; end of the frame*/
				if(D_5B9C) {
					D_5B9C --;
					D_5BF8 = D_5B9A;
				}
			case 0x8a:/*end of the frame*/
			stop:
				done = 1;
			break;
			case 0x84:/*n, n pictures: draw one of them at random, not the one of last time*/
				n = D_10CE[D_5BE8][D_5BF8];
				D_5BF8 ++;
				do
					i = rand() % n + D_5BF8;
				while(D_1302 == i);
				D_1302 = i;
				D_1304 = D_10CE[D_5BE8][i];
				D_5BF8 += n;
			break;
			case 0x87:/*choose a picture at random*/
				D_5B9E = D_0FE4[rand() % 9];
			break;
			case 0x86:/*draw it*/
				D_1304 = D_5B9E;
			break;
			case 0x8b:/*the glow*/
				CyclePalette();
			break;
			case 0x89:/*picture 0x32-0x34 at random*/
				D_1304 = rand() % 3 + 0x32;
			break;
			case 0x88:/*blinking picture 0x25/0x26*/
				D_1300 = (D_1300 + 1) & 0xff;
				if(D_1300 & 4)
					D_1304 = ((D_1300 & 0x10) >> 4) + 0x25;
			break;
		}
		if(D_1304 > -1)
			DrawMonitorShape(D_1304);
	}
}

/*the timer tick handler of part 3, MCGA: every other tick, rotate the palette
 entries 0x90-0x9f (the moongate's light)*/
/*C_06E9_05BC*/FlashMoongate() {
	if(D_5044 == 0x13) {
		if(!D_1308) {
			C_1368_0015();
			C_1475_00B8(0x9f, D_5BA8);
			for(D_5BA0 = 0x9e; D_5BA0 >= 0x90; D_5BA0--) {
				C_1475_00B8(D_5BA0, D_5BA2);
				C_1475_0099(D_5BA0 + 1, D_5BA2);
			}
			C_1475_0099(0x90, D_5BA8);
		}
		D_1308 = !D_1308;
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

	D_5BEE = s;
	D_5BEA = shapes;
	D_5BE6 = n;
	for(i = 0; i < D_5BE6; i++) {
		D_5E66[i] = rand() % (D_5BEE->right - D_5BEE->left) + D_5BEE->left - 0x104;
		D_5E7A[i] = rand() % 0x1e;
		D_5D26[i] = (rand() & 1) + 2;
		D_5E66[i] -= 0x8c;
	}
	D_5BF4 = -400;
	D_5BF6 = -5;
}

/*draw the clouds and move them right; one out on the right comes back on the left*/
/*C_06E9_06FD*/MoveClouds() {
	int i;

	for(i = 0; i < D_5BE6; i++) {
		DRV(3)(D_5BEE, D_5E66[i], D_5E7A[i], D_5BEA, D_5D26[i]);
		if(D_5E66[i] > D_5BEE->right) {
			D_5E66[i] = rand() % (D_5BEE->right - D_5BEE->left) + D_5BEE->left - 0x140;
			D_5E7A[i] = rand() % 0x1e;
			D_5D26[i] = (rand() & 1) + 2;
			D_5E66[i] -= 0x8c;
		}
		D_5E66[i] += D_605E;
	}
	/*the cloud bank, 320 wide, scrolls at half speed*/
	DRV(3)(D_5BEE, D_5BF4, D_5BF6, D_5BEA, 0);
	DRV(3)(D_5BEE, D_5BF4 - 0x140, D_5BF6, D_5BEA, 0);
	D_5BF4 += D_605E >> 1;
	if(D_5BF4 == 0x140)
		D_5BF4 = 0;
}

/*at most n - 1 drops of rain, moving every delay + 1 frames*/
/*C_06E9_081A*/InitRain(struct tSurface *s, int n, int delay) {
	D_606C = n - 1;
	D_6070 = 0;
	D_5BF2 = delay;
	D_5BF0 = s;
}

/*maybe a new drop at the top; move the drops down (and right), one out at the
 bottom comes back at the top; draw them (shapes 4-7 of intro2)*/
/*C_06E9_0838*/MoveRain() {
	int i;

	if(D_6070 != D_606C && rand() % D_6064 == 0) {
		D_5FC8[D_6070] = (rand() & 3) + 4;
		D_5BFA[D_6070] = rand() % (D_5BF0->right - D_5BF0->left + 0x15) + D_5BF0->left - 10;
		D_5D3A[D_6070] = rand() % 9 + D_5BF0->top - 4;
		D_5E8E[D_6070] = D_5BF2;
		DRV(3)(D_5BF0, D_5BFA[D_6070], D_5D3A[D_6070], D_6068, D_5FC8[D_6070]);
		D_6070 ++;
	}
	for(i = 0; i < D_6070; i++) {
		if(D_5E8E[i]) {
			D_5E8E[i] --;
		} else {
			D_5E8E[i] = D_5BF2;
			D_5BFA[i] += 2;
			D_5D3A[i] += 10;
			if(D_5D3A[i] > D_5BF0->bottom) {
				D_5D3A[i] = rand() % 9 + D_5BF0->top - 4;
				D_5BFA[i] = rand() % (D_5BF0->right - D_5BF0->left + 0x15) + D_5BF0->left - 10;
				D_5FC8[i] = (rand() & 3) + 4;
			}
		}
		DRV(3)(D_5BF0, D_5BFA[i], D_5D3A[i], D_6068, D_5FC8[i]);
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

/*from b toward a, by how fast the computer is (D_6836 from 1500 to 20000): b on
 a slow computer, a on a fast one. Not MCGA: b counts twice; EGA: twice the result*/
/*C_06E9_0AF0*/int ScaleSpeed(int a, int b) {
	unsigned d;
	unsigned speed;
	int mul;

	mul = 1;
	if(D_5044 == 0xd)
		mul ++;
	if(D_5044 != 0x13)
		b *= 2;
	speed = D_6836;
	if(D_6836 > 20000)
		speed = 20000;
	if(D_6836 < 1500)
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
	int allocated;/*1: D_5BBC has its own buffer*/
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
	D_5BAE = D_5BBC = D_5BCA = D_5732;
	/*a 320x32 work surface*/
	D_6072 = D_5710;
	D_6072.left = D_6072.top = 0;
	D_6072.right = 319;
	D_6072.bottom = 31;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.segment = 0xa600;
	C_0BC1_03D2(1);
	C_00E1_0561();
	C_1482_00BE();
	C_1482_0055();
	CheckInput();
	if(D_58F0)
		goto quit;
	/*---- part 1: the bedroom ----*/
	C_0BC1_0079(1);
	C_00E1_178A(2);
	switch(D_5044) {
		case 4: shapes = D_6060 = C_13BC_000C("intro1.cga"); break;
		case 7: shapes = D_6060 = C_13BC_000C("intro1.hrc"); break;
		case 9: shapes = D_6060 = C_13BC_000C("intro1.tga"); break;
		case 0xd: shapes = D_6060 = C_13BC_000C("intro1.ega"); break;
		case 0x13: shapes = D_6060 = C_13BC_000C("intro_1.shp"); break;
	}
	DRV(0xc)(&D_5732, 0);
	DRV(3)(&D_5732, 0, 0, shapes, 0x45);/*the title*/
	if(D_5746) {
		MUS(6)(D_5692);/*play*/
	}
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5732.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	for(i = 0; i < 50; i++) {
		CheckInput();
		if(D_58F0)
			goto end;
		if(D_5044 == 0x13)
			C_1368_0015();
		else
			Delay(0x50);
	}
	DRV(0xc)(&D_5732, 0);
	DRV(3)(&D_5732, 0, 0, shapes, 0x46);/*"presents"?*/
	if(D_5044 == 0x13) {
		C_136D_0346(&D_5710, 0);
		DRV(0xc)(&D_5710, 0);
		C_00E1_052D();
	}
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5BAE.segment = D_5BBC.segment = D_5BCA.segment = D_5732.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	for(i = 0; i < 50; i++) {
		CheckInput();
		if(D_58F0)
			goto end;
		if(D_5044 == 0x13)
			C_1368_0015();
		else
			Delay(0x50);
	}
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	DRV(0xc)(&D_5732, 0);
	C_00E1_052D();
	CheckInput();
	if(D_58F0)
		goto end;
	/*the bedroom at night: the monitor runs its scripts*/
	D_5FBA = D_5732;
	D_5FBA.left = 0xe5;
	D_5FBA.top = 0x32;
	D_5FBA.right = 0x11d;
	D_5FBA.bottom = 0x56;
	SetScript(2);
	C_09A0_000B(0);
	DRV(3)(&D_5BAE, 0, 0, shapes, 0);
	DRV(3)(&D_5BCA, 0, 0x3f, shapes, 0xd);
	DrawClock(&D_5732, shapes, 0xdd);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5BAE.segment = D_5BCA.segment = D_5732.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	CheckInput();
	if(D_58F0)
		goto end;
	D_6066 = 0;
	/*some frames without music, then until a key; then until a key again*/
	for(i = 0; i < D_6836 / 400; i++) {
		DRV(3)(&D_5BAE, 0, 0, shapes, 0);
		DRV(3)(&D_5BCA, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(D_606E) {
			DRV(3)(&D_5BCA, 0x8f, 0x5b, shapes, 0xe);
			D_606E = 0;
		}
		DrawClock(&D_5732, shapes, 0xdd);
		ShowFrame();
		if(CheckInput())
			break;
	}
	if(D_58F0)
		goto end;
	do {
		DRV(3)(&D_5BAE, 0, 0, shapes, 0);
		DRV(3)(&D_5BCA, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(D_606E) {
			DRV(3)(&D_5BCA, 0x8f, 0x5b, shapes, 0xe);
			D_606E = 0;
		}
		C_09A0_21C5();
		DrawClock(&D_5732, shapes, 0xdd);
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	while(kbhit())
		getch();
	C_09A0_000B(1);
	do {
		DRV(3)(&D_5BAE, 0, 0, shapes, 0);
		DRV(3)(&D_5BCA, 0, 0x3f, shapes, 0xd);
		RunScript();
		if(D_606E) {
			DRV(3)(&D_5BCA, 0x8f, 0x5b, shapes, 0xe);
			D_606E = 0;
		}
		C_09A0_21C5();
		DrawClock(&D_5732, shapes, 0xdd);
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	/*the view scrolls right to the window, 320 pixels: the monitor goes out on the
	 left, the window (shape 0xf) comes in*/
	C_09A0_000B(2);
	speed = ScaleSpeed(2, 10);
	/*(done is set before the last frame: the one with i at its end)*/
	for(i = 0, done = 0; !done && (done = (i == 320), 1); ) {
		if(i >= 0x87) {
			x = (i - 0x87) * 10 / 0x24 + (320 - i) + 0x4d;
			DRV(3)(&D_5BBC, x, 0, shapes, 0xf);
		}
		DRV(3)(&D_5BAE, D_5BAE.left - i, 0, shapes, 0);
		DRV(3)(&D_5BBC, D_5BBC.left + 320 - i, 0, shapes, 1);
		DRV(3)(&D_5BCA, D_5BCA.left - i * 2, 0x3f, shapes, 0xd);
		C_09A0_21C5();
		if(D_5FBA.right > 0 && i < 300)
			RunScript();
		D_5FBA.right -= speed;
		D_5FBA.left -= speed;
		if(D_5FBA.left < 0) {
			D_6066 -= D_5FBA.left;
			D_5FBA.left = 0;
		}
		DrawClock(&D_5732, shapes, 0xdd - i);
		if(D_5044 == 0x13)
			C_1368_0015();
		ShowFrame();
		if(CheckInput())
			i = 320 - speed;
		if(D_58F0)
			goto end;
		/*up to 320, the last step stops on it*/
		i += speed;
		i += (320 - i) * (i >= 320);
	}
	farfree(shapes);
	/*---- part 2: the storm ----*/
	C_0BC1_0079(1);
	switch(D_5044) {
		case 4: shapes = D_6068 = C_13BC_000C("intro2.cga"); break;
		case 7: shapes = D_6068 = C_13BC_000C("intro2.hrc"); break;
		case 9: shapes = D_6068 = C_13BC_000C("intro2.tga"); break;
		case 0xd: shapes = D_6068 = C_13BC_000C("intro2.ega"); break;
		case 0x13: shapes = D_6068 = C_13BC_000C("intro_2.shp"); break;
	}
	while(!CheckInput());
	if(D_58F0)
		goto end;
	D_5BAE.right = 0x101;
	D_5BAE.left = 0x39;
	D_5BAE.top = 0;
	D_5BAE.bottom = 0xb0;
	D_5BBC.top = 0;
	D_5BBC.bottom = 0x2e;
	speed3 = ScaleSpeed(1, 5);
	speed2 = ScaleSpeed(4, 12);
	D_605E = ScaleSpeed(2, 10);
	D_6064 = ScaleSpeed(4, 1);
	/*the storm*/
	InitRain(&D_5732, 150, 0);
	InitClouds(&D_5732, 5, shapes);
	C_09A0_000B(3);
	done = i = 0;
	frames = done;
	flashTime = face = scene = done;
	while(!done) {
		DRV(3)(&D_5732, 0, 0, shapes, 1);
		MoveClouds();
		/*lightning*/
		if(rand() % 6 == 0 && frames >= 60 && flashTime == 0 && started != 0) {
			flashTime = (rand() & 3) + 1;
			flashShape = rand() & 7;
			flashX = RandomX(&D_5BBC);
			flashY = RandomY(&D_5BBC);
		}
		if(flashTime) {
			DRV(3)(&D_5732, rand() % 3 + flashX - 2, rand() % 3 + flashY - 2, shapes, flashShape + 0xb);
		}
		if(D_5044 == 0x13 && started) {
			C_1368_0002();
			/*the sky lit by the lightning*/
			if((flashTime || scene == 3) && (rand() % 3 || scene == 3)) {
				C_1475_0099(0x58, flashColor);
				C_1475_0099(0x5a, flashColor);
				C_1475_0099(0x5c, flashColor);
			} else {
				C_1475_0099(0x58, sky0);
				C_1475_0099(0x5a, sky1);
				C_1475_0099(0x5c, sky2);
			}
		}
		DRV(3)(&D_5732, 0, 0x4c, shapes, 10);
		DRV(3)(&D_5732, 0, 0, shapes, 8);
		prevFace = face;
		while(face == prevFace)
			face = rand() % 5 + 8;
		if(scene == 3 || (scene == 4 && i < 0xa0)) {
			DRV(3)(&D_5732, 0x9e, 0x72, shapes, face + 0xb);
		}
		MoveRain();
		if(scene < 5) {
			if((flashTime && (rand() & 1)) || scene == 3) {
				DRV(3)(&D_5732, D_5BAE.left - i, D_5BAE.top, shapes, 0x1b);
			} else {
				DRV(3)(&D_5732, D_5BAE.left - i, D_5BAE.top, shapes, 0x1a);
			}
		}
		/*0-3: shape 0x1c (3: the face too); 4: it leaves on the left and the
		 two halves 0x18, 0x19 come in from the right; 5: they stay; 6: they part;
		 7: the end*/
		switch(scene) {
			case 1: case 2: case 3:
				DRV(3)(&D_5732, 0, 0, shapes, 0x1c);
			goto music;
			case 0:
				DRV(3)(&D_5732, 0, 0, shapes, 0x1c);
			break;
			case 4:
				DRV(3)(&D_5732, 0 - i, 0, shapes, 0x1c);
				DRV(3)(&D_5732, 320 - i, 0, shapes, 0x18);
				DRV(3)(&D_5732, 320 - i + 0xfd, 0, shapes, 0x19);
				i += speed2;
				if(i > 320) {
					scene = 5;
					i = 0;
				}
			goto music;
			case 5:
				DRV(3)(&D_5732, 0, 0, shapes, 0x18);
				DRV(3)(&D_5732, 0xfd, 0, shapes, 0x19);
			goto music;
			case 6:
				DRV(3)(&D_5732, 0 - i, 0, shapes, 0x18);
				DRV(3)(&D_5732, i + 0xfd, 0, shapes, 0x19);
				i += speed3;
				if(i > 0x4b) {
					C_09A0_000B(8);
					i = 0;
					scene = 7;
				}
			break;
			case 7:
			music:
				C_09A0_21C5();
			break;
		}
		if(!started) {
			started ++;
			if(D_5044 == 0x13) {
				C_1368_0015();
				C_0BC1_03D2(2);
				C_1475_00B8(0x58, sky0);
				C_1475_00B8(0x5a, sky1);
				C_1475_00B8(0x5c, sky2);
				C_1475_00B8(0x75, flashColor);
			}
		}
		ShowFrame();
		/*a key: the next scene*/
		if(CheckInput()) {
			switch(scene) {
				case 6:
					C_09A0_000B(8);
				case 0: case 4:
					scene ++;
					i = 0;
				break;
				case 1:
					C_09A0_000B(4);
				goto next;
				case 2:
					C_09A0_000B(5);
				goto next;
				case 3:
					C_09A0_000B(6);
				case 5:
				next:
					scene ++;
				break;
				default:
					done = 1;
			}
		}
		if(D_58F0)
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
	if(D_58F0)
		goto end;
	farfree(shapes);
	/*---- part 3: the moongate ----*/
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	DRV(0xc)(&D_5732, 0);
	C_0BC1_0079(2);
	switch(D_5044) {
		case 4: shapes = C_13BC_000C("intro3.cga"); break;
		case 7: shapes = C_13BC_000C("intro3.hrc"); break;
		case 9: shapes = C_13BC_000C("intro3.tga"); break;
		case 0xd:
			shapes = C_13BC_000C("intro3.ega");
			DRV(0xc)(&D_5710, 0);
			DRV(0x15)(&D_5710);
			DRV(0xc)(&D_5710, 0);
			DRV(0x15)(&D_5710);
		break;
		case 0x13: shapes = C_13BC_000C("intro_3.shp"); break;
	}
	C_0BC1_03D2(3);
	CheckInput();
	if(D_58F0)
		goto end;
	C_09A0_000B(9);
	DRV(3)(&D_5732, 0, 0, shapes, 0);
	if(D_5044 == 0x13) {
		C_136D_014D(&D_5732, &D_5710);
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5732.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
	CheckInput();
	if(D_58F0)
		goto end;
	C_1482_000F(FlashMoongate);/*the moongate's light*/
	for(i = 0; i < 3; i++) {
		DRV(3)(&D_5732, 0, 0, shapes, 0);
		ShowFrame();
		if(CheckInput())
			break;
	}
	if(D_58F0)
		goto end;
	do {
		DRV(3)(&D_5732, 0, 0, shapes, 0);
		C_09A0_21C5();
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	if(D_5044 == 0xd) {
		D_5710.bottom = 0x46;
		DRV(0xc)(&D_5710, 0);
		DRV(0x15)(&D_5710);
		DRV(0xc)(&D_5710, 0);
		DRV(0x15)(&D_5710);
		D_5710.bottom = 199;
	} else {
		D_5732.bottom = 100;
		DRV(0xc)(&D_5732, 0);
		D_5732.bottom = 199;
	}
	/*clipped to the upper half*/
	D_5BAE = D_5732;
	D_5BAE.bottom = 0x65;
	C_09A0_000B(10);
	speed = ScaleSpeed(4, 10);
	for(i = 199, done = 0; !done && (done = (i == 0x54), 1); ) {
		DRV(3)(&D_5732, 0, 0, shapes, 0);
		DRV(3)(&D_5732, 0xbd, i, shapes, 1);
		DRV(3)(&D_5732, 0x96, 0x64, shapes, 3);
		C_09A0_21C5();
		ShowFrame();
		if(CheckInput())
			i = speed + 0x54;
		if(D_58F0)
			goto end;
		i -= speed;
		i += (0x54 - i) * (i <= 0x54);
	}
	C_09A0_000B(0xb);
	while(!CheckInput());
	speed = ScaleSpeed(2, 8);
	for(i = 0x65, done = 0; !done && (done = (i == 6), 1); ) {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5732, 0xbd, 0x54, shapes, 1);
		DRV(3)(&D_5BAE, 0x7c, i, shapes, 2);
		C_09A0_21C5();
		ShowFrame();
		if(CheckInput())
			i = speed + 6;
		if(D_58F0)
			goto end;
		i -= speed;
		i += (6 - i) * (i <= 6);
	}
	speed = ScaleSpeed(4, 10);
	for(i = 0x54, done = 0; !done && (done = (i == 0xcc), 1); ) {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5732, 0xbd, i, shapes, 1);
		DRV(3)(&D_5BAE, 0x7c, 5, shapes, 2);
		C_09A0_21C5();
		ShowFrame();
		if(CheckInput())
			i = 0xcc - speed;
		i += speed;
		i += (0xcc - i) * (i >= 0xcc);
	}
	while(!CheckInput());
	if(D_58F0)
		goto end;
	speed = ScaleSpeed(2, 8);
	for(i = 0, done = 0; !done && (done = (i == 0x3f), 1); ) {
		DRV(3)(&D_5732, 0, 0 - i, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66 - i, shapes, 4);
		DRV(3)(&D_5BAE, 0x7c, 5 - i, shapes, 2);
		D_5732.top = 199 - i;
		DRV(0xc)(&D_5732, 0);
		D_5732.top = 0;
		DRV(3)(&D_5732, 0x9b, 200 - i * 2, shapes, 6);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(D_58F0)
			goto end;
		i += speed;
		i += (0x3f - i) * (i >= 0x3f);
	}
	C_09A0_000B(0xc);
	do {
		DRV(3)(&D_5732, 0, -0x3f, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x27, shapes, 4);
		DRV(3)(&D_5BAE, 0x7c, -0x3a, shapes, 2);
		D_5732.top = 0x87;
		DRV(0xc)(&D_5732, 0);
		D_5732.top = 0;
		DRV(3)(&D_5732, 0x9b, 0x4a, shapes, 6);
		C_09A0_21C5();
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	DRV(0xc)(&D_5732, 0);
	speed = ScaleSpeed(2, 8);
	for(i = 0x3f, done = 0; !done && (done = (i == 0), 1); ) {
		DRV(3)(&D_5732, 0, 0 - i, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66 - i, shapes, 4);
		DRV(3)(&D_5BAE, 0x7c, 5 - i, shapes, 2);
		D_5732.top = 199 - i;
		DRV(0xc)(&D_5732, 0);
		D_5732.top = 0;
		DRV(3)(&D_5732, 0x9b, 200 - i * 2, shapes, 6);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(D_58F0)
			goto end;
		i -= speed;
		i += (0 - i) * (i <= 0);
	}
	C_09A0_000B(0xd);
	do {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5BAE, 0x7c, 5, shapes, 2);
		C_09A0_21C5();
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xe);
	do {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5BAE, (rand() & 1) + 0x7c, (rand() & 1) + 5, shapes, 2);
		C_09A0_21C5();
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xf);
	if(D_5044 == 0x13) {
		C_1475_00B8(0x19, sky0);
		C_1475_00B8(0x19, flashColor);
		flashColor[0] = flashColor[1] = flashColor[2] = 0;
		C_1475_0099(0x19, flashColor);
	}
	do {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5BAE, (rand() & 1) + 0x7c, (rand() & 3) + 5, shapes, 2);
		C_09A0_21C5();
		ShowFrame();
	} while(!CheckInput());
	if(D_58F0)
		goto end;
	/*D_5BBC: a copy of the rectangle of D_5BAE, at 0,0*/
	D_5BBC.left = D_5BBC.right = 0;
	D_5BBC.bottom = D_5BAE.bottom - D_5BAE.top;
	D_5BBC.right = D_5BAE.right - D_5BAE.left;
	if(D_5044 != 0xd) {
		C_1277_000C(&D_5BBC);
		allocated = 1;
	} else {
		allocated = 0;
	}
	speed = ScaleSpeed(1, 3);
	for(i = 0, done = 0; !done && (done = (i == 0x3b), 1); ) {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5BAE, (rand() & 1) + 0x7c, (rand() & 3) + 5, shapes, 2);
		DRV(3)(&D_5732, -2, 0x12 - i, shapes, i / 3 + 7);
		ShowFrame();
		if(CheckInput())
			done = 1;
		if(D_58F0)
			goto end;
		i += speed;
		i += (0x3b - i) * (i >= 0x3b);
	}
	DRV(3)(&D_5732, 0, 0, shapes, 5);
	DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
	DRV(3)(&D_5BAE, 0x7c, 5, shapes, 2);
	if(D_5044 == 0xd) {
		D_5BD8 = D_5710;
		D_5BD8.segment = 0xa600;
		DRV(0xf)(&D_5710, &D_5BD8);
	} else {
		DRV(0xf)(&D_5BAE, &D_5BBC);
	}
	CheckInput();
	if(D_58F0)
		goto end;
	DRV(3)(&D_5732, -2, -0x2a, shapes, 0x1a);
	ShowFrame();
	CheckInput();
	if(D_58F0)
		goto end;
	/*the dissolve*/
	switch(D_5044) {
		case 4: case 7:
			C_12EA_000A(&D_5710, &D_5BAE, 0);
			speed = ScaleSpeed(200, 300);
			C_13F2_037A(&D_5BBC, &D_5BAE, speed);
		goto show;
		case 9:
			C_12EA_000A(&D_5710, &D_5BAE, 0);
			speed = ScaleSpeed(200, 300);
			C_13F2_0504(&D_5BBC, &D_5BAE, speed);
		goto show;
		case 0xd:
			D_5710.segment ^= 0x200;
			speed = ScaleSpeed(200, 300);
			C_13F2_01D5(&D_5BD8, &D_5710, speed);
			D_5710.segment ^= 0x200;
		break;
		case 0x13:
			C_12EA_000A(&D_5710, &D_5BAE, 0);
			C_13F2_000C(&D_5BBC, &D_5BAE, 0x1e);
		show:
			C_12EA_000A(&D_5732, &D_5BAE, 0);
		break;
	}
	CheckInput();
	if(D_58F0)
		goto end;
	speed = ScaleSpeed(2, 8);
	for(i = 5, done = 0; !done && (done = (i == 0x65), 1); ) {
		DRV(3)(&D_5732, 0, 0, shapes, 5);
		DRV(3)(&D_5732, 0x5e, 0x66, shapes, 4);
		DRV(3)(&D_5BAE, 0x7c, i, shapes, 2);
		ShowFrame();
		if(CheckInput())
			i = 0x65 - speed;
		if(D_58F0)
			goto end;
		i += speed;
		i += (0x65 - i) * (i >= 0x65);
	}
	if(D_5044 == 0x13)
		C_1475_0099(0x19, sky0);
	DRV(0xc)(&D_5732, 0);
	while(!CheckInput()) {
		DRV(3)(&D_5732, 0, 0, shapes, 0);
		DRV(3)(&D_5732, 0x96, 0x64, shapes, 3);
		ShowFrame();
	}
end:/*cleanup*/
	if(allocated) {
		C_1277_00D8(&D_5BBC);
		allocated = 0;
	}
	farfree(shapes);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	DRV(0xc)(&D_5732, 0);
quit:
	C_0BC1_0079(2);
	if(D_5044 == 0x13)
		C_1482_001F();
	C_1482_009C();
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	C_00E1_04F9();
	C_00E1_178A(0);
}

/*the frame drawn in the back buffer goes to the screen (EGA: page flip)*/
/*C_06E9_2AF6*/ShowFrame() {
	if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5BAE.segment = D_5BBC.segment = D_5BCA.segment = D_5732.segment = D_5710.segment;
	} else {
		DRV(0xf)(&D_5732, &D_5710);
	}
}

/*wait n times D_6836 / 400 loops*/
/*C_06E9_2B34*/Delay(int n) {
	int i;
	int dummy;/*(never set)*/

	while(n--)
		for(i = 0; i < D_6836 / 400; i++)
			dummy += n;
}
