/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0263: transfer a character from Ultima IV or Ultima V
 */
#include "u.h"
#include <alloc.h>
#include <stdio.h>
#include <string.h>

/*a drawing surface of the graphics driver (14 bytes)*/

/*a character of Ultima IV's PARTY.SAV (the program takes 40 bytes for it)*/
struct tU4Char {
	uint16 hp, hpMax;/*_00,_02*/
	int16 xp;/*_04*/
	int16 str, dex, intel;/*_06,_08,_0A*/
	int16 mp;/*_0C*/
	int16 unk0e, weapon, armor;/*_0E,_10,_12*/
	char name[16];/*_14*/
	char sex;/*_24*//*0x0b: male*/
	char klass;/*_25*/
	char status;/*_26*/
	char pad;/*_27*/
};
/*what follows the 8 characters in PARTY.SAV*/
struct tU4Rest {
	int32 food;/*_00*/
	int16 gold;/*_04*/
	int16 karma[8];/*_06*//*0: Avatarhood in that virtue*/
};
/*a character of Ultima V's SAVED.GAM*/
struct tU5Char {
	char name[9];/*_00*/
	char sex;/*_09*//*0x0b: male*/
	char klass;/*_0A*/
	char status;/*_0B*/
	unsigned char str, dex, intel, mp;/*_0C.._0F*/
	int16 hp, hpMax;/*_10,_12*/
	int16 xp;/*_14*/
	unsigned char level;/*_16*/
};

extern int16 videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface screen;/*the screen*/
extern struct tSurface backBuffer;
extern struct tSurface textArea;/*the console's surface*/
extern struct tSurface textAreaCopy;
extern int16 menuDrawn;
extern struct tSurface workArea;

/*0263's initialised data*/
/*03CA*/char u4PartyFile[] = "a:\\party.sav";/*Ultima IV's save file; [0]: the drive letter*/
/*03D7*/char u5SaveFile[] = "a:\\saved.gam";/*Ultima V's*/
/*03E4*/char u5SaveFile2[] = "a:\\ultima\\saved.gam";
/*03F8*/char u4PartyFile2[] = "a:\\ultima\\party.sav";
/*040C*/char classNames[][9] = {/*the names of the classes*/
	"Mage", "Bard", "Fighter", "Druid", "Tinker", "Paladin", "Ranger", "Shepherd",
	"Avatar"
};
/*045D*/char sheetKeys[] = "NSCA\x1b";/*keys of the character sheet's buttons: Name, Sex, Continue, Abort, Escape*/
/*0463*/char portraitKeys[] = "SPTA\x1b";/*keys of the portrait screen's: Sex, Portrait, Transfer (or Create), Abort, Escape*/

/*the character read from the old game*/
extern char oldName[];/*name*/
extern int16 oldStr, oldDex, oldInt;/*strength, dexterity, intelligence*/
extern char oldSex[];/*"Male"/"Female"*/
extern char oldClass[];/*class*/
extern char oldGood;/*'G'*/
extern char oldAvatar;/*1: an Avatar*/
extern int16 oldExp;/*experience*/
extern int16 oldLevel;/*level*/
extern int16 oldHp, oldHpMax;/*hit points, maximum*/
extern int16 oldMp;/*magic points*/

/*the character made for Ultima VI*/
extern char newName[];/*name*/
extern int16 newStr, newDex, newInt;/*strength, dexterity, intelligence*/
extern char newSex[];/*"Male"/"Female"*/
extern char newClass[];/*class ("Avatar")*/
extern char newGood;/*'G'*/
extern char newAvatar;
extern int16 newExp;/*experience*/
extern int16 newLevel;/*level*/
extern int16 newHp, newHpMax;/*hit points, maximum*/
extern int16 newMp;/*magic points*/
extern int16 newPortrait;/*portrait*/

