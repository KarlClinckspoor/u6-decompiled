/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 09A0: the passages of text of the introduction, the gypsy and the
 * end of character creation, printed in a text window under the picture
 */
#include "u.h"
#include <conio.h>

/*a 14-byte graphics area (see disk.c); the first word is the only one used here*/
/*a text window of the console library (C_132B_0006 makes one current)*/
struct tWin {
	struct tSurface *area;/*+00 where it is drawn*/
	int curX, curY;/*+02,+04 cursor*/
	int f06, f08;
	unsigned char bgColor;/*+0a*/
	unsigned char fgColor;/*+0b*/
	char *buf;/*+0c text buffer*/
	char *ptr;/*+0e write position in buf*/
	char f10;/*+10 3 at first, 2 for a passage that waits for a key (?)*/
};

extern int D_5044;/*video mode: 0x0d EGA, 0x13 MCGA*/
extern struct tWin D_566D;/*the console's normal text window*/
extern struct tSurface D_5710;/*the screen*/
extern struct tSurface D_5732;/*the drawing area (EGA: copy of the screen's)*/
extern struct tSurface D_6072;/*the text area of a passage*/
extern int D_6080;
extern int D_6082, D_6084;/*where the text area is on the screen: x (in 8-pixel columns?), y*/
extern struct tSurface D_6086;/*copy of D_5732 the text area is shown in*/
extern int D_58F2, D_58F4;
extern int D_404C;/*the disk asked for (disk.c)*/
extern char *D_4038[];/*names of the disks*/
extern char D_5A30[];/*the player's name*/

extern C_132B_0006(struct tWin *);/*make a text window current*/
extern C_12EE_0277(char far *, ...);/*printf in the current text window*/
extern C_128D_000C(char far *);/*print a text (word wrapped?) in the current window*/

#define QUOTE_END 0x7f/*the font's closing quotation mark, it seems*/

