/*
 * Ultima VI - The False Prophet: END.EXE
 * segment 00A9: configuration, the texts of the ending, the stars' setup, palettes,
 * main() (the whole ending: the lenses, the kings, the Codex), music
 *
 * GAME.EXE runs END.EXE with four arguments: the configuration (as in CONFIG.U6) and
 * the years, months and days the game took. Much of it is a copy of U.EXE's main.c
 * (SRC_U/main.c): ParseHexPort, ParseConfig, the palettes, ScaleSpeed, Delay and the
 * music. Unlike U.EXE's main.c it has no inline assembly (the driver's segment and the
 * Hercules set-up are in the next module).
 */
#include "end.h"
#include <alloc.h>
#include <conio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern union tDrv gfxDrv;/*1770 the graphics driver*/
extern void far *endShapes;/*1774 the pictures of the ending (end.xxx)*/
/*parts of the back buffer the kings and the lenses are drawn into (clipping rectangles)*/
extern struct tSurface leftHalf;/*1778 x 0..A0: Lord British's side*/
extern struct tSurface rightHalf;/*1786 x A0..: King Draxinusom's side*/
extern struct tSurface leftLens;/*1794 x 0..A0, y 0..7D: Lord British's lens rising*/
extern struct tSurface rightLens;/*17A2 x A0.., y 0..7D: the Gargoyle's lens*/
/*the three parts of the screen around the Codex, copied from the back buffer each frame
  (scrXxx on the screen, backXxx the same rectangle of the back buffer)*/
extern struct tSurface scrLower;/*17B0 y 51..*/
extern struct tSurface backLower;/*17BE*/
extern struct tSurface scrLeft;/*17CC x 0..69, y 0..50*/
extern struct tSurface backLeft;/*17DA*/
extern struct tSurface scrRight;/*17E8 x D5.. (EGA D8..), y 0..50*/
extern struct tSurface backRight;/*17F6*/
extern struct tSurface textSurface;/*1804 where the text is printed (off screen; EGA: page A600)*/
extern int16 codexStage;/*1812 the Codex: 0 closed, 1 shown, 2 the lenses' light on it, 3 open*/
extern struct tMouse mouseInfo;/*1818*/
extern int16 palette[][3];/*1837 the VGA palette (r, g, b)*/
extern struct tText console;/*1E37 the console's text window*/
extern char far *songData;/*1E48 the song (or midi.dat) loaded*/
extern int16 songPos;/*1E9A parse position in midi.dat*/
extern int16 textShape;/*1E9C the picture behind the text (end.xxx shape)*/
extern struct tSurface scrCodex;/*1E9E the Codex (x 6A..DA, y 0..5D) on the screen*/
extern int16 musicPort;/*1EAC music card i/o port*/
extern struct tSurface backCodex;/*1EAE the Codex in the back buffer*/
extern int16 textShapeX, textShapeY;/*1EBC, 1EBE where the picture behind the text goes*/
extern struct tSurface backBuffer;/*1EC0 the back buffer (EGA: the other page)*/
extern int16 musicCard;/*1ECE music card: 0 none, 1 AdLib, 2 CMS, 3 Tandy, 4 Roland, 5 Covox, 6 Innovation*/
extern struct tSurface egaSaved;/*1ED0 EGA: the back buffer allocated*/
extern struct tSurface textBack;/*1EDE where the text goes in the back buffer*/
extern int16 mouseOn;/*20FC mouse on*/
extern int16 adapter;/*20FE graphic adapter: 0 VGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules*/
extern union tMus musDrv;/*2100 the music driver*/
extern uint32 loopsPerTick;/*21BE (the OSI library's) speed of the computer: TIM_calibrate's loops in a tick*/
extern int16 far drvDrawPointer[];/*1328 (the OSI library's) graphic driver entries: (offset, segment) pairs*/