extern int16 transferFrom;/*the game the character comes from: 4, 5 (0 none)*/
extern int16 portraitIdx;/*portrait (0-5)*/
extern char *saveData;/*the save file read*/
extern void far *portraits;/*the portraits (montage)*/
extern void far *vellum;/*the vellum pictures*/
extern struct tSurface buttonAreas[5];/*the buttons*/
extern struct tSurface dialogArea;
extern struct tSurface nameWindow;
extern struct tSurface titleArea;/*the title*/
extern struct tSurface sheet;/*the character sheet*/

extern InputString(char *dest, int16 max);/*text input*/
#ifdef __TURBOC__
extern MenuSelect(struct tSurface *, char *);/*waits for a button or key: index in the keys (declared int here: this module reads all of AX; the port has the prototype)*/
#endif
extern SetPalette();
extern PlayMusic(int16 song);
extern int16 WriteNewCharacter();
extern int16 PollInput();/*nonzero when the player wants to go on (key or time)*/
extern ShowPassage(int16 n);
extern DisplayPassage();
extern AskForDisk(int16 n);/*AskForDisk*/
extern SURF_alloc(struct tSurface *);/*gives a surface an off-screen buffer*/
extern SURF_free(struct tSurface *);/*frees it*/
extern SURF_share(struct tSurface *, ...);
extern TXT_printf(char far *);/*console puts*/
extern TXT_sprintf(char far *, ...);/*console printf*/
extern TXT_sappend(char far *, ...);/*console printf*/
extern MOUSE_SHOW();/*mouse cursor on*/
extern MOUSE_HIDE();/*mouse cursor off*/
extern FADE_in(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern FADE_out(struct tSurface *, int16);/*MCGA: palette fade out*/
extern void far *LZW_load(char *);/*loads a file in memory*/
extern char *LOAD_near(char *);/*loads a file in memory*/

/*reads the first character of Ultima IV's PARTY.SAV (in saveData); nonzero if it looks wrong*/
/*C_0263_0000*/ReadU4Character() {
	struct tU4Rest *rest;
	struct tU4Char *p;
	int16 i;

	p = (struct tU4Char *)(saveData + 8);
	rest = (struct tU4Rest *)(p + 8);
	for(i = 0; i < 8; i++) {
		oldName[i] = p->name[i];
		oldName[i + 1] = 0;
	}
	oldStr = p->str;
	oldDex = p->dex;
	oldInt = p->intel;
	strcpy(oldSex, (p->sex == 0xb)?"Male":"Female");
	oldGood = 'G';
	/*an Avatar if all eight virtues are complete*/
	oldAvatar =
		rest->karma[0] == 0 && rest->karma[1] == 0 &&
		rest->karma[2] == 0 && rest->karma[3] == 0 &&
		rest->karma[4] == 0 && rest->karma[5] == 0 &&
		rest->karma[6] == 0 && rest->karma[7] == 0;
	if(oldAvatar)
		p->klass = 8;
	strcpy(oldClass, classNames[p->klass]);
	oldExp = p->xp;
	oldLevel = p->hpMax / 100;
	oldHp = p->hp;
	oldHpMax = p->hpMax;
	oldMp = p->mp;
	return
		oldStr > 99 || oldDex > 99 || oldInt > 99 ||
		oldHp > 9999 || oldHpMax > 9999 || oldExp > 9999 ||
		p->klass > 7;
}

/*reads the Avatar from Ultima V's SAVED.GAM (in saveData); nonzero if it looks wrong*/
/*C_0263_013D*/ReadU5Character() {
	int16 i;
	struct tU5Char *p;

	p = (struct tU5Char *)(saveData + 2);
	for(i = 0; i < 8; i++)
		oldName[i] = p->name[i];
	oldStr = p->str;
	oldDex = p->dex;
	oldInt = p->intel;
	strcpy(oldSex, (p->sex == 0xb)?"Male":"Female");
	oldGood = 'G';
	oldAvatar = 1;
	strcpy(oldClass, classNames[8]);
	oldExp = p->xp;
	oldLevel = p->level;
	oldHp = p->hp;
	oldHpMax = p->hpMax;
	oldMp = p->mp;
	return
		oldStr > 30 || oldDex > 30 || oldInt > 30 ||
		oldHp > 9999 || oldHpMax > 9999 || oldExp > 9999;
}

/*an Ultima IV attribute (up to 99) in Ultima VI's scale*/
/*C_0263_01FD*/ConvertU4Stat(int16 s) {
	if(s < 10)
		return s;
	if(s >= 30)
		return (s - 30) / 4 + 20;
	return (s - 9) / 2 + 10;
}

/*the Ultima VI character from the Ultima IV one*/
/*C_0263_0233*/MakeFromU4() {
	int16 i;

	strcpy(newName, oldName);
	newStr = ConvertU4Stat(oldStr);
	newDex = ConvertU4Stat(oldDex);
	newInt = ConvertU4Stat(oldInt);
	strcpy(newSex, oldSex);
	strcpy(newClass, classNames[8]);
	newGood = 'G';
	newExp = oldExp / 10;
	/*level: one more for each doubling of the experience above 1000*/
	newLevel = 1;
	for(i = oldExp / 1000; i > 0; i >>= 1)
		newLevel++;
	newHp = newHpMax = newLevel * 30;
	newMp = newInt;
	newAvatar = 1;
}

/*the Ultima VI character from the Ultima V one (same attribute scale)*/
/*C_0263_02D4*/MakeFromU5() {
	int16 i;

	strcpy(newName, oldName);
	newStr = oldStr;
	newDex = oldDex;
	newInt = oldInt;
	strcpy(newSex, oldSex);
	strcpy(newClass, classNames[8]);
	newGood = 'G';
	newAvatar = 1;
	newExp = oldExp / 10;
	newLevel = 1;
	for(i = oldExp / 1000; i > 0; i >>= 1)
		newLevel++;
	newHp = newHpMax = newLevel * 30;
	newMp = newInt;
}

/*the two columns of the character sheet (old and new character), and the buttons*/
/*C_0263_0360*/DrawSheet() {
	sheet.left += 0x35;
	switch(videoMode) {
		case 4: case 7:
			DRV(0xc)(&sheet, 3);
			textFg = 2; textBg = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&sheet, 0xe);
			textFg = 2; textBg = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&sheet, 0x31);
			textFg = 2; textBg = 0x31;
			break;
	}
	/*the old character*/
	textX = sheet.left;
	textY = sheet.top + 2;
	textAlign = 0;
	TXT_sprintf("%s\n%s\n%s\n", oldName, oldSex, oldClass);
	TXT_sappend("%d\n%d\n%d\n%d\n%d\n%d%P", oldStr, oldDex, oldInt, oldMp, oldExp, oldLevel);
	sheet.left += 0x20;
	sheet.left += 0x31;
	/*the new one*/
	if(videoMode > 7)
		textFg = 9;
	else
		textFg = 0;
	textX = sheet.left;
	textY = sheet.top + 2;
	TXT_sprintf("%s\n%s\n%s\n", newName, newSex, newClass);
	TXT_sappend("%d\n%d\n%d\n%d\n%d\n%d%P", newStr, newDex, newInt, newMp, newExp, newLevel);
	sheet.left -= 0x86;
	/*the buttons*/
	if(videoMode == 0xd)
		nameWindow.segment = screen.segment;
	DRV(3)(&nameWindow, buttonAreas[0].left, buttonAreas[0].top, vellum, 2);
	DRV(3)(&nameWindow, buttonAreas[1].left, buttonAreas[1].top, vellum, 3);
	DRV(3)(&nameWindow, buttonAreas[2].left, buttonAreas[2].top, vellum, 0x10);
	DRV(3)(&nameWindow, buttonAreas[3].left, buttonAreas[3].top, vellum, 4);
}

