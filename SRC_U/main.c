/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 00E1: configuration, keyboard input, palettes, the main menu,
 * main(), music
 *
 * This module has inline assembly (InitHercules, and a few lines in main):
 * Turbo C compiled it through the assembler (tcc -B), which is why its
 * forward jumps are "jmp short + nop", and why main, as a function with
 * asm statements, has neither jump optimization nor automatic register
 * variables (hence its explicit "register"s). Build it with
 * tcc -mm -O -G -S, then the assembler (MASM 3.0 /Mx gives the same bytes).
 */
#include "u.h"
#include <alloc.h>
#include <conio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*a drawing surface (the same as GAME.EXE's mouse.h)*/


extern int D_4A14[][3];/*the 5 main menu highlight colors (r, g, b)*/
extern int D_4A40[][3];/*VGA palette (r, g, b)*/
extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern char D_5046, D_5047;/*mouse buttons*/
extern int D_5048, D_504A;/*mouse x, y*/
extern void far *D_504E;/*mouse pointer shapes*/
extern int D_5052;/*mouse pointer shape*/
extern struct tSurface *D_5054;/*the surface the mouse pointer is drawn on*/
extern void far *D_5056;/*mouse background save area*/
extern int D_505F[][3];/*saved VGA palette*/
extern struct tSurface D_565F;/*text window*/
extern struct tSurface *D_566D;/*console: surface*/
extern int D_566F, D_5671;/*console: text cursor x, y*/
extern void far *D_5673;/*console: font*/
extern char D_5677, D_5678;/*console: text foreground, background color*/
extern char *D_5679;/*console: buffer*/
extern char D_567D;
extern char far *D_5692;/*song (or midi.dat) data*/
extern char D_5696[];/*the command line arguments, concatenated*/
extern struct tSurface D_56E6;
extern struct tSurface D_5710;/*the screen*/
extern int D_571E;/*parse position in midi.dat*/
extern int D_5720;/*music card i/o port*/
extern struct tSurface D_5722;
extern struct tSurface D_5732;/*the drawing surface (EGA: the other page)*/
extern unsigned long D_5740;/*free far memory*/
extern int D_5746;/*music card: 0 none, 1 AdLib, 2 CMS, 3 Tandy, 4 Roland, 5 Covox, 6 Innovation*/
extern struct tSurface D_5748;/*saved screen surface (EGA)*/
extern int D_575A;/*the main menu screen is drawn*/
extern int D_575C[];/*scanline offsets*/
extern int D_58EC;/*mouse on*/
extern int D_58EE;/*graphic adapter: 0 VGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules*/
extern int D_58F0;
extern void far *D_58F2;/*blocks.xxx*/
extern void far *D_58F6;/*mouse pointer shapes (cursors.xxx)*/
extern union tMus D_58FA;/*the music driver*/
extern int far drvDrawPointer[];/*graphic driver entries for the OSI library: (offset, segment) pairs; declared far in the original (its accesses load its segment)*/

extern void far *LoadZFile(char *);/*load a driver file*/
extern void far *LZW_load(char *);/*load a (compressed) file*/
extern SetDosHandlers();
extern FindDrives();/*FindDrives*/
extern LoadPalette(int);/*LoadPalette*/
extern Intro();
extern TransferCharacter();
extern CreateCharacter();
extern Credits();
extern StartGame();
extern ScaleSpeed(int, int);
extern Introduction();
extern Delay(int);
extern SURF_alloc(struct tSurface *);
extern SURF_free(struct tSurface *);
extern MOUSE_INIT(void *);/*mouse init*/
extern MOUSE_POINTER(void far *, int);/*mouse pointer shape*/
extern MOUSE_NOINIT(void *);/*mouse init (keyboard)*/
extern TXT_select(void *);/*console init*/
extern MOUSE_SERVE();/*mouse serve*/
extern VID_setMode(int);
extern MOUSE_SHOW();/*mouse show*/
extern MOUSE_HIDE();/*mouse hide*/
extern MOUSE_IN_RECT(struct tSurface *);/*mouse in the rectangle*/
extern TXT_putc(int);/*print a character*/
extern VID_inRetrace();
extern VID_waitRetrace();/*wait for the vertical retrace*/
extern FADE_in(struct tSurface *, struct tSurface *);
extern DIS_dissolve(struct tSurface *, struct tSurface *, int);
extern DIS_dissolveEGA(struct tSurface *, struct tSurface *, int);
extern DIS_dissolveCGA(struct tSurface *, struct tSurface *, int);
extern DIS_dissolveTDY(struct tSurface *, struct tSurface *, int);
extern PAL_set(int, int *);/*set a VGA palette entry*/
extern PAL_get(int, int *);/*read a VGA palette entry*/