extern void far *LoadZFile(char *);/*load a (compressed) driver*/
extern void far *LZW_load(char *);/*load a (compressed) file*/
extern int16 C_02AE_00BA();/*the graphics driver's segment*/
extern C_02AE_00D0();/*Hercules: 720x348 graphics, the scanline offsets*/
extern void far C_02AE_0001();/*the timer tick: the stars twinkle, the colours cycle*/
extern SURF_alloc(struct tSurface *);
extern SURF_free(struct tSurface *);
extern MOUSE_INIT(struct tMouse *);
extern MOUSE_NOINIT(struct tMouse *);
extern MOUSE_SERVE();
extern TXT_select(struct tText *);
extern TXT_sappend(char far *, ...);/*append to the text window's buffer (printf)*/
extern TXT_puts(char far *);/*print a string in the text window*/
extern VID_setMode(int16);
extern VID_waitRetrace();
extern PAL_set(int16, int16 *);
extern PAL_get(int16, int16 *);
extern PAL_load(char *);/*load a palette file (.lbm) into the DAC*/
extern FADE_in(struct tSurface *, struct tSurface *);
extern FADE_out(struct tSurface *, int16);
extern DIS_dissolve(struct tSurface *, struct tSurface *, int16);
extern DIS_dissolveEGA(struct tSurface *, struct tSurface *, int16);
extern DIS_dissolveCGA(struct tSurface *, struct tSurface *, int16);
extern DIS_dissolveTDY(struct tSurface *, struct tSurface *, int16);
extern TIM_install();
extern TIM_remove();
extern TIM_calibrate();
extern TIM_setCallback(void (far *)());
extern TIM_resetCallback();

Delay(int16);
int16 ScaleSpeed(int16, int16);
LoadMusicDriver(int16, int16);
PlayMusic(int16);

/*how long the game took, in words ("two years", "one month", ...)*/
/*0194*/char yearsText[64] = "";
/*01D4*/char monthsText[64] = "";
/*0214*/char daysText[64] = "";
/*0254*/int16 moving = 0;/*something moves in this frame (the lenses): no waiting for a key*/
/*0256*/int16 songPlaying = 0;/*song playing (1-based in songFiles), 0 none*/
/*0258*/char numberWords[30][13] = {
	"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
	"ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
	"eighteen", "nineteen", "twenty", "twenty-one", "twenty-two", "twenty-three",
	"twenty-four", "twenty-five", "twenty-six", "twenty-seven", "twenty-eight", "twenty-nine"
};