/*C_0263_055F*/ToggleSex() {
	switch(newSex[0]) {
		case 'M': strcpy(newSex, "Female"); break;
		case 'F': strcpy(newSex, "Male"); break;
	}
}

/*the character sheet: Name, Sex, Continue, Abort; 1 to go on, 0 to give up*/
/*C_0263_0592*/CharacterScreen() {
	int16 key;
	int16 bottom;
	int16 done;
	int16 ret;

	if(videoMode == 0xd)
		nameWindow.segment = screen.segment;
	MOUSE_HIDE();
	titleArea.right = sheet.right = 0x123;
	titleArea.left = sheet.left = 0x1e;
	titleArea.top = 0x57;
	titleArea.bottom = 0x63;
	sheet.top = 0x63;
	sheet.bottom = 0xae;
	buttonAreas[0].top = buttonAreas[1].top = buttonAreas[2].top = buttonAreas[3].top = 0xae;
	buttonAreas[0].bottom = buttonAreas[1].bottom = buttonAreas[2].bottom = buttonAreas[3].bottom = 0xbd;
	buttonAreas[0].left = 0x1c;
	buttonAreas[1].left = 0x5f;
	buttonAreas[2].left = 0x9a;
	buttonAreas[3].left = 0xdd;
	buttonAreas[0].right = 0x5d;
	buttonAreas[1].right = 0x98;
	buttonAreas[2].right = 0xdb;
	buttonAreas[3].right = 0x124;
	buttonAreas[4] = buttonAreas[3];
	SURF_share(&backBuffer, &titleArea, &sheet, &buttonAreas[0], &buttonAreas[1], &buttonAreas[2], &buttonAreas[3], &buttonAreas[4], 0);
	bottom = sheet.bottom;
	if(videoMode == 0xd) {
		titleArea.segment = sheet.segment = screen.segment;
		sheet.pScanlines = screen.pScanlines;
	} else {
		sheet.segment = nameWindow.segment;
		sheet.pScanlines = nameWindow.pScanlines;
	}
	sheet.bottom = --sheet.top;
	switch(videoMode) {
		case 4: case 7:
			DRV(0xc)(&nameWindow, 3);
			DRV(0xc)(&sheet, 3);
			textFg = 0; textBg = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&nameWindow, 0xe);
			DRV(0xc)(&sheet, 0xe);
			textFg = 0; textBg = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&nameWindow, 0x31);
			DRV(0xc)(&sheet, 0x3d);
			textFg = 0x3d; textBg = 0x31;
			break;
	}
	sheet.bottom = bottom;
	sheet.top++;
	/*the title*/
	textWin = &titleArea;
	textX = titleArea.left + 0x35;
	textY = titleArea.top + 2;
	TXT_printf("Ultima ");
	if(transferFrom == 4)
		TXT_printf("IV");
	else
		TXT_printf("V");
	textX = titleArea.left + 0x86;
	TXT_printf("Ultima VI");
	/*the labels*/
	sheet.right -= 0xd6;
	textWin = &sheet;
	textX = sheet.left;
	textY = sheet.top + 2;
	textAlign = 1;
	TXT_sprintf("Name\nSex\nClass\n");
	TXT_sappend("Str\nDex\nInt\nMagic\nExp\nLevel%P");
	sheet.right += 0xd6;
	DrawSheet();
	SURF_share(&screen, &dialogArea, &titleArea, &sheet, &buttonAreas[0], &buttonAreas[1], &buttonAreas[2], &buttonAreas[3], &buttonAreas[4], 0);
	if(videoMode != 0xd) {
		DRV(0xf)(&nameWindow, &dialogArea);
	} else {
		backBuffer = screen;
	}
	MOUSE_SHOW();
	done = 0;
	while(!done) {
		key = sheetKeys[MenuSelect(buttonAreas, sheetKeys)];
		switch(key) {
			case 'C':
				done = 1;
				ret = 1;
				break;
			case 'N':
				/*enter a new name*/
				textWin = &sheet;
				textFg = 0;
				textX = sheet.left + 0x86;
				textY = sheet.top + 2;
				MOUSE_HIDE();
				clreol();
				InputString(newName, 0xd);
				SURF_share(&backBuffer, &sheet, 0);
				if(videoMode == 0xd)
					sheet.segment = screen.segment;
				DrawSheet();
				SURF_share(&screen, &sheet, 0);
				if(videoMode != 0xd) {
					DRV(0xf)(&nameWindow, &dialogArea);
				}
				MOUSE_SHOW();
				break;
			case 'S':
				MOUSE_HIDE();
				ToggleSex();
				SURF_share(&backBuffer, &sheet, 0);
				if(videoMode == 0xd)
					sheet.segment = screen.segment;
				DrawSheet();
				SURF_share(&screen, &sheet, 0);
				if(videoMode != 0xd) {
					DRV(0xf)(&nameWindow, &dialogArea);
				}
				MOUSE_SHOW();
				break;
			case 0x1b: case 'A':
				done = 1;
				ret = 0;
				break;
		}
	}
	return ret;
}

