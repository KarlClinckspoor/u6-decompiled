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
	unsigned hp, hpMax;/*_00,_02*/
	int xp;/*_04*/
	int str, dex, intel;/*_06,_08,_0A*/
	int mp;/*_0C*/
	int unk0e, weapon, armor;/*_0E,_10,_12*/
	char name[16];/*_14*/
	char sex;/*_24*//*0x0b: male*/
	char klass;/*_25*/
	char status;/*_26*/
	char pad;/*_27*/
};
/*what follows the 8 characters in PARTY.SAV*/
struct tU4Rest {
	long food;/*_00*/
	int gold;/*_04*/
	int karma[8];/*_06*//*0: Avatarhood in that virtue*/
};
/*a character of Ultima V's SAVED.GAM*/
struct tU5Char {
	char name[9];/*_00*/
	char sex;/*_09*//*0x0b: male*/
	char klass;/*_0A*/
	char status;/*_0B*/
	unsigned char str, dex, intel, mp;/*_0C.._0F*/
	int hp, hpMax;/*_10,_12*/
	int xp;/*_14*/
	unsigned char level;/*_16*/
};

extern int D_5044;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern struct tSurface D_5710;/*the screen*/
extern struct tSurface D_5732;
extern struct tSurface D_565F;/*the console's surface*/
extern struct tSurface *D_566D;/*the console: surface it writes on*/
extern int D_566F;/*the console: x*/
extern int D_5671;/*the console: y*/
extern char D_5677, D_5678;/*the console: text colours*/
extern char D_567D;/*the console: alignment (0 left, 1 right, 2 centred)*/
extern struct tSurface D_5702;
extern int D_575A;
extern struct tSurface D_6072;

/*0263's initialised data*/
/*03CA*/char D_03CA[] = "a:\\party.sav";/*Ultima IV's save file; [0]: the drive letter*/
/*03D7*/char D_03D7[] = "a:\\saved.gam";/*Ultima V's*/
/*03E4*/char D_03E4[] = "a:\\ultima\\saved.gam";
/*03F8*/char D_03F8[] = "a:\\ultima\\party.sav";
/*040C*/char D_040C[][9] = {/*the names of the classes*/
	"Mage", "Bard", "Fighter", "Druid", "Tinker", "Paladin", "Ranger", "Shepherd",
	"Avatar"
};
/*045D*/char D_045D[] = "NSCA\x1b";/*keys of the character sheet's buttons: Name, Sex, Continue, Abort, Escape*/
/*0463*/char D_0463[] = "SPTA\x1b";/*keys of the portrait screen's: Sex, Portrait, Transfer (or Create), Abort, Escape*/

/*the character read from the old game*/
extern char D_5944[];/*name*/
extern int D_5954, D_5956, D_5958;/*strength, dexterity, intelligence*/
extern char D_595A[];/*"Male"/"Female"*/
extern char D_5962[];/*class*/
extern char D_5972;/*'G'*/
extern char D_5973;/*1: an Avatar*/
extern int D_5974;/*experience*/
extern int D_5976;/*level*/
extern int D_5978, D_597A;/*hit points, maximum*/
extern int D_597C;/*magic points*/

/*the character made for Ultima VI*/
extern char D_5A30[];/*name*/
extern int D_5A40, D_5A42, D_5A44;/*strength, dexterity, intelligence*/
extern char D_5A46[];/*"Male"/"Female"*/
extern char D_5A4E[];/*class ("Avatar")*/
extern char D_5A5E;/*'G'*/
extern char D_5A5F;
extern int D_5A60;/*experience*/
extern int D_5A62;/*level*/
extern int D_5A64, D_5A66;/*hit points, maximum*/
extern int D_5A68;/*magic points*/
extern int D_5A6A;/*portrait*/

extern int D_59B8;/*the game the character comes from: 4, 5 (0 none)*/
extern int D_5A2E;/*portrait (0-5)*/
extern char *D_5ADA;/*the save file read*/
extern void far *D_5AD2;/*the portraits (montage)*/
extern void far *D_5AD6;/*the vellum pictures*/
extern struct tSurface D_58FE[5];/*the buttons*/
extern struct tSurface D_5980;
extern struct tSurface D_599C;
extern struct tSurface D_59AA;/*the title*/
extern struct tSurface D_5A20;/*the character sheet*/