InitHercules();
LoadMusicDriver(int, int);
PlayMusic(int);

/*0194*/int D_0194 = 0;/*the introduction has been seen (main menu choice 0)*/
/*Hercules 720x348 graphics: the CRTC registers (low byte: index, high byte: value)*/
/*0196*/int D_0196[9] = {0x3500, 0x2d01, 0x2e02, 0x0703, 0x5b04, 0x0205, 0x5706, 0x5707, 0x0309};
/*01A8*/int D_01A8 = 0;/*song playing (1-based in D_01C0), 0 none, -2: song 2 loaded but not started*/

/*parse an hexadecimal music i/o port (e.g. "388") into D_5720;
returns the pointer to the first non hex digit*/
/*C_00E1_0006*/char *ParseHexPort(char *str) {
	int ch;

	for(D_5720 = 0; ((ch = toupper(*str)) >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'); str++) {
		D_5720 <<= 4;
		if(ch >= '0' && ch <= '9')
			D_5720 += ch - '0';
		else
			D_5720 += ch - 'A' + 10;
	}
	return str;
}

/*parse a configuration string (CONFIG.U6 or a command line argument):
V/E/T/C/H graphic adapter, M/K mouse or keyboard, N/A/X/Y/R/Z/I music
card (A, X, Z, I may be followed by an hexadecimal i/o port)*/
/*C_00E1_0064*/ParseConfig(char *str) {
	while(*str) {
		switch(toupper(*str++)) {
			case 'V': D_58EE = 0; break;
			case 'E': D_58EE = 1; break;
			case 'T': D_58EE = 2; break;
			case 'C': D_58EE = 3; break;
			case 'H': D_58EE = 4; break;
			case 'M': D_58EC = 1; break;
			case 'K': D_58EC = 0; break;
			case 'N': D_5746 = 0; break;
			case 'A': D_5746 = 1; D_5720 = 0x388;
				if(*str == '3' || *str == '2')
					str = ParseHexPort(str);
			break;
			case 'X': D_5746 = 2; D_5720 = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'Y': D_5746 = 3; break;
			case 'R': D_5746 = 4; break;
			case 'Z': D_5746 = 5; D_5720 = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'I': D_5746 = 6; D_5720 = 0x280;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
		}
	}
}

/*wait for a key, with a blinking '_' cursor; the arrows left/right
give 8/0x15, other extended keys 0*/
/*C_00E1_016C*/char GetKey() {
	int x, y;
	int color;
	int key;
	int blink, done;

	x = D_566F;
	y = D_5671;
	color = D_5677;
	blink = 0;
	done = 0;
	key = 0;
	while(!done) {
		VID_waitRetrace();
		MOUSE_SERVE();
		if(++blink == 20) {
			blink = 0;
			gotoxy(x, y);
			TXT_putc('_');
			D_5677 = (D_5677 == color)?D_5678:color;
		}
		if(kbhit()) {
			switch(key = getch()) {
				case 0:
					switch(key = getch()) {
						case 0x4b: key = 8; done = 1; break;
						case 0x4d: key = 0x15; done = 1; break;
						default: key = 0;
					}
				break;
				case 0xd: case 8: case 0x1b: case ' ':
					done = 1;
				break;
			}
			if(isalnum(key) || ispunct(key))
				done = 1;
		}
	}
	/*erase the cursor*/
	gotoxy(x, y);
	D_5677 = D_5678;
	TXT_putc('_');
	D_5677 = color;
	gotoxy(x, y);
	return key;
}

/*input a string of at most <max> characters into <dest>; returns 0
when cancelled (escape, or return on an empty string)*/
/*C_00E1_028A*/InputString(char *dest, int max) {
	int *xs, *ys;
	int color;
	int key;
	int done;
	int ret;
	int len;
	char *buf;

	len = 0;
	buf = calloc(max + 1, 1);
	xs = calloc(max + 1, 2);
	ys = calloc(max + 1, 2);
	done = 0;
	while(!done) {
		if(max == 1 && len)
			key = 0xd;
		else
			key = GetKey();
		if(key == 0x1b || (key == 0xd && !len)) {
			done = 1;
			ret = 0;
			continue;
		}
		if(key == 8 && len) {
			/*backspace: erase the last character*/
			len--;
			gotoxy(xs[len], ys[len]);
			color = D_5677;
			D_5677 = D_5678;
			TXT_putc(buf[len]);
			D_5677 = color;
			gotoxy(xs[len], ys[len]);
		}
		if(key == 0xd) {
			buf[len] = 0;
			strcpy(dest, buf);
			done = 1;
			ret = 1;
		} else if((isalnum(key) || key == ' ' || key == '-' || key == '\'' || key == '.' || key == ',') && len < max) {
			buf[len] = key;
			xs[len] = D_566F;
			ys[len] = D_5671;
			if(max != 1)
				TXT_putc(key);
			len++;
		}
	}
	free(ys);
	free(xs);
	free(buf);
	return ret;
}

/*choose one of the options: by its key in <keys>, or by a click in its
rectangle <rects>; returns its index*/
/*C_00E1_041B*/char MenuSelect(struct tSurface *rects, char *keys) {
	int i;
	int ch;
	int pressed;
	int sel;
	int over;

	sel = -1;
	over = -1;
	pressed = D_5046 || D_5047;
	do {
		if(!D_5046 && !D_5047)
			pressed = 0;
		if(kbhit()) {
			ch = toupper(getch());
			for(i = 0; keys[i]; i++)
				if(keys[i] == ch)
					return i;
		}
		MOUSE_SERVE();
		over = -1;
		for(i = 0; keys[i]; i++) {
			if(MOUSE_IN_RECT(&rects[i])) {
				over = i;
				break;
			}
		}
		/*a new click on an option*/
		if((D_5046 || D_5047) && !pressed && over != -1)
			sel = over;
	} while(sel == -1);
	return sel;
}

/*set the VGA palette from D_4A40*/
/*C_00E1_04F9*/SetPalette() {
	int i;

	if(D_5044 == 0x13) {
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_set(i, D_4A40[i]);
		}
	}
}