/*choosing the portrait: Sex, Portrait, Transfer (or Create), Abort; 1 when chosen*/
/*C_0263_0987*/PortraitScreen(int16 transfer) {
	int16 key;
	int16 frame;
	int16 bottom;
	struct tSurface portrait;
	int16 done;
	int16 ret;

	portrait = nameWindow;
	/*the last button: Transfer or Create*/
	if(transfer) {
		portraitKeys[2] = 'T';
		frame = 1;
	} else {
		portraitKeys[2] = 'C';
		frame = 0x10;
	}
	if(videoMode == 0xd) {
		nameWindow.segment = portrait.segment = screen.segment;
		MOUSE_HIDE();
	}
	portrait.right = 0x102;
	portrait.left = 0xc3;
	portrait.top = 0x76;
	portrait.bottom = 0xad;
	titleArea.right = sheet.right = 0x123;
	titleArea.left = sheet.left = 0x1e;
	titleArea.top = 0x57;
	titleArea.bottom = 0x63;
	sheet.top = 0x63;
	sheet.bottom = 0xae;
	buttonAreas[0].right = buttonAreas[1].right = buttonAreas[2].right = buttonAreas[3].right = 0x97;
	buttonAreas[0].left = 0x5e;
	buttonAreas[1].left = 0x3e;
	buttonAreas[2].left = 0x56;
	buttonAreas[3].left = 0x50;
	buttonAreas[0].top = 0x70;
	buttonAreas[1].top = 0x81;
	buttonAreas[2].top = 0x92;
	buttonAreas[3].top = 0xa3;
	buttonAreas[0].bottom = 0x7f;
	buttonAreas[1].bottom = 0x90;
	buttonAreas[2].bottom = 0xa1;
	buttonAreas[3].bottom = 0xb2;
	buttonAreas[4] = buttonAreas[3];
	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	SURF_share(&backBuffer, &titleArea, &sheet, &buttonAreas[0], &buttonAreas[1], &buttonAreas[2], &buttonAreas[3], &buttonAreas[4], 0);
	switch(videoMode) {
		case 4: case 7: DRV(0xc)(&nameWindow, 3); break;
		case 9: case 0xd: DRV(0xc)(&nameWindow, 0xe); break;
		case 0x13: DRV(0xc)(&nameWindow, 0x31); break;
	}
	bottom = sheet.bottom;
	if(videoMode == 0xd) {
		sheet.segment = screen.segment;
		sheet.pScanlines = screen.pScanlines;
	} else {
		sheet.segment = nameWindow.segment;
		sheet.pScanlines = nameWindow.pScanlines;
	}
	sheet.bottom = --sheet.top;
	switch(videoMode) {
		case 4: case 7:
			DRV(0xc)(&sheet, 3);
			textFg = 0; textBg = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&sheet, 0xe);
			textFg = 0; textBg = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&sheet, 0x3d);
			textFg = 0x3d; textBg = 0x31;
			break;
	}
	sheet.bottom = bottom;
	sheet.top++;
	/*the name as title*/
	textWin = &titleArea;
	textX = titleArea.left;
	textY = titleArea.top + 2;
	textAlign = 2;
	TXT_sprintf("%s%P", newName);
	/*the buttons*/
	DRV(3)(&nameWindow, buttonAreas[0].left, buttonAreas[0].top, vellum, 3);
	DRV(3)(&nameWindow, buttonAreas[1].left, buttonAreas[1].top, vellum, 0x12);
	DRV(3)(&nameWindow, buttonAreas[2].left, buttonAreas[2].top, vellum, frame);
	DRV(3)(&nameWindow, buttonAreas[3].left, buttonAreas[3].top, vellum, 4);
	SURF_share(&screen, &dialogArea, &titleArea, &sheet, &buttonAreas[0], &buttonAreas[1], &buttonAreas[2], &buttonAreas[3], &buttonAreas[4], 0);
	done = 0;
	while(!done) {
		/*the portrait*/
		MOUSE_HIDE();
		switch(videoMode) {
			case 4: case 7: DRV(0xc)(&portrait, 3); break;
			case 9: case 0xd: DRV(0xc)(&portrait, 0xe); break;
			case 0x13: DRV(0xc)(&portrait, 0x31); break;
		}
		newPortrait = ((newSex[0] == 'F')?0:6) + portraitIdx;
		DRV(3)(&portrait, portrait.left, portrait.top, portraits, newPortrait);
		if(videoMode != 0xd) {
			DRV(0xf)(&nameWindow, &dialogArea);
		}
		MOUSE_SHOW();
		key = portraitKeys[MenuSelect(buttonAreas, portraitKeys)];
		switch(key) {
			case 'S':
				ToggleSex();
				break;
			case 'P':
				portraitIdx = (portraitIdx + 1) % 6;
				break;
			case 'C': case 'T':
				done = 1;
				ret = 1;
				break;
			case 0x1b: case 'A':
				done = 1;
				ret = 0;
				break;
		}
	}
	return ret;
}