extern C_00E1_028A(char *, int);/*text input*/
extern C_00E1_041B(struct tSurface *, char *);/*waits for a button or key: index in the keys*/
extern C_00E1_04F9();
extern C_00E1_178A(int);
extern C_0406_0006();
extern C_06E9_0351();/*nonzero when the player wants to go on (key or time)*/
extern C_09A0_000B(int);
extern C_09A0_21C5();
extern C_0BC1_0079(int);/*AskForDisk*/
extern C_10E1_000A();
extern C_1277_000C(struct tSurface *);/*gives a surface an off-screen buffer*/
extern C_1277_00D8(struct tSurface *);/*frees it*/
extern C_12EA_000A(struct tSurface *, ...);
extern C_12EE_0229(char far *);/*console puts*/
extern C_12EE_0249(char far *, ...);/*console printf*/
extern C_12EE_0277(char far *, ...);/*console printf*/
extern C_1339_0000();/*mouse cursor on*/
extern C_1339_0016();/*mouse cursor off*/
extern C_136D_014D(struct tSurface *, struct tSurface *);/*MCGA: palette fade in*/
extern C_136D_0346(struct tSurface *, int);/*MCGA: palette fade out*/
extern void far *C_13BC_000C(char *);/*loads a file in memory*/
extern char *C_145A_0006(char *);/*loads a file in memory*/

/*reads the first character of Ultima IV's PARTY.SAV (in D_5ADA); nonzero if it looks wrong*/
/*C_0263_0000*/ReadU4Character() {
	struct tU4Rest *rest;
	struct tU4Char *p;
	int i;

	p = (struct tU4Char *)(D_5ADA + 8);
	rest = (struct tU4Rest *)(p + 8);
	for(i = 0; i < 8; i++) {
		D_5944[i] = p->name[i];
		D_5944[i + 1] = 0;
	}
	D_5954 = p->str;
	D_5956 = p->dex;
	D_5958 = p->intel;
	strcpy(D_595A, (p->sex == 0xb)?"Male":"Female");
	D_5972 = 'G';
	/*an Avatar if all eight virtues are complete*/
	D_5973 =
		rest->karma[0] == 0 && rest->karma[1] == 0 &&
		rest->karma[2] == 0 && rest->karma[3] == 0 &&
		rest->karma[4] == 0 && rest->karma[5] == 0 &&
		rest->karma[6] == 0 && rest->karma[7] == 0;
	if(D_5973)
		p->klass = 8;
	strcpy(D_5962, D_040C[p->klass]);
	D_5974 = p->xp;
	D_5976 = p->hpMax / 100;
	D_5978 = p->hp;
	D_597A = p->hpMax;
	D_597C = p->mp;
	return
		D_5954 > 99 || D_5956 > 99 || D_5958 > 99 ||
		D_5978 > 9999 || D_597A > 9999 || D_5974 > 9999 ||
		p->klass > 7;
}

/*reads the Avatar from Ultima V's SAVED.GAM (in D_5ADA); nonzero if it looks wrong*/
/*C_0263_013D*/ReadU5Character() {
	int i;
	struct tU5Char *p;

	p = (struct tU5Char *)(D_5ADA + 2);
	for(i = 0; i < 8; i++)
		D_5944[i] = p->name[i];
	D_5954 = p->str;
	D_5956 = p->dex;
	D_5958 = p->intel;
	strcpy(D_595A, (p->sex == 0xb)?"Male":"Female");
	D_5972 = 'G';
	D_5973 = 1;
	strcpy(D_5962, D_040C[8]);
	D_5974 = p->xp;
	D_5976 = p->level;
	D_5978 = p->hp;
	D_597A = p->hpMax;
	D_597C = p->mp;
	return
		D_5954 > 30 || D_5956 > 30 || D_5958 > 30 ||
		D_5978 > 9999 || D_597A > 9999 || D_5974 > 9999;
}

