/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	module talk

	Entry points of the conversation system: TALK_talkTo checks that
	the target can talk (an NPC, a shrine or one of the statues of
	Mondain/Minax/Exodus; a horse is replaced by its rider) and prints
	its name; TALK_initTalk sets the conversation script variables
	($G $N $P $T strings, #A..#Y integers, and clears $0-$9/#0-#9)
	before running the script interpreter (TalkDriver) with the
	conversation music.
	Variables are indexed 0-9 for digits, then 'A' - 0x37 = 10 ... 'Z' = 35.
*/
#include "u6.h"

/*start a conversation between party member #partyId and NPC objNum*/
/*C_16E1_0007*/TALK_initTalk(int16 partyId, int16 objNum) {
	int16 i;

	/*set strings $*/
	VarStr['G' - 0x37] = AvatarFemale?/*0920*/"milady":/*0927*/"milord";/*[g]reetings*/
	VarStr['N' - 0x37] = /*092E*/"lots of space for any possible NPC name";/*[n]ame of NPC*/
	VarStr['P' - 0x37] = Names[partyId];/*name of PC*/
	VarStr['T' - 0x37] =
		(Time_H < 12)?/*0956*/"morning":
		(Time_H < 18)?/*095E*/"afternoon":
		/*0968*/"evening"
	;/*[t]ime of day*/
	/*set variables #*/
	VarInt['A' - 0x37] = GetDex(Party[partyId]);/*PC's [a]gility*/
	VarInt['D' - 0x37] = Date_D;/*[d]ay*/
	VarInt['E' - 0x37] = ExpPoints[Party[partyId]];/*PC's [e]xperience*/
	VarInt['G' - 0x37] = (AvatarFemale != 0);/*[g]ender?*/
	VarInt['H' - 0x37] = Time_H;/*[h]our*/
	VarInt['I' - 0x37] = GetInt(Party[partyId]);/*PC's [i]ntelligence*/
	VarInt['K' - 0x37] = KARMA;/*[k]arma*/
	VarInt['L' - 0x37] = SpeakGargish;/*[l]anguage*/
	VarInt['M' - 0x37] = Date_M;/*[m]onth*/
	VarInt['O' - 0x37] = PartySize - 1;/*number of [o]ther party members*/
	VarInt['P' - 0x37] = HitPoints[Party[partyId]];/*PC's hit [p]oints*/
	VarInt['S' - 0x37] = GetStr(Party[partyId]);/*PC's [s]trength*/
	VarInt['W' - 0x37] = NPCMode[objNum];/*NPC's [w]ork type*/
	VarInt['Y' - 0x37] = Date_Y;/*[y]ear*/
	/*-- reset $0~$9 --*/
	for(i = 0; i < 10; i ++)
		VarStr[i] = TalkStrBuf[i];
	/*-- reset #0~#9 --*/
	for(i = 0; i < 10; i ++)
		VarInt[i] = 0;
	/*--*/
	/*console attribute 0x10 (paging?) during the talk; EndPagingAtPrompt makes
	  the next prompt clear it*/
	CurClip->attrFlags |= 0x10;
	MUS_PlaySong(0);
	TalkDriver(Party[partyId], objNum);
	MUS_PlayAreaSong();
	EndPagingAtPrompt = 1;
	/*the script may have taught the language (gargish)*/
	SpeakGargish = VarInt['L' - 0x37];
}

/*"talk" command: party member #partyId talks to objNum;
  showName: print the target's name first.
NPC object numbers:
;1 - avatar
;2 - dupre
;3 - shamino 
;4 - iolo
;5 - lord british
;...*/
/*C_16E1_015C*/TALK_talkTo(int16 partyId, int16 objNum, int16 showName) {
	int16 objTyp;

	objTyp = GetType(objNum);
	if(objTyp == OBJ_HORSE || objTyp == OBJ_MOUNTED_HORSE)
		objNum = COMBAT_getHead(objNum);
	if(
		(objNum >= 0 && objNum < 0x100) ||
		objTyp == OBJ_SHRINE || objTyp == OBJ_STATUE_OF_MONDAIN || objTyp == OBJ_STATUE_OF_MINAX || objTyp == OBJ_STATUE_OF_EXODUS
	) {
		if(showName) {
			if(objTyp == OBJ_SHRINE) {
				CON_printf(/*0970*/"shrine");
			} else if(objTyp == OBJ_STATUE_OF_MONDAIN || objTyp == OBJ_STATUE_OF_MINAX || objTyp == OBJ_STATUE_OF_EXODUS) {
				CON_printf(/*0977*/"statue");
			} else {
				if(TalkFlags[objNum] & 1)
					/*met already*/
					CON_printf((char *)TALK_GetCharName(objNum));
				else
					/*first time*/
					CON_printf((char *)GetObjectString(objNum));
			}
		}
		TALK_initTalk(partyId, objNum);
	} else {
		CON_printf(/*097E*/"nothing!\n");
	}
}