/*asks for the drive of the old game and reads its save file: 4 (Ultima IV), 5 (Ultima V), 0 Escape*/
/*C_0263_0E40*/FindCharacter() {
	int16 found;
	int16 ndrives;
	int16 key;
	struct tSurface title;
	struct tSurface text;
	struct tSurface bar;
	struct tSurface buttons[27];
	struct tSurface line;
	char keys[32];
	int16 i;
	int16 color;

	if(videoMode == 0xd)
		backBuffer.segment = screen.segment;
	for(i = 0; i < 27; i++) {
		SURF_share(&backBuffer, &buttons[i], 0);
		buttons[i].top = 0xae;
		buttons[i].bottom = 0xbd;
	}
	title.right = bar.right = text.right = 0x124;
	title.left = bar.left = text.left = 0x1e;
	title.top = 0x59;
	title.bottom = 0x64;
	text.top = 0x64;
	text.bottom = 0xae;
	bar.top = 0xae;
	bar.bottom = 0xbd;
	SURF_share(&backBuffer, &title, &text, &bar, 0);
	switch(videoMode) {
		case 4: case 7:
			DRV(0xc)(&nameWindow, 3);
			textFg = 0; textBg = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&nameWindow, 0xe);
			textFg = 0; textBg = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&nameWindow, 0x31);
			textFg = 0x3d; textBg = 0x31;
			break;
	}
	textWin = &title;
	textX = title.left;
	textY = title.top;
	textAlign = 2;
	TXT_sprintf("TRANSFER A CHARACTER%P");
	if(videoMode == 0xd) {
		text.segment = screen.segment;
		text.pScanlines = screen.pScanlines;
	}
	textWin = &text;
	textX = text.left;
	textY = title.top + 0x20;
	if(videoMode == 0x13)
		color = 0x31;
	else if(videoMode == 9 || videoMode == 0xd)
		color = 0xe;
	else
		color = 3;
	DRV(0xc)(&text, color);
	TXT_sprintf("Enter the letter of the\nfloppy/hard drive containing your\n");
	TXT_sappend("Ultima V Britannia\nor Ultima IV Player disk....%P");
	/*a line between the text and the drive buttons*/
	if(videoMode == 0xd)
		line = screen;
	else
		line = backBuffer;
	line.left = text.left;
	line.right = text.right - 1;
	line.top = line.bottom = text.top - 2;
	if(videoMode == 0x13)
		color = 0x3d;
	else
		color = 0;
	DRV(0xc)(&line, color);
	/*a button for each drive (select the current disk: returns the number of drives)*/
	ndrives = bdos(0xe, bdos(0x19, 0, 0), 0) & 0xff;
	for(i = 0; i < ndrives && i < 11; i++) {
		buttons[i].left = i * 0x11 + 0x1f;
		buttons[i].right = i * 0x11 + 0x2e;
		DRV(3)(&bar, buttons[i].left, buttons[i].top, vellum, i + 5);
		keys[i] = i + 'A';
	}
	/*and Escape*/
	keys[i] = 0x1b;
	keys[i + 1] = 0;
	buttons[i].right = 0x124;
	buttons[i].left = 0xdd;
	DRV(3)(&bar, buttons[i].left, buttons[i].top, vellum, 4);
	SURF_share(&screen, &title, &text, &bar, 0);
	for(i = 0; i < 27; i++)
		SURF_share(&screen, &buttons[i], 0);
	if(videoMode == 0x13) {
		if(transferFrom == 0) {
			FADE_in(&textAreaCopy, &textArea);
		} else {
			DRV(0xf)(&textAreaCopy, &textArea);
		}
	} else if(videoMode == 0xd) {
		DRV(0x15)(&screen);
	} else {
		DRV(0xf)(&textAreaCopy, &textArea);
	}
	MOUSE_SHOW();
	found = 0;
	transferFrom = 0;
	while(!found) {
		key = keys[MenuSelect(buttons, keys)];
		if(key == 0x1b)
			return 0;
		u4PartyFile[0] = u5SaveFile[0] = u5SaveFile2[0] = u4PartyFile2[0] = key;
		if((saveData = LOAD_near(u4PartyFile)) != 0 || (saveData = LOAD_near(u4PartyFile2)) != 0)
			transferFrom = 4;
		else if((saveData = LOAD_near(u5SaveFile)) != 0 || (saveData = LOAD_near(u5SaveFile2)) != 0)
			transferFrom = 5;
		if(transferFrom) {
			found = 1;
		} else {
			putchar(7);
			MOUSE_HIDE();
			textX = text.left;
			textY = title.top + 0x20;
			text.bottom = 0xbd;
			if(videoMode == 0x13)
				color = 0x31;
			else if(videoMode == 9 || videoMode == 0xd)
				color = 0xe;
			else
				color = 3;
			DRV(0xc)(&text, color);
			TXT_sprintf("No character found!  If you wish to transfer a ");
			TXT_sappend("character from a hard disk it must be saved in a subdirectory called \"\\ULTIMA\".%P");
			while(!PollInput());
			break;
		}
	}
	MOUSE_HIDE();
	return transferFrom;
}

