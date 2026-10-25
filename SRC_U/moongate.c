/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0C17: the introduction's night scene (the twinkling stars, the hands,
 * the moongate rising, the stones)
 */
#include "u.h"
#include <alloc.h>
#include <stdlib.h>

/*a drawing surface of the graphics driver (14 bytes)*/

extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface D_5710;/*the screen*/
extern struct tSurface D_5732;/*the page the introduction draws on*/
extern int D_575A;
extern int D_58F0;/*set when the player escapes the introduction*/
extern void far *D_5ADC;/*the shapes of the introduction (intro.*)*/
extern struct tSurface D_5BAE;/*twinkle: the surface the stars are drawn on*/
extern struct tSurface D_6072;/*an off-screen buffer*/
extern int D_6096;/*twinkle: palette index (a global: the twinkle runs from the timer)*/
extern char D_6098[];/*twinkle: a palette entry*/
extern char D_609E[];/*twinkle: a palette entry*/
extern int D_60A4;/*twinkle: star index*/
extern int D_60A6;/*twinkle: colour of the star's pixel*/
extern int D_60A8;/*twinkle: colour of a lit star*/
extern char far *D_60AA;/*starpos.dat*/
extern int D_60AE;/*twinkle: first star of the group that twinkles*/
extern int D_60B0;/*number of stars*/
extern int D_60B2;/*twinkle: random number (a shift register)*/
extern int D_60BC[];/*colour of each star*/
extern int D_62BC[];/*x of each star*/
extern int D_64BC[];/*y of each star*/
extern int D_66BC[];/*the hands: x0,y0, x1,y1*/
extern int D_66C4[];/*where the moving shapes go*/
extern int D_66D4[];/*the moving shapes: x,y each*/

