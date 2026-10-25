/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 09A0: the passages of text of the introduction, the gypsy and the
 * end of character creation, printed in a text window under the picture
 */
#include "u.h"
#include <conio.h>

/*a 14-byte graphics area (see disk.c); the first word is the only one used here*/

extern int16 videoMode;/*video mode: 0x0d EGA, 0x13 MCGA*/
extern struct tSurface screen;/*the screen*/
extern struct tSurface backBuffer;/*the drawing area (EGA: copy of the screen's)*/
extern struct tSurface workArea;/*the text area of a passage*/
extern int16 frameShape;
extern int16 frameX, frameY;/*where the text area is on the screen: x (in 8-pixel columns?), y*/
extern struct tSurface passageArea;/*copy of backBuffer the text area is shown in*/
extern int16 blockShapes, blockShapesSeg;
extern int16 wantDisk;/*the disk asked for (disk.c)*/
extern char *diskNames[];/*names of the disks*/
extern char newName[];/*the player's name*/

extern TXT_select(struct tText *);/*make a text window current*/
extern TXT_sappend(char far *, ...);/*printf in the current text window*/
extern TXT_puts(char far *);/*print a text (word wrapped?) in the current window*/

#define QUOTE_END 0x7f/*the font's closing quotation mark, it seems*/