/*the main menu's "Transfer a character"*/
/*C_0263_13D6*/TransferCharacter() {
	struct tSurface save;
	int16 ret;

	PlayMusic(4);
	if(videoMode == 0xd) {
		backBuffer.segment = screen.segment ^ 0x200;
		DRV(0xf)(&backBuffer, &screen);
		backBuffer = screen;
	}
	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = 0xa600;
	AskForDisk(2);
	if(videoMode != 0xd)
		SURF_free(&workArea);
	nameWindow = backBuffer;
	dialogArea = screen;
	nameWindow.right = dialogArea.right = 0x124;
	nameWindow.left = dialogArea.left = 0x1c;
	nameWindow.top = dialogArea.top = 0x57;
	nameWindow.bottom = dialogArea.bottom = 0xbd;
	textAreaCopy = textArea;
	SURF_share(&backBuffer, &textAreaCopy, 0);
	DRV(0xc)(&textAreaCopy, 0);
	/*save the screen*/
	save.right = textArea.right - textArea.left;
	save.bottom = textArea.bottom - textArea.top;
	save.left = save.top = 0;
	SURF_alloc(&save);
	switch(videoMode) {
		case 4: vellum = LZW_load("vellum1.cga"); break;
		case 7: vellum = LZW_load("vellum1.hrc"); break;
		case 9: vellum = LZW_load("vellum1.tga"); break;
		case 0xd: vellum = LZW_load("vellum1.ega"); break;
		case 0x13: vellum = LZW_load("vellum1.shp"); break;
	}
	switch(videoMode) {
		case 4: portraits = LZW_load("montage.cga"); break;
		case 7: portraits = LZW_load("montage.hrc"); break;
		case 9: portraits = LZW_load("montage.tga"); break;
		case 0xd: portraits = LZW_load("montage.ega"); break;
		case 0x13: portraits = LZW_load("montage.shp"); break;
	}
	if(videoMode == 0xd)
		textArea.segment = screen.segment ^ 0x200;
	DRV(0xf)(&textArea, &save);
	if(videoMode == 0x13)
		FADE_out(&textArea, 0);
	if(videoMode == 0xd)
		textAreaCopy.segment = textArea.segment = screen.segment;
	DRV(0xc)(&textArea, 0);
	SetPalette();
	/*the vellum*/
	DRV(3)(&textAreaCopy, textAreaCopy.left, textAreaCopy.top, vellum, 0);
	transferFrom = portraitIdx = 0;
	ret = 0;
	while(FindCharacter()) {
		switch(transferFrom) {
			case 4:
				ReadU4Character();
				MakeFromU4();
				free(saveData);
				break;
			case 5:
				ReadU5Character();
				MakeFromU5();
				free(saveData);
				break;
		}
		ret = 0;
		if(videoMode == 0xd) {
			backBuffer.segment = screen.segment ^ 0x200;
			DRV(0xf)(&backBuffer, &screen);
			DRV(0x15)(&screen);
			screen.segment ^= 0x200;
			nameWindow.segment = dialogArea.segment = screen.segment;
		}
		/*the character, then the portrait: until it is chosen or given up*/
		do {
			if(!CharacterScreen())
				break;
			ret = PortraitScreen(1);
		} while(!ret);
		if(videoMode == 0xd)
			screen.segment ^= 0x200;
		if(ret)
			break;
		MOUSE_HIDE();
	}
	MOUSE_HIDE();
	if(ret) {
		workArea.left = workArea.top = 0;
		workArea.right = 0x13f;
		workArea.bottom = 0x1f;
		if(videoMode != 0xd)
			SURF_alloc(&workArea);
		else
			workArea.segment = 0xa600;
		AskForDisk(8);
		if(WriteNewCharacter()) {
			if(videoMode == 0xd)
				backBuffer = screen;
			DRV(0xc)(&backBuffer, 0);
			if(videoMode == 0xd) {
				DRV(0x15)(&screen);
				backBuffer = screen;
			} else {
				DRV(0xf)(&backBuffer, &screen);
			}
			menuDrawn = 0;
			DRV(0xc)(&backBuffer, 0);
			ShowPassage(998);
			DisplayPassage();
			ret = 0;
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
		if(videoMode != 0xd)
			SURF_free(&workArea);
	}
	farfree(portraits);
	farfree(vellum);
	/*put the screen back*/
	if(videoMode == 0xd) {
		textArea.segment = screen.segment;
		textArea.pScanlines = screen.pScanlines;
	}
	DRV(0xc)(&textArea, 0);
	SetPalette();
	if(menuDrawn) {
		if(videoMode == 0x13) {
			FADE_in(&save, &textArea);
		} else {
			DRV(0xf)(&save, &textArea);
		}
		if(videoMode == 0xd) {
			DRV(0x15)(&screen);
		}
	}
	SURF_free(&save);
	workArea.left = workArea.top = 0;
	workArea.right = 0x13f;
	workArea.bottom = 0x37;
	if(videoMode != 0xd)
		SURF_alloc(&workArea);
	else
		workArea.segment = 0xa600;
	AskForDisk(2);
	if(videoMode != 0xd)
		SURF_free(&workArea);
	PlayMusic(0);
	return ret;
}