/*set the VGA palette from the saved one D_505F*/
/*C_00E1_052D*/RestorePalette() {
	int i;

	if(D_5044 == 0x13) {
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_set(i, D_505F[i]);
		}
	}
}

/*save the VGA palette into D_505F*/
/*C_00E1_0561*/SavePalette() {
	int i;

	if(D_5044 == 0x13) {
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_get(i, D_505F[i]);
		}
	}
}

/*the main menu: draws it (the first time) and returns the choice:
0 introduction, 1 create a character, 2 transfer a character,
3 acknowledgements, 4 journey onward, 5 quit*/
/*C_00E1_0595*/MainMenu() {
	void far *titles;
	void far *menu;
	struct tSurface rects[5];
	int colors[5] = {0xe, 0x21, 0x22, 0x23, 0x24};
	int effect;
	int done;
	int lastX, lastY;
	int over;
	struct tSurface dst, src;
	int i;
	int sel;

	switch(D_5044) {
		case 4: menu = LZW_load("mainmenu.cga"); break;
		case 7: menu = LZW_load("mainmenu.hrc"); break;
		case 9: menu = LZW_load("mainmenu.tga"); break;
		case 0xd: menu = LZW_load("mainmenu.ega"); D_5732 = D_5710; break;
		case 0x13: menu = LZW_load("mainmenu.shp"); break;
	}
	switch(D_5044) {
		case 4: titles = LZW_load("titles.cga"); break;
		case 7: titles = LZW_load("titles.hrc"); break;
		case 9: titles = LZW_load("titles.tga"); break;
		case 0xd: titles = LZW_load("titles.ega"); break;
		case 0x13: titles = LZW_load("titles.shp"); break;
	}
	if(D_01A8 != 1)
		PlayMusic(1);
	if(!D_575A) {
		SetPalette();
		/*the title*/
		src = D_5710;
		dst = D_5732;
		dst.right = 0x13f; dst.left = dst.top = 0; dst.bottom = 0x3f;
		src.right = 0x13f; src.left = src.top = 0; src.bottom = 0x3f;
		DRV(3)(&dst, 0x13, 0, titles, 0);
		if(D_5044 == 0x13) {
			FADE_in(&dst, &src);
		} else if(D_5044 != 0xd) {
			DRV(0xf)(&dst, &src);
		} else {
			DRV(0x15)(&D_5710);
			DRV(0xf)(&D_5732, &D_5710);
			D_5732 = D_5710;
		}
		/*"The False Prophet"*/
		src = D_5710;
		dst = D_5732;
		dst.right = 0x108; dst.left = 0x3b; dst.top = 0x2f; dst.bottom = 0x4b;
		src.right = 0x108; src.left = 0x3b; src.top = 0x2f; src.bottom = 0x4b;
		DRV(3)(&dst, dst.left, dst.top, titles, 1);
		switch(D_5044) {
			case 4: case 7:
				effect = ScaleSpeed(0x46, 0x78);
				DIS_dissolveCGA(&dst, &src, effect);
			break;
			case 9:
				effect = ScaleSpeed(0x46, 0x78);
				DIS_dissolveTDY(&dst, &src, effect);
			break;
			case 0xd:
				src.segment ^= 0x200;
				effect = ScaleSpeed(0x32, 0x64);
				DIS_dissolveEGA(&dst, &src, effect);
				DRV(0x15)(&D_5710);
				D_5732 = D_5710;
				Delay(10);
			break;
			case 0x13:
				DIS_dissolve(&dst, &src, 10);
			break;
		}
		/*the menu*/
		src = D_5710;
		dst = D_5732;
		dst.right = 0x12b; dst.left = 0x31; dst.top = 0x53; dst.bottom = 0xc4;
		src.right = 0x12b; src.left = 0x31; src.top = 0x53; src.bottom = 0xc4;
		DRV(3)(&dst, dst.left, dst.top, menu, 0);
		if(D_5044 == 0x13) {
			FADE_in(&dst, &src);
		} else if(D_5044 == 0xd) {
			DRV(0x15)(&D_5710);
		} else {
			DRV(0xf)(&dst, &src);
		}
		if(D_5044 == 0x13)
			for(i = 0; i < 5; i++)
				PAL_get(colors[i], D_4A14[i]);
		D_575A = 1;
	}
	D_58F0 = 0;
	if(D_5044 == 0xd) {
		D_5054 = &D_5732;
		D_5732.segment = D_5710.segment ^ 0x200;
	}
	MOUSE_SHOW();
	/*the options' rectangles*/
	rects[0].right = 0x106; rects[0].left = 0x3a; rects[0].top = 0x57; rects[0].bottom = 0x6b;
	rects[1].right = 0x106; rects[1].left = 0x3a; rects[1].top = 0x6c; rects[1].bottom = 0x7f;
	rects[2].right = 0x106; rects[2].left = 0x3a; rects[2].top = 0x80; rects[2].bottom = 0x94;
	rects[3].right = 0x106; rects[3].left = 0x3a; rects[3].top = 0x95; rects[3].bottom = 0xa8;
	rects[4].right = 0x106; rects[4].left = 0x3a; rects[4].top = 0xa9; rects[4].bottom = 0xc1;
	sel = 0;
	over = sel;
	done = sel;
	lastX = D_5048 + 1;
	lastY = D_504A;
	while(!done) {
		MOUSE_SERVE();
		if(D_5044 == 0x13) {
			/*highlight the selected option*/
			VID_inRetrace();
			for(i = 0; i < 5; i++)
				PAL_set(colors[i], D_4A14[abs(sel - i)]);
		}
		if(D_5048 != lastX || D_504A != lastY) {
			/*the mouse moved*/
			over = 0;
			lastX = D_5048;
			lastY = D_504A;
			for(i = 0; i < 5; i++) {
				if(MOUSE_IN_RECT(&rects[i])) {
					sel = i;
					over = 1;
				}
			}
		}
		if(kbhit()) {
			switch(toupper(getch())) {
				case 'I':
					sel = 0; done = 1;
					if(D_5044 == 0x13)
						for(i = 0; i < 5; i++)
							PAL_set(colors[i], D_4A14[abs(sel - i)]);
				break;
				case 'C':
					sel = 1; done = 1;
					if(D_5044 == 0x13)
						for(i = 0; i < 5; i++)
							PAL_set(colors[i], D_4A14[abs(sel - i)]);
				break;
				case 'T':
					sel = 2; done = 1;
					if(D_5044 == 0x13)
						for(i = 0; i < 5; i++)
							PAL_set(colors[i], D_4A14[abs(sel - i)]);
				break;
				case 'A':
					sel = 3; done = 1;
					if(D_5044 == 0x13)
						for(i = 0; i < 5; i++)
							PAL_set(colors[i], D_4A14[abs(sel - i)]);
				break;
				case 'J':
					sel = 4; done = 1;
					if(D_5044 == 0x13)
						for(i = 0; i < 5; i++)
							PAL_set(colors[i], D_4A14[abs(sel - i)]);
				break;
				case 0x11:/*ctrl-Q: quit*/
					sel = 5;
				case 0xd:
					done = 1;
				break;
				/*the songs*/
				case '1': PlayMusic(1); break;
				case '2':
					PlayMusic(2);
					if(D_5746 && D_01A8 == -2) {
						MUS(6)(D_5692);
						D_01A8 = 2;
					}
				break;
				case '3': PlayMusic(3); break;
				case '4': PlayMusic(4); break;
				case '5': PlayMusic(5); break;
				case '6': PlayMusic(6); break;
				case '7': PlayMusic(7); break;
				case '8': PlayMusic(8); break;
				case '9': PlayMusic(9); break;
				case '0': PlayMusic(10); break;
				case '-': PlayMusic(11); break;
				case '=': PlayMusic(12); break;
				case 0:
					switch(getch()) {
						case 0x48: case 0x4b:/*up, left*/
							if(sel)
								sel--;
						break;
						case 0x4d: case 0x50:/*right, down*/
							if(sel != 4)
								sel++;
						break;
					}
				break;
			}
		}
		if((D_5046 || D_5047) && over)
			done = 1;
	}
	farfree(titles);
	farfree(menu);
	MOUSE_HIDE();
	if(sel != 3)
		PlayMusic(0);
	return sel;
}