extern C_00E1_04F9();
extern C_00E1_052D();
extern C_00E1_0561();
extern C_00E1_178A(int);
extern C_06E9_033A();/*nonzero when a key (or the mouse) was pressed*/
extern C_06E9_0AF0(int, int);/*a random number between the two*/
extern C_06E9_2AF6();/*waits for the next frame*/
extern C_06E9_2B34(int);/*waits n ticks*/
extern C_09A0_000B(int);/*writes text n*/
extern C_09A0_21C5();
extern C_0BC1_03D2(int);
extern C_1277_000C(struct tSurface *);/*gives a surface an off-screen buffer*/
extern C_1277_00D8(struct tSurface *);/*frees it*/
extern C_1368_0015();
extern C_136D_0004(int *, int *, int);/*moves n coordinates one step towards their targets: 0 when all are there*/
extern C_136D_014D(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern C_136D_0346(struct tSurface *, int);/*MCGA: palette fade out*/
extern void far *C_13BC_000C(char *);/*loads a file in memory*/
extern C_13F2_000C(struct tSurface *, struct tSurface *, int);/*MCGA: dissolve*/
extern C_13F2_01D5(struct tSurface *, struct tSurface *, int);/*EGA: dissolve*/
extern C_13F2_037A(struct tSurface *, struct tSurface *, int);/*CGA, Hercules: dissolve*/
extern C_13F2_0504(struct tSurface *, struct tSurface *, int);/*Tandy: dissolve*/
extern C_1475_0099(int, char *);/*MCGA: sets a palette entry*/
extern C_1475_00B8(int, char *);/*MCGA: gets a palette entry*/
extern C_1482_000F(void (far *)());/*installs a timer routine*/
extern C_1482_001F();
extern C_1482_0055();
extern C_1482_009C();
extern C_1482_00BE();

/*40C4*/int D_40C4[4] = {-8, 0x4d, 0xb6, 0x4f};/*where the hands come to rest: x0,y0, x1,y1*/
/*40CC*/int D_40CC = 0;/*twinkle: toggles every tick (MCGA palette cycling every other tick)*/
/*40CE*/unsigned D_40CE = 0;/*twinkle: tick counter*/

KeyPressed();

/*a driver entry that returns something (D_4A34.p.ofs selects it)*/
#define DRVF(type) (*(type (far *)())D_4A34.f)

/*draws shape n of the introduction*/
#define SHAPE(x, y, n) DRV(3)(&D_5732, x, y, D_5ADC, n)

/*the pixel of star i in MCGA video memory*/
#define STAR_PIXEL(i) *(char far *)((long)(D_64BC[i] * 320) + D_62BC[i] + 0xa0000000L)

/*shows the page that was drawn*/
#define SHOW_PAGE() \
	if(D_5044 == 0x13) { \
		C_136D_014D(&D_5732, &D_5710); \
	} else if(D_5044 == 0xd) { \
		DRV(0x15)(&D_5710); \
		D_5732.segment = D_5710.segment; \
	} else { \
		DRV(0xf)(&D_5732, &D_5710); \
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
/*C_0C17_0000*/DrawHands(int x0, int x1, int y) {
	if(D_5044 == 0xd)
		D_5732 = D_5710;
	SHAPE(x0 - 8, y + 0x4d, 3);
	SHAPE(x1 + 0xb6, y + 0x4f, 4);
}

/*the timer routine of the night sky: every few ticks a group of stars (from D_60AE,
 1 to 5 of them) is lit, then put out; on MCGA the palette entries 0xe8-0xef cycle*/
/*C_0C17_0072*/void far Twinkle() {
	if(D_5044 == 0x13) {
		/*cycle the palette entries 0xe8-0xef, every other tick*/
		if(D_40CC == 0) {
			C_1368_0015();
			C_1475_00B8(0xef, D_609E);
			for(D_6096 = 0xee; D_6096 >= 0xe8; D_6096--) {
				C_1475_00B8(D_6096, D_6098);
				C_1475_0099(D_6096 + 1, D_6098);
			}
			C_1475_0099(0xe8, D_609E);
		}
		D_40CC = !D_40CC;
		if(!(D_40CE & 3)) {
			/*put out the stars that are lit*/
			for(D_60A4 = D_60AE; D_60A4 <= D_60B2 % 5 + D_60AE; D_60A4++)
				if(STAR_PIXEL(D_60A4) == 0xf)
					STAR_PIXEL(D_60A4) = D_60BC[D_60A4];
		}
		D_40CE++;
		if(D_40CE % 20 == 0) {
			/*light another group of stars*/
			D_60AE = (D_60AE + 7) % D_60B0;
			if(D_60B2 & 1) {
				D_60B2 >>= 1;
				D_60B2 ^= 0xb8;
			} else {
				D_60B2 >>= 1;
			}
			for(D_60A4 = D_60AE; D_60A4 <= D_60B2 % 5 + D_60AE; D_60A4++)
				if(STAR_PIXEL(D_60A4) == D_60BC[D_60A4])
					STAR_PIXEL(D_60A4) = 0xf;
		}
	} else {
		D_60A8 = 0xf;
		if(D_5044 < 9)
			D_60A8 = 3;
		if(D_5044 == 0xd) {
			D_4A34.p.ofs = 0x1e;
			if(DRVF(long)(0))
				return;
		}
		if(!(D_40CE & 3)) {
			for(D_60A4 = D_60AE; D_60A4 <= D_60B2 % 5 + D_60AE; D_60A4++) {
				D_4A34.p.ofs = 0x1b;
				D_60A6 = DRVF(int)(&D_5BAE, D_62BC[D_60A4], D_64BC[D_60A4]);
				if(D_60A6 == D_60A8) {
					DRV(0x18)(&D_5BAE, D_62BC[D_60A4], D_64BC[D_60A4], D_60BC[D_60A4]);
				}
			}
		}
		if(++D_40CE % 8 == 0) {
			D_60AE = (D_60AE + 7) % D_60B0;
			if(D_60B2 & 1) {
				D_60B2 >>= 1;
				D_60B2 ^= 0xb8;
			} else {
				D_60B2 >>= 1;
			}
			for(D_60A4 = D_60AE; D_60A4 <= D_60B2 % 5 + D_60AE; D_60A4++) {
				D_4A34.p.ofs = 0x1b;
				D_60A6 = DRVF(int)(&D_5BAE, D_62BC[D_60A4], D_64BC[D_60A4]);
				if(D_60BC[D_60A4] == D_60A6 && D_60A6 != 0) {
					DRV(0x18)(&D_5BAE, D_62BC[D_60A4], D_64BC[D_60A4], D_60A8);
				}
			}
		}
	}
}

/*waits n ticks, or until a key is pressed*/
/*C_0C17_03CA*/Delay(int n) {
	int i;

	for(i = n; i; i--) {
		C_06E9_2B34(1);
		if(KeyPressed())
			break;
	}
}

/*draws a small glowing spot of 9 pixels at 0x57-0x59,0x55-0x58*/
/*C_0C17_03EF*/DrawGlow() {
	int c;

	if(D_5044 == 0x13)
		c = 0xad;
	else if(D_5044 == 0xd || D_5044 == 9)
		c = 6;
	else
		c = 1;
	DRV(0x18)(&D_5732, 0x58, 0x55, c);
	DRV(0x18)(&D_5732, 0x57, 0x56, c);
	DRV(0x18)(&D_5732, 0x58, 0x56, c);
	DRV(0x18)(&D_5732, 0x59, 0x56, c);
	DRV(0x18)(&D_5732, 0x57, 0x57, c);
	DRV(0x18)(&D_5732, 0x58, 0x57, c);
	DRV(0x18)(&D_5732, 0x59, 0x57, c);
	DRV(0x18)(&D_5732, 0x57, 0x58, c);
	DRV(0x18)(&D_5732, 0x58, 0x58, c);
}

/*waits until no key is pressed*/
/*C_0C17_04FE*/FlushKeys() {
	while(KeyPressed());
}

/*the introduction: the night sky with its twinkling stars, then the scene with the hands and
 the stones; texts 0xc8-0xdb (C_09A0_000B) are told along it. A key goes on to the
 next part, Escape (D_58F0) leaves it.
 (the locals' order and which ones are declared after which matter for the stack
 layout and the si/di choice: found by trying)*/
/*C_0C17_0508*/Intro() {
	int frames;/*a random duration*/
	int speed;/*a random speed of the current animation*/
	int done;
	int a;
	int b;
	int tmp;
	int ofs;/*horizontal offset of shapes 8, 0xb/0xa; also a dissolve parameter*/
	int jitter;
	int left;/*the hands' trembling offsets and their directions*/
	int right;
	int dleft;
	int dright;
	int i;

	D_58F0 = 0;
	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd) {
		C_1277_000C(&D_6072);
	} else {
		D_6072.segment = 0xa600;
		D_5732 = D_5710;
	}
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	DRV(0xc)(&D_5732, 0);
	C_1482_00BE();
	frames = C_06E9_0AF0(0xf, 0x28);
	switch(D_5044) {
		case 4: D_5ADC = C_13BC_000C("intro.cga"); break;
		case 7: D_5ADC = C_13BC_000C("intro.hrc"); break;
		case 9: D_5ADC = C_13BC_000C("intro.tga"); break;
		case 0xd: D_5ADC = C_13BC_000C("intro.ega"); break;
		case 0x13: D_5ADC = C_13BC_000C("intro.shp"); break;
	}
	/*the stars: 200 x, 200 y, 200 colours (16 bits each)*/
	D_60AA = C_13BC_000C("starpos.dat");
	for(i = 0; i < 0x190; i += 2) {
		D_62BC[i / 2] = (D_60AA[i] & 0xff) + ((D_60AA[i + 1] & 0xff) << 8);
		D_64BC[i / 2] = (D_60AA[i + 0x190] & 0xff) + ((D_60AA[i + 0x191] & 0xff) << 8);
		D_60BC[i / 2] = (D_60AA[i + 0x320] & 0xff) + ((D_60AA[i + 0x321] & 0xff) << 8);
	}
	C_0BC1_03D2(7);
	C_00E1_0561();
	C_00E1_178A(3);
	SHAPE(0, 0, 6);
	SHAPE(0, 0x70, 5);
	SHOW_PAGE();
	/*shuffle the stars*/
	D_60B0 = 0xc1;
	for(i = 0; i < 0x200; i++) {
		a = rand() % D_60B0;
		b = rand() % D_60B0;
		tmp = D_62BC[a];
		D_62BC[a] = D_62BC[b];
		D_62BC[b] = tmp;
		tmp = D_64BC[a];
		D_64BC[a] = D_64BC[b];
		D_64BC[b] = tmp;
		tmp = D_60BC[a];
		D_60BC[a] = D_60BC[b];
		D_60BC[b] = tmp;
	}
	D_60B2 = rand() % 0xff + 1;
	D_60AE = 0;
	D_5BAE = D_5710;
	/*light the first group of stars*/
	if(D_5044 == 0x13) {
		for(i = D_60AE; i <= D_60B2 % 5 + D_60AE; i++)
			STAR_PIXEL(i) = 0xf;
	} else {
		if(D_5044 == 0xd)
			D_5710.segment ^= 0x200;
		for(i = 0; i < 200; i++) {
			D_4A34.p.ofs = 0x1b;
			D_60BC[i] = DRVF(int)(&D_5710, D_62BC[i], D_64BC[i]);
		}
		for(i = D_60AE; i <= D_60B2 % 5 + D_60AE; i++) {
			DRV(0x18)(&D_5710, D_62BC[i], D_64BC[i], 0xf);
		}
		if(D_5044 == 0xd)
			D_5710.segment ^= 0x200;
	}
	C_1482_0055();
	C_1482_000F(Twinkle);
	C_09A0_000B(0xc8);
	Delay(10);
	if(D_5044 == 0xd)
		D_5710.segment ^= 0x200;
	C_09A0_21C5();
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	}
	FlushKeys();
	while(!C_06E9_033A());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xc9);
	C_09A0_21C5();
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_5732, &D_5710);
	} else {
		D_5710.segment ^= 0x200;
	}
	FlushKeys();
	while(!C_06E9_033A());
	if(D_58F0)
		goto end;
	/*the hands come in from the sides*/
	D_66BC[0] = -0x54;
	D_66BC[1] = 0x6d;
	D_66BC[2] = 0x140;
	D_66BC[3] = 0xc8;
	if(D_5044 == 0xd)
		D_5732 = D_5710;
	speed = C_06E9_0AF0(1, 4);
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(C_136D_0004(D_66BC, D_40C4, 4));
			done = 1;
		}
		if(D_58F0)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !C_136D_0004(D_66BC, D_40C4, 4);
		SHAPE(0, 0, 6);
		SHAPE(D_66BC[0], D_66BC[1], 3);
		SHAPE(D_66BC[2], D_66BC[3], 4);
		SHAPE(0, 0x70, 5);
		C_06E9_2AF6();
	} while(!done);
	C_09A0_000B(0xca);
	C_09A0_21C5();
	C_06E9_2AF6();
	SHAPE(0, 0, 6);
	DrawHands(0, 0, 0);
	SHAPE(0, 0x70, 5);
	C_09A0_000B(0xcb);
	C_09A0_21C5();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_06E9_2AF6();
	SHAPE(0, 0, 6);
	DrawHands(0, 0, 0);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	C_09A0_000B(0xcc);
	C_09A0_21C5();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_06E9_2AF6();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	/*the moongate rises*/
	C_09A0_000B(0xcd);
	speed = C_06E9_0AF0(2, 5);
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
		C_06E9_2AF6();
		if(ofs) {
			ofs -= speed;
			if(ofs < 0)
				ofs = 0;
		}
		if(KeyPressed())
			i = speed + 0x1e;
		if(D_58F0)
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
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(3);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xce);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(3);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xcf);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 4);
		SHAPE(0x90, 0x1e, 8);
		SHAPE(0xde, 0x2c, 0xb);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(3);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xd0);
	speed = C_06E9_0AF0(2, 5);
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
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(3);
		if(KeyPressed())
			i = speed + 0x3a;
		if(D_58F0)
			goto end;
		i -= speed;
		i += (0x3a - i) * (i <= 0x3a);
	}
	while(!KeyPressed());
	if(D_58F0)
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
	C_06E9_2AF6();
	switch(D_5044) {
		case 4:
		case 7:
			ofs = C_06E9_0AF0(0xfa, 0x12c);
			DRV(0xc)(&D_5732, 0);
			C_13F2_037A(&D_5732, &D_5710, ofs);
			break;
		case 9:
			ofs = C_06E9_0AF0(0x12c, 0x1f4);
			DRV(0xc)(&D_5732, 0);
			C_13F2_0504(&D_5732, &D_5710, ofs);
			break;
		case 0xd:
			D_5732.segment = 0xa600;
			DRV(0xc)(&D_5732, 0);
			D_5710.segment ^= 0x200;
			ofs = C_06E9_0AF0(0xc8, 0x12c);
			C_13F2_01D5(&D_5732, &D_5710, ofs);
			DRV(0x15)(&D_5710);
			D_5732 = D_5710;
			DRV(0xc)(&D_5732, 0);
			break;
		case 0x13:
			DRV(0xc)(&D_5732, 0);
			C_13F2_000C(&D_5732, &D_5710, 0);
			break;
	}
	C_09A0_000B(0xd1);
	C_09A0_21C5();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	C_00E1_052D();
	C_09A0_000B(0xd2);
	C_09A0_21C5();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	C_00E1_052D();
	C_09A0_000B(0xd3);
	C_09A0_21C5();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	C_00E1_052D();
	C_09A0_000B(0xd4);
	C_09A0_21C5();
	SHOW_PAGE();
	FlushKeys();
	while(!KeyPressed());
	if(D_58F0)
		goto end;
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
		D_5732 = D_5710;
	}
	C_00E1_052D();
	SHAPE(0, 0, 6);
	SHAPE(0x78, 0x3a, 7);
	DrawHands(0, 0, 4);
	SHAPE(0x90, 0x1e, 8);
	SHAPE(0xde, 0x2c, 0xb);
	SHAPE(0x31, 0x44, 9);
	SHAPE(0, 0x70, 5);
	SHAPE(0xd2, 0x84, 0xc);
	DrawGlow();
	switch(D_5044) {
		case 4:
		case 7:
			ofs = C_06E9_0AF0(0xc8, 0x12c);
			C_13F2_037A(&D_5732, &D_5710, ofs);
			break;
		case 9:
			ofs = C_06E9_0AF0(0x12c, 0x1f4);
			C_13F2_0504(&D_5732, &D_5710, ofs);
			break;
		case 0xd:
			D_5710.segment ^= 0x200;
			ofs = C_06E9_0AF0(0xc8, 0x12c);
			C_13F2_01D5(&D_5732, &D_5710, ofs);
			D_5710.segment ^= 0x200;
			break;
		case 0x13:
			C_13F2_000C(&D_5732, &D_5710, 0);
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
		C_06E9_2AF6();
		TREMBLE(3);
		if(KeyPressed())
			break;
		if(D_58F0)
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
		C_06E9_2AF6();
	C_09A0_000B(0xd5);
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
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(3);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	speed = C_06E9_0AF0(4, 0xa);
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
		C_06E9_2AF6();
		TREMBLE(4);
		if(KeyPressed())
			i = 0x8d - speed;
		if(D_58F0)
			goto end;
		i += speed;
		i += (0x8d - i) * (i >= 0x8d);
	}
	D_66D4[0] = -0x20;
	D_66D4[1] = 0x7a;
	D_66D4[2] = 0x44;
	D_66D4[3] = 0x7a;
	D_66D4[4] = 0xa8;
	D_66D4[5] = 0xca;
	D_66C4[0] = 0x63;
	D_66C4[1] = 0x28;
	D_66C4[2] = 0xb7;
	D_66C4[3] = 0x35;
	D_66C4[4] = 0xf0;
	D_66C4[5] = 0x3e;
	speed = C_06E9_0AF0(3, 9);
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(C_136D_0004(D_66D4, D_66C4, 6));
			done = 1;
		}
		if(D_58F0)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !C_136D_0004(D_66D4, D_66C4, 6);
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(D_66D4[0], D_66D4[1], 0);
		SHAPE(D_66D4[2], D_66D4[3], 2);
		SHAPE(D_66D4[4], D_66D4[5], 1);
		SHAPE(0x31, 0x44, 9);
		SHAPE(0, 0x70, 5);
		SHAPE(0xd2, 0x84, 0xc);
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!done);
	C_09A0_000B(0xd6);
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
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	D_66D4[0] = 0x63;
	D_66D4[1] = 0x28;
	D_66D4[2] = 0xb7;
	D_66D4[3] = 0x35;
	D_66D4[4] = 0xf0;
	D_66D4[5] = 0x3e;
	D_66D4[6] = 0x31;
	D_66D4[7] = 0x44;
	D_66C4[0] = 0x12;
	D_66C4[1] = 0x8e;
	D_66C4[2] = 0x12;
	D_66C4[3] = 0x8e;
	D_66C4[4] = 0x76;
	D_66C4[5] = 0x7a;
	D_66C4[6] = 0x31;
	D_66C4[7] = 0x8a;
	done = 0;
	FlushKeys();
	do {
		if(KeyPressed()) {
			while(C_136D_0004(D_66D4, D_66C4, 8));
			done = 1;
		}
		if(D_58F0)
			goto end;
		for(i = 0; i < speed && !done; i++)
			done = !C_136D_0004(D_66D4, D_66C4, 8);
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(D_66D4[0], D_66D4[1], 0);
		SHAPE(D_66D4[2], D_66D4[3], 2);
		SHAPE(D_66D4[4], D_66D4[5], 1);
		SHAPE(D_66D4[6], D_66D4[7], 9);
		SHAPE(0, 0x70, 5);
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!done);
	C_09A0_000B(0xd7);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xd8);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		SHAPE(0x78, 0x3a, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xd9);
	speed = C_06E9_0AF0(1, 3);
	FlushKeys();
	i = 0x3a;
	done = 0;
	while(!done && (done = i == 0x86, 1)) {
		SHAPE(0, 0, 6);
		SHAPE(0x78, i, 7);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
		if(KeyPressed())
			i = 0x86 - speed;
		if(D_58F0)
			goto end;
		i += speed;
		i += (0x86 - i) * (i >= 0x86);
	}
	C_09A0_000B(0xda);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	C_09A0_000B(0xdb);
	FlushKeys();
	do {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_09A0_21C5();
		C_06E9_2AF6();
		TREMBLE(4);
	} while(!KeyPressed());
	if(D_58F0)
		goto end;
	FlushKeys();
	for(i = 0; i < frames / 2; i++) {
		SHAPE(0, 0, 6);
		DrawHands(left, right, 6 - rand() % 4);
		SHAPE(0, 0x70, 5);
		C_06E9_2AF6();
		TREMBLE(4);
		if(KeyPressed())
			break;
		if(D_58F0)
			break;
	}
end:
	C_1482_009C();
	C_1482_001F();
	farfree(D_5ADC);
	farfree(D_60AA);
	if(D_5044 == 0x13)
		C_136D_0346(&D_5710, 0);
	DRV(0xc)(&D_5710, 0);
	DRV(0xc)(&D_5732, 0);
	C_00E1_04F9();
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	D_575A = D_58F0 = 0;
	C_00E1_178A(0);
}

/*whether a key was pressed; also updates the surface the twinkling stars are drawn
 on (D_5BAE: the screen, the visible EGA page)*/
/*C_0C17_2FBB*/KeyPressed() {
	int ret;

	D_5BAE = D_5710;
	if(D_5044 == 0xd)
		D_5BAE.segment ^= 0x200;
	ret = C_06E9_033A();
	return ret;
}