/*parse an hexadecimal music i/o port (e.g. "388") into musicPort;
returns the pointer to the first non hex digit*/
/*C_00A9_0004*/char *ParseHexPort(char *str) {
	int16 ch;

	for(musicPort = 0; ((ch = toupper(*str)) >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'); str++) {
		musicPort <<= 4;
		if(ch >= '0' && ch <= '9')
			musicPort += ch - '0';
		else
			musicPort += ch - 'A' + 10;
	}
	return str;
}

/*parse a configuration string (CONFIG.U6 or the first argument):
V/E/T/C/H graphic adapter, M/K mouse or keyboard, N/A/X/Y/R/Z/I music
card (A, X, Z, I may be followed by an hexadecimal i/o port)*/
/*C_00A9_0062*/ParseConfig(char *str) {
	while(*str) {
		switch(toupper(*str++)) {
			case 'V': adapter = 0; break;
			case 'E': adapter = 1; break;
			case 'T': adapter = 2; break;
			case 'C': adapter = 3; break;
			case 'H': adapter = 4; break;
			case 'M': mouseOn = 1; break;
			case 'K': mouseOn = 0; break;
			case 'N': musicCard = 0; break;
			case 'A': musicCard = 1; musicPort = 0x388;
				if(*str == '3' || *str == '2')
					str = ParseHexPort(str);
			break;
			case 'X': musicCard = 2; musicPort = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'Y': musicCard = 3; break;
			case 'R': musicCard = 4; break;
			case 'Z': musicCard = 5; musicPort = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'I': musicCard = 6; musicPort = 0x280;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
		}
	}
}

/*the text of scene <page> (0..15): its picture behind (textShape at textShapeX,
textShapeY) and where it goes (textBack); prints it into textSurface, which DrawText
copies to the back buffer. The short ones are centred (justify 2)*/
/*C_00A9_016A*/ShowText(int16 page) {
	int16 color;
	struct tText t;

	t = console;
	while(kbhit())
		getch();
	textBack = backBuffer;
	t.surface = &textSurface;
	textSurface.top = textSurface.left = t.curX = t.curY = 0;
	t.justify = 3;
	switch(videoMode) {
		case 4: case 7: color = 3; t.fgColor = 0; break;
		case 9: case 0xd: color = 0xe; t.fgColor = 0; break;
		case 0x13: color = 0x31; t.fgColor = 0x3e; break;
	}
	t.bgColor = color;
	TXT_select(&t);
	t.bufPtr = t.buffer;
	switch(page) {
		case 0:
			textShapeX = 0x0;
			textShapeY = 0xa0;
			textBack.left = 0x8;
			textBack.top = 0xae;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("A glowing portal springs from the floor!");
			t.justify = 2;
			break;
		case 1:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x92;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x27;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("From its crimson depths Lord British emerges, trailed by ");
			TXT_sappend("the mage Nystul. Anguish and disbelief prevail on the ");
			TXT_sappend("royal seer's face, but Lord British directs his stony ");
			TXT_sappend("gaze at you and speaks as if to a wayward child.");
			break;
		case 2:
			textShapeX = 0x1;
			textShapeY = 0x97;
			textBack.left = 0x8;
			textBack.top = 0xa3;
			textShape = 0xc;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x17;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("\"Thou didst have just cause to burgle our ");
			TXT_sappend("Codex, I trust,%c His Majesty says. \"But for Virtue's sake...", 0x7f);
			break;
		case 3:
			textShapeX = 0x0;
			textShapeY = 0xa0;
			textBack.left = 0x8;
			textBack.top = 0xae;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("\"WHAT HAST THOU DONE WITH IT?%c", 0x7f);
			t.justify = 2;
			break;
		case 4:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x8e;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x2f;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("You pick up the concave lens and pass it to the King. ");
			TXT_sappend("\"Was the book ever truly ours, Your Majesty? Was it ");
			TXT_sappend("written for Britannia alone? Thou dost no longer ");
			TXT_sappend("hold the Codex, but is its wisdom indeed lost? Look ");
			TXT_sappend("into the Vortex, and let the Codex answer for itself!%c", 0x7f);
			break;
		case 5:
			textShapeX = 0x1;
			textShapeY = 0x97;
			textBack.left = 0x8;
			textBack.top = 0xa3;
			textShape = 0xc;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x17;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("As Lord British holds the glass before the ");
			TXT_sappend("wall, the Codex of Ultimate Wisdom wavers into ");
			TXT_sappend("view against a myriad of swimming stars!");
			break;
		case 6:
			textShapeX = 0x0;
			textShapeY = 0xa0;
			textBack.left = 0x8;
			textBack.top = 0xae;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("Yet the book remains closed.");
			t.justify = 2;
			break;
		case 7:
			textShapeX = 0x0;
			textShapeY = 0x98;
			textBack.left = 0x8;
			textBack.top = 0xa2;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("And waves of heat shimmer in the air, heralding the ");
			TXT_sappend("birth of another red gate!");
			break;
		case 8:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x8e;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x2f;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("King Draxinusom of the Gargoyles strides forward, ");
			TXT_sappend("flanked by a small army of wingless attendants. Like ");
			TXT_sappend("Lord British, he seems to suppress his rage only through ");
			TXT_sappend("a heroic effort of will. His scaly ");
			TXT_sappend("hand grasps your shoulder, and your Amulet of Submission grows ");
			TXT_sappend("very warm.");
			break;
		case 9:
			textShapeX = 0x0;
			textShapeY = 0xa0;
			textBack.left = 0x8;
			textBack.top = 0xae;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("\"Thy time hath come, Thief,%c he says.", 0x7f);
			t.justify = 2;
			break;
		case 10:
			textShapeX = 0x0;
			textShapeY = 0x98;
			textBack.left = 0x8;
			textBack.top = 0xa2;
			textShape = 0xb;
			textSurface.right = 0x12f;
			textSurface.bottom = 0xf;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("Quickly you reach down to seize the convex lens...");
			t.justify = 2;
			break;
		case 11:
			textShapeX = 0x1;
			textShapeY = 0x97;
			textBack.left = 0x8;
			textBack.top = 0xa3;
			textShape = 0xc;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x17;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("...and you press it into the hand of the towering ");
			TXT_sappend("Gargoyle king, meeting his sunken eyes. ");
			TXT_sappend("\"Join my Lord in his search for peace, I beg thee.%c", 0x7f);
			break;
		case 12:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x92;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x27;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("At your urging, King Draxinusom reluctantly raises his ");
			TXT_sappend("lens to catch the light. As Lord British holds up his own lens, ");
			TXT_sappend("every eye in the room, human and Gargoyle alike, fixes ");
			TXT_sappend("upon the image of the Codex which shines upon the wall.");
			break;
		case 13:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x92;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x27;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("The ancient book opens. Both kings gaze upon its pages in ");
			TXT_sappend("spellbound silence, as the eloquence of Ultimate Wisdom is revealed ");
			TXT_sappend("in the tongues of each lord's domain. You, too, can read the answers ");
			TXT_sappend("the Codex gives...");
			break;
		case 14:
			textShapeX = 0x1;
			textShapeY = 0x85;
			textBack.left = 0x8;
			textBack.top = 0x92;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x27;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("...and when its wisdom is gleaned, when Lord British ");
			TXT_sappend("and King Draxinusom turn to each other as friends, ");
			TXT_sappend("hating no longer, fearing no more, you know that your ");
			TXT_sappend("mission in Britannia has ended at last.");
			break;
		case 15:
			textShapeX = 1;
			textShapeY = 0x44;
			textBack.left = 8;
			textBack.top = 0x4d;
			textShape = 0xa;
			textSurface.right = 0x12f;
			textSurface.bottom = 0x2f;
			DRV(0xc)(&textSurface, color);
			TXT_sappend("CONGRATULATIONS\n");
			TXT_sappend("Thou hast completed Ultima VI: The False Prophet, ");
			TXT_sappend("in ");
			/*"two years, three months,\nand one day"*/
			if(yearsText[0]) {
				TXT_sappend("%s", yearsText);
				if(monthsText[0] || daysText[0])
					TXT_sappend(", ", yearsText);
			}
			if(monthsText[0]) {
				TXT_sappend("%s", monthsText);
				if(daysText[0])
					TXT_sappend(",\n", monthsText);
			}
			if((yearsText[0] || monthsText[0]) && daysText[0])
				TXT_sappend("and ");
			if(daysText[0] || (!yearsText[0] && !monthsText[0]))
				TXT_sappend("%s", daysText);
			TXT_sappend(".\nReport thy feat unto Lord British at Origin Systems!");
			t.justify = 2;
			break;
	}
	TXT_puts(t.buffer);
	TXT_select(&console);
}

/*draw the picture behind the text and copy the text onto it, in the back buffer*/
/*C_00A9_099A*/DrawText() {
	if(videoMode == 0xd)
		backBuffer.segment = textBack.segment = screen.segment;
	DRV(3)(&backBuffer, textShapeX, textShapeY, endShapes, textShape);
	DRV(0xf)(&textSurface, &textBack);
}

/*0 nothing, 1 a mouse click (once the buttons are up), else the key pressed*/
/*C_00A9_09E5*/int16 CheckInput() {
	MOUSE_SERVE();
	if(mouseInfo.buttonL || mouseInfo.buttonR) {
		while(mouseInfo.buttonL || mouseInfo.buttonR)
			MOUSE_SERVE();
		return 1;
	}
	if(!kbhit())
		return 0;
	return getch();
}

/*wait n delays, or until a key or a click*/
/*C_00A9_0A25*/WaitInput(int16 n) {
	while(n) {
		Delay(1);
		if(CheckInput())
			break;
		n--;
	}
}

/*wait n times loopsPerTick / 400 loops*/
/*C_00A9_0A49*/Delay(int16 n) {
	int16 i;
	int16 dummy;/*(never set)*/

	while(n--)
		for(i = 0; i < loopsPerTick / 400; i++)
			dummy += n;
}

/*the 64 stars around the Codex: where, their colours, the random generators' seeds*/
/*C_00A9_0A90*/InitStars() {
	int16 i;

	for(i = 0; i < 0x40; i++) {
		starX[i] = rand() % 0x71;
		starY[i] = rand() % 0x5e;
		if(videoMode == 0x13)
			starColor[i] = rand() % 16 + 0xe0;
		else if(videoMode == 0xd || videoMode == 9)
			starColor[i] = rand() % 15 + 1;
		else
			starColor[i] = rand() % 3 + 1;
	}
	do
		starSeedX = rand();
	while(starSeedX == 0);
	do
		starSeedY = rand();
	while(starSeedY == 0);
}

/*copy the back buffer around the Codex to the screen: the left and right of it and
below it (the Codex itself the stars and the dissolves draw)*/
/*C_00A9_0B26*/ShowAround() {
	if(videoMode == 0xd) {
		backLeft.segment = backRight.segment = backLower.segment = screen.segment;
		scrLeft.segment = scrRight.segment = scrLower.segment = screen.segment ^ 0x200;
	}
	DRV(0xf)(&backRight, &scrRight);
	DRV(0xf)(&backLeft, &scrLeft);
	DRV(0xf)(&backLower, &scrLower);
}

/*set the VGA palette from palette*/
/*C_00A9_0B88*/SetPalette() {
	int16 i;

	if(videoMode == 0x13) {
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_set(i, palette[i]);
		}
	}
}