/*prints passage n (0-15 the introduction, 100-137 the gypsy, 200-219 the arrival in
Britannia, 998/999 messages) in the text area D_6072, after setting where it goes on the
screen; DisplayPassage puts it there*/
/*C_09A0_000B*/ShowPassage(int n) {
	struct tWin win;
	int color;

	win = D_566D;
	/*flush the keyboard*/
	while(kbhit())
		getch();
	D_6086 = D_5732;
	win.area = &D_6072;
	D_6072.top = D_6072.left = win.curX = win.curY = 0;
	win.f10 = 3;
	switch(D_5044) {
	case 4: case 7:
		color = 3;
		win.bgColor = 0;
		break;
	case 9: case 0xd:
		color = 0xe;
		win.bgColor = 0;
		break;
	case 0x13:
		color = 0x31;
		win.bgColor = 0x3e;
		break;
	}
	win.fgColor = color;
	/*print in a copy of the normal window, drawn in D_6072*/
	C_132B_0006(&win);
	win.ptr = win.buf;
	/*each passage: where its text area is, then it is cleared and the text printed*/
	switch(n) {
	case 0:
		D_6082 = 1;
		D_6084 = 159;
		D_6086.left = 8;
		D_6086.top = 169;
		D_6080 = 1;
		D_6072.right = 304;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Upon your world, five seasons have passed since your ");
		C_12EE_0277("triumphant homecoming from Britannia.");
		win.f10 = 2;
		break;
	case 1:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("You have traded the Avatar's life of peril and adventure ");
		C_12EE_0277("for the lonely serenity of a world at peace. But ");
		C_12EE_0277("television supermen cannot take the place of friends ");
		C_12EE_0277("who died at your side!");
		break;
	case 2:
		D_6082 = 33;
		D_6084 = 157;
		D_6086.left = 40;
		D_6086.top = 165;
		D_6080 = 0;
		D_6072.right = 234;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Outside, a chill wind rises...");
		win.f10 = 2;
		break;
	case 3:
		D_6082 = 0;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 166;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("...and in moments, the storm is upon you.");
		win.f10 = 2;
		break;
	case 4:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 162;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Tongues of lightning lash the sky, conducting an unceasing ");
		C_12EE_0277("crescendo of thunder....");
		break;
	case 5:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 162;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("In a cataclysm of sound and light, a bolt of searing ");
		C_12EE_0277("blue fire strikes the earth!");
		break;
	case 6:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 162;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Lightning among the stones!\nIs this a sign from ");
		C_12EE_0277("distant Britannia?");
		win.f10 = 2;
		break;
	case 8:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("You bolt from your house, stumbling, running blind in the");
		C_12EE_0277(" storm. Into the forest, down the path, through the ");
		C_12EE_0277("rain... to the stones.");
		break;
	case 9:
		D_6082 = 1;
		D_6084 = 12;
		D_6086.left = 8;
		D_6086.top = 20;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Near the stones, the smell of damp, blasted earth hangs ");
		C_12EE_0277("in the air. In a frozen moment of lightning-");
		C_12EE_0277("struck daylight, you glimpse a tiny obsidian stone in the ");
		C_12EE_0277("midst of the circle!");
		break;
	case 10:
		D_6082 = 33;
		D_6084 = 30;
		D_6086.left = 40;
		D_6086.top = 38;
		D_6080 = 0;
		D_6072.right = 234;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Wondering, you pick it up....");
		win.f10 = 2;
		break;
	case 11:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("...and from the heart of the stones, a softly glowing door ");
		C_12EE_0277("ascends in silence!");
		break;
	case 12:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Exultant memories wash over you as you clutch the stone. ");
		C_12EE_0277("When last you saw an orb such as this, it was cast down ");
		C_12EE_0277("by Lord British to banish the tyrant Blackthorn!");
		break;
	case 13:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("But your joy soon gives way to apprehension.\n\n");
		C_12EE_0277("The gate to Britannia has always been blue...\nas ");
		C_12EE_0277("blue as the morning sky.");
		win.f10 = 2;
		break;
	case 14:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Abruptly, the portal quivers and begins to sink ");
		C_12EE_0277("into the ground.  Its crimson light wanes!");
		break;
	case 15:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 174;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Desperation makes the decision an easy one.");
		win.f10 = 2;
		break;
	case 100:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 174;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Welcome, O Seeker!%c", QUOTE_END);
		win.f10 = 2;
		break;
	case 101:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("A lonely stroll along an unfamiliar forest path brings you ");
		C_12EE_0277("upon a curious gypsy wagon, its exotic colors dappled in the ");
		C_12EE_0277("summer shade.");
		break;
	case 102:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("A woman's voice rings out with friendship, beckoning you into ");
		C_12EE_0277("across the wagon's threshold and, as it happens, ");
		C_12EE_0277("into another life....");
		break;
	case 103:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 143;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 39;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"At last thou hast come to fulfill thy destiny,%c ", QUOTE_END);
		C_12EE_0277("the gypsy says. She smiles, as if in great relief.\n\n");
		C_12EE_0277("\"Sit before me now, and I shall pour the light of Virtue ");
		C_12EE_0277("into the shadows of thy future.%c", QUOTE_END);
		break;
	case 104:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 143;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 39;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("On a wooden table eight bottles stand, a rainbow of bubbling ");
		C_12EE_0277("liquids.\n\n");
		C_12EE_0277("\"Behold the Virtues of the Avatar,%c the woman ", QUOTE_END);
		C_12EE_0277("says. \"Let us begin the casting!%c", QUOTE_END);
		break;
	case 105:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Entrusted to deliver an uncounted purse of gold, thou dost ");
		C_12EE_0277("meet a poor beggar. Dost thou A) deliver the gold Honestly, ");
		C_12EE_0277("knowing the trust in thee was well-placed; or B) show Compassion and ");
		C_12EE_0277("give the beggar a coin, knowing it won't be missed?%c", QUOTE_END);
		break;
	case 106:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou has been prohibited by thy absent Lord from joining ");
		C_12EE_0277("thy friends in a close-pitched battle. Dost thou A) refrain, ");
		C_12EE_0277("so thou may Honestly claim obedience; or B) show Valor and aid ");
		C_12EE_0277("thy comrades, knowing thou may deny it later?%c", QUOTE_END);
		break;
	case 107:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"A merchant owes thy friend money, now long past due. Thou ");
		C_12EE_0277("dost see the same merchant drop a purse of gold. Dost thou ");
		C_12EE_0277("A) Honestly return the purse intact; or B) Justly give thy ");
		C_12EE_0277("friend a portion of the gold first?%c", QUOTE_END);
		break;
	case 108:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thee and thy friend are valiant but penniless warriors. Thou ");
		C_12EE_0277("both set forth to slay a mighty dragon. Thy friend dost ");
		C_12EE_0277("think he slew it, but the killing blow was thine. Dost thou ");
		C_12EE_0277("A) Honestly claim the reward; or B) Sacrifice the gold for the ");
		C_12EE_0277("sake of thy friendship?%c", QUOTE_END);
		break;
	case 109:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou art sworn to protect thy Lord at any cost, yet thou knowest ");
		C_12EE_0277("he hath committed a crime. Authorities ask thee of the affair. ");
		C_12EE_0277("Dost thou A) Break thine oath by Honestly speaking; or B) uphold ");
		C_12EE_0277("Honor by silently keeping thine oath?%c", QUOTE_END);
		break;
	case 110:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thy friend doth seek admittance to thy spiritual order. Thou ");
		C_12EE_0277("art asked to vouch for his purity of spirit, of which thou art ");
		C_12EE_0277("uncertain. Dost thou A) Honestly express thy doubt; or B) Vouch ");
		C_12EE_0277("for him, hoping for his Spiritual improvement?%c", QUOTE_END);
		break;
	case 111:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thy Lord doth mistakenly believe he slew a dragon. Thou hast ");
		C_12EE_0277("proof that thy lance felled the beast. When asked, dost thou ");
		C_12EE_0277("A) Honestly claim the kill; or B) Humbly permit thy Lord his ");
		C_12EE_0277("belief?%c", QUOTE_END);
		break;
	case 112:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou dost manage to disarm thy mortal enemy in a duel. He is ");
		C_12EE_0277("at thy mercy. Dost thou A) Show Compassion by permitting him to ");
		C_12EE_0277("yield; or B) Slay him, as expected of a Valiant duelist?%c", QUOTE_END);
		break;
	case 113:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"After twenty years thou hast found the slayer of thy best ");
		C_12EE_0277("friends. The villain proves to be a man who provides the sole ");
		C_12EE_0277("support for a young girl. Dost thou A) Spare him in Compassion ");
		C_12EE_0277("for the child; or B) Slay him in the name of Justice?%c", QUOTE_END);
		break;
	case 114:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thee and thy friends hath been routed and ordered to retreat. ");
		C_12EE_0277("In defiance of thy orders, dost thou A) Stop in Compassion to aid ");
		C_12EE_0277("a wounded comrade; or B) Sacrifice thyself to slow the pursuing ");
		C_12EE_0277("enemy so that others might escape?%c", QUOTE_END);
		break;
	case 115:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou art sworn to uphold a Lord who doth participate in the ");
		C_12EE_0277("forbidden torture of prisoners. Each night their cries of pain ");
		C_12EE_0277("reach thee. Dost thou A) Show Compassion by reporting the deeds; ");
		C_12EE_0277("or B) Honor thy oath and ignore the deeds?%c", QUOTE_END);
		break;
	case 116:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou hast been taught to preserve all life as sacred. A man ");
		C_12EE_0277("lies fatally stung by a venomous serpent. He prays for a ");
		C_12EE_0277("merciful death. Dost thou A) Show Compassion and end his pain; ");
		C_12EE_0277("or B) heed thy Spiritual beliefs and turn away?%c", QUOTE_END);
		break;
	case 117:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"The Captain of the King's Guard asks that one among thee ");
		C_12EE_0277("visit a hospital to cheer the children with tales of valiant personal ");
		C_12EE_0277("deeds. Dost thou A) Show thy Compassion and play the braggart; ");
		C_12EE_0277("or B) Humbly let another go?%c", QUOTE_END);
		break;
	case 118:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou hast been sent to secure a needed treaty with a distant ");
		C_12EE_0277("Lord. Thy host is agreeable to thy proposal but insults thy ");
		C_12EE_0277("country at dinner. Dost thou A) Valiantly bear the slurs; or ");
		C_12EE_0277("B) Justly rise and demand an apology?%c", QUOTE_END);
		break;
	case 119:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"A burly knight accosts thee and demands thy food. Dost thou ");
		C_12EE_0277("A) Valiantly refuse and engage the knight, or B) Sacrifice ");
		C_12EE_0277("thy rations unto the hungry knight?%c", QUOTE_END);
		break;
	case 120:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"During battle thou art ordered to guard thy commander's empty ");
		C_12EE_0277("tent. The battle goes poorly and thou dost yearn to aid thy ");
		C_12EE_0277("fellows. Dost thou A) Valiantly enter the battle; ");
		C_12EE_0277("or B) Honor thy post as guard?%c", QUOTE_END);
		break;
	case 121:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"A local bully pushes for a fight. Dost thou A) Valiantly ");
		C_12EE_0277("trounce the rogue; or B) Decline, knowing in thy Spirit that ");
		C_12EE_0277("no lasting good will come of it?%c", QUOTE_END);
		break;
	case 122:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Although a simple fisherman, thou art also a skillful swordsman. Thy ");
		C_12EE_0277("Lord dost seek to assemble a peacetime guard. Dost thou A) Answer ");
		C_12EE_0277("the call, so that all may witness thy Valor; or B) Humbly decline the ");
		C_12EE_0277("offer to join thy Lord's largely ceremonial knighthood?%c", QUOTE_END);
		break;
	case 123:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"During a pitched battle, thou dost see a fellow desert his ");
		C_12EE_0277("post, endangering many. As he flees, he is set upon by several enemies.");
		C_12EE_0277(" Dost thou A) Justly let him fight alone; or B) Risk the Sacrifice ");
		C_12EE_0277("of thine own life to aid him?%c", QUOTE_END);
		break;
	case 124:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou hast sworn to do thy Lord's bidding in all. He covets ");
		C_12EE_0277("a piece of land and orders its owner removed. Dost thou A) Serve ");
		C_12EE_0277("Justice, refusing to act, thus being disgraced; or B) Honor thine ");
		C_12EE_0277("oath and evict the landowner?%c", QUOTE_END);
		break;
	case 125:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou dost believe that virtue resides in all people. Thou dost see ");
		C_12EE_0277("a rogue steal from thy Lord. Dost thou A) Call him to Justice; or B) ");
		C_12EE_0277("Personally try to sway him back to the Spiritual path of good?%c", QUOTE_END);
		break;
	case 126:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Unwitnessed, thou hast slain a mighty dragon in defense ");
		C_12EE_0277("of thy life. A poor warrior claims the offered reward. ");
		C_12EE_0277("Dost thou A) Justly step forward to claim the bounty; ");
		C_12EE_0277("or B) Humbly go about thy life, secure in thy self-esteem?%c", QUOTE_END);
		break;
	case 127:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou art a bounty hunter sworn to return an alleged murderer. ");
		C_12EE_0277("After his capture, thou dost come to believe him innocent. Dost ");
		C_12EE_0277("thou A) Sacrifice thy sizable bounty for thy belief; or B) Honor ");
		C_12EE_0277("thine oath to return him as promised?%c", QUOTE_END);
		break;
	case 128:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou hast spent thy life in charitable and righteous work. Thine ");
		C_12EE_0277("uncle the innkeeper lies ill and asks thee to take over his tavern. ");
		C_12EE_0277("Dost thou A) Sacrifice thy life of purity to aid thy kin; or B) ");
		C_12EE_0277("Decline and follow the call of Spirituality?%c", QUOTE_END);
		break;
	case 129:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thou art an elderly, wealthy merchant. Thy end is near. Dost thou ");
		C_12EE_0277("A) Sacrifice all thy wealth to feed hundreds of starving children, ");
		C_12EE_0277("receiving public adulation; or B) Humbly live out thy life, willing ");
		C_12EE_0277("thy fortune to thy heirs?%c", QUOTE_END);
		break;
	case 130:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"In thy youth thou didst pledge to marry thy sweetheart. Now ");
		C_12EE_0277("thou art on a sacred quest in distant lands. Thy sweetheart ");
		C_12EE_0277("doth ask thee to keep thy vow. Dost thou A) Honor thy pledge to ");
		C_12EE_0277("wed; or B) Follow thy Spiritual crusade?%c", QUOTE_END);
		break;
	case 131:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Though thou art but a peasant shepherd, thou art discovered to be the ");
		C_12EE_0277("sole descendant of a noble family long thought extinct. ");
		C_12EE_0277("Dost thou A) Honorably take up the arms of thy ancestors; or ");
		C_12EE_0277("B) Humbly resume thy life of simplicity and peace?%c", QUOTE_END);
		break;
	case 132:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 133;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 55;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Thy parents wish thee to become an apprentice. Two positions ");
		C_12EE_0277("are available. Dost thou A) Become an acolyte in a worthy Spiritual ");
		C_12EE_0277("order; or B) Become an assistant to a humble village cobbler?%c", QUOTE_END);
		break;
	case 133:
		D_6082 = 1;
		D_6084 = 124;
		D_6086.left = 8;
		D_6086.top = 140;
		D_6080 = 3;
		D_6072.right = 303;
		D_6072.bottom = 47;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"The path of the Avatar lies beneath thy feet, worthy %s,%c ", D_5A30, QUOTE_END);
		C_12EE_0277("the gypsy intones. With a mysterious smile, she passes you the flask of shimmering liquids. ");
		C_12EE_0277("\"Drink of these waters and go forth among our people, who ");
		C_12EE_0277("shall receive thee in joy!%c", QUOTE_END);
		break;
	case 134:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("As you drink from the flask, vertigo overwhelms you. ");
		C_12EE_0277("A soothing mist obscures the gypsy's face, and you sink ");
		C_12EE_0277("without fear into an untroubled sleep.");
		break;
	case 135:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("You wake in a different time, upon another world's shore. ");
		C_12EE_0277("Though the Avatar's quests bring you both triumph and tragedy, ");
		C_12EE_0277("never do you stray from the path of the Eight Virtues.");
		break;
	case 136:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("The sagas of Ultima IV and Ultima V chronicle your perilous ");
		C_12EE_0277("travels, and your name and your deeds are written forever among ");
		C_12EE_0277("Britannia's legends....");
		break;
	case 137:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Finally, tempered by your struggles against the enemies of Virtue, you ");
		C_12EE_0277("are proven ready to answer the epic challenge of Ultima VI!");
		break;
	case 200:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Dazed, you emerge from the portal to find yourself ");
		C_12EE_0277("standing on a desolate plain. Nearby rests a massive ");
		C_12EE_0277("rune-struck altar, shrouded in moonlit fog.");
		break;
	case 201:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("At first the plain is still. Then a hundred voices ");
		C_12EE_0277("raise a slow, deathlike song, drawing closer and ");
		C_12EE_0277("closer with each passing moment. You are seized ");
		C_12EE_0277("by an urge to run...");
		break;
	case 202:
		D_6082 = 33;
		D_6084 = 157;
		D_6086.left = 40;
		D_6086.top = 165;
		D_6080 = 0;
		D_6072.right = 234;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("...but you have no place to go.");
		break;
	case 203:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Before you can offer a protest to the creatures ");
		C_12EE_0277("who surround you, scaly claws grasp your body.");
		break;
	case 204:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("With unearthly strength, the monsters bind you to the altar stone!");
		break;
	case 205:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Kneeling, the hordes sway and chant as a stately ");
		C_12EE_0277("winged nightmare steps forward.");
		win.f10 = 2;
		break;
	case 206:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("The leader unwraps a velvet covered, brass-bound ");
		C_12EE_0277("book and recites from it in a formal, stilted tongue.");
		break;
	case 207:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 164;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 23;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Shouts and jeers explode from the masses as the priest ");
		C_12EE_0277("slams shut the book. In his hand a malignant dagger ");
		C_12EE_0277("drips with moonlight.");
		break;
	case 208:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("You close your eyes. A dying scream, certainly your own, ");
		C_12EE_0277("curdles the air.");
		win.f10 = 2;
		break;
	case 209:
		D_6082 = 1;
		D_6084 = 80;
		D_6086.left = 8;
		D_6086.top = 94;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("Catcalls, the dagger, a scream, Death....");
		break;
	case 210:
		D_6082 = 1;
		D_6084 = 80;
		D_6086.left = 8;
		D_6086.top = 94;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("Pandemonium. Shrieks of rage, of terror.");
		break;
	case 211:
		D_6082 = 1;
		D_6084 = 80;
		D_6086.left = 8;
		D_6086.top = 94;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("From the inevitable, an impossibility emerges.");
		break;
	case 212:
		D_6082 = 1;
		D_6084 = 80;
		D_6086.left = 8;
		D_6086.top = 94;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("You are still alive.");
		break;
	case 213:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Silent red light fills the darkness. There is the wooden ");
		C_12EE_0277("clack of a crossbow, and a violet- fletched rose blooms ");
		C_12EE_0277("in the priest's barren forehead.");
		break;
	case 214:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Friendly faces vault from a newborn moongate, while a ");
		C_12EE_0277("rain of quarrels holds the furious mob at bay. The ");
		C_12EE_0277("knight Dupre's sword flashes twice in the ");
		C_12EE_0277("darkness, slicing away your bonds!");
		break;
	case 215:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("\"Quickly, old friend! To the gate!%c Accompanied by the ", QUOTE_END);
		C_12EE_0277("swordsman Shamino and a grinning, crossbow-wielding ");
		C_12EE_0277("Iolo the Bard, Dupre thrusts a spare sword into your ");
		C_12EE_0277("hand.");
		break;
	case 216:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Snatching the fallen priest's book, Iolo dives ");
		C_12EE_0277("into the redness with Shamino at his heels. The howling throng ");
		C_12EE_0277("surges forward, all of one terrible mind.");
		break;
	case 217:
		D_6082 = 1;
		D_6084 = 160;
		D_6086.left = 8;
		D_6086.top = 170;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("The gate wanes rapidly as you and Dupre charge through...");
		win.f10 = 2;
		break;
	case 218:
		D_6082 = 33;
		D_6084 = 157;
		D_6086.left = 41;
		D_6086.top = 165;
		D_6080 = 0;
		D_6072.right = 234;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		win.f10 = 2;
		C_12EE_0277("...but not rapidly enough.");
		break;
	case 219:
		D_6082 = 1;
		D_6084 = 152;
		D_6086.left = 8;
		D_6086.top = 160;
		D_6080 = 2;
		D_6072.right = 303;
		D_6072.bottom = 31;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("From the mob's vanguard, three of the abominations ");
		C_12EE_0277("scramble toward the gate. Driven by fury, ");
		C_12EE_0277("the creatures hurl their bodies into the ");
		C_12EE_0277("portal's last handspan of light!");
		break;
	case 998:
		D_6082 = 1;
		D_6084 = 72;
		D_6086.left = 8;
		D_6086.top = 80;
		D_6080 = 1;
		D_6072.right = 303;
		D_6072.bottom = 15;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("A game is already in progress.  Please run the install ");
		C_12EE_0277("program to create a new character disk.");
		win.f10 = 2;
		break;
	case 999:
		D_6082 = 32;
		D_6084 = 72;
		D_6086.left = 40;
		D_6086.top = 80;
		D_6080 = 0;
		D_6072.right = 200;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("Please insert the ");
		C_12EE_0277(D_4038[D_404C]);
		C_12EE_0277(" disk.");
		win.f10 = 2;
		break;
	default:
		D_6082 = 1;
		D_6084 = 159;
		D_6086.left = 9;
		D_6086.top = 169;
		D_6080 = 0;
		D_6072.right = 200;
		D_6072.bottom = 7;
		DRV(0xc)(&D_6072, color);
		C_12EE_0277("<< Passage undefined! >>");
	}
	C_128D_000C(win.buf);
	C_132B_0006(&D_566D);
}

/*draws the passage's frame (?) and copies the text area to the screen*/
/*C_09A0_21C5*/DisplayPassage() {
	if(D_5044 == 0xd)
		D_5732.segment = D_6086.segment = D_5710.segment;
	DRV(3)(&D_5732, D_6082, D_6084, D_58F2, D_58F4, D_6080);
	DRV(0xf)(&D_6072, &D_6086);
}