/*wait for a key or a mouse click*/
/*C_00E1_0ED4*/WaitKeyOrClick() {
	int key;

	key = 0;
	MOUSE_SHOW();
	while(!key) {
		MOUSE_SERVE();
		if(kbhit())
			key = getch();
		if(D_5046 || D_5047)
			key = D_5046 || D_5047;
	}
	MOUSE_HIDE();
	return key;
}

/*the program: configuration (CONFIG.U6, then the command line), graphic
and music drivers, screen surfaces, console, mouse, then the main menu
until "journey onward" or quit*/
/*C_00E1_0F23*/main(register int argc, char **argv) {
	int i;
	int fd;
	char buf[80];
	char cfg[10];
	register int done;

	/*the command line, for the game*/
	D_5696[0] = 0;
	for(i = 1; i < argc; i++) {
		sprintf(buf, "%s%s", D_5696, argv[i]);
		strcpy(D_5696, buf);
	}
	SetDosHandlers();
	FindDrives();
	srand(time(0));
	/*the configuration: CONFIG.U6 then the command line*/
	D_58F0 = 0;
	D_58EC = 1;
	D_58EE = 0;
	D_5746 = 0;
	fd = OSI_open("config.u6");
	if(fd >= 0) {
		OSI_read(fd, 0L, 6L, cfg);
		cfg[6] = 0;
		ParseConfig(cfg);
		OSI_close(fd);
	}
	for(i = 1; i < argc; i++)
		ParseConfig(argv[i]);
	/*the graphic driver*/
	switch(D_58EE) {
		case 0: D_5044 = 0x13; D_4A34.f = (void (far *)())LoadZFile("mcgadrv.bin"); break;
		case 1: D_5044 = 0xd; D_4A34.f = (void (far *)())LoadZFile("egadrv.bin"); break;
		case 2: D_5044 = 9; D_4A34.f = (void (far *)())LoadZFile("tgadrv.bin"); break;
		case 3: D_5044 = 4; D_4A34.f = (void (far *)())LoadZFile("cgadrv.bin"); break;
		case 4: D_5044 = 7; D_4A34.f = (void (far *)())LoadZFile("cgadrv.bin"); break;
	}
	/*some of its entries, for the OSI library (i = the driver's segment)*/
	asm mov bx,offset DGROUP:D_4A34
	asm mov ax,[bx+2]
	asm mov i,ax
	drvDrawPointer[0] = 3; drvDrawPointer[1] = i;
	drvDrawPointer[2] = 6; drvDrawPointer[3] = i;
	drvDrawPointer[4] = 9; drvDrawPointer[5] = i;
	drvDrawPointer[6] = 0x12; drvDrawPointer[7] = i;
	drvDrawPointer[8] = 0xc; drvDrawPointer[9] = i;
	switch(D_5044) {
		case 4: case 9: D_5710.segment = 0xb800; break;
		case 7: D_5710.segment = 0xb000; InitHercules(); break;
		case 0xd: D_5710.segment = 0xa200; break;
		case 0x13: D_5710.segment = 0xa000; break;
	}
	LoadMusicDriver(D_5746, D_5720);
	if(D_5044 != 7) {
		DRV(0)(D_575C);
	}
	D_5710.pScanlines = D_575C;
	D_5710.right = 0x13f;
	D_5710.left = 0;
	D_5710.top = 0;
	D_5710.bottom = 0xc7;
	D_565F = D_5722 = D_56E6 = D_5732 = D_5710;
	if(D_5044 == 0xd) {
		D_5748 = D_5710;
		D_5748.right = 0x9f;
		SURF_alloc(&D_5748);
		D_5748.right = 0x13f;
		D_5732 = D_5710;
	} else {
		SURF_alloc(&D_5732);
	}
	/*the console*/
	D_565F.right = 300;
	D_565F.left = 0x12;
	D_565F.top = 0x50;
	D_565F.bottom = 0xc5;
	D_566D = &D_5722;
	D_5673 = LZW_load("u6.set");
	TXT_select(&D_566D);
	D_5677 = 0xf;
	D_5678 = 0;
	D_5679 = malloc(0x400);
	D_567D = 3;
	/*the mouse*/
	switch(D_5044) {
		case 4: D_58F6 = LZW_load("cursors.cga"); break;
		case 7: D_58F6 = LZW_load("cursors.hrc"); break;
		case 9: D_58F6 = LZW_load("cursors.tga"); break;
		case 0xd: D_58F6 = LZW_load("cursors.ega"); break;
		case 0x13: D_58F6 = LZW_load("intro.ptr"); break;
	}
	switch(D_5044) {
		case 4: D_58F2 = LZW_load("blocks.cga"); break;
		case 7: D_58F2 = LZW_load("blocks.hrc"); break;
		case 9: D_58F2 = LZW_load("blocks.tga"); break;
		case 0xd: D_58F2 = LZW_load("blocks.ega"); break;
		case 0x13: D_58F2 = LZW_load("blocks.shp"); break;
	}
	D_5054 = &D_5710;
	D_5056 = farmalloc(0x400L);
	D_5048 = D_504A = D_5052 = 0;
	D_504E = D_58F6;
	if(D_58EC == 0)
		MOUSE_NOINIT(&D_5046);
	else
		MOUSE_INIT(&D_5046);
	MOUSE_POINTER(D_58F6, 0);
	MOUSE_HIDE();
	done = 0;
	D_575A = 0;
	if(D_5044 == 0x13) {
		LoadPalette(0);
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_get(i, D_4A40[i]);
		}
	}
	D_5740 = farcoreleft();
	Introduction();
	while(!done) {
		switch(MainMenu()) {
			case 0:/*introduction*/
				Intro();
				D_0194 = 1;
			break;
			case 1:/*create a character*/
				if(CreateCharacter())
					break;
				else {
					if(D_5044 == 0xd)
						D_5732 = D_5710;
					if(!D_0194)
						Intro();
					StartGame();
				}
			break;
			case 2:/*transfer a character*/
				if(TransferCharacter()) {
					if(D_5044 == 0xd)
						D_5732 = D_5710;
					if(!D_0194)
						Intro();
					StartGame();
				}
			break;
			case 3:/*acknowledgements*/
				Credits();
			break;
			case 4:/*journey onward*/
				StartGame();
			break;
			default:
				done = 1;
		}
	}
	PlayMusic(0);
	if(D_5044 == 0xd)
		D_5732 = D_5748;
	SURF_free(&D_5732);
	VID_setMode(3);
}