/*save the VGA palette into palette*/
/*C_00A9_0BBC*/SavePalette() {
	int16 i;

	if(videoMode == 0x13) {
		for(i = 0; i < 0x100; i++) {
			if(!(i & 0xf))
				VID_waitRetrace();
			PAL_get(i, palette[i]);
		}
	}
}

/*the ending. argv[1] the configuration, argv[2..4] the years, months and days the
game took*/
/*C_00A9_0BF0*/main(int16 argc, char **argv) {
	int16 speed;/*how fast the lenses move*/
	int16 done;
	int16 fd;
	int16 months, days, years;
	char cfg[8];
	register int16 effect;/*the dissolves' speed*/
	register int16 y;/*the lens's y (first: the graphics driver's segment)*/

	if(argc != 5) {
		printf("\"The path to victory is marked with the laws of Virtue,\nnot the commands of DOS.\"\n\n     -- Lord British\n");
		exit(0);
	}
	/*the configuration: CONFIG.U6 then the first argument*/
	mouseOn = 1;
	adapter = 0;
	musicCard = 0;
	fd = OSI_open("config.u6");
	if(fd >= 0) {
		OSI_read(fd, 0L, 6L, cfg);
		cfg[6] = 0;
		ParseConfig(cfg);
		OSI_close(fd);
	}
	ParseConfig(argv[1]);
	/*how long the game took*/
	years = atoi(argv[2]);
	months = atoi(argv[3]);
	days = atoi(argv[4]);
	if(years) {
		strcpy(yearsText, numberWords[years]);
		strcat(yearsText, " year");
		if(years != 1)
			strcat(yearsText, "s");
	}
	if(months) {
		strcpy(monthsText, numberWords[months]);
		strcat(monthsText, " month");
		if(months != 1)
			strcat(monthsText, "s");
	}
	if(days || (!years && !months)) {
		strcpy(daysText, numberWords[days]);
		strcat(daysText, " day");
		if(days != 1)
			strcat(daysText, "s");
	}
	srand(time(0));
	/*the graphic driver*/
	switch(adapter) {
		case 0: videoMode = 0x13; gfxDrv.f = (void (far *)())LoadZFile("mcgadrv.bin"); break;
		case 1: videoMode = 0xd; gfxDrv.f = (void (far *)())LoadZFile("eegadrv.bin"); break;
		case 2: videoMode = 9; gfxDrv.f = (void (far *)())LoadZFile("tgadrv.bin"); break;
		case 3: videoMode = 4; gfxDrv.f = (void (far *)())LoadZFile("cgadrv.bin"); break;
		case 4: videoMode = 7; gfxDrv.f = (void (far *)())LoadZFile("cgadrv.bin"); break;
	}
	/*some of its entries, for the OSI library*/
	y = C_02AE_00BA();
	drvDrawPointer[0] = 3; drvDrawPointer[1] = y;
	drvDrawPointer[2] = 6; drvDrawPointer[3] = y;
	drvDrawPointer[4] = 9; drvDrawPointer[5] = y;
	drvDrawPointer[6] = 0x12; drvDrawPointer[7] = y;
	drvDrawPointer[8] = 0xc; drvDrawPointer[9] = y;
	switch(videoMode) {
		case 4: case 9: screen.segment = 0xb800; break;
		case 7: screen.segment = 0xb000; C_02AE_00D0(); break;
		case 0xd: screen.segment = 0xa200; break;
		case 0x13: screen.segment = 0xa000; break;
	}
	LoadMusicDriver(musicCard, musicPort);
	PlayMusic(10);
	if(videoMode != 7) {
		DRV(0)(scanlineOfs);
	}
	screen.pScanlines = scanlineOfs;
	screen.right = 0x13f;
	screen.left = 0;
	screen.top = 0;
	screen.bottom = 0xc7;
	backBuffer = scrCodex = screen;
	if(videoMode == 0xd) {
		egaSaved = screen;
		egaSaved.right = 0x9f;
		SURF_alloc(&egaSaved);
		egaSaved.right = 0x13f;
		backBuffer = screen;
	} else {
		SURF_alloc(&backBuffer);
	}
	/*the parts of the picture*/
	leftHalf = rightHalf = leftLens = rightLens = backCodex = backBuffer;
	backCodex.right = scrCodex.right = 0xda;
	backCodex.left = scrCodex.left = 0x6a;
	backCodex.top = scrCodex.top = 0;
	backCodex.bottom = scrCodex.bottom = 0x5d;
	leftHalf.right = rightHalf.left = leftLens.right = rightLens.left = 0xa0;
	leftLens.bottom = rightLens.bottom = 0x7d;
	backLeft = backRight = backLower = backBuffer;
	scrLeft = scrRight = scrLower = screen;
	backLeft.right = scrLeft.right = 0x69;
	backRight.left = scrRight.left = 0xd5;
	backLeft.bottom = scrLeft.bottom = scrRight.bottom = backRight.bottom = 0x50;
	backLower.top = scrLower.top = 0x51;
	if(videoMode == 0xd)
		backRight.left = scrRight.left = 0xd8;
	scene = codexStage = 0;
	/*the text*/
	textSurface = backBuffer;
	if(videoMode != 0xd)
		SURF_alloc(&textSurface);
	else
		textSurface.segment = 0xa600;
	textWin = &textSurface;
	textFont = LZW_load("u6.set");
	TXT_select(&console);
	textFg = 0xf;
	textBg = 0;
	textBuf = malloc(0x400);
	textAlign = 3;
	/*the mouse*/
	mouseInfo.surface = &screen;
	mouseInfo.saveBuf = farmalloc(0x400L);
	if(mouseOn == 0)
		MOUSE_NOINIT(&mouseInfo);
	else
		MOUSE_INIT(&mouseInfo);
	switch(videoMode) {
		case 4: endShapes = LZW_load("end.cga"); break;
		case 7: endShapes = LZW_load("end.hrc"); break;
		case 9: endShapes = LZW_load("end.tga"); break;
		case 0xd: endShapes = LZW_load("end.ega"); break;
		case 0x13: endShapes = LZW_load("end.shp"); PAL_load("endpal.lbm"); break;
	}
	SavePalette();
	TIM_calibrate();
	/*the first picture: the Codex closed, the room, the two kings*/
	DRV(3)(&backBuffer, 0x6a, 0, endShapes, 4);
	DRV(3)(&backBuffer, 0, 0, endShapes, 7);
	DRV(3)(&backBuffer, 0x49, 0x57, endShapes, 5);
	DRV(3)(&backBuffer, 0xda, 0x57, endShapes, 6);
	ShowText(0);
	if(videoMode == 0x13)
		FADE_in(&backBuffer, &screen);
	if(videoMode == 0xd) {
		screen.segment ^= 0x200;
		DRV(0xf)(&backBuffer, &screen);
		screen.segment ^= 0x200;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	DrawText();
	InitStars();
	WaitInput(0x1e);
	ShowAround();
	TIM_install();
	if(videoMode == 0x13)
		TIM_setCallback(C_02AE_0001);
	/*Lord British's lens rises*/
	speed = ScaleSpeed(3, 6);
	y = 0x7d;
	done = 0;
	while(!done && (done = y == -12, 1)) {
		DRV(3)(&backBuffer, 0, 0, endShapes, 7);
		DRV(3)(&leftLens, 9, y, endShapes, 3);
		DRV(3)(&backBuffer, 0x49, 0x57, endShapes, 5);
		DRV(3)(&backBuffer, 0xda, 0x57, endShapes, 6);
		DrawText();
		ShowAround();
		if(CheckInput())
			y = speed + -12;
		y -= speed;
		y += (-12 - y) * (y <= -12);
	}
	ShowText(1);
	while(!CheckInput())
		;
	/*the scenes*/
	done = 0;
	do {
		moving = 0;
		DRV(3)(&backBuffer, 0, 0, endShapes, 7);
		if(scene < 2 && y < 0x7d) {
			DRV(3)(&leftLens, 9, y, endShapes, 3);
			if(scene == 1) {
				y += speed;
				moving++;
			}
		}
		if(scene == 6 || scene == 7) {
			moving++;
			DRV(3)(&rightLens, 0xe6, y, endShapes, 3);
			y -= speed;
			if(y < -12)
				y = -12;
		}
		if(scene == 8 && y < 0x7d) {
			moving++;
			DRV(3)(&rightLens, 0xe6, y += speed, endShapes, 3);
		}
		if(scene >= 7 && scene <= 10)
			DRV(3)(&rightHalf, 0, 0, endShapes, 8);
		if(scene > 10) {
			DRV(3)(&rightHalf, 0, 0, endShapes, 9);
		} else {
			DRV(3)(&backBuffer, 0xda, 0x57, endShapes, 6);
		}
		if(scene < 4) {
			DRV(3)(&leftHalf, 0, 0, endShapes, 8);
			DRV(3)(&backBuffer, 0x49, 0x57, endShapes, 5);
		} else {
			DRV(3)(&leftHalf, 0, 0, endShapes, 9);
		}
		if(scene != 14)
			DrawText();
		ShowAround();
		/*the Codex appears*/
		if(scene == 4 && codexStage != 1) {
			WaitInput(5);
			DRV(3)(&backCodex, 0x6a, 0, endShapes, 0);
			if(videoMode != 0x13)
				TIM_setCallback(C_02AE_0001);
			switch(videoMode) {
				case 4: case 7:
					effect = ScaleSpeed(0x1e, 0x3c);
					DIS_dissolveCGA(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x97, 0x1d, endShapes, 1);
					DIS_dissolveCGA(&backCodex, &scrCodex, effect);
				break;
				case 9:
					effect = ScaleSpeed(0x1e, 0x3c);
					DIS_dissolveTDY(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x97, 0x1d, endShapes, 1);
					DIS_dissolveTDY(&backCodex, &scrCodex, effect);
				break;
				case 0xd:
					effect = ScaleSpeed(0x1e, 0x3c);
					scrCodex.segment = screen.segment ^ 0x200;
					DIS_dissolveEGA(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x97, 0x1d, endShapes, 1);
					DIS_dissolveEGA(&backCodex, &scrCodex, effect);
					scrCodex.segment = screen.segment;
				break;
				case 0x13:
					DIS_dissolve(&backCodex, &scrCodex, 0x46);
					DRV(3)(&backCodex, 0x97, 0x1d, endShapes, 1);
					DIS_dissolve(&backCodex, &scrCodex, 0x1e);
				break;
			}
			while(kbhit())
				getch();
			codexStage = 1;
		}
		/*the light of the two lenses on it*/
		if(scene == 12 && codexStage != 2) {
			WaitInput(5);
			PlayMusic(12);
			switch(videoMode) {
				case 4: case 7:
					effect = ScaleSpeed(0x1e, 0x3c);
					DRV(3)(&backCodex, 0x85, 0x17, endShapes, 2);
					DIS_dissolveCGA(&backCodex, &scrCodex, effect);
				break;
				case 9:
					effect = ScaleSpeed(0x1e, 0x3c);
					DRV(3)(&backCodex, 0x85, 0x17, endShapes, 2);
					DIS_dissolveTDY(&backCodex, &scrCodex, effect);
				break;
				case 0xd:
					effect = ScaleSpeed(0x1e, 0x3c);
					scrCodex.segment = screen.segment ^ 0x200;
					DRV(3)(&backCodex, 0x85, 0x17, endShapes, 2);
					DIS_dissolveEGA(&backCodex, &scrCodex, effect);
					scrCodex.segment = screen.segment;
				break;
				case 0x13:
					DRV(3)(&backCodex, 0x85, 0x17, endShapes, 2);
					DIS_dissolve(&backCodex, &scrCodex, 0x1e);
				break;
			}
			while(kbhit())
				getch();
			codexStage = 2;
		}
		/*the Codex opens*/
		if(scene == 14 && codexStage != 3) {
			WaitInput(0x1e);
			DRV(3)(&backCodex, 0x6a, 0, endShapes, 0);
			switch(videoMode) {
				case 4: case 7:
					effect = ScaleSpeed(0x1e, 0x3c);
					DIS_dissolveCGA(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x6a, 0, endShapes, 4);
					DIS_dissolveCGA(&backCodex, &scrCodex, effect);
				break;
				case 9:
					effect = ScaleSpeed(0x1e, 0x3c);
					DIS_dissolveTDY(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x6a, 0, endShapes, 4);
					DIS_dissolveTDY(&backCodex, &scrCodex, effect);
				break;
				case 0xd:
					effect = ScaleSpeed(0x1e, 0x3c);
					scrCodex.segment = screen.segment ^ 0x200;
					DIS_dissolveEGA(&backCodex, &scrCodex, effect);
					DRV(3)(&backCodex, 0x6a, 0, endShapes, 4);
					DIS_dissolveEGA(&backCodex, &scrCodex, effect);
					scrCodex.segment = screen.segment;
				break;
				case 0x13:
					DIS_dissolve(&backCodex, &scrCodex, 0xf);
					DRV(3)(&backCodex, 0x6a, 0, endShapes, 4);
					DIS_dissolve(&backCodex, &scrCodex, 0x50);
				break;
			}
			TIM_resetCallback();
			codexStage = 3;
			done = 1;
		}
		/*the next scene: at a key or a click (at once when nothing moves)*/
		if(!moving)
			while(!CheckInput())
				;
		if(CheckInput() || !moving) {
			switch(++scene) {
				case 6:/*Draxinusom comes*/
					ShowText(7);
					PlayMusic(11);
					y = 0x7d;
				break;
				case 7:
					ShowText(8);
					y = -12;
				break;
				case 14:
				break;
				case 15:
					done = 1;
				break;
				default:
					ShowText(scene + 1);
			}
		}
	} while(!done);
	/*the end: congratulations*/
	TIM_remove();
	if(videoMode == 0x13)
		FADE_out(&screen, 0);
	DRV(0xc)(&screen, 0);
	SetPalette();
	ShowText(15);
	DRV(0xc)(&backBuffer, 0);
	DrawText();
	if(videoMode == 0x13) {
		FADE_in(&backBuffer, &screen);
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
		backBuffer.segment = screen.segment;
	} else {
		DRV(0xf)(&backBuffer, &screen);
	}
	while(kbhit())
		getch();
	while(!CheckInput())
		;
	if(videoMode != 0xd)
		SURF_free(&textSurface);
	if(videoMode == 0xd)
		backBuffer = egaSaved;
	SURF_free(&backBuffer);
	PlayMusic(0);
	VID_setMode(3);
}

/*from b toward a, by how fast the computer is (loopsPerTick from 1500 to 20000): b on
 a slow computer, a on a fast one. Not MCGA: b counts twice; EGA: twice the result
 (U.EXE's)*/
/*C_00A9_1D9C*/int16 ScaleSpeed(int16 a, int16 b) {
	uint16 d;
	uint16 speed;
	int16 mul;

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

/*the music drivers (index 1..6 = musicCard)*/
/*03DE*/char *musicDrivers[] = {
	"U6ADLIB.DRV",
	"U6CMS.DRV",
	"U6TMUS.DRV",
	"U6ROLAND.DRV",
	"U6COVOX.DRV",
	"U6INNOVA.DRV"
};

/*load the driver of music card <card> and init it with the i/o <port>*/
/*C_00A9_1E52*/LoadMusicDriver(int16 card, int16 port) {
	if(card) {
		musDrv.f = (void (far pascal *)())LoadZFile(musicDrivers[card - 1]);
		MUS(0)(port);
	}
}

/*midi.dat parser: returns the next decimal number, or -1 when the
closing ')' comes first*/
/*C_00A9_1E82*/MidiNumber() {
	int16 ch, n;

	while(1) {
		ch = songData[songPos];
		if(ch == ')')
			return -1;
		if(isdigit(ch))
			break;
		songPos++;
	}
	n = 0;
	while(1) {
		ch = songData[songPos];
		if(!isdigit(ch))
			break;
		n = n * 10;
		n += ch - '0';
		songPos++;
	}
	return n;
}

/*set up the (Roland) driver's channels for song <song> (0-based): midi.dat
holds a "( a b a b ...)" group per song; without it all 16 channels get
(2, 0)*/
/*C_00A9_1EDB*/MidiChannels(int16 song) {
	int16 fd, ch;
	int16 parens, channel, val, prog;

	fd = OSI_open("midi.dat");
	songData = farmalloc(0x401L);
	if(fd >= 0) {
		OSI_read(fd, 0L, 0x400L, songData);
		parens = 0;
		for(songPos = 0; songPos < 0x400; songPos++) {
			if(parens > song) {
				for(channel = 0; (val = MidiNumber()) >= 0; channel++) {
					prog = MidiNumber();
					MUS(0xc)(channel, val, prog);
				}
				break;
			}
			if(songData[songPos] == '(')
				parens++;
		}
		OSI_close(fd);
	} else {
		for(ch = 0; ch < 16; ch++) {
			MUS(0xc)(ch, 2, 0);
		}
	}
	farfree(songData);
}

/*the songs (index 1..12 for PlayMusic)*/
/*03EA*/char *songFiles[] = {
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

/*play song <song> (1-based in songFiles), 0 stops the music*/
/*C_00A9_1FC8*/PlayMusic(int16 song) {
	if(musicCard == 0)
		return;
	if(song == songPlaying)
		return;
	if(songPlaying) {
		MUS(9)();
		songPlaying = 0;
		farfree(songData);
	}
	if(song && song != songPlaying) {
		if(musicCard == 4)
			MidiChannels(song - 1);
		songData = LZW_load(songFiles[song - 1]);
		MUS(6)(songData);
		songPlaying = song;
	}
}