/*prints passage n (0-15 the introduction, 100-137 the gypsy, 200-219 the arrival in
Britannia, 998/999 messages) in the text area workArea, after setting where it goes on the
screen; DisplayPassage puts it there*/
/*C_09A0_000B*/ShowPassage(int16 n) {
	struct tText win;
	int16 color;

	win = console;
	/*flush the keyboard*/
	while(kbhit())
		getch();
	passageArea = backBuffer;
	win.surface = &workArea;
	workArea.top = workArea.left = win.curX = win.curY = 0;
	win.justify = 3;
	switch(videoMode) {
	case 4: case 7:
		color = 3;
		win.fgColor = 0;
		break;
	case 9: case 0xd:
		color = 0xe;
		win.fgColor = 0;
		break;
	case 0x13:
		color = 0x31;
		win.fgColor = 0x3e;
		break;
	}
	win.bgColor = color;
	/*print in a copy of the normal window, drawn in workArea*/
	TXT_select(&win);
	win.bufPtr = win.buffer;
	/*each passage: where its text area is, then it is cleared and the text printed*/
	switch(n) {
	case 0:
		frameX = 1;
		frameY = 159;
		passageArea.left = 8;
		passageArea.top = 169;
		frameShape = 1;
		workArea.right = 304;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Upon your world, five seasons have passed since your ");
		TXT_sappend("triumphant homecoming from Britannia.");
		win.justify = 2;
		break;
	case 1:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("You have traded the Avatar's life of peril and adventure ");
		TXT_sappend("for the lonely serenity of a world at peace. But ");
		TXT_sappend("television supermen cannot take the place of friends ");
		TXT_sappend("who died at your side!");
		break;
	case 2:
		frameX = 33;
		frameY = 157;
		passageArea.left = 40;
		passageArea.top = 165;
		frameShape = 0;
		workArea.right = 234;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Outside, a chill wind rises...");
		win.justify = 2;
		break;
	case 3:
		frameX = 0;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 166;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("...and in moments, the storm is upon you.");
		win.justify = 2;
		break;
	case 4:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 162;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Tongues of lightning lash the sky, conducting an unceasing ");
		TXT_sappend("crescendo of thunder....");
		break;
	case 5:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 162;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("In a cataclysm of sound and light, a bolt of searing ");
		TXT_sappend("blue fire strikes the earth!");
		break;
	case 6:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 162;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Lightning among the stones!\nIs this a sign from ");
		TXT_sappend("distant Britannia?");
		win.justify = 2;
		break;
	case 8:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("You bolt from your house, stumbling, running blind in the");
		TXT_sappend(" storm. Into the forest, down the path, through the ");
		TXT_sappend("rain... to the stones.");
		break;
	case 9:
		frameX = 1;
		frameY = 12;
		passageArea.left = 8;
		passageArea.top = 20;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Near the stones, the smell of damp, blasted earth hangs ");
		TXT_sappend("in the air. In a frozen moment of lightning-");
		TXT_sappend("struck daylight, you glimpse a tiny obsidian stone in the ");
		TXT_sappend("midst of the circle!");
		break;
	case 10:
		frameX = 33;
		frameY = 30;
		passageArea.left = 40;
		passageArea.top = 38;
		frameShape = 0;
		workArea.right = 234;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Wondering, you pick it up....");
		win.justify = 2;
		break;
	case 11:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("...and from the heart of the stones, a softly glowing door ");
		TXT_sappend("ascends in silence!");
		break;
	case 12:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Exultant memories wash over you as you clutch the stone. ");
		TXT_sappend("When last you saw an orb such as this, it was cast down ");
		TXT_sappend("by Lord British to banish the tyrant Blackthorn!");
		break;
	case 13:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("But your joy soon gives way to apprehension.\n\n");
		TXT_sappend("The gate to Britannia has always been blue...\nas ");
		TXT_sappend("blue as the morning sky.");
		win.justify = 2;
		break;
	case 14:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Abruptly, the portal quivers and begins to sink ");
		TXT_sappend("into the ground.  Its crimson light wanes!");
		break;
	case 15:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 174;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Desperation makes the decision an easy one.");
		win.justify = 2;
		break;
	case 100:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 174;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Welcome, O Seeker!%c", QUOTE_END);
		win.justify = 2;
		break;
	case 101:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("A lonely stroll along an unfamiliar forest path brings you ");
		TXT_sappend("upon a curious gypsy wagon, its exotic colors dappled in the ");
		TXT_sappend("summer shade.");
		break;
	case 102:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("A woman's voice rings out with friendship, beckoning you into ");
		TXT_sappend("across the wagon's threshold and, as it happens, ");
		TXT_sappend("into another life....");
		break;
	case 103:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 143;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 39;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"At last thou hast come to fulfill thy destiny,%c ", QUOTE_END);
		TXT_sappend("the gypsy says. She smiles, as if in great relief.\n\n");
		TXT_sappend("\"Sit before me now, and I shall pour the light of Virtue ");
		TXT_sappend("into the shadows of thy future.%c", QUOTE_END);
		break;
	case 104:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 143;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 39;
		DRV(0xc)(&workArea, color);
		TXT_sappend("On a wooden table eight bottles stand, a rainbow of bubbling ");
		TXT_sappend("liquids.\n\n");
		TXT_sappend("\"Behold the Virtues of the Avatar,%c the woman ", QUOTE_END);
		TXT_sappend("says. \"Let us begin the casting!%c", QUOTE_END);
		break;
	case 105:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Entrusted to deliver an uncounted purse of gold, thou dost ");
		TXT_sappend("meet a poor beggar. Dost thou A) deliver the gold Honestly, ");
		TXT_sappend("knowing the trust in thee was well-placed; or B) show Compassion and ");
		TXT_sappend("give the beggar a coin, knowing it won't be missed?%c", QUOTE_END);
		break;
	case 106:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou has been prohibited by thy absent Lord from joining ");
		TXT_sappend("thy friends in a close-pitched battle. Dost thou A) refrain, ");
		TXT_sappend("so thou may Honestly claim obedience; or B) show Valor and aid ");
		TXT_sappend("thy comrades, knowing thou may deny it later?%c", QUOTE_END);
		break;
	case 107:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"A merchant owes thy friend money, now long past due. Thou ");
		TXT_sappend("dost see the same merchant drop a purse of gold. Dost thou ");
		TXT_sappend("A) Honestly return the purse intact; or B) Justly give thy ");
		TXT_sappend("friend a portion of the gold first?%c", QUOTE_END);
		break;
	case 108:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thee and thy friend are valiant but penniless warriors. Thou ");
		TXT_sappend("both set forth to slay a mighty dragon. Thy friend dost ");
		TXT_sappend("think he slew it, but the killing blow was thine. Dost thou ");
		TXT_sappend("A) Honestly claim the reward; or B) Sacrifice the gold for the ");
		TXT_sappend("sake of thy friendship?%c", QUOTE_END);
		break;
	case 109:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou art sworn to protect thy Lord at any cost, yet thou knowest ");
		TXT_sappend("he hath committed a crime. Authorities ask thee of the affair. ");
		TXT_sappend("Dost thou A) Break thine oath by Honestly speaking; or B) uphold ");
		TXT_sappend("Honor by silently keeping thine oath?%c", QUOTE_END);
		break;
	case 110:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thy friend doth seek admittance to thy spiritual order. Thou ");
		TXT_sappend("art asked to vouch for his purity of spirit, of which thou art ");
		TXT_sappend("uncertain. Dost thou A) Honestly express thy doubt; or B) Vouch ");
		TXT_sappend("for him, hoping for his Spiritual improvement?%c", QUOTE_END);
		break;
	case 111:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thy Lord doth mistakenly believe he slew a dragon. Thou hast ");
		TXT_sappend("proof that thy lance felled the beast. When asked, dost thou ");
		TXT_sappend("A) Honestly claim the kill; or B) Humbly permit thy Lord his ");
		TXT_sappend("belief?%c", QUOTE_END);
		break;
	case 112:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou dost manage to disarm thy mortal enemy in a duel. He is ");
		TXT_sappend("at thy mercy. Dost thou A) Show Compassion by permitting him to ");
		TXT_sappend("yield; or B) Slay him, as expected of a Valiant duelist?%c", QUOTE_END);
		break;
	case 113:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"After twenty years thou hast found the slayer of thy best ");
		TXT_sappend("friends. The villain proves to be a man who provides the sole ");
		TXT_sappend("support for a young girl. Dost thou A) Spare him in Compassion ");
		TXT_sappend("for the child; or B) Slay him in the name of Justice?%c", QUOTE_END);
		break;
	case 114:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thee and thy friends hath been routed and ordered to retreat. ");
		TXT_sappend("In defiance of thy orders, dost thou A) Stop in Compassion to aid ");
		TXT_sappend("a wounded comrade; or B) Sacrifice thyself to slow the pursuing ");
		TXT_sappend("enemy so that others might escape?%c", QUOTE_END);
		break;
	case 115:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou art sworn to uphold a Lord who doth participate in the ");
		TXT_sappend("forbidden torture of prisoners. Each night their cries of pain ");
		TXT_sappend("reach thee. Dost thou A) Show Compassion by reporting the deeds; ");
		TXT_sappend("or B) Honor thy oath and ignore the deeds?%c", QUOTE_END);
		break;
	case 116:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou hast been taught to preserve all life as sacred. A man ");
		TXT_sappend("lies fatally stung by a venomous serpent. He prays for a ");
		TXT_sappend("merciful death. Dost thou A) Show Compassion and end his pain; ");
		TXT_sappend("or B) heed thy Spiritual beliefs and turn away?%c", QUOTE_END);
		break;
	case 117:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"The Captain of the King's Guard asks that one among thee ");
		TXT_sappend("visit a hospital to cheer the children with tales of valiant personal ");
		TXT_sappend("deeds. Dost thou A) Show thy Compassion and play the braggart; ");
		TXT_sappend("or B) Humbly let another go?%c", QUOTE_END);
		break;
	case 118:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou hast been sent to secure a needed treaty with a distant ");
		TXT_sappend("Lord. Thy host is agreeable to thy proposal but insults thy ");
		TXT_sappend("country at dinner. Dost thou A) Valiantly bear the slurs; or ");
		TXT_sappend("B) Justly rise and demand an apology?%c", QUOTE_END);
		break;
	case 119:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"A burly knight accosts thee and demands thy food. Dost thou ");
		TXT_sappend("A) Valiantly refuse and engage the knight, or B) Sacrifice ");
		TXT_sappend("thy rations unto the hungry knight?%c", QUOTE_END);
		break;
	case 120:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"During battle thou art ordered to guard thy commander's empty ");
		TXT_sappend("tent. The battle goes poorly and thou dost yearn to aid thy ");
		TXT_sappend("fellows. Dost thou A) Valiantly enter the battle; ");
		TXT_sappend("or B) Honor thy post as guard?%c", QUOTE_END);
		break;
	case 121:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"A local bully pushes for a fight. Dost thou A) Valiantly ");
		TXT_sappend("trounce the rogue; or B) Decline, knowing in thy Spirit that ");
		TXT_sappend("no lasting good will come of it?%c", QUOTE_END);
		break;
	case 122:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Although a simple fisherman, thou art also a skillful swordsman. Thy ");
		TXT_sappend("Lord dost seek to assemble a peacetime guard. Dost thou A) Answer ");
		TXT_sappend("the call, so that all may witness thy Valor; or B) Humbly decline the ");
		TXT_sappend("offer to join thy Lord's largely ceremonial knighthood?%c", QUOTE_END);
		break;
	case 123:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"During a pitched battle, thou dost see a fellow desert his ");
		TXT_sappend("post, endangering many. As he flees, he is set upon by several enemies.");
		TXT_sappend(" Dost thou A) Justly let him fight alone; or B) Risk the Sacrifice ");
		TXT_sappend("of thine own life to aid him?%c", QUOTE_END);
		break;
	case 124:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou hast sworn to do thy Lord's bidding in all. He covets ");
		TXT_sappend("a piece of land and orders its owner removed. Dost thou A) Serve ");
		TXT_sappend("Justice, refusing to act, thus being disgraced; or B) Honor thine ");
		TXT_sappend("oath and evict the landowner?%c", QUOTE_END);
		break;
	case 125:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou dost believe that virtue resides in all people. Thou dost see ");
		TXT_sappend("a rogue steal from thy Lord. Dost thou A) Call him to Justice; or B) ");
		TXT_sappend("Personally try to sway him back to the Spiritual path of good?%c", QUOTE_END);
		break;
	case 126:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Unwitnessed, thou hast slain a mighty dragon in defense ");
		TXT_sappend("of thy life. A poor warrior claims the offered reward. ");
		TXT_sappend("Dost thou A) Justly step forward to claim the bounty; ");
		TXT_sappend("or B) Humbly go about thy life, secure in thy self-esteem?%c", QUOTE_END);
		break;
	case 127:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou art a bounty hunter sworn to return an alleged murderer. ");
		TXT_sappend("After his capture, thou dost come to believe him innocent. Dost ");
		TXT_sappend("thou A) Sacrifice thy sizable bounty for thy belief; or B) Honor ");
		TXT_sappend("thine oath to return him as promised?%c", QUOTE_END);
		break;
	case 128:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou hast spent thy life in charitable and righteous work. Thine ");
		TXT_sappend("uncle the innkeeper lies ill and asks thee to take over his tavern. ");
		TXT_sappend("Dost thou A) Sacrifice thy life of purity to aid thy kin; or B) ");
		TXT_sappend("Decline and follow the call of Spirituality?%c", QUOTE_END);
		break;
	case 129:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thou art an elderly, wealthy merchant. Thy end is near. Dost thou ");
		TXT_sappend("A) Sacrifice all thy wealth to feed hundreds of starving children, ");
		TXT_sappend("receiving public adulation; or B) Humbly live out thy life, willing ");
		TXT_sappend("thy fortune to thy heirs?%c", QUOTE_END);
		break;
	case 130:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"In thy youth thou didst pledge to marry thy sweetheart. Now ");
		TXT_sappend("thou art on a sacred quest in distant lands. Thy sweetheart ");
		TXT_sappend("doth ask thee to keep thy vow. Dost thou A) Honor thy pledge to ");
		TXT_sappend("wed; or B) Follow thy Spiritual crusade?%c", QUOTE_END);
		break;
	case 131:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Though thou art but a peasant shepherd, thou art discovered to be the ");
		TXT_sappend("sole descendant of a noble family long thought extinct. ");
		TXT_sappend("Dost thou A) Honorably take up the arms of thy ancestors; or ");
		TXT_sappend("B) Humbly resume thy life of simplicity and peace?%c", QUOTE_END);
		break;
	case 132:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 133;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 55;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Thy parents wish thee to become an apprentice. Two positions ");
		TXT_sappend("are available. Dost thou A) Become an acolyte in a worthy Spiritual ");
		TXT_sappend("order; or B) Become an assistant to a humble village cobbler?%c", QUOTE_END);
		break;
	case 133:
		frameX = 1;
		frameY = 124;
		passageArea.left = 8;
		passageArea.top = 140;
		frameShape = 3;
		workArea.right = 303;
		workArea.bottom = 47;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"The path of the Avatar lies beneath thy feet, worthy %s,%c ", newName, QUOTE_END);
		TXT_sappend("the gypsy intones. With a mysterious smile, she passes you the flask of shimmering liquids. ");
		TXT_sappend("\"Drink of these waters and go forth among our people, who ");
		TXT_sappend("shall receive thee in joy!%c", QUOTE_END);
		break;
	case 134:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("As you drink from the flask, vertigo overwhelms you. ");
		TXT_sappend("A soothing mist obscures the gypsy's face, and you sink ");
		TXT_sappend("without fear into an untroubled sleep.");
		break;
	case 135:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("You wake in a different time, upon another world's shore. ");
		TXT_sappend("Though the Avatar's quests bring you both triumph and tragedy, ");
		TXT_sappend("never do you stray from the path of the Eight Virtues.");
		break;
	case 136:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("The sagas of Ultima IV and Ultima V chronicle your perilous ");
		TXT_sappend("travels, and your name and your deeds are written forever among ");
		TXT_sappend("Britannia's legends....");
		break;
	case 137:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Finally, tempered by your struggles against the enemies of Virtue, you ");
		TXT_sappend("are proven ready to answer the epic challenge of Ultima VI!");
		break;
	case 200:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Dazed, you emerge from the portal to find yourself ");
		TXT_sappend("standing on a desolate plain. Nearby rests a massive ");
		TXT_sappend("rune-struck altar, shrouded in moonlit fog.");
		break;
	case 201:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("At first the plain is still. Then a hundred voices ");
		TXT_sappend("raise a slow, deathlike song, drawing closer and ");
		TXT_sappend("closer with each passing moment. You are seized ");
		TXT_sappend("by an urge to run...");
		break;
	case 202:
		frameX = 33;
		frameY = 157;
		passageArea.left = 40;
		passageArea.top = 165;
		frameShape = 0;
		workArea.right = 234;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("...but you have no place to go.");
		break;
	case 203:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Before you can offer a protest to the creatures ");
		TXT_sappend("who surround you, scaly claws grasp your body.");
		break;
	case 204:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("With unearthly strength, the monsters bind you to the altar stone!");
		break;
	case 205:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Kneeling, the hordes sway and chant as a stately ");
		TXT_sappend("winged nightmare steps forward.");
		win.justify = 2;
		break;
	case 206:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("The leader unwraps a velvet covered, brass-bound ");
		TXT_sappend("book and recites from it in a formal, stilted tongue.");
		break;
	case 207:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 164;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 23;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Shouts and jeers explode from the masses as the priest ");
		TXT_sappend("slams shut the book. In his hand a malignant dagger ");
		TXT_sappend("drips with moonlight.");
		break;
	case 208:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("You close your eyes. A dying scream, certainly your own, ");
		TXT_sappend("curdles the air.");
		win.justify = 2;
		break;
	case 209:
		frameX = 1;
		frameY = 80;
		passageArea.left = 8;
		passageArea.top = 94;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("Catcalls, the dagger, a scream, Death....");
		break;
	case 210:
		frameX = 1;
		frameY = 80;
		passageArea.left = 8;
		passageArea.top = 94;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("Pandemonium. Shrieks of rage, of terror.");
		break;
	case 211:
		frameX = 1;
		frameY = 80;
		passageArea.left = 8;
		passageArea.top = 94;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("From the inevitable, an impossibility emerges.");
		break;
	case 212:
		frameX = 1;
		frameY = 80;
		passageArea.left = 8;
		passageArea.top = 94;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("You are still alive.");
		break;
	case 213:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Silent red light fills the darkness. There is the wooden ");
		TXT_sappend("clack of a crossbow, and a violet- fletched rose blooms ");
		TXT_sappend("in the priest's barren forehead.");
		break;
	case 214:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Friendly faces vault from a newborn moongate, while a ");
		TXT_sappend("rain of quarrels holds the furious mob at bay. The ");
		TXT_sappend("knight Dupre's sword flashes twice in the ");
		TXT_sappend("darkness, slicing away your bonds!");
		break;
	case 215:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("\"Quickly, old friend! To the gate!%c Accompanied by the ", QUOTE_END);
		TXT_sappend("swordsman Shamino and a grinning, crossbow-wielding ");
		TXT_sappend("Iolo the Bard, Dupre thrusts a spare sword into your ");
		TXT_sappend("hand.");
		break;
	case 216:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Snatching the fallen priest's book, Iolo dives ");
		TXT_sappend("into the redness with Shamino at his heels. The howling throng ");
		TXT_sappend("surges forward, all of one terrible mind.");
		break;
	case 217:
		frameX = 1;
		frameY = 160;
		passageArea.left = 8;
		passageArea.top = 170;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("The gate wanes rapidly as you and Dupre charge through...");
		win.justify = 2;
		break;
	case 218:
		frameX = 33;
		frameY = 157;
		passageArea.left = 41;
		passageArea.top = 165;
		frameShape = 0;
		workArea.right = 234;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		win.justify = 2;
		TXT_sappend("...but not rapidly enough.");
		break;
	case 219:
		frameX = 1;
		frameY = 152;
		passageArea.left = 8;
		passageArea.top = 160;
		frameShape = 2;
		workArea.right = 303;
		workArea.bottom = 31;
		DRV(0xc)(&workArea, color);
		TXT_sappend("From the mob's vanguard, three of the abominations ");
		TXT_sappend("scramble toward the gate. Driven by fury, ");
		TXT_sappend("the creatures hurl their bodies into the ");
		TXT_sappend("portal's last handspan of light!");
		break;
	case 998:
		frameX = 1;
		frameY = 72;
		passageArea.left = 8;
		passageArea.top = 80;
		frameShape = 1;
		workArea.right = 303;
		workArea.bottom = 15;
		DRV(0xc)(&workArea, color);
		TXT_sappend("A game is already in progress.  Please run the install ");
		TXT_sappend("program to create a new character disk.");
		win.justify = 2;
		break;
	case 999:
		frameX = 32;
		frameY = 72;
		passageArea.left = 40;
		passageArea.top = 80;
		frameShape = 0;
		workArea.right = 200;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		TXT_sappend("Please insert the ");
		TXT_sappend(diskNames[wantDisk]);
		TXT_sappend(" disk.");
		win.justify = 2;
		break;
	default:
		frameX = 1;
		frameY = 159;
		passageArea.left = 9;
		passageArea.top = 169;
		frameShape = 0;
		workArea.right = 200;
		workArea.bottom = 7;
		DRV(0xc)(&workArea, color);
		TXT_sappend("<< Passage undefined! >>");
	}
	TXT_puts(win.buffer);
	TXT_select(&console);
}

/*draws the passage's frame (?) and copies the text area to the screen*/
/*C_09A0_21C5*/DisplayPassage() {
	if(videoMode == 0xd)
		backBuffer.segment = passageArea.segment = screen.segment;
	DRV(3)(&backBuffer, frameX, frameY, blockShapes, blockShapesSeg, frameShape);
	DRV(0xf)(&workArea, &passageArea);
}