/*an Ultima IV attribute (up to 99) in Ultima VI's scale*/
/*C_0263_01FD*/ConvertU4Stat(int s) {
	if(s < 10)
		return s;
	if(s >= 30)
		return (s - 30) / 4 + 20;
	return (s - 9) / 2 + 10;
}

/*the Ultima VI character from the Ultima IV one*/
/*C_0263_0233*/MakeFromU4() {
	int i;

	strcpy(D_5A30, D_5944);
	D_5A40 = ConvertU4Stat(D_5954);
	D_5A42 = ConvertU4Stat(D_5956);
	D_5A44 = ConvertU4Stat(D_5958);
	strcpy(D_5A46, D_595A);
	strcpy(D_5A4E, D_040C[8]);
	D_5A5E = 'G';
	D_5A60 = D_5974 / 10;
	/*level: one more for each doubling of the experience above 1000*/
	D_5A62 = 1;
	for(i = D_5974 / 1000; i > 0; i >>= 1)
		D_5A62++;
	D_5A64 = D_5A66 = D_5A62 * 30;
	D_5A68 = D_5A44;
	D_5A5F = 1;
}

/*the Ultima VI character from the Ultima V one (same attribute scale)*/
/*C_0263_02D4*/MakeFromU5() {
	int i;

	strcpy(D_5A30, D_5944);
	D_5A40 = D_5954;
	D_5A42 = D_5956;
	D_5A44 = D_5958;
	strcpy(D_5A46, D_595A);
	strcpy(D_5A4E, D_040C[8]);
	D_5A5E = 'G';
	D_5A5F = 1;
	D_5A60 = D_5974 / 10;
	D_5A62 = 1;
	for(i = D_5974 / 1000; i > 0; i >>= 1)
		D_5A62++;
	D_5A64 = D_5A66 = D_5A62 * 30;
	D_5A68 = D_5A44;
}

/*the two columns of the character sheet (old and new character), and the buttons*/
/*C_0263_0360*/DrawSheet() {
	D_5A20.left += 0x35;
	switch(D_5044) {
		case 4: case 7:
			DRV(0xc)(&D_5A20, 3);
			D_5677 = 2; D_5678 = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&D_5A20, 0xe);
			D_5677 = 2; D_5678 = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&D_5A20, 0x31);
			D_5677 = 2; D_5678 = 0x31;
			break;
	}
	/*the old character*/
	D_566F = D_5A20.left;
	D_5671 = D_5A20.top + 2;
	D_567D = 0;
	C_12EE_0249("%s\n%s\n%s\n", D_5944, D_595A, D_5962);
	C_12EE_0277("%d\n%d\n%d\n%d\n%d\n%d%P", D_5954, D_5956, D_5958, D_597C, D_5974, D_5976);
	D_5A20.left += 0x20;
	D_5A20.left += 0x31;
	/*the new one*/
	if(D_5044 > 7)
		D_5677 = 9;
	else
		D_5677 = 0;
	D_566F = D_5A20.left;
	D_5671 = D_5A20.top + 2;
	C_12EE_0249("%s\n%s\n%s\n", D_5A30, D_5A46, D_5A4E);
	C_12EE_0277("%d\n%d\n%d\n%d\n%d\n%d%P", D_5A40, D_5A42, D_5A44, D_5A68, D_5A60, D_5A62);
	D_5A20.left -= 0x86;
	/*the buttons*/
	if(D_5044 == 0xd)
		D_599C.segment = D_5710.segment;
	DRV(3)(&D_599C, D_58FE[0].left, D_58FE[0].top, D_5AD6, 2);
	DRV(3)(&D_599C, D_58FE[1].left, D_58FE[1].top, D_5AD6, 3);
	DRV(3)(&D_599C, D_58FE[2].left, D_58FE[2].top, D_5AD6, 0x10);
	DRV(3)(&D_599C, D_58FE[3].left, D_58FE[3].top, D_5AD6, 4);
}