/*Hercules graphics: 720x348, 4 interleaved banks of 90 bytes per line;
the 200 lines are drawn on 300 (x1.5)*/
/*C_00E1_15A8*/InitHercules() {
	asm mov dx,3bfh
	asm mov al,1
	asm out dx,al
	asm mov dx,3b8h
	asm xor al,al
	asm out dx,al
	asm sub dl,4
	asm mov cx,9
	asm mov si,offset D_0196
crtc:
	asm lodsw
	asm out dx,ax
	asm loop crtc
	asm add dl,4
	asm mov al,0ah
	asm out dx,al
	/*clear the screen*/
	asm mov ax,0b000h
	asm mov es,ax
	asm mov cx,3fffh
	asm xor ax,ax
	asm xor di,di
	asm rep stosw
	/*the scanline offsets*/
	asm xor si,si
	asm xor dx,dx
	asm xor di,di
line:
	asm mov ax,di
	asm add ax,0fh
	asm xor bx,bx
	asm shr ax,1
	asm rcr bx,1
	asm shr ax,1
	asm rcr bx,1
	asm shr bx,1
	asm mov ah,5ah
	asm mul ah
	asm add bx,ax
	asm shl si,1
	asm add bx,5
	asm mov D_575C[si],bx
	asm sar si,1
	asm inc si
	asm inc di
	asm inc dl
	asm cmp dl,2
	asm jnz next
	asm xor dl,dl
	asm inc di
next:
	asm cmp si,0c8h
	asm jnz line
}

