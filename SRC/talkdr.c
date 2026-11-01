/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	talkdr: conversation engine module

	Interpreter for the NPC conversation scripts stored (compressed) in
	CONVERSE.A / CONVERSE.B and loaded into TalkBuf by LoadConversation().
	A script is a byte stream: bytes < 0x80 are text that is printed as is,
	bytes >= 0x80 are opcodes (OP_xxx below).
	Script layout:
	  OP_SCRIPT_MARK <npc num> <npc name> OP_DESC <description text...>
	  OP_MAIN <statements...> then a loop of
	  OP_ASKTOP / OP_GETxxx (input) followed by keyword blocks
	  OP_KEY "kw1,kw2,..." OP_RES <answer statements> OP_ENDRES ...
	Expressions are stored in postfix order and terminated by
	OP_END_OF_FACTOR (or OP_LET_VALUE); they are evaluated by parse_factor()
	on a small local stack of longs. Numeric constants are prefixed by
	OP_BYTE / OP_WORD / OP_ADDRESS.
	Variables: VarInt[] (#0..#9, #A..#Z) and VarStr[] ($0..$9, $A..$Z);
	a letter variable 'X' lives at index 'X' - 0x37 (i.e. 'A' == 10).
	Flags: TalkFlags[npc] holds 8 per-NPC script flags (OP_SET/OP_CLR/OP_TST).
	TalkNpcs[0] is the NPC being talked to (the OP_NPC pseudo-number),
	TalkNpcs[1] is the party member doing the talking.
	TalkDriver() is the entry point (called from talk.c after the
	variables have been initialized).
*/
#include "u6.h"

/*comparison operators (push 1/0)*/
#define OP_SUP            0x81
#define OP_SUPE           0x82
#define OP_INF            0x83
#define OP_INFE           0x84
#define OP_DIF            0x85
#define OP_EQU            0x86

/*arithmetic/logical operators*/
#define OP_ADD            0x90
#define OP_SUB            0x91
#define OP_MUL            0x92
#define OP_DIV            0x93
#define OP_OR             0x94
#define OP_AND            0x95

#define OP_CANCARRY       0x9a
#define OP_WEIGHT         0x9b
#define OP_GETHORSE       0x9c
#define OP_HORSED         0x9d
#define OP_REST           0x9e
#define OP_OWNS           0x9f

#define OP_RND            0xa0
#define OP_IF             0xa1
#define OP_ENDIF          0xa2
#define OP_ELSE           0xa3
#define OP_SET            0xa4
#define OP_CLR            0xa5
#define OP_LET            0xa6
#define OP_END_OF_FACTOR  0xa7
#define OP_LET_VALUE      0xa8
#define OP_TST            0xab

#define OP_GOTO           0xb0
/*after an OP_ADDRESS operand: "no index expression follows"*/
#define OP_NOINDEX           0xb1
#define OP_VARINT         0xb2
#define OP_VARSTR         0xb3
/*read 16 bit value from an array: (array address, index)*/
#define OP_DATA            0xb4
#define OP_PRINTSTR       0xb5
#define OP_LEAVE          0xb6
#define OP_STRSEARCH      0xb7
#define OP_ENDSEARCH      0xb8
#define OP_GIVEOBJ        0xb9
#define OP_TAKEOBJ        0xba
#define OP_TEST_OBJ       0xbb
/*stub: pops 2 operands, pushes constant 100*/
#define OP_STUB_BC            0xbc
/*stub: pops 2 operands, pushes constant 100*/
#define OP_STUB_BD            0xbd
#define OP_SHOW_INVENTORY 0xbe
#define OP_SHOW_CONVERSE  0xbf
#define OP_SELECT_OBJECT  0xc0
#define OP_OWNER          0xc1
#define OP_OBJTYPE        0xc2
#define OP_VALSEARCH      0xc3
#define OP_ADDKARMA       0xc4
#define OP_SUBKARMA       0xc5
#define OP_ISINPARTY      0xc6
#define OP_WHOSGOT        0xc7
#define OP_MOVEOBJ        0xc8
#define OP_TRANSFEROBJ    0xc9
#define OP_JOIN           0xca
#define OP_WAIT           0xcb
#define OP_LEAVEPARTY     0xcc
#define OP_SETMODE        0xcd

#define OP_DELAY          0xd0

/*constant prefixes*/
#define OP_ADDRESS        0xd2
#define OP_BYTE           0xd3
#define OP_WORD           0xd4
/*a parameter to OP_PRINTSTR: print nothing*/
#define OP_PRINT_NONE            0xd5
#define OP_RESURRECT      0xd6
#define OP_ISONSCREEN     0xd7
/*$Y := name of npc*/
#define OP_SETNAME_Y            0xd8
#define OP_HEAL           0xd9
#define OP_WOUNDED        0xda
#define OP_CURE           0xdb
#define OP_POISONED      0xdc
/*get party member npc num: (party index, any/onscreen only)*/
#define OP_PARTYMEMBER            0xdd

/*$Y := name of npc, or "you" for the avatar*/
#define OP_SETNAME_Y_YOU            0xdf
#define OP_ADDEXP         0xe0
#define OP_ADDLVL         0xe1
#define OP_ADDSTR         0xe2
#define OP_ADDINT         0xe3
#define OP_ADDDEX         0xe4

/*pseudo npc number: "the npc being talked to"*/
#define OP_NPC            0xeb

#define OP_ENDRES         0xee
#define OP_KEY            0xef
/*OP_FIRST_SECTION: opcodes >= this stop parse_statement (section/input commands)*/
#define OP_FIRST_SECTION            0xf0
#define OP_DESC           0xf1
#define OP_MAIN           0xf2
#define OP_RES            0xf6
#define OP_ASKTOP         0xf7
#define OP_GET            0xf8
#define OP_GETSTR         0xf9
#define OP_GETCHR         0xfa
#define OP_GETINT         0xfb
#define OP_GETDIGIT       0xfc
/*start of script (followed by npc num) / end of conversation*/
#define OP_SCRIPT_MARK            0xff


/*====---- ----====*/
unsigned char Unused_0988 = 0;/*in this module?(unreferenced)*/
unsigned char Unused_0989 = 0;/*in this module?(unreferenced)*/

/*set by OP_LEAVE: end the conversation*/
/*098A*/static unsigned char mustLeave = 0;

/*098B*/unsigned char IsInConversation = 0;
/*====---- ----====*/

/*keyword match: returns 1 if the first <len> chars of <pattern> match
(case insensitive, '?' is a wildcard) the start of any space separated
word of <input>*/
/*C_1703_0006*/static str_i_compare(unsigned char *pattern, unsigned char *input, int16 len) {
	unsigned char *word;
	int16 i;
	unsigned char buf[100];

	strcpy(buf, input);
	for(word = strtok(buf, /*098C*/" "); word; word = strtok(0, /*098C*/" ")) {
		for(i = 0; i < len; i ++) {
			if(tolower(pattern[i]) != tolower(word[i]) && pattern[i] != '?')
				break;
		}
		if(i == len)
			return 1;
	}
	return 0;
}

/*translate the OP_NPC pseudo-number into the current npc's number*/
/*C_1703_008A*/static mk_npcnum(int16 npc) {
	if(npc == OP_NPC)
		return TalkNpcs[0];
	return npc;
}

/*get NPC name: loads the npc's script into ScratchBuf and copies the
name (which follows OP_SCRIPT_MARK and the npc num, and ends with the first
opcode byte) into TalkNpcName; returns TalkNpcName*/
/*C_1703_00A0*/static char *TALK_GetNpcName(int16 objNum) {
	int16 i;

	if(LoadConversation(objNum, ScratchBuf)) {
		for(i = 0; i < 50; i ++)
			TalkNpcName[i] = ((unsigned char far *)ScratchBuf + i)[2];
	} else {
		TalkNpcName[0] = 0;
	}
	i = 0;
	while(!(TalkNpcName[i] & 0x80) && TalkNpcName[i++]);
	if(i == 0)
		CON_printf(/*098E*/"\nUndefined.\n");
	else if(objNum == 0x6e)/*patch for "Dr_ Cat"*/
		TalkNpcName[2] = '.';
	TalkNpcName[i] = 0;

	return TalkNpcName;
}

/*name of a character: party members use the party name table (Names[]),
other npcs the name from their conversation script*/
/*C_1703_0116*/char *TALK_GetCharName(int16 objNum) {
	int16 i;

	if(IsPlrControl(objNum)) {
		for(i = 0; i < 8; i ++)
			if(Party[i] == objNum)
				break;
		return Names[i];
	}
	TALK_GetNpcName(objNum);

	return TalkNpcName;
}

/*is object onscreen? (within the visible 9x9 part of the 11x11 view
and not on a hidden/dark tile); party members are always "onscreen"
while in a vehicle*/
/*C_1703_0153*/IsNpcOnScreen(int16 objNum) {
	int16 x, y, tile;

	if(IN_VEHICLE && IsPlrControl(objNum))
		return 1;
	if(
		GetX(objNum) > MapX - 5 && GetX(objNum) < MapX + 5 &&
		GetY(objNum) > MapY - 5 && GetY(objNum) < MapY + 5
	) {
		x = GetX(objNum) - (MapX - 5);
		y = GetY(objNum) - (MapY - 5);
		/*objects displayed there, or an object tile*/
		if(Obj_11x11[y][x] || Tile_11x11[y][x] >= TIL_LEATHER_HELM)
			return 1;
		tile = Tile_11x11[y][x];
		return (tile != TIL_HIDDEN && tile != TIL_DARKNESS);
	}
	return 0;
}

/*set #N -- number of nearby party members (not counting the speaker)*/
/*C_1703_028B*/static TALK_SetNearbyPartyVar() {
	int16 i, num;

	num = 0;
	for(i = 0; i < PartySize; i ++) {
		if(IsNpcOnScreen(Party[i]))
			num ++;
	}
	VarInt['N' - 0x37] = num - 1;
}

/*add npc to the party
returns 0:ok, 1:party is in a vehicle, 2:party is full*/
/*C_1703_02B6*/JoinParty(int16 objNum) {
	unsigned char *name;
	int16 nameLen;

	if(IN_VEHICLE)
		return 1;
	if(PartySize == 8)
		return 2;
	PartySize ++;
	Party[PartySize - 1] = objNum;
	name = TALK_GetNpcName(objNum);
	nameLen = strlen(name);
	if(nameLen >= 14)
		strncpy(Names[PartySize - 1], name, 13);
	else
		strcpy(Names[PartySize - 1], name);
	SetPlrControl(objNum);
	if(GetAlignment(objNum) == NEUTRAL)
		SetAlignment(objNum, GOOD);
	NPCMode[objNum] = AI_FOLLOW;
	NPCComMode[objNum] = AI_FRONT;
	/*a playing musician stops playing*/
	if(GetType(objNum) == OBJ_MUSICIAN_SHORT_CAPE)
		SetType(objNum, OBJ_MUSICIAN_LONG_CAPE);
	return 0;
}

/*remove npc from the party; he drops his whole inventory at his feet
returns 0:ok, 1:party is in a vehicle, 2:not in party*/
/*C_1703_0398*/static LeaveParty(int16 objNum) {
	int16 i;
	int16 slot, invObj;

	if(IN_VEHICLE)
		return 1;
	for(slot = 0; slot < PartySize; slot ++)
		if(Party[slot] == objNum)
			break;
	if(PartySize == slot)
		return 2;
	for(i = slot; i < PartySize; i ++) {
		Party[i] = Party[i + 1];
		strcpy(Names[i], Names[i + 1]);
	}
	PartySize --;
	ClrPlrControl(objNum);
	NPCMode[objNum] = AI_WANDER;
	if(GetAlignment(objNum) == GOOD)
		SetAlignment(objNum, NEUTRAL);
	for(invObj = FindInv(objNum); invObj >= 0; invObj = FindInv(objNum)) {
		MoveObj(invObj, GetX(objNum), GetY(objNum), MapZ);
		SetOkToGet(invObj);
	}
	MoveObj(objNum, GetX(objNum), GetY(objNum), MapZ);
	return 0;
}

/*is object owned by party onscreen?
returns the first onscreen party member carrying an object of type
<objType> (and quality <objQual>, 0 = any), or -1*/
/*C_1703_04C6*/FindPartyMemberWithObj(int16 objType, int16 objQual) {
	int16 i;

	for(i = 0; i < PartySize; i ++) {
		if(IsNpcOnScreen(Party[i]) && OBJ_FindInContainer(Party[i], objType, objQual) != -1)
			return Party[i];
	}
	return -1;
}

/*this macro creates an "offset" into the current
conversation file. though, TalkBase is always 0
(initialized during the startup)*/
#define TALK_OFFSET(i) (int16)(i) - TalkBase

#define PARSE_U8 (TalkBuf[Talk_PC ++])
#define READ_U16 (*(uint16 far *)(TalkBuf + Talk_PC))
#define READ_S16 (*(int16 far *)(TalkBuf + Talk_PC))
#define READ_U32 (*(uint32 far *)(TalkBuf + Talk_PC))

/*evaluate a postfix expression starting at Talk_PC, up to (and
including) OP_END_OF_FACTOR/OP_LET_VALUE; returns its value.
Constants are pushed; an operator pops its operands from the top of
lstack and leaves its result in the lowest operand's slot.*/
/*C_1703_050E*/static parse_factor() {
	int16 sidx;/*stack index*/
	NEARLONG lstack[10];/*local "stack"*/
	int32 arrayAddr;
	int16 i, j, opcode, cmpLen;
	unsigned char cparam0, cparam1, varIdx;
	int16 isOperator, isString;
	int16 iparam0, iparam1, iparam2;
	unsigned char scratch[80];
	NEARPTR inputStr;

	sidx = 0;
	i = 0;
	j = 0;
	isString = 0;
	/*reset local stack*/
	for(; sidx < 10; sidx ++)
		lstack[sidx] = 0L;
	sidx = 0;
	opcode = PARSE_U8;
	while(opcode != OP_END_OF_FACTOR && opcode != OP_LET_VALUE) {
		isOperator = 0;
		switch(opcode) {
			case OP_ADDRESS: lstack[sidx] = READ_U32; Talk_PC += 4; break;
			case OP_BYTE: lstack[sidx] = PARSE_U8; break;
			case OP_WORD: lstack[sidx] = READ_U16; Talk_PC += 2; break;
			default:
				/*a raw byte: either an operator or a small constant*/
				isOperator = 1;
				lstack[sidx] = opcode;
		}
		if(isOperator) {
			switch(lstack[sidx]) {
				case OP_RND:/*random number in [min, max] (8 bits)*/
					cparam0 = lstack[sidx - 2];
					cparam1 = lstack[sidx - 1];
					lstack[sidx - 2] = OSI_rand(cparam0, cparam1);
					sidx -= 2;
				break;
				case OP_TEST_OBJ:/*(npc, object type)*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					lstack[sidx - 2] = TestObj(iparam0, iparam1);
					sidx -= 2;
				break;
				case OP_OWNS:/*(npc, object type, quality)*/
					iparam0 = mk_npcnum(lstack[sidx - 3]);
					iparam1 = lstack[sidx - 2];
					iparam2 = lstack[sidx - 1];
					if(OBJ_FindInContainer(iparam0, iparam1, iparam2) != -1)
						lstack[sidx - 3] = 1;
					else
						lstack[sidx - 3] = 0;
					sidx -= 3;
				break;
				case OP_WEIGHT:/*(object type, quantity): weight in 1/10 units*/
					iparam0 = lstack[sidx - 1];/*quantity*/
					iparam1 = lstack[sidx - 2];/*object type*/
					iparam2 = TypeWeight[iparam1];
					/*gold and reagents weigh less*/
					if(IsWeightPer10Type(iparam1))
						lstack[sidx - 2] = iparam0 * iparam2;
					else
						lstack[sidx - 2] = iparam0 * iparam2 * 10;
					sidx -= 2;
				break;
				case OP_JOIN:/*(npc): see JoinParty for the result*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					lstack[sidx - 1] = JoinParty(iparam0);
					sidx --;
				break;
				case OP_CANCARRY:/*(npc): remaining capacity (STR * 20 stones)*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					STAT_ComputeEquipment(iparam0);
					iparam1 = STREN[iparam0] * 200;
					iparam2 = (WeightEquip + WeightInven) * 10;
					if(iparam1 > iparam2)
						lstack[sidx - 1] = iparam1 - iparam2;
					else
						lstack[sidx - 1] = 0;
					sidx --;
				break;
				case OP_LEAVEPARTY:/*(npc): see LeaveParty for the result*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					lstack[sidx - 1] = LeaveParty(iparam0);
					sidx --;
				break;
				case OP_TST:/*(npc, flag bit)*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					lstack[sidx - 2] = (TalkFlags[iparam0] >> iparam1) & 1;
					sidx -= 2;
				break;
				case OP_SELECT_OBJECT:/*let the player pick an item: pushes its number or 0x8001*/
					CON_printf(/*099B*/"\n\nSelect item: ");
					MouseMode = 1;
					StatusDisplay = CMD_STAT_EQUIP;
					CON_getch();
					if(Selection.obj != -1 && Selection.x != -1)
						lstack[sidx] = Selection.obj;
					else
						lstack[sidx] = 0x8001;
				break;
				case OP_OWNER:/*(object): its owner (assoc field)*/
					lstack[sidx - 1] = (int16)GetAssoc(lstack[sidx - 1]);
					/*patches the next operand of the script in place*/
					TalkBuf[Talk_PC + 1] = mk_npcnum(TalkBuf[Talk_PC + 1]);
					sidx --;
				break;
				case OP_WOUNDED:/*(npc)*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					if(MaxHP(iparam0) > HitPoints[iparam0])
						lstack[sidx - 1] = 1;
					else
						lstack[sidx - 1] = 0;
					sidx --;
				break;
				case OP_POISONED:/*(npc)*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					lstack[sidx - 1] = IsPoisoned(iparam0);
					sidx --;
				break;
				case OP_ADDEXP:/*(npc, amount): new value, max 9999*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					ExpPoints[iparam0] += iparam1;
					if(ExpPoints[iparam0] > 9999)
						ExpPoints[iparam0] = 9999;
					lstack[sidx - 2] = ExpPoints[iparam0];
					sidx -= 2;
				break;
				case OP_ADDLVL:/*(npc, amount): new level*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					Level[iparam0] += iparam1;
					lstack[sidx - 2] = GetLevel(iparam0);
					sidx -= 2;
				break;
				case OP_ADDSTR:/*(npc, amount): new value, max 30*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					STREN[iparam0] += iparam1;
					if(STREN[iparam0] > 30)
						STREN[iparam0] = 30;
					lstack[sidx - 2] = STREN[iparam0];
					sidx -= 2;
				break;
				case OP_ADDINT:/*(npc, amount): new value, max 30*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					INTEL[iparam0] += iparam1;
					if(INTEL[iparam0] > 30)
						INTEL[iparam0] = 30;
					lstack[sidx - 2] = INTEL[iparam0];
					sidx -= 2;
				break;
				case OP_ADDDEX:/*(npc, amount): new value, max 30*/
					iparam0 = mk_npcnum(lstack[sidx - 2]);
					iparam1 = lstack[sidx - 1];
					DEXTE[iparam0] += iparam1;
					if(DEXTE[iparam0] > 30)
						DEXTE[iparam0] = 30;
					lstack[sidx - 2] = DEXTE[iparam0];
					sidx -= 2;
				break;
				case OP_PARTYMEMBER:/*get party member npc num (0 if none)*/
					iparam0 = lstack[sidx - 2];/*party member index*/
					iparam1 = lstack[sidx - 1];/*if 0, member must be onScreen*/
					if(iparam0 < PartySize) {
						if(iparam1) {
							lstack[sidx - 2] = Party[iparam0];
							sidx -= 2;
							break;
						}
						/*party's onscreen members list*/
						j = 0;
						for(i = 0; i < PartySize; i ++) {
							if(IsNpcOnScreen(Party[i]))
								scratch[j++] = i;
						}
						for(cparam0 = j; cparam0 < PartySize; cparam0 ++)
							scratch[cparam0] = 0xff;

						if(scratch[iparam0] != 0xff) {
							lstack[sidx - 2] = Party[scratch[iparam0]];
							sidx -= 2;
							break;
						}
					}
					lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_ISONSCREEN:/*(npc)*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					lstack[sidx - 1] = IsNpcOnScreen(iparam0);
					sidx --;
				break;
				case OP_WHOSGOT:/*(object type, quality): onscreen party member carrying it, or 0x8001*/
					iparam0 = lstack[sidx - 2];
					iparam1 = lstack[sidx - 1];
					i = FindPartyMemberWithObj(iparam0, iparam1);
					if(i == -1)
						lstack[sidx - 2] = 0x8001;
					else
						lstack[sidx - 2] = i;
					sidx -= 2;
				break;
				case OP_ISINPARTY:/*(npc): in party, onscreen and able to act*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					if(
						IsPlrControl(iparam0) &&
						IsNpcOnScreen(iparam0) &&
						!IsDead(iparam0) &&
						!IsAsleep(iparam0) &&
						!IsParalyzed(iparam0)
					)
						lstack[sidx - 1] = 1;
					else
						lstack[sidx - 1] = 0;
					sidx --;
				break;
				case OP_HORSED:/*(npc)*/
					iparam0 = mk_npcnum(lstack[sidx - 1]);
					if(GetType(iparam0) == OBJ_MOUNTED_HORSE)
						lstack[sidx - 1] = 1;
					else
						lstack[sidx - 1] = 0;
					sidx --;
				break;
				case OP_OBJTYPE:/*(object)*/
					lstack[sidx - 1] = GetType(lstack[sidx - 1]);
					sidx --;
				break;
				case OP_STUB_BC:/*constant 100?*/
				case OP_STUB_BD:/*constant 100?*/
					lstack[sidx - 2] = 100;
					sidx -= 2;
				break;
				case OP_DATA:/*read a 16bit value from array*/
					arrayAddr = lstack[sidx - 2];/*array address*/
					varIdx = lstack[sidx - 1];/*array index*/
					TalkSavedPC = Talk_PC;
					Talk_PC = (varIdx << 1) + TALK_OFFSET(arrayAddr);
					lstack[sidx - 2] = READ_S16;
					Talk_PC = TalkSavedPC;
					sidx -= 2;
				break;
				case OP_VARINT:/*# var*/
					varIdx = lstack[sidx - 1];
					lstack[sidx - 1] = (uint16)VarInt[varIdx];
					sidx --;
				break;
				case OP_VARSTR:/*$ string: pushes a pointer*/
					varIdx = lstack[sidx - 1];
					lstack[sidx - 1] = PTR2INT16(VarStr[varIdx]);
					/*from now on, OP_EQU compares strings*/
					isString = 1;
					sidx --;
				break;
				case OP_ADD:
					lstack[sidx - 2] = lstack[sidx - 1] + lstack[sidx - 2];
					sidx -= 2;
				break;
				case OP_SUB:
					lstack[sidx - 2] = lstack[sidx - 2] - lstack[sidx - 1];
					sidx -= 2;
				break;
				case OP_MUL:
					lstack[sidx - 2] = lstack[sidx - 1] * lstack[sidx - 2];
					sidx -= 2;
				break;
				case OP_DIV:
					if(lstack[sidx - 1])
						lstack[sidx - 2] = (uint32)lstack[sidx - 2] / (uint32)lstack[sidx - 1];
					else
						CON_printf(/*09AB*/"\nDivide error from talkdr.\n");
					sidx -= 2;
				break;
				case OP_AND:
					if(lstack[sidx - 1] && lstack[sidx - 2])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_OR:
					if(lstack[sidx - 1] || lstack[sidx - 2])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_EQU:
					if(isString) {
						if(stricmp((char *)lstack[sidx - 1], (char *)lstack[sidx - 2]) == 0)
							lstack[sidx - 2] = 1;
						else
							lstack[sidx - 2] = 0;
					} else {
						if(lstack[sidx - 1] == lstack[sidx - 2])
							lstack[sidx - 2] = 1;
						else
							lstack[sidx - 2] = 0;
					}
					sidx -= 2;
				break;
				/*relational operators compare unsigned*/
				case OP_SUP:
					if((uint32)lstack[sidx - 2] > (uint32)lstack[sidx - 1])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_SUPE:
					if((uint32)lstack[sidx - 2] >= (uint32)lstack[sidx - 1])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_INF:
					if((uint32)lstack[sidx - 2] < (uint32)lstack[sidx - 1])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_INFE:
					if((uint32)lstack[sidx - 2] <= (uint32)lstack[sidx - 1])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_DIF:
					if(lstack[sidx - 1] != lstack[sidx - 2])
						lstack[sidx - 2] = 1;
					else
						lstack[sidx - 2] = 0;
					sidx -= 2;
				break;
				case OP_STRSEARCH:
					/*(array of strings, $var): index of the first entry whose
					first 4 chars match a word of $var (see str_i_compare),
					or the number of entries if none*/
					varIdx = lstack[sidx - 1];/*string variable*/
					arrayAddr = lstack[sidx - 2];/*array variable*/
					inputStr = VarStr[varIdx];
					TalkSavedPC = Talk_PC;
					Talk_PC = TALK_OFFSET(arrayAddr);
					for(i = 0; (opcode = PARSE_U8) != OP_ENDSEARCH; ) {
						Talk_PC --;
						i ++;
						for(j = 0; opcode = PARSE_U8; j ++)
							scratch[j] = opcode;
						scratch[j] = 0;
						if(strlen(scratch) >= 4)
							cmpLen = 4;
						else
							cmpLen = strlen(scratch);
						if(str_i_compare(scratch, inputStr, cmpLen))
							break;
					}
					if(opcode == OP_ENDSEARCH)
						lstack[sidx - 2] = i;
					else
						lstack[sidx - 2] = i - 1;
					Talk_PC = TalkSavedPC;
					sidx -= 2;
				break;
				case OP_VALSEARCH:
					/*(array of 16bit values, #var): index of the entry equal
					to #var, or 0x8001 if none*/
					varIdx = lstack[sidx - 1];/*integer variable*/
					arrayAddr = lstack[sidx - 2];/*array variable*/
					iparam1 = VarInt[varIdx];
					TalkSavedPC = Talk_PC;
					Talk_PC = TALK_OFFSET(arrayAddr);
					for(i = 0; (opcode = TalkBuf[Talk_PC]) != OP_ENDSEARCH; Talk_PC += 2) {
						i ++;
						iparam0 = READ_S16;
						if(iparam0 == iparam1)
							break;
					}
					if(opcode == OP_ENDSEARCH)
						lstack[sidx - 2] = 0x8001;
					else
						lstack[sidx - 2] = i - 1;
					Talk_PC = TalkSavedPC;
					sidx -= 2;
				break;
			}
		}
		sidx ++;
		opcode = PARSE_U8;
	}
	if(sidx > 1)
		CON_printf(/*09C7*/"Error occured.\n");
	return lstack[0];
}

/*resolve an OP_ADDRESS operand (Talk_PC points just after OP_ADDRESS):
reads the U32 address; if OP_NOINDEX follows it is a plain address,
otherwise an index expression follows and selects an element of the
array (isStringArray: skip <index> NUL-terminated strings; else skip
<index> 16bit words).
Leaves Talk_PC on the data and returns the PC where the caller must
resume afterwards (for word arrays, back on the OP_LET_VALUE that
parse_factor consumed)*/
/*C_1703_1494*/static uint32 TALK_ResolveAddress(int16 isStringArray) {
	int16 index;
	uint32 resumePC, address;

	address = READ_U32;
	Talk_PC += 4;
	if(PARSE_U8 != OP_NOINDEX) {
		Talk_PC --;
		index = parse_factor();
		resumePC = Talk_PC;
		Talk_PC = TALK_OFFSET(address);
		if(isStringArray) {
			while(index > 0)
				if(PARSE_U8 == 0)
					index --;
		} else {
			Talk_PC += index << 1;
			resumePC -= 1;
		}
	} else {
		resumePC = Talk_PC;
		Talk_PC = TALK_OFFSET(address);
	}
	return resumePC;
}

/*execute one opcode (already read) of a statement;
objNum is the npc being talked to.
returns 0:ok, 1:syntax error, -1:unknown opcode*/
/*C_1703_153B*/static execute_op(unsigned char opcode, int16 objNum) {
	int16 tmp, varIdx;
	uint32 gotoAddr;
	int16 ch, varType;
	int16 *pVarInt;
	int16 arg0, arg1, arg2, arg3, count;
	int16 far *pArrayElem;
	char *pStr;

	switch(opcode) {
		case OP_ENDIF:
		case OP_END_OF_FACTOR: break;
		case OP_LEAVE:
			mustLeave = 1;
		break;
		case OP_PRINTSTR:
			ch = PARSE_U8;
			if(ch == OP_ADDRESS) {/*constant string[address]*/
				TalkSavedPC = TALK_ResolveAddress(1);
				while(ch = PARSE_U8)
					CON_putch(ch);
				Talk_PC = TalkSavedPC;
			} else if(ch != OP_PRINT_NONE) {/*VarStr: <x> <index> OP_VARSTR <x>*/
				varIdx = PARSE_U8;
				varType = PARSE_U8;
				if(varType != OP_VARSTR)
					return 1;
				CON_printf(VarStr[varIdx]);
				Talk_PC ++;
			}
		break;
		case OP_LET:
			/*destination: OP_ADDRESS (array element) or <index> OP_VARINT/OP_VARSTR*/
			varIdx = PARSE_U8;
			if(varIdx == OP_ADDRESS) {
				TalkSavedPC = TALK_ResolveAddress(0);
				pArrayElem = (int16 far *)(TalkBuf + Talk_PC);
				Talk_PC = TalkSavedPC;
				varType = 0;
			} else {
				varType = PARSE_U8;
			}
			if(PARSE_U8 != OP_LET_VALUE)
				return 1;
			if(varType == 0) {
				*pArrayElem = parse_factor();
			} else if(varType == OP_VARINT) {
				pVarInt = VarInt + varIdx;
				*pVarInt = parse_factor();
			} else {/*String affectation*/
				pStr = VarStr[varIdx];
				if((ch = PARSE_U8) == OP_ADDRESS) {
					/*from a constant string*/
					TalkSavedPC = TALK_ResolveAddress(1);
					for(tmp = 0; ch = PARSE_U8; tmp ++)
						pStr[tmp] = ch;
					pStr[tmp] = 0;
					Talk_PC = TalkSavedPC;
				} else {
					/*from another string variable*/
					varIdx = ch;
					varType = PARSE_U8;
					if(varType != OP_VARSTR)
						return 1;
					strcpy(pStr, VarStr[varIdx]);
				}
			}
		break;
		case OP_IF:
			tmp = parse_factor();
			if(tmp == -1)
				return 1;
			if(tmp == 0) {
				/*false: skip to the matching OP_ELSE/OP_ENDIF
				(no nesting; skips the operands that may contain those bytes)*/
				for(ch = PARSE_U8; ch != OP_ELSE && ch != OP_ENDIF; ch = PARSE_U8) {
					if(ch == OP_GOTO)
						Talk_PC +=4;
					else if(ch == OP_BYTE)
						Talk_PC ++;
					else if(ch == OP_WORD)
						Talk_PC += 2;
				}
			}
		break;
		case OP_ELSE:
				/*end of the "true" part: skip to OP_ENDIF*/
				for(ch = PARSE_U8; ch != OP_ENDIF; ch = PARSE_U8) {
					if(ch == OP_GOTO)
						Talk_PC +=4;
					else if(ch == OP_BYTE)
						Talk_PC ++;
					else if(ch == OP_WORD)
						Talk_PC += 2;
				}
		break;
		case OP_GOTO:
			gotoAddr = READ_U32;
			Talk_PC = TALK_OFFSET(gotoAddr);
		break;
		case OP_GIVEOBJ: case OP_TAKEOBJ:
			/*(npc, object type, quality, quantity)*/
			arg0 = mk_npcnum(parse_factor());
			arg1 = parse_factor();
			arg2 = parse_factor();
			arg3 = parse_factor();
			if(opcode == OP_TAKEOBJ)
				TakeObj(arg0, arg1, QualQuan(arg2, arg3));
			else
				GiveObj(arg0, arg1, QualQuan(arg2, arg3));
		break;
		case OP_WAIT:
			PromptCh = 1;
			CON_getch();
			PromptCh = 5;
		break;
		case OP_SETMODE:/*(npc, AI mode)*/
			arg0 = mk_npcnum(parse_factor());
			arg1 = parse_factor();
			NPCMode[arg0] = arg1;
			if(arg1 == AI_SLEEP) {
				SetAsleep(arg0);
				SetTypeUnconscious(arg0);
			}
			if(arg1 != AI_PLAY && GetType(arg0) == OBJ_MUSICIAN_SHORT_CAPE)
				SetType(arg0, OBJ_MUSICIAN_LONG_CAPE);
		break;
		case OP_MOVEOBJ:/*(object, new owner npc): put in npc's inventory*/
			arg0 = mk_npcnum(parse_factor());
			arg1 = parse_factor();
			InsertObj(arg0, arg1, INVEN);
		break;
		case OP_TRANSFEROBJ:/*(object type, quality, from npc, to npc)*/
			arg0 = parse_factor();
			arg1 = parse_factor();
			arg2 = mk_npcnum(parse_factor());
			arg3 = mk_npcnum(parse_factor());
			InsertObj(FindInvType(arg2, arg0, arg1), arg3, INVEN);
		break;
		case OP_ADDKARMA:/*max 99*/
			arg0 = parse_factor();
			KARMA += arg0;
			if(KARMA > 99)
				KARMA = 99;
		break;
		case OP_SUBKARMA:/*min 0*/
			arg0 = parse_factor();
			if(KARMA > arg0)
				KARMA -= arg0;
			else
				KARMA = 0;
		break;
		case OP_DELAY:/*(n): n * 20 animation frames*/
			arg0 = parse_factor();
			for(count = 0; count < arg0 * 20; count ++)
				DisplayTick();
		break;
		case OP_SET: case OP_CLR:/*(npc, flag bit)*/
			arg0 = mk_npcnum(parse_factor());
			arg1 = parse_factor();
			if(opcode == OP_SET)
				TalkFlags[arg0] |= 1 << arg1;
			else
				TalkFlags[arg0] &= ~(1 << arg1);
		break;
		case OP_RESURRECT:/*(npc): raise the dead body in npc's inventory*/
			arg0 = mk_npcnum(parse_factor());
			for(arg1 = Link[arg0]; arg1 >= 0; arg1 = Link[arg1])
				if(GetType(arg1) == OBJ_DEAD_BODY)
					break;
			SPELL_ReviveNPC(arg1);
		break;
		case OP_HEAL:/*(npc): full hit points*/
			arg0 = mk_npcnum(parse_factor());
			HitPoints[arg0] = MaxHP(arg0);
		break;
		case OP_CURE:/*(npc)*/
			arg0 = mk_npcnum(parse_factor());
			ClrPoisoned(arg0);
		break;
		case OP_SETNAME_Y:/*set var $Y to the npc's name*/
			arg0 = mk_npcnum(parse_factor());
			VarStr['Y' - 0x37] = TALK_GetCharName(arg0);
		break;
		case OP_SETNAME_Y_YOU:/*set var $Y to the npc's name ("you" for the avatar)*/
			arg0 = mk_npcnum(parse_factor());
			if(arg0 == 1)
				VarStr['Y' - 0x37] = /*09D7*/"you";
			else
				VarStr['Y' - 0x37] = TALK_GetCharName(arg0);
		break;
		case OP_REST:
			/*the onscreen party sleeps (and heals) until 5 o'clock*/
			for(count = 0; count < PartySize; count ++)
				if(IsNpcOnScreen(Party[count]))
					HealPartyMember(count, 0);
			ResetMapView();
			AllowNPCTeleport = 1;
			if(Time_H == 5)
				PassTime(60);
			while(Time_H != 5)
				PassTime(60);
			AllowNPCTeleport = 0;
			ScreenFade = 1;
			VIS_UpdateView();
			DisplayTick();
		break;
		case OP_SHOW_INVENTORY:
			/*(1: the active party member, else the npc talked to)*/
			arg0 = parse_factor();
			ShowNPCInven = 0;
			NPC_2Handed = 0;
			if(arg0 == 1) {
				StatusMember = Active;
			} else if(IsPlrControl(objNum)) {
				for(tmp = 0; tmp < 0x10; tmp ++)
					if(Party[tmp] == objNum) {
						StatusMember = tmp;
						break;
					}
			} else {
				ShowNPCInven = objNum;
			}
			STAT_ShowInventory(objNum);
		break;
		case OP_SHOW_CONVERSE:
			arg0 = mk_npcnum(parse_factor());
			ShowPortraitAndInventory(arg0);/*display character's portrait*/
		break;
		case OP_GETHORSE:/*(npc): mount a horse*/
			arg0 = mk_npcnum(parse_factor());
			MountHorse(arg0, MapX, MapY, OBJ_MakeDirFrame(OBJ_MOUNTED_HORSE, GetDirection(arg0)), 0);
		break;
		default:
			CON_printf(/*09DB*/"Unknown command.\n");
			return -1;
	}
	return 0;
}

/*execute statements (printing text bytes) until an input/section
opcode, 0, OP_ENDRES or OP_LEAVE; Talk_PC is left on the stop opcode*/
static /*C_1703_1C45*/parse_statement(int16 objNum) {
	unsigned char opcode;

	opcode = PARSE_U8;
	while(
		opcode != OP_ASKTOP &&
		opcode < OP_FIRST_SECTION && opcode != 0 &&
		opcode != OP_GETSTR && opcode != OP_GET &&
		opcode != OP_ENDRES
	) {
		if(opcode < 0x80) {
			if(opcode >= ' ' || opcode == '\n') {
				CON_putch(opcode);
			} else {
				CON_printf(/*09ED*/"\nTry to print out a control char\n");
				break;
			}
		} else if(opcode == OP_KEY) {
			/*only the first matching KEY block of an input answers*/
			if(TalkAnswered == 0) {
				TALK_MatchKeywords(objNum);
			} else {
				while(opcode != OP_ENDRES)
					opcode = PARSE_U8;
			}
		} else {
			execute_op(opcode, objNum);
		}
		if(mustLeave)
			break;
		opcode = PARSE_U8;
	}
	Talk_PC --;
}

/*KEY/RES: Talk_PC is just after OP_KEY. Compares each comma separated
keyword (TalkKeyword) with the player's input (TalkInput; "*" matches anything);
on a match, runs the answer after OP_RES (and sets TalkAnswered), otherwise
skips to the next OP_KEY/OP_ENDRES/OP_SCRIPT_MARK*/
/*C_1703_1D01*/static TALK_MatchKeywords(int16 objNum) {
	int16 op, len;
	int16 moreKeys;

	moreKeys = 1;
	if(TalkInput[0] == 0)
		strcpy(TalkInput, /*0A0F*/"bye");
	while(moreKeys) {
		moreKeys = 0;
		for(len = 0; (TalkKeyword[len] = PARSE_U8) != ',' && TalkKeyword[len] != OP_RES; len ++);
		if(TalkKeyword[len] == ',')
			moreKeys = 1;
		else
			Talk_PC --;
		TalkKeyword[len] = 0;
		if(str_i_compare(TalkKeyword, TalkInput, strlen(TalkKeyword)) || TalkKeyword[0] == '*') {
			while(PARSE_U8 != OP_RES);
			CON_printf(/*098E + 0Bh*/"\n");
			TalkAnswered = 1;
			parse_statement(objNum);
			return;
		}
	}
	/*no match: skip the answer*/
	do {
		op = PARSE_U8;
		if(op == OP_GOTO)
			Talk_PC += 4;
		else if(op == OP_BYTE)
			Talk_PC += 1;
		else if(op == OP_WORD)
			Talk_PC += 2;
	} while(op != OP_KEY && op != OP_ENDRES && op != OP_SCRIPT_MARK);
	if(op == OP_KEY || op == OP_SCRIPT_MARK)
		Talk_PC --;
}

/*conversation entry point: <speaker> (a party member) talks to <npcObj>.
Checks whether the npc can answer, loads its script, prints the
description, runs the main section, then loops reading the player's
input and running the matching keyword answers until OP_LEAVE/OP_SCRIPT_MARK*/
/*C_1703_1E23*/TalkDriver(int16 speaker, int16 npcObj) {
	int16 idx, number;
	int16 opcode, dir, savedStatus, savedMember, objTyp;

	TalkNpcs[1] = speaker;
	TalkNpcs[0] = npcObj;
	if(IsPlrControl(TalkNpcs[0]) && !IsNpcOnScreen(TalkNpcs[0])) {
		CON_printf(/*0A13*/"\nNot on screen.\n");
		return;
	}
	if(SoloMember != -1 && SoloMember != 0) {
		CON_printf(/*0A24*/"\nNot in solo mode.\n");
		return;
	}
	objTyp = GetType(npcObj);
	/*statue or shrine: its quality is the npc number of the script to use*/
	if(objTyp == OBJ_STATUE_OF_MONDAIN || objTyp == OBJ_STATUE_OF_MINAX || objTyp == OBJ_STATUE_OF_EXODUS || objTyp == OBJ_SHRINE)
		TalkNpcs[0] = GetQual(npcObj);

	if(TalkNpcs[0] == 0 || TalkNpcs[0] >= 0xe0) {
		if(IsDead(TalkNpcs[0]) && SeanceFlag) {
			CON_printf(/*0A38*/"\nYou hear a deep moan.\n");
			return;
		}
	}

	if(TalkNpcs[0] == npcObj) {
		if(
			(IsDead(TalkNpcs[0]) && !SeanceFlag) ||
			IsAsleep(TalkNpcs[0]) || IsParalyzed(TalkNpcs[0]) ||
			NPCMode[TalkNpcs[0]] == AI_VIGILANTE ||
			NPCMode[TalkNpcs[0]] == AI_FEAR ||
			NPCMode[TalkNpcs[0]] == AI_RETREAT ||
			NPCMode[TalkNpcs[0]] == AI_ARREST ||
			GetAlignment(TalkNpcs[0]) == EVIL ||
			GetAlignment(TalkNpcs[0]) == CHAOTIC
		) {
			CON_printf(/*0A50*/"\n\nNo response\n");
			return;
		}
	}
	if(IsArmageddon) {
		CON_printf(/*0A50+1*/"\nNo response\n");
		return;
	}
	if(TalkNpcs[1] == TalkNpcs[0]) {
		CON_printf(/*0A5F*/"\nTalking to yourself?\n");
		return;
	}

	/*face the npc*/
	dir = MkDirection(PointerX, PointerY);
	if(dir != -1) {
		UpdateMoveFrame(Party[Active], dir);
		VIS_UpdateView();
	}

	/*temporary npcs (>= 0xe0) only talk if guard, wisp or gargoyle*/
	if(
		TalkNpcs[0] == 0 ||
		(TalkNpcs[0] >= 0xe0 && objTyp != OBJ_GUARD && objTyp != OBJ_WISP && objTyp != OBJ_GARGOYLE && objTyp != OBJ_WINGED_GARGOYLE) ||
		!LoadConversation(TalkNpcs[0], TalkBuf)
	) {
		CON_printf(/*0A76*/"\nFunny, no response.\n");
		return;
	}
	TALK_SetNearbyPartyVar();/*compute #N*/
	Talk_PC = 0;
	/*skip OP_SCRIPT_MARK ncpnum*/
	idx = 2;
	/*get the npc name: $N*/
	while((opcode = TalkBuf[idx++]) != OP_DESC)
		VarStr['N' - 0x37][idx - 3] = opcode;
	VarStr['N' - 0x37][idx - 3] = 0;
	/* */
	IsInConversation = 1;
	savedStatus = StatusDisplay;
	savedMember = StatusMember;
	if(!SeanceFlag) {
		if(GetType(npcObj) != OBJ_SHRINE)
			ShowPortraitAndInventory(npcObj);/*display character's portrait*/
		CON_printf(/*0A8C*/"\n\nYou see ");
		while(PARSE_U8 != OP_DESC);
		/*remember the description for "look"*/
		TalkDescPC = Talk_PC;
		parse_statement(TalkNpcs[0]);
	}
	CON_printf(/*098E + 0Bh*/"\n");
	while(PARSE_U8 != OP_MAIN);

	opcode = PARSE_U8;
	if(opcode != OP_ASKTOP) {
		Talk_PC --;
		parse_statement(TalkNpcs[0]);
	} else {
		Talk_PC --;
	}

	/*main loop: get some input, then run the keyword blocks*/
	while(!mustLeave) {
		TalkSavedPC = Talk_PC;
		opcode = PARSE_U8;
		if(opcode == OP_ASKTOP) {
			CON_printf(/*0A97*/"\nyou say:");
			CON_gets(TalkInput, 50);
			CON_printf(/*098E + 0Bh*/"\n");
			TalkAnswered = 0;
		} else if(opcode == OP_GETSTR) {/*<index> OP_VARSTR*/
			CON_gets(TalkInput, 50);
			idx = PARSE_U8;
			if(PARSE_U8 != OP_VARSTR)
				break;
			strcpy(VarStr[idx], TalkInput);
			CON_printf(/*0AA1*/"\n\n");
			TalkAnswered = 0;
		} else if(opcode == OP_GETINT) {/*<index> OP_VARINT*/
			CON_gets(TalkInput, 50);
			idx = PARSE_U8;
			if(PARSE_U8 != OP_VARINT)
				break;
			number = ParseNumberInput(TalkInput, 0);
			VarInt[idx] = (number == -1 || number == -2)?0:number;
			CON_printf(/*0AA1*/"\n\n");
			TalkAnswered = 0;
		} else if(opcode == OP_GETDIGIT) {/*<index> OP_VARINT*/
			do {
				TalkInput[0] = CON_getch();
				TalkInput[1] = 0;
				number = ParseNumberInput(TalkInput, 0);
				if(TalkInput[0] == '\r' || TalkInput[0] == 27)
					number = 0;
			} while(number == -1 || number == -2);
			idx = PARSE_U8;
			if(PARSE_U8 != OP_VARINT)
				break;
			VarInt[idx] = number;
			CON_printf(/*0AA4*/"%d\n\n", number);
			TalkAnswered = 0;
		} else if(opcode == OP_GETCHR) {
			TalkInput[0] = CON_getch();
			TalkInput[1] = 0;
			CON_printf(/*0AA9*/"%s\n\n", TalkInput);
			TalkAnswered = 0;
		} else if(opcode == OP_WAIT) {
			CON_getch();
		} else if(opcode == OP_GET) {/*OP_GET c0 .. cn OP_KEY -- GET [...]*/
			/*wait for one of the allowed keys*/
			for(idx = 0; (opcode = PARSE_U8) != OP_KEY; idx ++)
				TalkKeyword[idx] = opcode;
			Talk_PC --;
			TalkKeyword[idx] = 0;
			do {
				opcode = CON_getch();
				for(idx = 0; TalkKeyword[idx] && toupper(TalkKeyword[idx]) != opcode; idx++);
			} while(TalkKeyword[idx] == 0);
			if(opcode >= ' ')
				CON_printf(/*0AAE*/"%c", opcode);
			TalkInput[0] = opcode;
			TalkInput[1] = 0;
			CON_printf(/*098E + 0Bh*/"\n");
			TalkAnswered = 0;
		} else if(opcode == OP_KEY) {
			CON_printf(/*0AB1*/"\nCommand for input key expected.");
			break;
		} else if(opcode == OP_SCRIPT_MARK) {
			break;
		} else if(opcode == OP_ENDRES) {
			/**/
		} else {
			CON_printf(/*0AD2*/"\nLogical error occured.");
			break;
		}
		/*$Z is the last player's input*/
		VarStr['Z' - 0x37] = TalkInput;

		if(stricmp(TalkInput, /*0AEA*/"look") == 0) {
			/*replay the description, then resume*/
			CON_printf(/*0A8C + 2*/"You see ");
			Talk_PC = TalkDescPC;
			parse_statement(TalkNpcs[0]);
			Talk_PC = TalkSavedPC;
		} else {
			parse_statement(TalkNpcs[0]);
		}
	}
	mustLeave = 0;
	IsInConversation = 0;
	/*restore the status display (the displayed member may have left)*/
	if(PartySize == StatusMember) {
		StatusMember = 0;
		PartyListTop = 0;
		StatusDisplay = CMD_STAT_PARTY;
	} else {
		StatusDisplay = savedStatus;
		StatusMember = savedMember;
	}
	StatusDirty ++;
}