/*C_0263_055F*/ToggleSex() {
	switch(D_5A46[0]) {
		case 'M': strcpy(D_5A46, "Female"); break;
		case 'F': strcpy(D_5A46, "Male"); break;
	}
}

/*the character sheet: Name, Sex, Continue, Abort; 1 to go on, 0 to give up*/
/*C_0263_0592*/CharacterScreen() {
	int key;
	int bottom;
	int done;
	int ret;

	if(D_5044 == 0xd)
		D_599C.segment = D_5710.segment;
	C_1339_0016();
	D_59AA.right = D_5A20.right = 0x123;
	D_59AA.left = D_5A20.left = 0x1e;
	D_59AA.top = 0x57;
	D_59AA.bottom = 0x63;
	D_5A20.top = 0x63;
	D_5A20.bottom = 0xae;
	D_58FE[0].top = D_58FE[1].top = D_58FE[2].top = D_58FE[3].top = 0xae;
	D_58FE[0].bottom = D_58FE[1].bottom = D_58FE[2].bottom = D_58FE[3].bottom = 0xbd;
	D_58FE[0].left = 0x1c;
	D_58FE[1].left = 0x5f;
	D_58FE[2].left = 0x9a;
	D_58FE[3].left = 0xdd;
	D_58FE[0].right = 0x5d;
	D_58FE[1].right = 0x98;
	D_58FE[2].right = 0xdb;
	D_58FE[3].right = 0x124;
	D_58FE[4] = D_58FE[3];
	C_12EA_000A(&D_5732, &D_59AA, &D_5A20, &D_58FE[0], &D_58FE[1], &D_58FE[2], &D_58FE[3], &D_58FE[4], 0);
	bottom = D_5A20.bottom;
	if(D_5044 == 0xd) {
		D_59AA.segment = D_5A20.segment = D_5710.segment;
		D_5A20.pScanlines = D_5710.pScanlines;
	} else {
		D_5A20.segment = D_599C.segment;
		D_5A20.pScanlines = D_599C.pScanlines;
	}
	D_5A20.bottom = --D_5A20.top;
	switch(D_5044) {
		case 4: case 7:
			DRV(0xc)(&D_599C, 3);
			DRV(0xc)(&D_5A20, 3);
			D_5677 = 0; D_5678 = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&D_599C, 0xe);
			DRV(0xc)(&D_5A20, 0xe);
			D_5677 = 0; D_5678 = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&D_599C, 0x31);
			DRV(0xc)(&D_5A20, 0x3d);
			D_5677 = 0x3d; D_5678 = 0x31;
			break;
	}
	D_5A20.bottom = bottom;
	D_5A20.top++;
	/*the title*/
	D_566D = &D_59AA;
	D_566F = D_59AA.left + 0x35;
	D_5671 = D_59AA.top + 2;
	C_12EE_0229("Ultima ");
	if(D_59B8 == 4)
		C_12EE_0229("IV");
	else
		C_12EE_0229("V");
	D_566F = D_59AA.left + 0x86;
	C_12EE_0229("Ultima VI");
	/*the labels*/
	D_5A20.right -= 0xd6;
	D_566D = &D_5A20;
	D_566F = D_5A20.left;
	D_5671 = D_5A20.top + 2;
	D_567D = 1;
	C_12EE_0249("Name\nSex\nClass\n");
	C_12EE_0277("Str\nDex\nInt\nMagic\nExp\nLevel%P");
	D_5A20.right += 0xd6;
	DrawSheet();
	C_12EA_000A(&D_5710, &D_5980, &D_59AA, &D_5A20, &D_58FE[0], &D_58FE[1], &D_58FE[2], &D_58FE[3], &D_58FE[4], 0);
	if(D_5044 != 0xd) {
		DRV(0xf)(&D_599C, &D_5980);
	} else {
		D_5732 = D_5710;
	}
	C_1339_0000();
	done = 0;
	while(!done) {
		key = D_045D[C_00E1_041B(D_58FE, D_045D)];
		switch(key) {
			case 'C':
				done = 1;
				ret = 1;
				break;
			case 'N':
				/*enter a new name*/
				D_566D = &D_5A20;
				D_5677 = 0;
				D_566F = D_5A20.left + 0x86;
				D_5671 = D_5A20.top + 2;
				C_1339_0016();
				C_10E1_000A();
				C_00E1_028A(D_5A30, 0xd);
				C_12EA_000A(&D_5732, &D_5A20, 0);
				if(D_5044 == 0xd)
					D_5A20.segment = D_5710.segment;
				DrawSheet();
				C_12EA_000A(&D_5710, &D_5A20, 0);
				if(D_5044 != 0xd) {
					DRV(0xf)(&D_599C, &D_5980);
				}
				C_1339_0000();
				break;
			case 'S':
				C_1339_0016();
				ToggleSex();
				C_12EA_000A(&D_5732, &D_5A20, 0);
				if(D_5044 == 0xd)
					D_5A20.segment = D_5710.segment;
				DrawSheet();
				C_12EA_000A(&D_5710, &D_5A20, 0);
				if(D_5044 != 0xd) {
					DRV(0xf)(&D_599C, &D_5980);
				}
				C_1339_0000();
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
/*C_0263_0987*/PortraitScreen(int transfer) {
	int key;
	int frame;
	int bottom;
	struct tSurface portrait;
	int done;
	int ret;

	portrait = D_599C;
	/*the last button: Transfer or Create*/
	if(transfer) {
		D_0463[2] = 'T';
		frame = 1;
	} else {
		D_0463[2] = 'C';
		frame = 0x10;
	}
	if(D_5044 == 0xd) {
		D_599C.segment = portrait.segment = D_5710.segment;
		C_1339_0016();
	}
	portrait.right = 0x102;
	portrait.left = 0xc3;
	portrait.top = 0x76;
	portrait.bottom = 0xad;
	D_59AA.right = D_5A20.right = 0x123;
	D_59AA.left = D_5A20.left = 0x1e;
	D_59AA.top = 0x57;
	D_59AA.bottom = 0x63;
	D_5A20.top = 0x63;
	D_5A20.bottom = 0xae;
	D_58FE[0].right = D_58FE[1].right = D_58FE[2].right = D_58FE[3].right = 0x97;
	D_58FE[0].left = 0x5e;
	D_58FE[1].left = 0x3e;
	D_58FE[2].left = 0x56;
	D_58FE[3].left = 0x50;
	D_58FE[0].top = 0x70;
	D_58FE[1].top = 0x81;
	D_58FE[2].top = 0x92;
	D_58FE[3].top = 0xa3;
	D_58FE[0].bottom = 0x7f;
	D_58FE[1].bottom = 0x90;
	D_58FE[2].bottom = 0xa1;
	D_58FE[3].bottom = 0xb2;
	D_58FE[4] = D_58FE[3];
	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	C_12EA_000A(&D_5732, &D_59AA, &D_5A20, &D_58FE[0], &D_58FE[1], &D_58FE[2], &D_58FE[3], &D_58FE[4], 0);
	switch(D_5044) {
		case 4: case 7: DRV(0xc)(&D_599C, 3); break;
		case 9: case 0xd: DRV(0xc)(&D_599C, 0xe); break;
		case 0x13: DRV(0xc)(&D_599C, 0x31); break;
	}
	bottom = D_5A20.bottom;
	if(D_5044 == 0xd) {
		D_5A20.segment = D_5710.segment;
		D_5A20.pScanlines = D_5710.pScanlines;
	} else {
		D_5A20.segment = D_599C.segment;
		D_5A20.pScanlines = D_599C.pScanlines;
	}
	D_5A20.bottom = --D_5A20.top;
	switch(D_5044) {
		case 4: case 7:
			DRV(0xc)(&D_5A20, 3);
			D_5677 = 0; D_5678 = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&D_5A20, 0xe);
			D_5677 = 0; D_5678 = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&D_5A20, 0x3d);
			D_5677 = 0x3d; D_5678 = 0x31;
			break;
	}
	D_5A20.bottom = bottom;
	D_5A20.top++;
	/*the name as title*/
	D_566D = &D_59AA;
	D_566F = D_59AA.left;
	D_5671 = D_59AA.top + 2;
	D_567D = 2;
	C_12EE_0249("%s%P", D_5A30);
	/*the buttons*/
	DRV(3)(&D_599C, D_58FE[0].left, D_58FE[0].top, D_5AD6, 3);
	DRV(3)(&D_599C, D_58FE[1].left, D_58FE[1].top, D_5AD6, 0x12);
	DRV(3)(&D_599C, D_58FE[2].left, D_58FE[2].top, D_5AD6, frame);
	DRV(3)(&D_599C, D_58FE[3].left, D_58FE[3].top, D_5AD6, 4);
	C_12EA_000A(&D_5710, &D_5980, &D_59AA, &D_5A20, &D_58FE[0], &D_58FE[1], &D_58FE[2], &D_58FE[3], &D_58FE[4], 0);
	done = 0;
	while(!done) {
		/*the portrait*/
		C_1339_0016();
		switch(D_5044) {
			case 4: case 7: DRV(0xc)(&portrait, 3); break;
			case 9: case 0xd: DRV(0xc)(&portrait, 0xe); break;
			case 0x13: DRV(0xc)(&portrait, 0x31); break;
		}
		D_5A6A = ((D_5A46[0] == 'F')?0:6) + D_5A2E;
		DRV(3)(&portrait, portrait.left, portrait.top, D_5AD2, D_5A6A);
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_599C, &D_5980);
		}
		C_1339_0000();
		key = D_0463[C_00E1_041B(D_58FE, D_0463)];
		switch(key) {
			case 'S':
				ToggleSex();
				break;
			case 'P':
				D_5A2E = (D_5A2E + 1) % 6;
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
	int found;
	int ndrives;
	int key;
	struct tSurface title;
	struct tSurface text;
	struct tSurface bar;
	struct tSurface buttons[27];
	struct tSurface line;
	char keys[32];
	int i;
	int color;

	if(D_5044 == 0xd)
		D_5732.segment = D_5710.segment;
	for(i = 0; i < 27; i++) {
		C_12EA_000A(&D_5732, &buttons[i], 0);
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
	C_12EA_000A(&D_5732, &title, &text, &bar, 0);
	switch(D_5044) {
		case 4: case 7:
			DRV(0xc)(&D_599C, 3);
			D_5677 = 0; D_5678 = 3;
			break;
		case 9: case 0xd:
			DRV(0xc)(&D_599C, 0xe);
			D_5677 = 0; D_5678 = 0xe;
			break;
		case 0x13:
			DRV(0xc)(&D_599C, 0x31);
			D_5677 = 0x3d; D_5678 = 0x31;
			break;
	}
	D_566D = &title;
	D_566F = title.left;
	D_5671 = title.top;
	D_567D = 2;
	C_12EE_0249("TRANSFER A CHARACTER%P");
	if(D_5044 == 0xd) {
		text.segment = D_5710.segment;
		text.pScanlines = D_5710.pScanlines;
	}
	D_566D = &text;
	D_566F = text.left;
	D_5671 = title.top + 0x20;
	if(D_5044 == 0x13)
		color = 0x31;
	else if(D_5044 == 9 || D_5044 == 0xd)
		color = 0xe;
	else
		color = 3;
	DRV(0xc)(&text, color);
	C_12EE_0249("Enter the letter of the\nfloppy/hard drive containing your\n");
	C_12EE_0277("Ultima V Britannia\nor Ultima IV Player disk....%P");
	/*a line between the text and the drive buttons*/
	if(D_5044 == 0xd)
		line = D_5710;
	else
		line = D_5732;
	line.left = text.left;
	line.right = text.right - 1;
	line.top = line.bottom = text.top - 2;
	if(D_5044 == 0x13)
		color = 0x3d;
	else
		color = 0;
	DRV(0xc)(&line, color);
	/*a button for each drive (select the current disk: returns the number of drives)*/
	ndrives = bdos(0xe, bdos(0x19, 0, 0), 0) & 0xff;
	for(i = 0; i < ndrives && i < 11; i++) {
		buttons[i].left = i * 0x11 + 0x1f;
		buttons[i].right = i * 0x11 + 0x2e;
		DRV(3)(&bar, buttons[i].left, buttons[i].top, D_5AD6, i + 5);
		keys[i] = i + 'A';
	}
	/*and Escape*/
	keys[i] = 0x1b;
	keys[i + 1] = 0;
	buttons[i].right = 0x124;
	buttons[i].left = 0xdd;
	DRV(3)(&bar, buttons[i].left, buttons[i].top, D_5AD6, 4);
	C_12EA_000A(&D_5710, &title, &text, &bar, 0);
	for(i = 0; i < 27; i++)
		C_12EA_000A(&D_5710, &buttons[i], 0);
	if(D_5044 == 0x13) {
		if(D_59B8 == 0) {
			C_136D_014D(&D_5702, &D_565F);
		} else {
			DRV(0xf)(&D_5702, &D_565F);
		}
	} else if(D_5044 == 0xd) {
		DRV(0x15)(&D_5710);
	} else {
		DRV(0xf)(&D_5702, &D_565F);
	}
	C_1339_0000();
	found = 0;
	D_59B8 = 0;
	while(!found) {
		key = keys[C_00E1_041B(buttons, keys)];
		if(key == 0x1b)
			return 0;
		D_03CA[0] = D_03D7[0] = D_03E4[0] = D_03F8[0] = key;
		if((D_5ADA = C_145A_0006(D_03CA)) != 0 || (D_5ADA = C_145A_0006(D_03F8)) != 0)
			D_59B8 = 4;
		else if((D_5ADA = C_145A_0006(D_03D7)) != 0 || (D_5ADA = C_145A_0006(D_03E4)) != 0)
			D_59B8 = 5;
		if(D_59B8) {
			found = 1;
		} else {
			putchar(7);
			C_1339_0016();
			D_566F = text.left;
			D_5671 = title.top + 0x20;
			text.bottom = 0xbd;
			if(D_5044 == 0x13)
				color = 0x31;
			else if(D_5044 == 9 || D_5044 == 0xd)
				color = 0xe;
			else
				color = 3;
			DRV(0xc)(&text, color);
			C_12EE_0249("No character found!  If you wish to transfer a ");
			C_12EE_0277("character from a hard disk it must be saved in a subdirectory called \"\\ULTIMA\".%P");
			while(!C_06E9_0351());
			break;
		}
	}
	C_1339_0016();
	return D_59B8;
}

/*the main menu's "Transfer a character"*/
/*C_0263_13D6*/TransferCharacter() {
	struct tSurface save;
	int ret;

	C_00E1_178A(4);
	if(D_5044 == 0xd) {
		D_5732.segment = D_5710.segment ^ 0x200;
		DRV(0xf)(&D_5732, &D_5710);
		D_5732 = D_5710;
	}
	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.segment = 0xa600;
	C_0BC1_0079(2);
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	D_599C = D_5732;
	D_5980 = D_5710;
	D_599C.right = D_5980.right = 0x124;
	D_599C.left = D_5980.left = 0x1c;
	D_599C.top = D_5980.top = 0x57;
	D_599C.bottom = D_5980.bottom = 0xbd;
	D_5702 = D_565F;
	C_12EA_000A(&D_5732, &D_5702, 0);
	DRV(0xc)(&D_5702, 0);
	/*save the screen*/
	save.right = D_565F.right - D_565F.left;
	save.bottom = D_565F.bottom - D_565F.top;
	save.left = save.top = 0;
	C_1277_000C(&save);
	switch(D_5044) {
		case 4: D_5AD6 = C_13BC_000C("vellum1.cga"); break;
		case 7: D_5AD6 = C_13BC_000C("vellum1.hrc"); break;
		case 9: D_5AD6 = C_13BC_000C("vellum1.tga"); break;
		case 0xd: D_5AD6 = C_13BC_000C("vellum1.ega"); break;
		case 0x13: D_5AD6 = C_13BC_000C("vellum1.shp"); break;
	}
	switch(D_5044) {
		case 4: D_5AD2 = C_13BC_000C("montage.cga"); break;
		case 7: D_5AD2 = C_13BC_000C("montage.hrc"); break;
		case 9: D_5AD2 = C_13BC_000C("montage.tga"); break;
		case 0xd: D_5AD2 = C_13BC_000C("montage.ega"); break;
		case 0x13: D_5AD2 = C_13BC_000C("montage.shp"); break;
	}
	if(D_5044 == 0xd)
		D_565F.segment = D_5710.segment ^ 0x200;
	DRV(0xf)(&D_565F, &save);
	if(D_5044 == 0x13)
		C_136D_0346(&D_565F, 0);
	if(D_5044 == 0xd)
		D_5702.segment = D_565F.segment = D_5710.segment;
	DRV(0xc)(&D_565F, 0);
	C_00E1_04F9();
	/*the vellum*/
	DRV(3)(&D_5702, D_5702.left, D_5702.top, D_5AD6, 0);
	D_59B8 = D_5A2E = 0;
	ret = 0;
	while(FindCharacter()) {
		switch(D_59B8) {
			case 4:
				ReadU4Character();
				MakeFromU4();
				free(D_5ADA);
				break;
			case 5:
				ReadU5Character();
				MakeFromU5();
				free(D_5ADA);
				break;
		}
		ret = 0;
		if(D_5044 == 0xd) {
			D_5732.segment = D_5710.segment ^ 0x200;
			DRV(0xf)(&D_5732, &D_5710);
			DRV(0x15)(&D_5710);
			D_5710.segment ^= 0x200;
			D_599C.segment = D_5980.segment = D_5710.segment;
		}
		/*the character, then the portrait: until it is chosen or given up*/
		do {
			if(!CharacterScreen())
				break;
			ret = PortraitScreen(1);
		} while(!ret);
		if(D_5044 == 0xd)
			D_5710.segment ^= 0x200;
		if(ret)
			break;
		C_1339_0016();
	}
	C_1339_0016();
	if(ret) {
		D_6072.left = D_6072.top = 0;
		D_6072.right = 0x13f;
		D_6072.bottom = 0x1f;
		if(D_5044 != 0xd)
			C_1277_000C(&D_6072);
		else
			D_6072.segment = 0xa600;
		C_0BC1_0079(8);
		if(C_0406_0006()) {
			if(D_5044 == 0xd)
				D_5732 = D_5710;
			DRV(0xc)(&D_5732, 0);
			if(D_5044 == 0xd) {
				DRV(0x15)(&D_5710);
				D_5732 = D_5710;
			} else {
				DRV(0xf)(&D_5732, &D_5710);
			}
			D_575A = 0;
			DRV(0xc)(&D_5732, 0);
			C_09A0_000B(998);
			C_09A0_21C5();
			ret = 0;
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
		if(D_5044 != 0xd)
			C_1277_00D8(&D_6072);
	}
	farfree(D_5AD2);
	farfree(D_5AD6);
	/*put the screen back*/
	if(D_5044 == 0xd) {
		D_565F.segment = D_5710.segment;
		D_565F.pScanlines = D_5710.pScanlines;
	}
	DRV(0xc)(&D_565F, 0);
	C_00E1_04F9();
	if(D_575A) {
		if(D_5044 == 0x13) {
			C_136D_014D(&save, &D_565F);
		} else {
			DRV(0xf)(&save, &D_565F);
		}
		if(D_5044 == 0xd) {
			DRV(0x15)(&D_5710);
		}
	}
	C_1277_00D8(&save);
	D_6072.left = D_6072.top = 0;
	D_6072.right = 0x13f;
	D_6072.bottom = 0x37;
	if(D_5044 != 0xd)
		C_1277_000C(&D_6072);
	else
		D_6072.segment = 0xa600;
	C_0BC1_0079(2);
	if(D_5044 != 0xd)
		C_1277_00D8(&D_6072);
	C_00E1_178A(0);
	return ret;
}