/*the music drivers (index 1..6 = D_5746)*/
/*01B4*/char *D_01B4[] = {
	"U6ADLIB.DRV",
	"U6CMS.DRV",
	"U6TMUS.DRV",
	"U6ROLAND.DRV",
	"U6COVOX.DRV",
	"U6INNOVA.DRV"
};

/*load the driver of music card <card> and init it with the i/o <port>*/
/*C_00E1_1614*/LoadMusicDriver(int card, int port) {
	if(card) {
		D_58FA.f = (void (far pascal *)())LoadZFile(D_01B4[card - 1]);
		MUS(0)(port);
	}
}

/*midi.dat parser: returns the next decimal number, or -1 when the
closing ')' comes first*/
/*C_00E1_1644*/MidiNumber() {
	int ch, n;

	while(1) {
		ch = D_5692[D_571E];
		if(ch == ')')
			return -1;
		if(isdigit(ch))
			break;
		D_571E++;
	}
	n = 0;
	while(1) {
		ch = D_5692[D_571E];
		if(!isdigit(ch))
			break;
		n = n * 10;
		n += ch - '0';
		D_571E++;
	}
	return n;
}

/*set up the (Roland) driver's channels for song <song> (0-based): midi.dat
holds a "( a b a b ...)" group per song; without it all 16 channels get
(2, 0)*/
/*C_00E1_169D*/MidiChannels(int song) {
	int fd, ch;
	int parens, channel, val, prog;

	fd = OSI_open("midi.dat");
	D_5692 = farmalloc(0x401L);
	if(fd >= 0) {
		OSI_read(fd, 0L, 0x400L, D_5692);
		parens = 0;
		for(D_571E = 0; D_571E < 0x400; D_571E++) {
			if(parens > song) {
				for(channel = 0; (val = MidiNumber()) >= 0; channel++) {
					prog = MidiNumber();
					MUS(0xc)(channel, val, prog);
				}
				break;
			}
			if(D_5692[D_571E] == '(')
				parens++;
		}
		OSI_close(fd);
	} else {
		for(ch = 0; ch < 16; ch++) {
			MUS(0xc)(ch, 2, 0);
		}
	}
	farfree(D_5692);
}

/*the songs (index 1..12 for PlayMusic)*/
/*01C0*/char *D_01C0[] = {
	"ultima.m",
	"bootup.m",
	"intro.m",
	"create.m",
	"forest.m",
	"hornpipe.m",
	"engage.m",
	"stones.m",
	"dungeon.m",
	"brit.m",
	"gargoyle.m",
	"end.m"
};

/*play song <song> (1-based in D_01C0), 0 stops the music; song 2 is only
loaded (D_01A8 = -2), the main menu starts it*/
/*C_00E1_178A*/PlayMusic(int song) {
	if(D_5746 == 0)
		return;
	if(song == D_01A8)
		return;
	if(D_01A8) {
		MUS(9)();
		D_01A8 = 0;
		farfree(D_5692);
	}
	if(song && song != D_01A8) {
		if(D_5746 == 4)
			MidiChannels(song - 1);
		D_5692 = LZW_load(D_01C0[song - 1]);
		if(song != 2) {
			MUS(6)(D_5692);
		} else {
			song = -2;
		}
		D_01A8 = song;
	}
}
