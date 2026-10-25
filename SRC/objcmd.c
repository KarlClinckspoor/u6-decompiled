/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	"look" "drop" "get" "move" "use" module

	Implements the object-manipulation commands of the main command loop
	(called from gameloop.c): CmdLook "look" (also searching, reading
	books/signs, spellbook pages, clocks), CmdDrop "drop" (throwing,
	breakable objects), CmdGet "get" (weight checks, stealing),
	CmdMove "move" (pushing on the map, giving to party members,
	moving into/out of containers) and CmdUse "use", which
	dispatches on the object type to one handler per usable object
	(doors, chests, keys, torches, potions, instruments, cannon, powder
	kegs, drawbridge cranks, levers, vehicles, horses, balloon, crystal
	ball, telescope, runes, vortex cube, ...).
	Object type lists (containers ContainerTypes, breakables BreakableTypes, usables
	UsableTypes, books BookTypes, signs SignTypes) drive the generic checks.
	Some handlers (door/chest locking, trap, drawbridge, explosions,
	horse mounting, "peer") are also used by the spell code (spell.c),
	the AI and the conversation code.
*/
#include "u6.h"

#undef abs
#include <math.h>

/*party member object for party slot partyIdx; when the party is in a
 vehicle the whole party is represented by Party[0] (the vehicle)*/
/*C_27A1_0008*/static GetPartyObjOrVehicle(int partyIdx) {
	if(IN_VEHICLE)
		return Party[0];
	else
		return Party[partyIdx];
}

/*X of objNum, or X of object 0 (the party's vehicle) when in a vehicle*/
/*C_27A1_0028*/static GetObjXOrVehicle(int objNum) {
	if(IN_VEHICLE)
		return GetX(0);
	else
		return GetX(objNum);
}

/*Y of objNum, or Y of object 0 (the party's vehicle) when in a vehicle*/
/*C_27A1_0051*/static GetObjYOrVehicle(int objNum) {
	if(IN_VEHICLE)
		return GetY(0);
	else
		return GetY(objNum);
}

/*container types*/
/*1C00*/int ContainerTypes[] = {
	OBJ_OPEN_CHEST,OBJ_BACKPACK,OBJ_SPELLBOOK,
	OBJ_OPEN_BARREL,OBJ_BAG,OBJ_BASKET,
	OBJ_OPEN_CRATE,OBJ_DEAD_BODY,OBJ_VORTEX_CUBE
};

/*is typeObject a container (one of ContainerTypes)? returns 1/0*/
/*C_27A1_0082*/IsContainerType(int typeObject) {
	int i;

	for(i = 0; i < 9; i ++)
		if(ContainerTypes[i] == typeObject)
			return 1;
	return 0;
}

/*can itemObj be put into containerObj?
 containerObj must be a container type other than spellbook/dead body
 (those are handled separately), itemObj must be an item (>= 0x100);
 chests/barrels/crates must be open (frame 0) and can not themselves be
 put into another container. returns 1/0*/
/*C_27A1_00A9*/CanPutInContainer(int containerObj, int itemObj) {
	int objTyp;

	if(containerObj != -1) {
		objTyp = GetType(containerObj);
		if(
			IsContainerType(objTyp) &&
			itemObj >= 0x100 &&
			objTyp != OBJ_SPELLBOOK && objTyp != OBJ_DEAD_BODY
		) {
			if((objTyp != OBJ_OPEN_CHEST && objTyp != OBJ_OPEN_BARREL && objTyp != OBJ_OPEN_CRATE) || GetFrame(containerObj) == 0) {
				objTyp = GetType(itemObj);
				if(objTyp != OBJ_OPEN_CHEST && objTyp != OBJ_OPEN_BARREL && objTyp != OBJ_OPEN_CRATE)
					return 1;
			}
		}
	}
	return 0;
}

/*types that break when thrown*/
/*1C12*/int BreakableTypes[] = {
	OBJ_FLASK_OF_OIL,OBJ_SILVER_SNAKE_VENOM,OBJ_CRYSTAL_BALL,OBJ_MIRROR,OBJ_WINE,
	OBJ_MEAD,OBJ_ALE,OBJ_WINEGLASS,OBJ_PLATE,OBJ_MUG,
	OBJ_HONEY_JAR,OBJ_JAR_OF_HONEY,OBJ_POTION,OBJ_WATER_VASE,OBJ_DRAGON_EGG
};

/*does an object of type typeObject break when thrown (one of BreakableTypes)?*/
/*C_27A1_012C*/static IsBreakableType(int typeObject) {
	int i;

	for(i = 0; i < 15; i ++) {
		if(BreakableTypes[i] == typeObject)
			return 1;
	}
	return 0;
}

/*is objNum (and everything it contains) flagged OWNED, i.e. "ok to
 take"? taking an object without the flag is stealing (see "get")*/
static /*C_27A1_0153*/IsOkToGet(int objNum) {
	int child;

	if(!IsOwned(objNum))
		return 0;
	for(child = Link[objNum]; child >= 0 && GetCoordUse(child) != LOCXYZ && OBJ_IsInside(child, objNum); child = Link[child]) {
		if(!IsOwned(child))
			return 0;
	}
	return 1;
}

/*set the OWNED ("ok to take") flag on objNum and all its contents*/
/*C_27A1_019E*/SetOkToGet(int objNum) {
	int child;

	SetOwned(objNum);
	for(child = Link[objNum]; child >= 0 && GetCoordUse(child) != LOCXYZ && OBJ_IsInside(child, objNum); child = Link[child])
		SetOwned(child);
}

/*usable types (roughly in alphabetical order)*/
/*1C30*/int UsableTypes[] = {
	OBJ_ALE,OBJ_BALLOON_INFLATED,OBJ_BALLOON_PLANS,OBJ_OPEN_BARREL,OBJ_BELL,OBJ_BEEHIVE,OBJ_BRAZIER,OBJ_LOAF_OF_BREAD,OBJ_BUCKET_OF_MILK,OBJ_BUTTER,
	OBJ_CAKE,OBJ_CAMPFIRE,OBJ_CANDLE,OBJ_CANDELABRA,OBJ_CANNON,OBJ_CEDAR_DOOR,OBJ_CHEESE,OBJ_OPEN_CHEST,OBJ_HORSE_CHOPS,OBJ_CHURN,
	OBJ_COW,OBJ_CRANK,OBJ_OPEN_CRATE,OBJ_CRYSTAL_BALL,OBJ_BALLOON_DEFLATED,OBJ_FAN,OBJ_FIREPLACE,OBJ_FISH,OBJ_FISHING_POLE,OBJ_FOUNTAIN,
	OBJ_GARGISH_VOCABULARY,OBJ_GEM,OBJ_BUNCH_OF_GRAPES,OBJ_HAM,OBJ_HARP,OBJ_HARPSICHORD,OBJ_HORSE,OBJ_MOUNTED_HORSE,OBJ_JAR_OF_HONEY,OBJ_LOCKPICK,
	OBJ_LADDER,OBJ_LEVER,OBJ_LUTE,OBJ_MEAD,OBJ_MEAT_RIB,OBJ_MOONSTONE,OBJ_PORTION_OF_MEAT,OBJ_OAKEN_DOOR,OBJ_ORB_OF_THE_MOONS,OBJ_PANPIPES,
	OBJ_PASSTHROUGH_116,OBJ_PASSTHROUGH_118,OBJ_PICK,OBJ_POTION,OBJ_POWDER_KEG,OBJ_PULL_CHAIN,OBJ_RAFT,OBJ_RIBS,OBJ_ROLLS,OBJ_RUBBER_DUCKY,
	OBJ_RUNE_OF_HONESTY,OBJ_RUNE_OF_COMPASSION,OBJ_RUNE_OF_VALOR,OBJ_RUNE_OF_JUSTICE,OBJ_RUNE_OF_SACRIFICE,OBJ_RUNE_OF_HONOR,OBJ_RUNE_OF_SPIRITUALITY,OBJ_RUNE_OF_HUMILITY,OBJ_SECRET_DOOR,OBJ_SEXTANT,
	OBJ_SHIP,OBJ_SHOVEL,OBJ_SILVER_HORN,OBJ_SKIFF,OBJ_STAFF,OBJ_STEEL_DOOR,OBJ_SWITCH,OBJ_TELESCOPE,OBJ_TORCH,OBJ_KEY,
	OBJ_VORTEX_CUBE,OBJ_WELL,OBJ_WINDOWED_DOOR,OBJ_WINE,OBJ_XYLOPHONE
};

/*can objects of type typeObject be "use"d (one of UsableTypes)?*/
/*C_27A1_01DE*/IsUsableType(int typeObject) {
	int i;

	for(i = 0; i < 85; i ++)
		if(UsableTypes[i] == typeObject)
			return 1;
	return 0;
}

/*parse a number typed by the player.
 coordMode 0: plain number; returns the value, -1 for an empty line
   accepted with <Enter>, -2 for an escaped line or a bad character.
 coordMode 1: latitude "<deg>N|S" (S positive, N negative),
 coordMode 2: longitude "<deg>E|W" (E positive, W negative);
   returns 100 when the direction letter is missing/wrong.*/
/*C_27A1_0205*/ParseNumberInput(unsigned char *str, int coordMode) {
	int i, value;

	value = 0;
	for(i = 0; i < strlen(str); i ++) {
		if(str[i] >= '0' && str[i] <= '9') {
			value = value * 10 + (str[i] - '0');
			continue;
		}
		if(coordMode == 1) {/*latitude*/
			if(toupper(str[i]) == 'S')
				return value;
			else if(toupper(str[i]) == 'N')
				return -value;
			else
				return 100;
		}
		if(coordMode == 2) {/*longitude*/
			if(toupper(str[i]) == 'E')
				return value;
			else if(toupper(str[i]) == 'W')
				return -value;
			else
				return 100;
		}
		return -2;
	}
	if(coordMode)
		return 100;
	/*GetsAborted is set by CON_gets when the input was escaped*/
	if(i == 0 && GetsAborted == 0)
		return -1;
	if(i == 0 && GetsAborted)
		return -2;
	return value;
}

/*display the portrait/equipment/inventory of objNum in the status
 area (used by "look" on a person and by conversations), wait for a key
 and restore the previous status display.
 Party members and monsters with readied items show their equipment;
 shrines and the 3 statues show the portrait given by their quality.*/
/*C_27A1_02D9*/ShowPortraitAndInventory(int objNum) {
	int slotOrType;
	int item, i, partyIdx, shrineTyp, isGeneric, savedInvenMember, showInven, savedStatus;
	char *nameStr;

	shrineTyp = 0;
	isGeneric = 0;
	showInven = 0;
	HideMouse
	savedStatus = StatusDisplay;
	NPC_2Handed = 0;
	if(IsPlrControl(objNum)) {
		/*party member: make him the displayed member (StatusMember)*/
		ShowNPCInven = 0;
		for(i = 0; i < 0x10; i ++) {
			if(Party[i] == objNum) {
				StatusMember = partyIdx = i;
				break;
			}
		}
	} else {
		ShowNPCInven = objNum;
	}

	/*fill Equipment[] with the readied objects (a two-handed weapon
	takes the right hand and marks the left one with 1)*/
	for(i = 0; i < 8; i ++)
		Equipment[i] = 0;
	for(item = FindInv(objNum); item >= 0; item = NextInv()) {
		if(GetCoordUse(item) == EQUIP && GetType(item) != OBJ_EFFECT && GetType(item) != OBJ_CHARGE) {
			showInven = 1;
			slotOrType = STAT_GetEquipSlot(item);
			if(slotOrType == SLOT_2HND) {
				slotOrType = SLOT_RHND;
				Equipment[SLOT_LHND] = 1;
				if(IsPlrControl(objNum))
					Party_2Handed[partyIdx] = 1;
				else
					NPC_2Handed = 1;
			} else if(slotOrType == SLOT_RHND && Equipment[SLOT_RHND]) {
				slotOrType = SLOT_LHND;
			} else if(slotOrType == SLOT_LHND && Equipment[SLOT_LHND]) {
				slotOrType = SLOT_RHND;
			} else if(slotOrType == SLOT_RING) {
				if(Equipment[SLOT_RFNG])
					slotOrType = SLOT_LFNG;
				else
					slotOrType = SLOT_RFNG;
			}
			Equipment[slotOrType] = item;
		}
	}

	savedInvenMember = StatusMember;
	slotOrType = GetType(objNum);
	if(slotOrType == OBJ_SHRINE || slotOrType == OBJ_STATUE_OF_MONDAIN || slotOrType == OBJ_STATUE_OF_MINAX || slotOrType == OBJ_STATUE_OF_EXODUS)
		shrineTyp = GetQual(objNum);
	else if(slotOrType == OBJ_WISP || slotOrType == OBJ_GUARD || slotOrType == OBJ_GARGOYLE)
		isGeneric = 1;
	if(showInven || objNum < 0xe0 || shrineTyp || isGeneric) {
		GR_SET_COLOR(ColorPaper);
		GR_FILL_RECT(168, 6, 311, 103);
		CON_setClip(&TextWindows[1]);
		DrawSkyBar();
	}
	if(showInven && (objNum < 0xe0 || isGeneric)) {
		DrawPortrait(objNum, 248, 24);/*draw portrait*/
		DrawPaperDoll(192, 40);/*draw equipment*/
		STAT_DrawEquipment(0, 8);/*draw inventory*/
		StatusDirty ++;
	} else if(showInven) {
		DrawPaperDoll(224, 40);/*draw equipment*/
		STAT_DrawEquipment(32, 8);/*draw inventory*/
		StatusDirty ++;
	} else if(objNum < 0xe0 || isGeneric) {
		DrawPortrait(objNum, 216, 24);/*draw portrait*/
		StatusDirty ++;
	} else if(shrineTyp) {
		DrawPortrait(shrineTyp, 216, 24);/*draw portrait*/
		StatusDirty ++;
	}
	StatusMember = savedInvenMember;
	if(showInven || objNum < 0xe0 || shrineTyp || isGeneric) {
		if(objNum == 0)
			objNum = 0x82;/*pushme/pullyu?*/
		/*generic name unless already met; bug: "(!TalkFlags[] & 1)" is
		"TalkFlags[] == 0" instead of "!(TalkFlags[] & 1)" (met flag)*/
		if(shrineTyp || (!TalkFlags[objNum] & 1)/*bug?*/)
			nameStr = GetObjectString(objNum);
		else
			nameStr = TALK_GetCharName(objNum);
		CON_gotoxy((17 - strlen(nameStr)) / 2, 10);
		CON_printf(/*1D6D*/"%s\n", nameStr);
		StatusDisplay = CMD_STAT_TALK;
		if(!IsInConversation)
			CON_getch();
	}
	CON_setClip(&TextWindows[3]);
	ShowMouse
	ShowNPCInven = 0;
	NPC_2Handed = 0;
	StatusDisplay = savedStatus;
}

/*print the article ("a ", "an " or "the ") of tile numTile*/
/*C_27A1_061E*/PrintTileArticle(int numTile) {
	switch(TileFlag2[numTile] & TILE_FLAG2_ARTICLE) {
		case 0x40: CON_printf(/*1D71*/"a "); break;
		case 0x80: CON_printf(/*1D74*/"an "); break;
		case 0xc0: CON_printf(/*1D78*/"the "); break;
	}
}

/*1CDA*/int BookTypes[] = {
	OBJ_BOOK,
	OBJ_BOOK_OF_CIRCLES,
	OBJ_NOTE,
	OBJ_BALLOON_PLANS,
	OBJ_CODEX
};

/*is objNum a readable book (one of BookTypes)?*/
/*C_27A1_066D*/IsBook(int objNum) {
	int i, objTyp;

	objTyp = GetType(objNum);
	for(i = 0; i < 5; i ++) {
		if(BookTypes[i] == objTyp)
			return 1;
	}
	return 0;
}

/*1CE4*/int SignTypes[] = {
	OBJ_SIGN,
	OBJ_GARGOYLE_SIGN,
	OBJ_PICTURE,
	OBJ_CROSS,
	OBJ_TOMBSTONE
};

/*is objNum a readable sign (one of SignTypes)?*/
/*C_27A1_06A2*/IsSign(int objNum) {
	int i, objTyp;

	objTyp = GetType(objNum);
	for(i = 0; i < 5; i ++) {
		if(SignTypes[i] == objTyp)
			return 1;
	}
	return 0;
}

/*can objNum be read by the active character?
 books must be carried or adjacent; signs can be read from afar*/
/*C_27A1_06D7*/CanReadObj(int objNum/*bp06*/) {
	int x, y;

	x = GetX(Party[Active]);
	y = GetY(Party[Active]);
	if(IsBook(objNum)) {
		/*(tests Selection.obj, which is always objNum here)*/
		if(GetCoordUse(Selection.obj) != LOCXYZ)
			return 1;
		if(CLOSE_ENOUGH(1, GetX(objNum), GetY(objNum), x, y))
			return 1;
		else
			return 0;
	}
	if(IsSign(objNum))
		return 1;
	return 0;
}

/*print the text of book/sign objNum: its quality is the 1-based
 index in BOOK.DAT (a table of 16-bit offsets followed by the texts)*/
/*C_27A1_078F*/PrintBookText(int objNum) {
	int fd, bookNum;
	int bookBase, textOfs;

	MUS_PlaySong(0);
	bookBase = 0;
	bookNum = GetQual(objNum);
	CON_printf(/*1D7D*/"\n\n");
	DISK_confirm(DISK_POPULACE_A);
	fd = OSI_open(/*1D80*/"BOOK.DAT");
	/*offset table entry, then the text itself*/
	OSI_read(fd, (long)(bookBase + bookNum - 1) << 1, 2, &textOfs);
	OSI_read(fd, (long)textOfs, 0x2800, TalkBuf);
	/*text attribute 0x10 until the next prompt (EndPagingAtPrompt requests the reset)*/
	CurClip->attrFlags |= 0x10;
	CON_printf(TalkBuf);
	EndPagingAtPrompt = 1;
	OSI_close(fd);
	MUS_PlayAreaSong();
}

/*print the count (stackables, gold) or the article of objNum before
 its name; returns the count (1 for a single object)*/
/*C_27A1_0841*/PrintObjCountOrArticle(int objNum) {
	int count;
	int quanType;

	/*QuanType: 4 = gold (the whole Amount is the count),
	2 = stackable (low byte), 0 = single object*/
	quanType = QuanType(GetType(objNum));
	if(quanType == 4) {
		count = Amount[objNum];
		CON_printf(/*1D89*/"%d ", count);
	} else if(quanType == 2 && !IsLit(objNum)) {
		/*(a lit torch uses its quantity as burning time)*/
		count = GetQuan(objNum);
		CON_printf(/*1D89*/"%d ", count);
	} else {
		count = 1;
		PrintTileArticle(TILE_FRAME(objNum));
	}
	return count;
}

/*is objNum a closed chest/crate/barrel or a spellbook, i.e. a
 container whose content can not be found by searching?*/
/*C_27A1_08D3*/IsClosedContainer(int objNum) {
	int objTyp, objFrm;

	objTyp = GetType(objNum);
	objFrm = GetFrame(objNum);
	if(objTyp == OBJ_OPEN_CHEST || objTyp == OBJ_OPEN_CRATE || objTyp == OBJ_OPEN_BARREL) {
		if(objFrm)
			return 1;
	}
	if(objTyp == OBJ_SPELLBOOK)
		return 1;
	return 0;
}

/*first object at objNum's location (other than objNum) whose tile is not
 flagged "ignore", skipping NPCs; -1 if none.
 (used to select the real object under an ignorable one)*/
/*C_27A1_0919*/FindNonIgnoredObjAt(int objNum) {
	int other;

	for(other = FindLoc(GetX(objNum), GetY(objNum), MapZ); other >= 0; other = NextLoc()) {
		if(other == objNum)
			continue;
		if(!IsTileIg(TILE_FRAME(other))) {
			if(other > 0xff)
				break;
		}
	}
	return other;
}

/*"search" objNum at (x,y) (done after looking at something adjacent):
 reveals a closed secret door, or empties the content of an open
 container on the ground and lists what was found*/
/*C_27A1_09A1*/SearchObj(int objNum, int x, int y) {
	int item;
	int actor, actorX, actorY, objTyp, objFrm;
	int found, nextItem;

	found = 0;
	actor = Party[Active];
	actorX = GetX(actor);
	actorY = GetY(actor);
	objTyp = GetType(objNum);
	objFrm = GetFrame(objNum);
	if(!CLOSE_ENOUGH(1, x, y, actorX, actorY))
		return;
	if(objNum < 0x100)
		return;
	if(IsTerrainDamage(TILE_FRAME(objNum)))
		return;
	if(objTyp == OBJ_SPELLBOOK)
		return;
	CON_printf(/*1D8D*/"\nSearching here, you find ");
	if(objTyp == OBJ_SECRET_DOOR) {
		/*frame bit 0 set = discovered/open*/
		if(!(objFrm & 1)) {
			CON_printf(/*1DA8*/"a secret door");
			SetFrame(objNum, objFrm ^ 1);
			VIS_UpdateView();
			DisplayTick();
			found = 1;
		}
	} else {
		if(IsTileIg(TILE_FRAME(objNum))) {
			objNum = FindNonIgnoredObjAt(objNum);
			objTyp = GetType(objNum);
			objFrm = GetFrame(objNum);
		}
		if(!IsClosedContainer(objNum)) {
			/*drop every visible content object (not charges/effects)
			on the container's tile*/
			item = FindInv(objNum);
			while(item >= 0) {
				objTyp = GetType(item);
				if(objTyp != OBJ_CHARGE && objTyp != OBJ_EFFECT) {
					MoveObj(item, GetX(objNum), GetY(objNum), MapZ);
					MoveObj(actor, actorX, actorY, MapZ);
					VIS_UpdateView();
					DisplayTick();
					/*the list changed: restart from the first remaining
					visible content to know whether this is the last one*/
					for(
						nextItem = FindInv(objNum);
						nextItem >= 0 && (GetType(nextItem) == OBJ_CHARGE || GetType(nextItem) == OBJ_EFFECT);
						nextItem = NextInv()
					);
					if(found && nextItem >= 0)
						CON_printf(/*1DB6*/", ");
					else if(found && nextItem == -1)
						CON_printf(/*1DB9*/" and ");
					PrintObjCountOrArticle(item);
					CON_printf((char *)GetObjectString(item));
					found = 1;
					item = nextItem;
					continue;
				}
				item = NextInv();
			}
		}
	}
	if(!found) {
		CON_printf(/*1DBF*/"nothing.\n");
		return;
	}
	CON_printf(/*1DC6*/".\n");
}

/*"look" command: describe the selected object/tile (name, weight,
 damage/armor points), read books and signs, show spellbook pages
 and clocks/sundials, then search the place if adjacent*/
/*C_27A1_0C67*/CmdLook() {
	int objTyp, weight;
	unsigned char numTile, ch;
	int x, y, i, dir;
	int numOrDamage, armor, savedStatus, spellNum, savedActive;
	int owner;
	unsigned char reagents;
	int other, readSomething, tileNum;

	readSomething = 0;
	CurClip->attrFlags |= 0x10;
	CON_printf(/*1DC9*/"Thou dost see ");
	dir = MkDirection(PointerX, PointerY);
	if(dir != -1) {
		UpdateMoveFrame(Party[Active], dir);
		VIS_UpdateView();
	}
	/*SelectionIsDark: the pointed tile is hidden/dark*/
	if(SelectionIsDark) {
		CON_printf(/*1DD8*/"darkness.\n");
		EndPagingAtPrompt = 1;
		return;
	}
	x = GetX(Party[Active]);
	y = GetY(Party[Active]);
	if(Selection.obj == -1 || IsInvisible(Selection.obj)) {
		/*no object: describe the map tile under the pointer
		(inside the 11x11 map window)*/
		if(PointerX < 168 && PointerY < 168) {
			Selection.x = MapX - 5 + SCR2TIL(PointerX);
			Selection.y = MapY - 5 + SCR2TIL(PointerY);
			numTile = GetTileAtXYZ(Selection.x, Selection.y, MapZ);
			PrintTileArticle(numTile);
			CON_printf((char *)GetTileString(numTile));
		} else {
			CON_printf(/*1DE3*/"nothing");
		}
		CON_printf(/*1DC6*/".\n");
		if(CLOSE_ENOUGH(1, Selection.x, Selection.y, x, y))
			CON_printf(/*1DEB*/"\nSearching here, you find nothing.\n");
		EndPagingAtPrompt = 1;
		return;
	}
	objTyp = GetType(Selection.obj);
	if(Selection.obj < 0x100 || objTyp == OBJ_HORSE)
		Selection.obj = COMBAT_getHead(Selection.obj);
	if(objTyp == OBJ_STATUE_OF_MONDAIN || objTyp == OBJ_STATUE_OF_MINAX || objTyp == OBJ_STATUE_OF_EXODUS || Selection.obj < 0x100) {
		/*a person/creature (or statue): name and portrait*/
		if(!IsPlrControl(Selection.obj) || IN_VEHICLE)
			PrintObjCountOrArticle(Selection.obj);
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_putch('\n');
		if(!IN_VEHICLE)
			ShowPortraitAndInventory(Selection.obj);
	} else {
		tileNum = TILE_FRAME(Selection.obj);
		numOrDamage = PrintObjCountOrArticle(Selection.obj);
		/*right/bottom half of a 2-tile sign: use the main part*/
		if(tileNum == TIL_SIGN_4)
			Selection.obj = FindLoc(Selection.x, Selection.y - 1, MapZ);
		else if(tileNum == TIL_SIGN_5)
			Selection.obj = FindLoc(Selection.x - 1, Selection.y, MapZ);
		CON_printf((char *)GetObjectString(Selection.obj));
		/*weights are in tenths of stones*/
		weight = GetWeight(Selection.obj) + Encumbrance(Selection.obj);
		if(weight != 0 && weight < 255) {
			if(numOrDamage == 1)
				CON_printf(/*1E0F*/". It weighs");
			else
				CON_printf(/*1E1B*/". They weigh");
			CON_printf(/*1E28*/" %d.%d stones", weight / 10, weight % 10);
		}
		numOrDamage = GetDamagePoints(Selection.obj);
		armor = GetArmorPoints(Selection.obj);
		if(numOrDamage > 0) {
			if(!(weight != 0 && weight < 255))
				CON_printf(/*1E36*/". It");
			else if(armor)
				CON_printf(/*1E3B*/",");
			else
				CON_printf(/*1E3D*/" and");
			CON_printf(/*1E42*/" can do %d point\\s of damage", numOrDamage);
		}
		if(armor > 0)
			CON_printf(/*1E5F*/" and can absorb %d point\\s of damage", armor);
		if(CanReadObj(Selection.obj)) {
			if(GetCoordUse(Selection.obj) != LOCXYZ) {
				if(GetQual(Selection.obj)) {
					if(!readSomething)
						CON_printf(/*1E84*/":");
					PrintBookText(Selection.obj);
					readSomething = 1;
				}
			} else {
				/*on the map: read every text object on that tile*/
				for(other = FindLoc(Selection.x, Selection.y, MapZ); other >= 0; other = NextLoc()) {
					if(GetQual(other) == 0)
						continue;
					if(!readSomething)
						CON_printf(/*1E84*/":");
					PrintBookText(other);
					readSomething = 1;
				}
			}
			CON_putch('\n');
		} else if(
			objTyp == OBJ_SPELLBOOK && FindInv(Selection.obj) >= 0 &&
			(
				CLOSE_ENOUGH(1, Selection.x, Selection.y, x, y) ||
				GetCoordUse(Selection.obj) != LOCXYZ
			)
		) {
			/*browse the spellbook (as its owner); selecting a spell
			shows its words of power and reagents*/
			Spellbook[0] = Selection.obj;
			Spellbook[1] = 0;
			savedActive = Active;
			owner = GetAssoc(Selection.obj);
			for(i = 0; i < 8; i ++) {
				if(Party[i] == owner) {
					Active = i;
					break;
				}
			}
			SPELL_RefreshList();
			SPELL_DrawBG();
			SPELL_RefreshPage();
			savedStatus = StatusDisplay;
			StatusDisplay = CMD_STAT_SPELLBOOK;
			ch = CON_getch();
			if(ch == CMD_TARGET && Selection.obj != -1) {
				/*here Selection.obj is the index in the page's spell list*/
				spellNum = SpellbookPage[Selection.obj];
				CON_printf(/*1D7D*/"\n\n");
				CON_printf(/*1E86*/"%s-", SpellName[spellNum]);
				i = 0;
				ch = SpellCode[spellNum][i++];
				CON_printf(/*1E8A*/"%s", MagicWords[ch - 'A']);
				while(ch = SpellCode[spellNum][i++])
					CON_printf(/*1E8D*/" %s", MagicWords[ch - 'A']);
				CON_printf(/*1E91*/"\nIngredients: \n");
				/*one bit per reagent*/
				reagents = Reagents_needed[spellNum];
				if(reagents == 0)
					CON_printf(/*1EA1*/"None\n");
				for(i = 0; i < 8; i ++) {
					if((reagents >> i) & 1)
						CON_printf(/*1EA7*/" %s\n", Reagents_name[i]);
				}
			} else {
				CON_printf(/*1DC6*/".\n");
			}
			Active = savedActive;
			StatusDisplay = savedStatus;
			StatusMember = 0;
			StatusDirty = 1;
		} else if(
			objTyp == OBJ_CLOCK ||
			(objTyp == OBJ_SUNDIAL && Time_H >= 5 && Time_H < 20)/*sundial: daytime only*/
		) {
			CON_printf(/*1EAC*/"\n\nThe time is ");
			if(Time_H == 0)
				CON_printf(/*1EBB*/"12:%02d A.M.\n", Time_M);
			else if(Time_H < 12)
				CON_printf(/*1EC9*/"%d:%02d A.M.\n", Time_H, Time_M);
			else if(Time_H == 12)
				CON_printf(/*1ED7*/"12:%02d P.M.\n", Time_M);
			else
				CON_printf(/*1EE5*/"%d:%02d P.M.\n", Time_H - 12, Time_M);
		} else {
			CON_printf(/*1DC6*/".\n");
		}
	}
	if(!readSomething)
		SearchObj(Selection.obj, Selection.x, Selection.y);
	EndPagingAtPrompt = 1;
}

/*is there an object at (x,y) with a "supporting" tile (e.g. a table)
 on which things can be placed? (stops at an impassable object)*/
/*C_27A1_1330*/static HasSupportAt(int x, int y) {
	int objNum;

	for(objNum = FindLoc(x, y, MapZ); objNum >= 0; objNum = NextLoc()) {
		if(IsTileSu(TILE_FRAME(objNum)))
			return 1;
		if(IsTerrainImpass(TILE_FRAME(objNum)))
			return 0;
	}
	return 0;
}

/*ask "How many?" for a stack of objNum (not when readied);
 returns the quantity chosen (all on empty input, 0 if invalid),
 1 for a single object*/
/*C_27A1_13BE*/AskHowMany(int objNum) {
	int answer, available;
	char buf[5];
	int quanType, cursorY, savedSel, savedSelMode;

	quanType = QuanType(GetType(objNum));
	if(quanType && GetCoordUse(objNum) != EQUIP) {
		if(quanType == 2)
			available = GetQuan(objNum);
		else
			available = Amount[objNum];
		if(available != 1) {
			savedSel = Selection.obj;
			cursorY = CurClip->curY;
			CON_printf(/*1EF3*/"How many? ");
			savedSelMode = (char)SelectMode;
			DrawSlotCursor(0);
			SelectMode = 0;
			CON_gets(buf, 4);
			SelectMode = savedSelMode;
			Selection.obj = savedSel;
			answer = ParseNumberInput(&buf, 0);
			if(answer == -1) {
				CON_gotoxy(10, cursorY);
				CON_printf(/*1EFE*/"%d\n", available);
				return available;
			}
			if(answer > available || answer == 0 || answer == -2) {
				CON_gotoxy(10, cursorY);
				CON_printf(/*1F02*/"0   \n");
				return 0;
			}
			CON_putch('\n');
			return answer;
		}
	}
	answer = 1;
	return answer;
}

/*"drop" command: drop/throw the selected inventory object of the
 displayed party member to a location within 8 tiles; fragile objects
 break when thrown farther than an adjacent tile*/
/*C_27A1_14DA*/CmdDrop() {
	int obj, actor;
	int objTyp, destX, destY, newObj, target;
	struct coord xyz;
	int qtyCode, count, quanType, cantDrop;

	if(Selection.obj == -1 || GetCoordUse(Selection.obj) == LOCXYZ || Selection.obj == InvenOwner) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
		return;
	}
	CON_printf((char *)GetObjectString(Selection.obj));
	CON_putch('\n');
	objTyp = GetType(Selection.obj);
	quanType = QuanType(objTyp);
	actor = Party[StatusMember];
	obj = Selection.obj;
	cantDrop = STAT_CheckUnready(obj, actor);
	if(cantDrop == 0) {
		count = AskHowMany(Selection.obj);
		if(count == 0)
			return;
		CON_printf(/*1F08*/"Location: ");
		MouseMode = 1;
		if(SelectMode != 0) {
			DrawSlotCursor(0);
			SelectMode = 1;
		}
		CON_getch();
		CON_putch('\n');
		/*target: top object at the destination (maybe a container)*/
		target = FindLoc(Selection.x, Selection.y, MapZ);
		if(Selection.x == -1 || Selection.y == -1) {
			CON_printf(/*1F13*/"\n%s\n", SpellMessages[6]);
			return;
		}
		if(!CanMoveTo(obj, Selection.x, Selection.y) && !HasSupportAt(Selection.x, Selection.y)) {
			if(
				target == -1 ||
				(target != -1 && !CanPutInContainer(target, obj))
			) {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[6]);
				return;
			}
		}
		/*is there a clear line of throw (no animation)?*/
		if(!COMBAT_Missile(actor, &Selection.x, &Selection.y, 8, 0)) {
			CON_printf(/*1F13*/"\n%s\n", SpellMessages[6]);
			return;
		}
		/*unready it*/
		STAT_StopEquipEffect(obj, actor);
		if(GetCoordUse(obj) == EQUIP) {
			Equipment[STAT_FindEquipSlot(obj)] = 0;
			if(STAT_GetEquipSlot(obj) == SLOT_2HND) {
				Party_2Handed[StatusMember] = 0;
				Equipment[SLOT_LHND] = 0;
			}
		}
		STAT_ComputeWeights(actor);
		SubMov(actor, 3);
		/*animate the throw; destX/destY = where it lands*/
		destX = Selection.x;
		destY = Selection.y;
		COMBAT_Missile(actor, &destX, &destY, 8, TILE_FRAME(obj));
		if(
			!CLOSE_ENOUGH(1, Selection.x, Selection.y, GetX(actor), GetY(actor)) &&
			IsBreakableType(objTyp)
		) {
			if(quanType != 0) {
				if(quanType == 2)
					qtyCode = QualQuan(GetQual(obj), count);
				else
					qtyCode = count;
				TakeObj(actor, objTyp, qtyCode);
			} else {
				/*broken dragon egg / broken mirror remain on the ground*/
				if(objTyp == OBJ_DRAGON_EGG) {
					SetFrame(obj, 1);
					MoveObj(obj, destX, destY, MapZ);
					SetOkToGet(obj);
				} else if(objTyp == OBJ_MIRROR) {
					SetFrame(obj, 2);
					MoveObj(obj, destX, destY, MapZ);
					SetOkToGet(obj);
				} else {
					DeleteObj(obj);
				}
			}
			MUS_PlaySfx(15, 0);
			CON_printf(/*1F18*/"\nIt broke!\n");
		} else {
			if(CanPutInContainer(target, obj) && !HasSupportAt(Selection.x, Selection.y)) {
				/*into the container at the destination*/
				if(quanType) {
					if(quanType == 2)
						qtyCode = QualQuan(GetQual(obj), count);
					else
						qtyCode = count;
					newObj = GiveObj(target, objTyp, qtyCode);
					if(newObj != -1)
						SetOkToGet(newObj);
					TakeObj(actor, objTyp, qtyCode);
				} else {
					InsertObj(obj, target, CONTAINED);
					SetOkToGet(obj);
				}
			} else {
				/*on the ground (a new stack for a partial quantity)*/
				if(quanType) {
					if(quanType == 2)
						qtyCode = QualQuan(GetQual(obj), count);
					else
						qtyCode = count;
					SetCoordXYZ(xyz, destX, destY, MapZ);
					newObj = AddObj(0, xyz, objTyp, 0, qtyCode);
					SetOkToGet(newObj);
					TakeObj(actor, objTyp, qtyCode);
				} else {
					MoveObj(obj, destX, destY, MapZ);
					SetOkToGet(obj);
				}
			}
		}
		VIS_UpdateView();
		StatusDirty = 0;
	}
	STAT_RefreshInventory(InvenOwner);
}

/*"get" command: pick up the selected adjacent object; taking an object
 not flagged OWNED ("ok to take") on the surface is stealing*/
/*C_27A1_18F5*/CmdGet() {
	int objTyp, actor;
	int weight, strength, actorX, actorY, dir;
	int idx, tileNum, caught;

	caught = 0;
	actor = Party[Active];
	/*pointing at an NPC or an "ignore" tile: take what is under it*/
	if(
		(Selection.obj <= 0xff && Selection.x != -1) ||
		(IsTileIg(TILE_FRAME(Selection.obj)))
	) Selection.obj = FindNonIgnoredObjAt(Selection.obj);
	if(Selection.obj == -1 || GetCoordUse(Selection.obj) != LOCXYZ) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
		return;
	}
	dir = MkDirection(PointerX, PointerY);
	if(dir != -1) {
		UpdateMoveFrame(actor, dir);
		VIS_UpdateView();
	}
	PrintObjCountOrArticle(Selection.obj);
	CON_printf((char *)GetObjectString(Selection.obj));
	CON_putch('\n');
	actorX = GetX(actor);
	actorY = GetY(actor);
	if(!CLOSE_ENOUGH(1, Selection.x, Selection.y, actorX, actorY)) {
		CON_printf(/*1F24*/"\n%s!\n", SpellMessages[3]);
		return;
	}
	tileNum = TILE_FRAME(Selection.obj);
	if(IsTerrainDamage(tileNum)) {
		CON_printf(/*1F2A*/"\n%s", SpellMessages[6]);
		TerrainDamage(actor, tileNum);
		CON_putch('\n');
		COMBAT_displaySeverity(actor);
		return;
	}
	weight = GetWeight(Selection.obj) + Encumbrance(Selection.obj);
	strength = STREN[Party[Active]];
	if(Active != StatusMember || StatusDisplay != CMD_STAT_EQUIP)
		STAT_ComputeWeights(Party[Active]);
	objTyp = GetType(Selection.obj);
	/*weight 0 (except light reagents/gold) or 10000 (255 in the table)
	means "can not be taken"; a character carries 2 x STR stones
	(weights are in tenths of stones)*/
	if(
		(weight == 0 && !IsWeightPer10Type(objTyp)) ||
		weight >= 10000 || Selection.obj < 0x100 || objTyp == OBJ_DRAGON
	) {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[6]);
	} else if(strength * 20 < weight + WeightInven + WeightEquip) {
		CON_printf(/*1F2E*/"\nThe total is too heavy.\n");
	} else {
		SubMov(actor, 3);
		if(QuanType(objTyp) && (objTyp != OBJ_TORCH || GetFrame(Selection.obj) == 0)) {
			/*stackable: merge into the inventory*/
			if(GiveObj(actor, objTyp, Amount[Selection.obj]) >= 0)
				DeleteObj(Selection.obj);
		} else if(objTyp == OBJ_TORCH && GetFrame(Selection.obj) == 1) {
			/*a lit torch goes directly into a free hand*/
			if(Active == StatusMember) {
				if(Equipment[SLOT_RHND] == 0) {
					Equipment[SLOT_RHND] = Selection.obj;
				} else if(Equipment[SLOT_LHND] == 0) {
					Equipment[SLOT_LHND] = Selection.obj;
				} else {
					CON_printf(/*1F48*/"No free hand\n");
					return;
				}
			}
			InsertObj(Selection.obj, actor, EQUIP);
		} else {
			InsertObj(Selection.obj, actor, INVEN);
			if(objTyp == OBJ_CANDLE || objTyp == OBJ_CANDELABRA) {
				SetFrame(Selection.obj, 0);/*doused*/
			} else if(objTyp == OBJ_OPEN_CHEST && GetFrame(Selection.obj) == 0) {
				SetFrame(Selection.obj, 1);/*closed*/
			} else if(objTyp == OBJ_MOONSTONE) {
				/*no longer buried: forget its moongate position*/
				idx = GetFrame(Selection.obj);
				MoonstonePos[idx][0] = 0;
				MoonstonePos[idx][1] = 0;
				MoonstonePos[idx][2] = 0;
			} else if(objTyp == OBJ_BALLOON_INFLATED) {
				SetType(Selection.obj, OBJ_BALLOON_DEFLATED);
			}
		}
		if(Active == StatusMember && StatusDisplay == CMD_STAT_EQUIP)
			STAT_RefreshInventory(Party[StatusMember]);
		if(!IsOkToGet(Selection.obj) && MapZ == 0) {
			/*stealing: lose karma; any awake neutral NPC on screen
			calls the guards*/
			SetOkToGet(Selection.obj);
			SubKarma(1);
			MUS_PlaySfx(13, 0);
			for(idx = 1; idx < 0xe0; idx ++) {
				if(
					GetAlignment(idx) == NEUTRAL &&
					IsNpcOnScreen(idx) &&
					!IsAsleep(idx)
				) {
					CON_printf(/*1F56*/"\n\"Stop Thief!!!\"\n");
					COMBAT_RaiseAlarm();
					caught = 1;
					break;
				}
			}
			if(!caught)
				CON_printf(/*1F68*/"\nStealing!!!\n");
		}
		VIS_UpdateView();
	}
	if(Active != StatusMember)
		STAT_ComputeWeights(Party[StatusMember]);
	MouseMode = 0;
}

/*is the push of objNum from (x,y) in direction dir blocked?
 the destination must be passable or hold a supporting object; for a
 diagonal push both orthogonal neighbours must be free too.
 returns 1 if blocked*/
/*C_27A1_1DAB*/IsPushBlocked(int objNum, int x, int y, int dir/*bp06*/) {
	int destX;
	int destY, sideX1, sideY1, sideX2, sideY2;

	destX = DirIncrX[dir] + x;
	destY = DirIncrY[dir] + y;
	if(!CanMoveTo(objNum, destX, destY) && !HasSupportAt(destX, destY))
		return 1;
	/*odd directions are the diagonals*/
	if(!(dir & 1))
		return 0;
	sideX1 = DirIncrX[dir-1] + x;
	sideY1 = DirIncrY[dir-1] + y;
	sideX2 = DirIncrX[dir+1] + x;
	sideY2 = DirIncrY[dir+1] + y;
	if(CanMoveTo(objNum, sideX1, sideY1) || HasSupportAt(sideX1, sideY1))
		return 0;
	if(CanMoveTo(objNum, sideX2, sideY2) || HasSupportAt(sideX2, sideY2))
		return 0;
	return 1;
}

/*the outermost holder of objNum: the first object up the containment
 chain that lies on the map*/
/*C_27A1_1E69*/GetOuterHolder(int objNum) {
	do
		objNum = GetAssoc(objNum);
	while(GetCoordUse(objNum) != LOCXYZ);
	return objNum;
}

/*"move" command: push an object on the map by one tile, or move an
 inventory object to another party member or into/out of a container*/
/*C_27A1_1E8B*/CmdMove() {
	/*1c*/
	int obj, objTyp;
	unsigned char ch;
	int quanType, destX, destY, objX, objY, dir;
	int weight, count, qtyCode, srcMember, target, actor, cantMove;

	if(Selection.obj == -1 || Selection.obj == InvenOwner) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
		return;
	}
	CON_printf((char *)GetObjectString(Selection.obj));
	CON_putch('\n');
	objTyp = GetType(Selection.obj);
	quanType = QuanType(objTyp);
	if(GetCoordUse(Selection.obj) == LOCXYZ) {
		if(!CLOSE_ENOUGH(1, Selection.x, Selection.y, MapX, MapY)) {
			CON_printf(/*1F24*/"\n%s!\n", SpellMessages[3]);
			return;
		}
		dir = MkDirection(PointerX, PointerY);
		if(dir != -1) {
			UpdateMoveFrame(Party[Active], dir);
			VIS_UpdateView();
		}
		objX = GetX(Selection.obj);
		objY = GetY(Selection.obj);
		MouseMode = 0;
		/*center the mouse direction cursor on the object
		(88,88 = center of the map window)*/
		DirOriginX = 88 + TIL2SCR(objX - MapX);
		DirOriginY = 88 + TIL2SCR(objY - MapY);
		CON_printf(/*1F76*/"To ");
		if(CON_getch() != CMD_DIRECTION) {
			CON_printf(/*1F7A*/"nowhere.\n");
			MouseMode = 0;
			DirOriginX = 88;
			DirOriginY = 88;
			CON_UpdateMousePointer();
			return;
		}
		CON_printf(/*1F84*/"%s.\n", DirNames[AdvanceDir]);
		actor = Party[Active];
		weight = GetWeight(Selection.obj) + Encumbrance(Selection.obj);
		if(TypeWeight[GetType(Selection.obj)] == 0 || GetType(Selection.obj) == OBJ_DRAGON) {
			CON_printf(/*1F13*/"\n%s\n", SpellMessages[6]);
			goto c_2237;
		}
		/*an NPC resists being pushed (strength contest), can't push
		oneself; (frame & 3) == 3: lying NPC?*/
		if(Selection.obj < 0x100 && (SPELL_ResistStrVsStr(actor, Selection.obj) || actor == Selection.obj ||
			((GetFrame(Selection.obj) & 3) == 3 && GetType(Selection.obj) != OBJ_DEAD_BODY))) {
			CON_printf(/*1F13*/"\n%s\n", SpellMessages[1]);
			MouseMode = 0;
			DirOriginX = 88;
			DirOriginY = 88;
			CON_UpdateMousePointer();
			return;
		}
		SubMov(Party[Active], 5);
		destX = DirIncrX[AdvanceDir] + objX;
		destY = DirIncrY[AdvanceDir] + objY;
		target = FindLoc(destX, destY, MapZ);
		/*pushed onto a container: put it inside*/
		if(target != -1 && CanPutInContainer(target, Selection.obj) && weight < 0xff) {
			InsertObj(Selection.obj, target, CONTAINED);
		} else {
			if(IsPushBlocked(Selection.obj, objX, objY, AdvanceDir)) {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[4]);
				if(GetType(Selection.obj) == OBJ_CANNON &&
					((GetFrame(Selection.obj) != (AdvanceDir >> 1)) & 3)/*parenthesis problem:bug?*/
					) {
					SetFrame(Selection.obj, (AdvanceDir >> 1) & 3);
				}
/*c_21a8:*/
				MouseMode = 0;
				DirOriginX = 88;
				DirOriginY = 88;
				CON_UpdateMousePointer();
				return;
			}
/*21c0*/
			/*a cannon is first turned towards the push direction
			(frame = direction / 2), then moved*/
			if(GetType(Selection.obj) == OBJ_CANNON &&
				((GetFrame(Selection.obj) != (AdvanceDir >> 1)) & 3)/*parenthesis problem:bug?*/
				) {
				SetFrame(Selection.obj, (AdvanceDir >> 1) & 3);
			} else {
				MoveObj(Selection.obj, destX, destY, MapZ);
			}
		}
/*c_2232:*/
		VIS_UpdateView();
c_2237:
		DirOriginX = 88;
		DirOriginY = 88;
		CON_UpdateMousePointer();
	} else {
		/*inventory object of the displayed member StatusMember*/
		cantMove = STAT_CheckUnready(Selection.obj, Party[StatusMember]);
		if(cantMove) {
			STAT_RefreshInventory(InvenOwner);
			return;
		}
		if(!IsNpcOnScreen(Party[StatusMember])) {
			CON_printf(/*1F89*/"\nThis person is not around!\n");
			return;
		}
		count = AskHowMany(Selection.obj);
		if(!count)
			return;
		CON_printf(/*1F76*/"To ");
		obj = Selection.obj;
		srcMember = StatusMember;
		if(objTyp == OBJ_AMULET_OF_SUBMISSION) {
			CON_printf(/*1FA6*/"\n\n%s\n", SpellMessages[12]);
			return;
		}
		if(SelectMode != 0) {
			SelectRange = 7;
			DrawSlotCursor(0);
			SelectMode = 1;
		}
		ch = CON_getch();
		if(ch != CMD_TARGET || Selection.obj == -1) {
			CON_printf(/*1FAC*/"nobody.\n");
			return;
		}
		if(GetCoordUse(Selection.obj) == LOCXYZ && (
			(Selection.obj < 0x100 && !IsNpcOnScreen(Selection.obj)) ||
			(Selection.obj >= 0x100 && !CLOSE_ENOUGH(1, Selection.x, Selection.y, MapX, MapY)))) {
			CON_printf(/*1FB5*/"\n\n%s!\n", SpellMessages[3]);
			return;
		}
		if(Selection.obj < 0x100) {
			/*give to another party member*/
			if(Party[srcMember] != Selection.obj) {
				CON_printf((char *)GetObjectString(Selection.obj));
				CON_printf(/*1DC6*/".\n");
				if(!IsNpcOnScreen(Selection.obj)) {
					CON_printf(/*1F89*/"\nThis person is not around!\n");
					return;
				}
				if(!IsPlrControl(Selection.obj)) {
					CON_printf(/*1FBC*/"\nOnly within the party!\n");
					return;
				}
				STAT_ComputeWeights(Selection.obj);
				if(quanType) {
					if(IsWeightPer10Type(objTyp))
						weight = TypeWeight[objTyp] * count / 10;
					else
						weight = TypeWeight[objTyp] * count;
				} else {
					weight = GetWeight(obj) + Encumbrance(obj);
				}
				if(STREN[Selection.obj] * 20 < weight + WeightInven + WeightEquip) {
					CON_printf(/*1FD5*/"\nCan't carry!\n");
					STAT_ComputeWeights(Party[StatusMember]);
					return;
				}
				STAT_StopEquipEffect(obj, Party[srcMember]);
				if(GetCoordUse(obj) == EQUIP && Party[srcMember] != Selection.obj) {
					Equipment[STAT_FindEquipSlot(obj)] = 0;
					if(STAT_GetEquipSlot(obj) == SLOT_2HND) {
						Party_2Handed[srcMember] = 0;
						Equipment[SLOT_LHND] = 0;
					}
				}
				if(quanType) {
					if(quanType == 2)
						qtyCode = QualQuan(GetQual(obj), count);
					else
						qtyCode = count;
					GiveObj(Selection.obj, objTyp, qtyCode);
					TakeObj(GetAssoc(obj), objTyp, qtyCode);
				} else {
					InsertObj(obj, Selection.obj, INVEN);
				}
				STAT_ComputeWeights(Party[StatusMember]);
			} else {
				CON_printf(/*1FE4*/"yourself.\n");
			}
			dir = MkDirection(PointerX, PointerY);
			if(dir != -1) {
				UpdateMoveFrame(Party[Active], dir);
				VIS_UpdateView();
			}
			SubMov(Party[Active], 5);
		} else {
			/*into (or out of) a container of the same owner*/
			if(GetCoordUse(Selection.obj) == LOCXYZ) {
				CON_printf(/*1FAC*/"nobody.\n");
				return;
			}
			if(!IsContainerType(GetType(Selection.obj))) {
				CON_printf(/*1FEF*/"\n\nnot a container\n");
				return;
			}
			if(GetOuterHolder(Selection.obj) != GetOuterHolder(obj)) {
				CON_printf(/*2002*/"\n\nCan't move to another person's bag.\n");
				return;
			}
			if(obj < 0x100) {
				CON_printf(/*2029*/"\n\nIt's too big to fit inside a container.\n");
				return;
			}
			if(GetCoordUse(obj) == EQUIP) {
				CON_printf(/*2054*/"\n\nYou have to unready it first.\n");
				return;
			}
			if(Selection.obj == obj) {
				CON_printf(/*2075*/"\n\nHow can a container go into itself!\n");
				return;
			}
			if(GetType(Selection.obj) == OBJ_SPELLBOOK && GetType(obj) != OBJ_SPELL) {
				CON_printf(/*209C*/"\n\nOnly spells can go into the spellbook.\n");
				return;
			}
			if(GetType(Selection.obj) == OBJ_VORTEX_CUBE && GetType(obj) != OBJ_MOONSTONE) {
				CON_printf(/*20C6*/"\n\nOnly moonstones can go into the vortex cube.\n");
				return;
			}
			if(GetAssoc(obj) == Selection.obj) {
				/*already in it: move it out to the container's holder
				(GetQual() below may read obj after DeleteObj(), which
				does not clear Amount[]; same in the "into" case)*/
				if(GetType(Selection.obj) == OBJ_SPELLBOOK && GetType(obj) == OBJ_SPELL) {
					CON_printf(/*20F6*/"\n\nSpells are not suppose to be taken out from the Spellbook.\n");
					return;
				}
				CON_printf(/*2134*/"out of ");
				SubMov(Party[Active], 5);
				if(quanType) {
					if(quanType == 2) {
						if(GetQuan(obj) == count)
							DeleteObj(obj);
						else
							SubQuan(obj, count);
						qtyCode = QualQuan(GetQual(obj), count);
					} else {
						if(Amount[obj] == count)
							DeleteObj(obj);
						else
							Amount[obj] -= count;
						qtyCode = count;
					}
					GiveObj(GetAssoc(Selection.obj), objTyp, qtyCode);
				} else {
					InsertObj(obj, GetAssoc(Selection.obj), CONTAINED);
				}
			} else {
				CON_printf(/*213C*/"into ");
				SubMov(Party[Active], 5);
				if(quanType) {
					if(quanType == 2) {
						if(GetQuan(obj) == count)
							DeleteObj(obj);
						else
							SubQuan(obj, count);
						qtyCode = QualQuan(GetQual(obj), count);
					} else {
						if(Amount[obj] == count)
							DeleteObj(obj);
						else
							Amount[obj] -= count;
						qtyCode = count;
					}
					GiveObj(Selection.obj, objTyp, qtyCode);
				} else {
					InsertObj(obj, Selection.obj, CONTAINED);
				}
			}
			InvenSize --;
			CON_printf((char *)GetObjectString(Selection.obj));
			CON_putch('\n');
		}
		if(StatusDisplay == CMD_STAT_EQUIP)
			STAT_RefreshInventory(InvenOwner);
	}
/*c_2892*/
	StatusDirty = 0;
	MouseMode = 0;
}

/*spring the trap (OBJ_EFFECT with quality SPELL_TRAP) set on objNum:
 acid, poison, bomb or gas, then remove the trap*/
/*C_27A1_28A3*/SpringTrap(int objNum) {
	int victim;
	int trapKind, x, y;
	int far *hit;

	/*on the map the active character is hit, in an inventory the
	displayed member StatusMember*/
	if(GetCoordUse(objNum) == LOCXYZ) {
		victim = GetPartyObjOrVehicle(Active);
		x = GetObjXOrVehicle(objNum);
		y = GetObjYOrVehicle(objNum);
	} else {
		victim = GetPartyObjOrVehicle(StatusMember);
		x = GetObjXOrVehicle(victim);
		y = GetObjYOrVehicle(victim);
	}
	trapKind = OSI_rand(0, 3);
	switch(trapKind) {
		case 0:
			CON_printf(/*2142*/"Acid!\n");
			LoseHP(victim, OSI_rand(1, 20));
			CON_putch('\n');
			COMBAT_displaySeverity(victim);
		break;
		case 1:
			CON_printf(/*2149*/"Poison!\n");
			SetPoisoned(victim);
			COMBAT_showHit(victim);
		break;
		case 2:
			CON_printf(/*2152*/"Bomb!\n");
			for(hit = Explosion(x, y, TIL_FIRE_MISSILE); *hit >= 0; hit ++) {
				if(*hit < 0x100) {
					LoseHP(*hit, OSI_rand(1, 20));
					CON_putch('\n');
					COMBAT_displaySeverity(*hit);
				}
			}
		break;
		case 3:
			CON_printf(/*2159*/"Gas!\n");
			for(hit = Explosion(x, y, TIL_POISON_MISSILE); *hit >= 0; hit ++) {
				if(*hit < 0x100) {
					SetPoisoned(*hit);
					COMBAT_showHit(*hit);
				}
			}
		break;
	}
	DeleteObj(OBJ_FindInContainer(objNum, OBJ_EFFECT, SPELL_TRAP));
	if(StatusDisplay == CMD_STAT_PARTY || StatusDisplay == CMD_STAT_PORTRAIT)
		StatusDirty ++;
}

/*use/lock/unlock door objNum and print the result.
 door frames: 0-3 open, 4-7 closed, 8-11 locked, 12-15 magically
 locked (low 2 bits = orientation/part).
 withKey: (un)lock with key/lockpick; magicLock: "lock" spell;
 magicUnlock: "unlock magic" spell; all 0: open/close*/
/*C_27A1_2A44*/UseDoor(int objNum, int withKey, int magicLock, int magicUnlock) {
	int frame;

	CON_putch('\n');
	frame = GetFrame(objNum);
	/*state changes are done by xor-ing the state bits 2-3:
	^4 open<->closed, ^0xc closed<->locked, ^8 closed<->magic locked,
	^4 locked->magic locked*/
	if(frame < 4) {
		if(!withKey && !magicLock && !magicUnlock) {
			CON_printf(/*215F*/"closed!\n");
			SetFrame(objNum, frame ^ 4);
		} else {
			CON_printf(/*2168*/"Can't (Un)lock an opened door\n");
		}
	} else if(frame >= 4 && frame < 8) {
		if(withKey) {
			CON_printf(/*2187*/"locked!\n");
			SetFrame(objNum, frame ^ 0xc);
		} else if(magicLock) {
			CON_printf(/*2190*/"magically locked!\n");
			SetFrame(objNum, frame ^ 8);
		} else {
			/*plain use (or "unlock magic" on a closed door): open it,
			springing its trap if any*/
			if(FindInvType(objNum, OBJ_EFFECT, SPELL_TRAP) != -1)
				SpringTrap(objNum);
			else
				CON_printf(/*21A3*/"opened!\n");
			SetFrame(objNum, frame ^ 4);
		}
	} else if(frame >= 8 && frame < 0xc) {
		if(withKey) {
			CON_printf(/*21AC*/"unlocked!\n");
			SetFrame(objNum, frame ^ 0xc);
		} else if(magicLock) {
			CON_printf(/*2190*/"magically locked!\n");
			SetFrame(objNum, frame ^ 4);
		} else {
			/*("unlock magic" does not open a key lock)*/
			CON_printf(/*21B7*/"locked\n");
		}
	} else {
		/*magically locked: only "unlock magic" helps*/
		if(magicUnlock) {
			CON_printf(/*21AC*/"unlocked!\n");
			SetFrame(objNum, frame ^ 8);
		} else {
			CON_printf(/*2190*/"magically locked!\n");
		}
	}
}

/*use/lock/unlock chest objNum and print the result.
 chest frames: 0 open, 1 closed, 2 locked, 3 magically locked.
 parameters as for UseDoor; opening a chest also searches it*/
/*C_27A1_2BBC*/UseChest(int objNum, int withKey, int magicLock, int magicUnlock) {
	int frame;

	CON_putch('\n');
	frame = GetFrame(objNum);
	if(frame == 0) {
		if(!withKey && !magicLock && !magicUnlock) {
			CON_printf(/*215F*/"closed!\n");
			SetFrame(objNum, 1);
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		}
	} else if(frame == 1) {
		if(!withKey && !magicLock && !magicUnlock) {
			if(FindInvType(objNum, OBJ_EFFECT, SPELL_TRAP) != -1)
				SpringTrap(objNum);
			else
				CON_printf(/*21A3*/"opened!\n");
			SetFrame(objNum, 0);
			SearchObj(objNum, GetX(objNum), GetY(objNum));
		} else if(withKey) {
			CON_printf(/*2187*/"locked!\n");
			SetFrame(objNum, 2);
		} else if(magicLock) {
			CON_printf(/*2190*/"magically locked!\n");
			SetFrame(objNum, 3);
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		}
	} else if(frame == 2) {
		if(withKey) {
			CON_printf(/*21AC*/"unlocked!\n");
			SetFrame(objNum, 1);
		} else
		if(magicLock) {
			CON_printf(/*2190*/"magically locked!\n");
			SetFrame(objNum, 3);
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		}
	} else {
		if(magicUnlock) {
			CON_printf(/*21AC*/"unlocked!\n");
			SetFrame(objNum, 1);
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		}
	}
	if(GetCoordUse(objNum) != LOCXYZ)
		STAT_DrawInventory();
}

/*does a dexterity check fail? (true -> the lockpick breaks)
 chance of failure: (45 - dexterity) / 2 out of 30*/
/*C_27A1_2D34*/FailsDexCheck(int dexterity) {
	return OSI_rand(1, 30) < ((45 - dexterity) / 2);
}

/*does an intelligence check fail? (true -> the crystal ball hurts you)*/
/*C_27A1_2D61*/FailsIntCheck(int intelligence) {
	return OSI_rand(1, 30) < ((45 - intelligence) / 2);
}

/*use key/lockpick objNum: ask for a door or chest and (un)lock it.
 a key works if its quality matches the lock's, a lockpick only on
 locks of quality 0 and may break*/
/*C_27A1_2D8E*/UseKeyOrLockpick(int objNum) {
	int targetTyp;
	int ch, x, y, toolTyp, actor;

	actor = Party[Active];
	toolTyp = GetType(objNum);
	CON_printf(/*21BF*/"On ");
	if(SelectMode != 0) {
		DrawSlotCursor(0);
		SelectMode = 1;
	}
	ch = CON_getch();
	if(ch == CMD_TARGET && Selection.obj != -1) {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_putch('\n');
		targetTyp = GetType(Selection.obj);
		x = GetX(actor);
		y = GetY(actor);
		if(GetCoordUse(Selection.obj) == LOCXYZ && !CLOSE_ENOUGH(1, Selection.x, Selection.y, x, y)) {
			CON_printf(/*1F25*/"%s!\n", SpellMessages[3]);
		} else if((targetTyp >= OBJ_OAKEN_DOOR && targetTyp <= OBJ_STEEL_DOOR) || targetTyp == OBJ_OPEN_CHEST) {
			/*the lockpick may break first (one of the stack is lost)*/
			if(toolTyp == OBJ_LOCKPICK && FailsDexCheck(GetDex(actor))) {
				MUS_PlaySfx(13, 0);
				CON_printf(/*21C3*/"\nKey broke.\n");
				if(GetQuan(objNum) == 1)
					DeleteObj(objNum);
				else
					SetQuan(objNum, GetQuan(objNum) - 1);
				if(StatusDisplay == CMD_STAT_EQUIP)
					STAT_DrawInventory();
			} else if(
				(toolTyp == OBJ_KEY && GetQual(Selection.obj) && GetQual(objNum) == GetQual(Selection.obj)) ||
				(toolTyp == OBJ_LOCKPICK && GetQual(Selection.obj) == 0)
			) {
				if(targetTyp == OBJ_OPEN_CHEST)
					UseChest(Selection.obj, 1, 0, 0);
				else
					UseDoor(Selection.obj, 1, 0, 0);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		}
	} else {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
	}
}

/*use torch objNum: light it (taking one torch off a stack and readying
 it in a free hand); using a lit torch puts it out (it is consumed)*/
/*C_27A1_2FD1*/UseTorch(int objNum) {
	int stackObj;
	int quantity;
	struct coord xyz;

	quantity = GetQuan(objNum);
	if(IsLit(objNum)) {
		DeleteObj(objNum);
		CON_putch('\n');
		CON_printf(BurnOutMsg);
		return;
	}
	if(Equipment[SLOT_RHND] && Equipment[SLOT_LHND]) {
		CON_printf(/*21D0*/"\nNo free hand to hold the torch.\n");
	} else {
		if(quantity > 1) {
			/*split one torch off the stack*/
			stackObj = objNum;
			if(GetCoordUse(objNum) == LOCXYZ) {
				SetCoordXYZ(xyz, GetX(stackObj), GetY(stackObj), MapZ);
				objNum = AddObj(GetCoordUse(stackObj), xyz, OBJ_TORCH, GetFrame(stackObj), QualQuan(GetQual(stackObj), 1));
			} else {
				objNum = AddInvObj(OBJ_TORCH, EQUIP, 1, Party[StatusMember]);
			}
			SetQuan(stackObj, quantity - 1);
		}
		if(GetCoordUse(objNum) != LOCXYZ) {
			/*ready it in a free hand*/
			if(GetCoordUse(objNum) == INVEN || GetCoordUse(objNum) == CONTAINED) {
				if(Equipment[SLOT_RHND] == 0)
					Equipment[SLOT_RHND] = objNum;
				else
					Equipment[SLOT_LHND] = objNum;
				SetCoordUse(objNum, EQUIP);
				STAT_ComputeWeights(Party[StatusMember]);
			}
		}
		/*a lit torch's quantity is its remaining burning time*/
		SetLit(objNum);
		SetFrame(objNum, 1);
		SetQuan(objNum, 200);
		PassTime(0);/*refresh the time-dependent state (light)*/
		CON_printf(/*21F2*/"\nTorch is lit.\n");
	}
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();
}

/*use gem objNum (or the "peer" spell when objNum is 0): show the
 surroundings map and consume one gem*/
/*C_27A1_319F*/UseGem(int objNum) {
	int quantity;

	PeerMap();/*"peer" map view*/
	if(objNum) {
		if((quantity = GetQuan(objNum)) > 1)
			SetQuan(objNum, quantity - 1);
		else
			DeleteObj(objNum);
	}
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();
}

/*use a light source (fireplace, brazier, candelabra, campfire, candle):
 toggle frame bit 0 (lit/doused)*/
/*C_27A1_31F6*/UseLightSource(int objNum) {
	int objFrm;
	int objTyp;

	objTyp = GetType(objNum);
	objFrm = GetFrame(objNum);
	CON_putch('\n');
	switch(objTyp) {
		case OBJ_FIREPLACE: CON_printf(/*2202*/"fireplace"); break;
		case OBJ_BRAZIER: CON_printf(/*220C*/"brazier"); break;
		case OBJ_CANDELABRA: CON_printf(/*2214*/"candelabra"); break;
		case OBJ_CAMPFIRE: CON_printf(/*221F*/"campfire"); break;
		case OBJ_CANDLE: CON_printf(/*2228*/"candle"); break;
	}
	/*frame bit 0: lit; bit 1 (variant) is kept*/
	if(objFrm & 1) {
		SetFrame(objNum, objFrm & 2);
		CON_printf(/*222F*/" is doused.\n");
	} else {
		SetFrame(objNum, objFrm | 1);
		CON_printf(/*21F8*/" is lit.\n");
	}
}

/*use secret door objNum: toggle it open/closed (frame bit 0)*/
/*C_27A1_32FA*/UseSecretDoor(int objNum) {
	int objFrm;

	objFrm = GetFrame(objNum);
	if(objFrm & 1)
		SetFrame(objNum, objFrm & 2);
	else
		SetFrame(objNum, objFrm | 1);
}

/*note frequencies for the keys '0'..'9' of the instruments*/
/*1CEE*/int NoteFrequencies[] = {0x1EAB,0x0C2C,0x0DA9,0x0F56,0x103F,0x123C,0x1478,0x16FA,0x1857,0x1B53};

/*play a musical instrument: each digit key '0'-'9' plays a note of
 NoteFrequencies with speaker sound soundId, any other key stops*/
/*C_27A1_335A*/static PlayInstrument(int soundId) {
	int ch;

	SelectMode = 0;
	while(isdigit(ch = CON_getch()))
		MUS_PlaySfx(soundId, NoteFrequencies[ch - '0']);
}

/*use bell/pull chain: animate the bell tile and play the bell sound*/
/*C_27A1_338D*/static UseBell() {
	int i, step;

	/*find the bell's tile animation and swing it through 4 frames*/
	for(i = 0; i < NumAnimData; i ++) {
		if(AnimTile[i] == BaseTile[OBJ_BELL]) {
			for(step = 0; step < 0x10; step ++) {
				GR_SET_ANIM_FRAME(AnimTile[i], AnimFirstFrame[i] + (step >> 2));
				DrawMapWindow(1);
				PaletteAnimation();
			}
			GR_SET_ANIM_FRAME(AnimTile[i], AnimFirstFrame[i]);
			break;
		}
	}
	DisplayTick();
	OSI_playWavedNote(2000, 1, 16000, 20000, -1);
}

/*use moonstone objNum: bury it under the active character if he stands
 on a diggable ground tile, and remember its position for the
 moongates in MoonstonePos*/
/*C_27A1_3425*/UseMoonstone(int objNum) {
	int x, y;
	int numTile, stoneNum;

	x = GetX(Party[Active]);
	y = GetY(Party[Active]);
	numTile = GetTileAtXYZ(x, y, MapZ);
	/*grass/earth-like ground tiles*/
	if(
		(numTile >= TIL_001 && numTile <= TIL_007) ||
		(numTile >= TIL_010 && numTile <= TIL_06F)
	) {
		CON_printf(/*223C*/"buried.\n");
		stoneNum = GetFrame(objNum);
		MoonstonePos[stoneNum][0] = x;
		MoonstonePos[stoneNum][1] = y;
		MoonstonePos[stoneNum][2] = MapZ;
		STAT_ComputeWeights(Party[StatusMember]);
		/*put it on the ground, then move the character back on top*/
		MoveObj(objNum, x, y, MapZ);
		MoveObj(Party[Active], x, y, MapZ);
		if(StatusDisplay == CMD_STAT_EQUIP)
			STAT_DrawInventory();
	} else {
		CON_printf(/*2245*/"Cannot be buried here!\n");
	}
}

/*use (readied) staff objNum: cast the spell of its first OBJ_CHARGE
 (quality = spell number) and consume that charge*/
/*C_27A1_3537*/static UseStaff(int objNum) {
	int spellNum, i;
	int charge;
	unsigned char letter;

	charge = FindInv(objNum);
	if(charge  == -1) {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
		return;
	}
	spellNum = GetQual(charge);
	/*print the words of power ("With An Flam ...")*/
	CON_printf(/*225D*/"With ");
	i = 0;
	letter = SpellCode[spellNum][i++];
	CON_printf(/*1E8A*/"%s", MagicWords[letter - 'A']);
	while(letter = SpellCode[spellNum][i++])
		CON_printf(/*1E8D*/" %s", MagicWords[letter - 'A']);
	if(SelectMode != 0)
		SelectRange = 7;
	SPELL_Execute(spellNum, 0);
	TakeObj(objNum, OBJ_CHARGE, QualQuan(spellNum, 1));
}

/*uncalled: cast a fireball as from a fire wand*/
/*__27A1_3617*/UseFireWand() {
	if(SelectMode != 0)
		SelectRange = 7;
	SPELL_CastMissile(SPELL_FIREBALL, TIL_FIRE_MISSILE, 8, 1, 0);/*fireball?*/
}

/*uncalled: cast a lightning bolt as from a lightning wand*/
/*__27A1_363C*/UseLightningWand() {
	if(SelectMode != 0)
		SelectRange = 7;
	SPELL_CastMissile(SPELL_LIGHTNING, TIL_MISSILE_0, 8, 1, 0);/*lightning?*/
}

/*use well: fill an empty bucket carried by the active character*/
/*C_27A1_3661*/UseWell() {
	int actor, bucket;

	actor = GetPartyObjOrVehicle(Active);
	bucket = FindInvType(actor, OBJ_BUCKET, 0);
	if(bucket == -1) {
		CON_printf(/*2263*/"\nYou need a bucket.\n");
		return;
	}
	STAT_ComputeWeights(actor);
	/*a full bucket weighs 3.2 stones more*/
	if(STREN[actor] * 20 < (WeightInven + WeightEquip + 32)) {
		CON_printf(/*2278*/"\nThe bucket of water is too heavy for you.\n");
		return;
	}
	DeleteObj(bucket);
	GiveObj(actor, OBJ_BUCKET_OF_WATER, 1);/*bucket of water*/
	CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
}

/*use cow: milk it into an empty bucket*/
/*C_27A1_36E7*/UseCow() {
	int actor, bucket;

	actor = GetPartyObjOrVehicle(Active);
	bucket = FindInvType(actor, OBJ_BUCKET, 0);
	if(bucket == -1) {
		CON_printf(/*2263*/"\nYou need a bucket.\n");
		return;
	}
	STAT_ComputeWeights(actor);
	if(STREN[actor] * 20 < (WeightInven + WeightEquip + 32)) {
		CON_printf(/*22A4*/"\nThe bucket of milk is too heavy for you.\n");
		return;
	}
	DeleteObj(bucket);
	GiveObj(actor, OBJ_BUCKET_OF_MILK, 1);
	CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
}

/*use churn: turn a bucket of milk into butter (and an empty bucket)*/
/*C_27A1_376D*/UseChurn() {
	int actor, milk;

	actor = GetPartyObjOrVehicle(Active);
	milk = FindInvType(actor, OBJ_BUCKET_OF_MILK, 0);
	if(milk == -1) {
		CON_printf(/*22CF*/"\nYou need some milk.\n");
		return;
	}
	DeleteObj(milk);
	GiveObj(actor, OBJ_BUTTER, 1);/*butter*/
	GiveObj(actor, OBJ_BUCKET, 1);
	CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
}

/*use beehive: fill an empty honey jar*/
/*C_27A1_37D3*/UseBeehive() {
	int jar, actor;

	actor = GetPartyObjOrVehicle(Active);
	jar = FindInvType(actor, OBJ_HONEY_JAR, 0);
	if(jar == -1) {
		CON_printf(/*22E5*/"\nYou need a honey jar.\n");
		return;
	}
	SetType(jar, OBJ_JAR_OF_HONEY);/*full honey jar*/
	CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
	STAT_DrawInventory();
}

/*use (drink) potion objNum on a party member; the color (frame)
 gives the effect. the potion is consumed*/
/*C_27A1_3832*/UsePotion(int objNum) {
	int color;

	CON_printf(/*1E8A*/"%s", SpellMessages[9]);
	MouseMode = 1;
	if(SelectMode != 0) {
		DrawSlotCursor(0);
		SelectMode = 1;
		SelectRange = 7;
	}
	CON_getch();
	if(Selection.obj == -1 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
		return;
	}
	CON_printf((char *)GetObjectString(Selection.obj));
	CON_putch('\n');
	if(!IsPlrControl(Selection.obj)) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[2]);
		return;
	}
	/*the frame is the potion's color*/
	color = GetFrame(objNum);
	switch(color) {
		case 0:/*orange - awaken*/
			if(IsAsleep(Selection.obj)) {
				ClrAsleep(Selection.obj);
				ObjShapeType[Selection.obj] = OrigShapeType[Selection.obj];
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 1:/*red - cure*/
			if(IsPoisoned(Selection.obj)) {
				ClrPoisoned(Selection.obj);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 2:/*yellow - heal*/
			SPELL_DoHeal(SPELL_HEAL);
		break;
		case 3:/*green - poison (unless immune to poison)*/
			if(!IsMonsterImmunePoison(GetMonsterClass(GetType(Selection.obj)))) {
				SetPoisoned(Selection.obj);
				COMBAT_showHit(Selection.obj);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 4:/*blue - sleep*/
			/*bug?should be IsMonsterImmuneSleep (immune to sleep)? this tests
			MONSTER_UNDEAD (undead)*/
			if(!IsMonsterUndead(GetMonsterClass(GetType(Selection.obj)))) {
				COMBAT_showHit(Selection.obj);
				SetAsleep(Selection.obj);
				SetTypeUnconscious(Selection.obj);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 5:/*purple - protection*/
			if(!IsProtected(Selection.obj)) {
				SetProtected(Selection.obj);
				MagicalHalo(Selection.obj, 1);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 6:/*black - invisibility*/
			if(!IsInvisible(Selection.obj)) {
				SetInvisible(Selection.obj);
				MagicalHalo(Selection.obj, 1);
			} else {
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
			}
		break;
		case 7:/*white - xray vision*/
			SPELL_XRay(SPELL_X_RAY);
		break;
	}
	DeleteObj(objNum);
	if(StatusDisplay == CMD_STAT_PARTY || StatusDisplay == CMD_STAT_PORTRAIT)
		StatusDirty ++;
}

/*cannon frame (N,E,S,W) -> x/y step*/
/*1D02*/char CannonDirX[] = { 0, 1, 0, -1};
/*1D06*/char CannonDirY[] = {-1, 0, 1,  0};

/*use (fire) cannon objNum in the direction given by its frame
 (CannonDirX/CannonDirY); the cannon ball hits the first NPC within 5 tiles*/
/*C_27A1_3A35*/UseCannon(int objNum) {
	int dir, dist;
	int x, y, target, reached;

	CON_printf(/*22FD*/"\nFire!\n");
	dir = GetFrame(objNum);
	x = GetX(objNum);
	y = GetY(objNum);
	/*aim at the first NPC in line (or 6 tiles away)*/
	for(dist = 1; dist < 6; dist ++) {
		target = FindLoc(CannonDirX[dir] * dist + x, CannonDirY[dir] * dist + y, MapZ);
		if(target >= 0 && target < 0x100)
			break;
	}
	x += CannonDirX[dir] * dist;
	y += CannonDirY[dir] * dist;
	reached = COMBAT_Missile(objNum, &x, &y, 5, TIL_MISSILE_7);
	/*MissileEnd: last position of the missile; when stopped by an
	obstacle, explode on the obstacle*/
	if(!reached) {
		MissileEnd[0] += CannonDirX[dir];
		MissileEnd[1] += CannonDirY[dir];
		Explosion(MissileEnd[0], MissileEnd[1], TIL_MISSILE_1);
	}
	/*1-60 damage to what is hit (stackable items are spared)*/
	target = FindLoc(MissileEnd[0], MissileEnd[1], MapZ);
	if(target >= 0 && !QuanType(GetType(target)))
		LoseHP(target, OSI_rand(1, 60));
}

/*use powder keg objNum: light the fuse (KegFuse = minutes left,
 KegObject = the keg); only one keg may burn at a time*/
/*C_27A1_3B59*/static UsePowderKeg(int objNum) {
	/*frame 1: fuse burning (the explosion is triggered by the clock)*/
	if(GetFrame(objNum) == 0) {
		if(KegFuse) {
			CON_printf(/*2305*/"\nNot now\n");
			return;
		}
		SetFrame(objNum, 1);
		KegFuse = 3;
		KegObject = objNum;
		CON_printf(/*230F*/"\nPowder lit!\n");
		if(GetCoordUse(objNum) == LOCXYZ) {
			VIS_UpdateView();
			DisplayTick();
		} else {
			STAT_DrawInventory();
		}
	} else {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
	}
}

/*after an explosion at (x,y): detonate nearby powder kegs and blow up
 nearby doors that are not magically locked*/
/*C_27A1_3BE1*/AfterExplosion(int x, int y) {
	int objNum;
	int i, j;

	/*(note: y + 2 is excluded: a 5x4 area)*/
	for(i = x - 2; i <= x + 2; i ++) {
		for(j = y - 2; j < y + 2; j ++) {
			/*detonate other kegs*/
			objNum = FindTypeAt(i, j, MapZ, OBJ_POWDER_KEG);
			if(objNum >= 0) {
				SetFrame(objNum, 1);
				VIS_UpdateView();
				DisplayTick();
				Detonate(objNum);
			}
			/*blow the doors (first door type found that is not
			magically locked, frames 12-15)*/
			objNum = FindTypeAt(i, j, MapZ, OBJ_STEEL_DOOR);
			if(objNum == -1 || (GetFrame(objNum) & 0xc) == 0xc)
				objNum = FindTypeAt(i, j, MapZ, OBJ_OAKEN_DOOR);
			if(objNum == -1 || (GetFrame(objNum) & 0xc) == 0xc)
				objNum = FindTypeAt(i, j, MapZ, OBJ_WINDOWED_DOOR);
			if(objNum == -1 || (GetFrame(objNum) & 0xc) == 0xc)
				objNum = FindTypeAt(i, j, MapZ, OBJ_CEDAR_DOOR);
			if(objNum >= 0 && (GetFrame(objNum) & 0xc) != 0xc) {
				DeleteObj(objNum);
				VIS_UpdateView();
				DisplayTick();
				CON_printf(/*231D*/"\nThe door is blown up!\n");
			}
		}
	}
}

/*powder keg objNum explodes (on the map or in an inventory):
 damage everything around and chain-react via AfterExplosion*/
/*C_27A1_3D2B*/Detonate(int objNum) {
	int holder;
	int x, y;
	int far *hit;

	if(GetCoordUse(objNum) == LOCXYZ) {
		x = GetX(objNum);
		y = GetY(objNum);
	} else {
		holder = objNum;
		do
			holder = GetAssoc(holder);
		while(GetCoordUse(holder) != LOCXYZ);
		x = GetX(holder);
		y = GetY(holder);
	}
	/*Explosion() returns the -1 terminated list of objects hit*/
	DeleteObj(objNum);
	for(hit = Explosion(x, y, TIL_MISSILE_1); *hit >= 0; hit ++) {
		if(!QuanType(GetType(*hit)))
			LoseHP(*hit, OSI_rand(1, 60));
	}
	AfterExplosion(x, y);
	VIS_UpdateView();
	DisplayTick();
}

/*first object of type objTyp at (x,y,z), -1 if none*/
static /*C_27A1_3E21*/FindTypeAt(int x, int y, int z, int objTyp) {
	int objNum;

	for(objNum = FindLoc(x, y, z); objNum >= 0 && GetType(objNum) != objTyp; objNum = NextLoc());
	return objNum;
}

/*first object at (x,y,z) that is not part of a drawbridge (something
 standing on the bridge), 0 if none*/
/*C_27A1_3E59*/static FindObjOnBridgeAt(int x, int y, int z) {
	int objNum;
	
	for(objNum = FindLoc(x, y, z); objNum >= 0 && GetType(objNum) == OBJ_DRAWBRIDGE; objNum = NextLoc());
	if(objNum == -1)
		return 0;
	return objNum;
}

/*first NPC (object < 0x100) at (x,y,z), 0 if none*/
/*C_27A1_3E9A*/FindNPCAt(int x, int y, int z) {
	int objNum;
	
	for(objNum = FindLoc(x, y, z); objNum >= 0 && objNum >= 0x100; objNum = NextLoc());
	if(objNum == -1)
		return 0;
	return objNum;
}

/*ground tiles on which a lowered drawbridge ends*/
/*1D0A*/unsigned char BridgeEndTiles[] = {TIL_010,TIL_01C,TIL_020,TIL_02C,TIL_0D6};

/*is tileNum one of the BridgeEndTiles tiles where a lowered drawbridge ends?*/
/*C_27A1_3ED1*/IsBridgeEndTile(int tileNum) {
	int i;

	for(i = 0; i < 5; i ++) {
		if(BridgeEndTiles[i] == tileNum)
			return 1;
	}
	return 0;
}

/*create a permanent (not LOCAL) object of type typeObject/frame
 shapeNum at (x,y,MapZ); returns its number*/
/*C_27A1_3EF6*/static AddPermanentObj(int x, int y, int typeObject, int shapeNum) {
	int numObject;
	struct coord xyz;

	SetCoordXYZ(xyz, x, y, MapZ);
	numObject = AddObj(0, xyz, typeObject, shapeNum, 0);
	ClrLocal(numObject);

	return numObject;
}

/*raise or lower the drawbridge whose top-left part is objNum.
 lowered bridge (top-left frame 3): rows of frames 3,4..4,5 and a last
 row 0,1..1,2 on the first BridgeEndTiles tile row; raised bridge (top-left
 frame 6): a single row 6,7..7,8 one tile up and left, one part wider.
 the first part (of each row) carries the quality that links the
 bridge to its crank; the new parts are permanent objects.
 returns 0 nothing happens, 1 raised, 2 lowered,
 3 can't raise (something on it), 4 can't lower (something below)*/
/*C_27A1_3F47*/static ToggleDrawbridge(int objNum) {
	register int tmp_x, tmp_y;
	int obj_x, obj_y, k, objQual, tmp_num, objFrm, numTile, width, lastPart;

	objQual = GetQual(objNum);
	objFrm = GetFrame(objNum);

	tmp_num = objNum;
	obj_x = GetX(objNum);
	obj_y = GetY(objNum);
	tmp_x = obj_x;
	tmp_y = obj_y;
	/***********/
	/*try close*/
	/***********/
	if(objFrm == 3) {
		/*is there something on the bridge?*/
		while(tmp_num >= 0) {
			while(tmp_num >= 0) {
				if(FindObjOnBridgeAt(tmp_x, tmp_y, MapZ))
					return 3;/*can't close (raise)*/
				tmp_num = FindTypeAt(++tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
			}
			tmp_x = obj_x;
			tmp_num = FindTypeAt(tmp_x, ++tmp_y, MapZ, OBJ_DRAWBRIDGE);
		}
		/*close it*/
		tmp_num = objNum;
		tmp_x = obj_x;
		tmp_y = obj_y;
		objNum = AddPermanentObj(tmp_x - 1, tmp_y - 1, OBJ_DRAWBRIDGE, 6);
		SetQual(objNum, objQual);
		while(tmp_num >= 0) {
			DeleteObj(tmp_num);
			lastPart = AddPermanentObj(tmp_x ++, tmp_y - 1, OBJ_DRAWBRIDGE, 7);
			tmp_num = FindTypeAt(tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
		}
		DeleteObj(lastPart);
		AddPermanentObj(tmp_x - 1, tmp_y - 1, OBJ_DRAWBRIDGE, 8);
		tmp_x = obj_x;
		tmp_y = obj_y + 1;
		tmp_num = FindTypeAt(tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
		while(tmp_num >= 0) {
			while(tmp_num >= 0) {
				DeleteObj(tmp_num);
				tmp_num = FindTypeAt(++tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
			}
			tmp_x = obj_x;
			tmp_num = FindTypeAt(tmp_x, ++tmp_y, MapZ, OBJ_DRAWBRIDGE);
		}
		VIS_UpdateView();
		return 1;/*closed (raised)*/
	}
	/**********/
	/*try open*/
	/**********/
	if(objFrm == 6) {
		/*width = number of parts in the raised row + 1, so that a
		lowered row is 1 + (width - 4) + 1 parts*/
		width = 1;
		while(tmp_num >= 0) {
			width ++;
			tmp_num = FindTypeAt(++tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
		}
		/*is something in the way of the bridge? (only the middle
		parts are checked)*/
		tmp_x = obj_x + 1;
		tmp_y = obj_y + 1;
		while(!IsBridgeEndTile(numTile = GetTileAtXYZ(tmp_x, tmp_y, MapZ))) {
			for(k = 0; k < width - 4; k ++) {
				if(FindObjOnBridgeAt(tmp_x, tmp_y, MapZ))
					return 4;/*can't open (lower)*/
				tmp_x ++;
			}
			tmp_x = obj_x + 1;
			tmp_y ++;
		}
		/*open it*/
		tmp_num = objNum;
		tmp_x = obj_x;
		tmp_y = obj_y;
		while(tmp_num >= 0) {
			DeleteObj(tmp_num);
			tmp_num = FindTypeAt(++tmp_x, tmp_y, MapZ, OBJ_DRAWBRIDGE);
		}
		tmp_x = obj_x + 1;
		tmp_y = obj_y + 1;
		while(!IsBridgeEndTile(numTile = GetTileAtXYZ(tmp_x, tmp_y, MapZ))) {
			objNum = AddPermanentObj(tmp_x++, tmp_y, OBJ_DRAWBRIDGE, 3);
			SetQual(objNum, objQual);
			for(k = 0; k < width - 4; k ++)
				AddPermanentObj(tmp_x ++, tmp_y, OBJ_DRAWBRIDGE, 4);
			AddPermanentObj(tmp_x, tmp_y, OBJ_DRAWBRIDGE, 5);
			tmp_x = obj_x + 1;
			tmp_y ++;
		}
		/*last row; objects already standing there are moved in place
		again to put them back on top of the new bridge parts*/
		AddPermanentObj(tmp_x ++, tmp_y, OBJ_DRAWBRIDGE, 0);
		tmp_num = FindObjOnBridgeAt(tmp_x - 1, tmp_y, MapZ);
		if(tmp_num)
			MoveObj(tmp_num, GetX(tmp_num), GetY(tmp_num), MapZ);
		for(k = 0; k < width - 4; k ++) {
			AddPermanentObj(tmp_x ++, tmp_y, OBJ_DRAWBRIDGE, 1);
			tmp_num = FindObjOnBridgeAt(tmp_x - 1, tmp_y, MapZ);
			if(tmp_num)
				MoveObj(tmp_num, GetX(tmp_num), GetY(tmp_num), MapZ);
		}
		AddPermanentObj(tmp_x, tmp_y, OBJ_DRAWBRIDGE, 2);
		tmp_num = FindObjOnBridgeAt(tmp_x, tmp_y, MapZ);
		if(tmp_num)
			MoveObj(tmp_num, GetX(tmp_num), GetY(tmp_num), MapZ);
		VIS_UpdateView();
		return 2;/*opened (lowered)*/
	}

	return 0;/*nothing happens*/
}

/*use crank objNum: operate the drawbridge with the same quality*/
/*C_27A1_433D*/UseCrank(int objNum) {
	int bridge, animOrResult;
	int crankQual, bridgeFrm, i;

	crankQual = GetQual(objNum);
	for(
		bridge = SearchArea(0, 0, 0x3ff, 0x3ff);
		bridge >= 0 && (GetType(bridge) != OBJ_DRAWBRIDGE || GetQual(bridge) != crankQual);
		bridge = NextArea()
	);
	if(bridge == -1) {
		CON_printf(/*2335*/"\nNo correspondent drawbridge\n");
		return;
	}
	/*animate crank and chain (turning one way or the other)*/
	bridgeFrm = GetFrame(bridge);
	if(bridgeFrm == 3)
		animOrResult = 2;
	else
		animOrResult = 1;
	SetTileAnimation(TIL_CRANK, animOrResult);
	SetTileAnimation(TIL_CHAIN, animOrResult);
	for(i = 0; i < 20; i ++)
		DisplayTick();
	animOrResult = ToggleDrawbridge(bridge);
	switch(animOrResult) {
		case 0: CON_printf(/*2353*/"\nStrange, nothing happened.\n"); break;
		case 1: CON_printf(/*2370*/"\nClose the drawbridge.\n"); break;
		case 2: CON_printf(/*2388*/"\nOpen the drawbridge.\n"); break;
		case 3: CON_printf(/*239F*/"\nCan't close the drawbridge.\n"); break;
		case 4: CON_printf(/*23BD*/"\nCan't open the drawbridge.\n"); break;
	}
	SetTileAnimation(TIL_CRANK, 0);
	SetTileAnimation(TIL_CHAIN, 0);
}

/*use lever objNum: toggle the portcullis on every doorway that has the
 lever's quality*/
/*C_27A1_4479*/UseLever(int objNum) {
	int doorway, portcullis;
	int objQual, doorX, doorY, result;
	struct coord xyz;

	result = 0;
	CON_printf(/*23DA*/"\nSwitch the lever, ");
	SetFrame(objNum, GetFrame(objNum) ^ 1);
	objQual = GetQual(objNum);
	if(objQual) {
		doorway = SearchArea(0, 0, 0x3ff, 0x3ff);
		while(doorway >= 0) {
			while(doorway >= 0 && (GetType(doorway) != OBJ_DOORWAY || GetQual(doorway) != objQual))
				doorway = NextArea();
			if(doorway < 0)
				continue;
			doorX = GetX(doorway);
			doorY = GetY(doorway);
			portcullis = FindTypeAt(doorX, doorY, MapZ, OBJ_PORTCULLIS);
			if(portcullis == -1) {
				/*close: not while someone stands in the doorway; the
				portcullis frame follows the doorway's orientation*/
				if(FindNPCAt(doorX, doorY, MapZ)) {
					result = 2;
					break;
				}
				if(GetFrame(doorway) & 2) {
					SetCoordXYZ(xyz, doorX, doorY, MapZ);
					portcullis = AddObj(0, xyz, OBJ_PORTCULLIS, 3, 0);
				} else {
					SetCoordXYZ(xyz, doorX, doorY, MapZ);
					portcullis = AddObj(0, xyz, OBJ_PORTCULLIS, 1, 0);
				}
				ClrLocal(portcullis);
			} else {
				DeleteObj(portcullis);
			}
			result = 1;
			doorway = NextArea();
		}
	}
	if(result == 0)
		CON_printf(/*23EE*/"strange, nothing happened.\n");
	else if(result == 1)
		CON_printf(/*240A*/"you hear a noise.\n");
	else if(result == 2)
		CON_printf(/*241D*/"can't close the portcullis.\n");
}

/*use switch objNum: toggle the electric field on every doorway that has
 the switch's quality*/
/*C_27A1_4672*/UseSwitch(int objNum) {
	int doorway, field;
	int objQual, doorX, doorY, result;
	struct coord xyz;

	result = 0;
	CON_printf(/*243A*/"\nOperate the switch, ");
	SetFrame(objNum, GetFrame(objNum) ^ 1);
	objQual = GetQual(objNum);
	if(objQual) {
		doorway = SearchArea(0, 0, 0x3ff, 0x3ff);
		while(doorway >= 0) {
			while(doorway >= 0 && (GetType(doorway) != OBJ_DOORWAY || GetQual(doorway) != objQual))
				doorway = NextArea();
			if(doorway < 0)
				continue;
			doorX = GetX(doorway);
			doorY = GetY(doorway);
			field = FindTypeAt(doorX, doorY, MapZ, OBJ_ELECTRIC_FIELD);
			if(field == -1) {
				SetCoordXYZ(xyz, doorX, doorY, MapZ);
				field = AddObj(0, xyz, OBJ_ELECTRIC_FIELD, 0, 0);
				ClrLocal(field);
			} else {
				DeleteObj(field);
			}
			result = 1;
			doorway = NextArea();
		}
	}
	if(result == 0)
		CON_printf(/*23EE*/"strange, nothing happened.\n");
	else
		CON_printf(/*240A*/"you hear a noise.\n");
}

/*use gargish vocabulary: the Avatar can now understand Gargish*/
/*C_27A1_47E0*/UseGargishVocabulary() {
	CON_printf(/*2450*/"\nYou study the scroll!\n");
	SpeakGargish = 1;
}

/*use balloon plans: if the party carries a mammoth silk bag, a balloon
 basket, a cauldron and a rope, assemble a deflated balloon*/
/*C_27A1_47F3*/UseBalloonPlans() {
	int missing, actor;
	int parts[4];
	int i, balloon;
	struct coord xyz;

	missing = 0;
	CON_putch('\n');
	for(i = 0; i < 4; i ++)
		parts[i] = 0;
	/*parts[]: party member carrying each part*/
	parts[0] = FindPartyMemberWithObj(OBJ_MAMMOTH_SILK_BAG, 0);
	if(parts[0] == -1) {
		CON_printf(/*2468*/"Missing a mammoth silk bag.\n");
		missing = 1;
	}
	parts[1] = FindPartyMemberWithObj(OBJ_BALLOON_BASKET, 0);
	if(parts[1] == -1) {
		CON_printf(/*2485*/"Missing a balloon basket.\n");
		missing = 1;
	}
	parts[2] = FindPartyMemberWithObj(OBJ_CAULDRON, 0);
	if(parts[2] == -1) {
		CON_printf(/*24A0*/"Missing a cauldron.\n");
		missing = 1;
	}
	parts[3] = FindPartyMemberWithObj(OBJ_ROPE, 0);
	if(parts[3] == -1) {
		CON_printf(/*24A0*/"Missing a rope.\n");
		missing = 1;
	}
	if(missing)
		return;
	DeleteObj(OBJ_FindInContainer(parts[0], OBJ_MAMMOTH_SILK_BAG, 0));
	DeleteObj(OBJ_FindInContainer(parts[1], OBJ_BALLOON_BASKET, 0));
	DeleteObj(OBJ_FindInContainer(parts[2], OBJ_CAULDRON, 0));
	DeleteObj(OBJ_FindInContainer(parts[3], OBJ_ROPE, 0));
	actor = Party[Active];
	SetCoordXYZ(xyz, GetX(actor), GetY(actor), MapZ);
	balloon = AddObj(0, xyz, OBJ_BALLOON_DEFLATED, 0, 0);
	SetOkToGet(balloon);
	ClrLocal(balloon);
	MoveObj(actor, GetX(actor), GetY(actor), MapZ);
	CON_printf(/*24C6*/"Done!\n");
}

/*use deflated balloon objNum: inflate it next to the active character*/
/*C_27A1_49C3*/UseDeflatedBalloon(int objNum) {
	int i, j;
	struct coord xyz, xyzUnused/*unused?*/;
	int balloon, actor, actorX, actorY, placed;

	placed = 0;
	DeleteObj(objNum);
	actor = Party[Active];
	actorX = GetX(actor);
	actorY = GetY(actor);
	SetCoordXYZ(xyz, GetX(actor), GetY(actor), MapZ);
	balloon = AddObj(0, xyz, OBJ_BALLOON_INFLATED, 3, 0);
	ClrLocal(balloon);
	/*first free neighbouring tile (bug?: this skips the corner (-1,-1)
	instead of the character's own tile (i,j) == (1,1))*/
	for(i = 0; i < 3; i ++) {
		for(j = 0; j < 3; j ++) {
			if(!i && !j)
				continue;
			if(CanMoveTo(balloon, actorX + i - 1, actorY + j - 1)) {
				MoveObj(balloon, actorX + i - 1, actorY + j - 1, MapZ);
				CON_printf(/*24CD*/"\nDone!\n");
				placed = 1;
				break;
			}
		}
		if(placed)
			break;
	}
	if(!placed)
		CON_printf(/*24D5*/"\nNo room!\n");
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_RefreshInventory(InvenOwner);
}

/*remove all gargoyle eggs within 20 tiles of (x,y)*/
/*C_27A1_4B0B*/static DestroyGargoyleEggs(int x, int y) {
	int egg, content;

	for(egg = SearchArea(x - 20, y - 20, x + 20, y + 20); egg >= 0; egg = NextArea()) {
		if(GetType(egg) == OBJ_EGG) {
			/*an egg's content is the creature it hatches*/
			content = FindInv(egg);
			if(GetType(content) == OBJ_WINGED_GARGOYLE || GetType(content) == OBJ_GARGOYLE) {
				while(content >= 0) {
					DeleteObj(content);
					content = FindInv(egg);
				}
				DeleteObj(egg);
			}
		}
	}
}

/*mantras of the 8 virtues (honesty..humility, same order as the runes)*/
/*1D0F*/char *Mantras[] = {
	/*24E0*/"Ahm",
	/*24E4*/"Mu",
	/*24E7*/"Ra",
	/*24EA*/"Beh",
	/*24EE*/"Cah",
	/*24F2*/"Summ",
	/*24F7*/"Om",
	/*24FA*/"Lum"
};
/*shrines coords? (x, y) per virtue; unreferenced. spirituality (index 6)
is 0,0: its shrine is not on the surface map*/
unsigned ShrineX[] = {0x3A4,0x200,0x090,0x124,0x334,0x144,0x000,0x39C};
unsigned ShrineY[] = {0x108,0x170,0x394,0x02C,0x064,0x33C,0x000,0x360};

/*use rune objNum: ask for the mantra; if it is right and the matching
 moonstone lies next to the character (at a shrine), remove the force
 field and the gargoyle eggs around it*/
/*C_27A1_4B98*/static UseRune(int objNum) {
	int i, j;
	char mantra[5];
	int virtue, x, y;
	int moonstone, forcefield, found;

	found = 0;
	virtue = GetType(objNum) - OBJ_RUNE_OF_HONESTY;
	CON_printf(/*24FE*/"Mantra: ");
	CON_gets(mantra, 4);
	CON_putch('\n');
	for(i = 0; i < 8; i ++)
		if(stricmp(mantra, Mantras[i]) == 0)
			break;
	if(i == 8 || i != virtue) {
		CON_printf(/*2507*/"\nWrong mantra!\n");
		return;
	}
	virtue = i;/*??? (already equal)*/
	/*look for the virtue's moonstone around the character
	(note: "break" only leaves the inner loop)*/
	x = GetX(Party[Active]);
	y = GetY(Party[Active]);
	for(i = 0; i < 3; i ++) {
		for(j = 0; j < 3; j ++) {
			if(i != 1 || j != 1) {
				moonstone = FindTypeAt(x + i - 1, y + j - 1, MapZ, OBJ_MOONSTONE);
				if(moonstone >= 0 && GetFrame(moonstone) == virtue) {
					/*delete forcefield*/
					forcefield = FindTypeAt(x + i - 1, y + j - 1, MapZ, OBJ_FORCE_FIELD);
					if(forcefield >= 0)
						DeleteObj(forcefield);
					/*destroy gargoyles eggs*/
					DestroyGargoyleEggs(GetX(moonstone), GetY(moonstone));

					found = 1;
					break;
				}
			}
		}
	}
	if(found) {
		SPELL_CastEffect(1);
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
	} else {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
	}
}

/*use fishing pole: needs water next to the character; 1/5 chance to
 catch a fish (at most one fish per game hour, see CaughtFish)*/
/*C_27A1_4D46*/UseFishingPole() {
	int i, actor;
	int j, actorX, actorY, nearWater;

	nearWater = 0;
	actor = GetPartyObjOrVehicle(Active);
	actorX = GetObjXOrVehicle(actor);
	actorY = GetObjYOrVehicle(actor);
	for(i = 0; i < 3; i ++) {
		for(j = 0; j < 3; j ++) {
			if(i == 1 && j == 1)
				continue;
			/*bug: "actorY + i - 1" should be "actorY + j - 1", so only
			the two diagonal neighbours are actually tested*/
			if(IsTerrainWet(GetTileAtXYZ(actorX + i - 1, actorY + i - 1, MapZ))) {
				if(!CaughtFish && OSI_rand(0, 4) == 0) {
					STAT_ComputeWeights(actor);
					if(STREN[actor] * 20 < WeightInven + WeightEquip + 5) {
						CON_printf(/*2517*/"\nGot it, but can't carry it.\n");
					} else {
						CON_printf(/*2535*/"\nGot it!\n");
						AddInvObj(OBJ_FISH, INVEN, 1, actor);
						CaughtFish = 1;
					}
				} else {
					CON_printf(/*253F*/"\nDidn't get a fish.\n");
				}
				nearWater = 1;
				break;
			}
		}
		if(nearWater)
			break;
	}
	if(nearWater == 0)
		CON_printf(/*2554*/"\nYou need to stand next to water.\n");
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();
}

/*drink wine/mead/ale objNum*/
/*C_27A1_4E6F*/UseDrink(int objNum) {
	DeleteObj(objNum);
	DrunkCounter ++;
	CON_printf(/*2577*/"\nYou drink it.\n");
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();
}

/*wishes the fountain may grant (all give food)*/
/*1D3F*/char *FountainWishes[] = {
	/*2587*/"Food",
	/*258C*/"Mutton",
	/*2593*/"Wine",
	/*2598*/"Fruit",
	/*259E*/"Meat"
};

/*use fountain: make a wish; wishing for food has a 1/5 chance to
 give a portion of meat*/
/*C_27A1_4E9B*/UseFountain() {
	int i, actor;
	int answer, matched;
	char wish[10];

	matched = 0;
	actor = GetPartyObjOrVehicle(Active);
	CON_printf(/*25A3*/"Make a wish?");
	answer = CON_getch();
	if(answer == 'Y') {
		CON_printf(/*25B0*/"\nWish for: ");
		CON_gets(wish, 9);
		/*bug: FountainWishes has only 5 entries, the loop reads 5 more pointers
		past its end (whatever follows in the data segment)*/
		for(i = 0; i < 10/*bug?*/; i ++) {
			if(stricmp(wish, FountainWishes[i]) == 0) {
				if(OSI_rand(0, 4) == 0) {
					CON_printf(/*25BC*/"\n\nYou got food");
					STAT_ComputeWeights(actor);
					if(STREN[actor] * 20 < WeightInven + WeightEquip + 2) {
						CON_printf(/*25CB*/", but you can't carry it.\n");
					} else {
						GiveObj(actor, OBJ_PORTION_OF_MEAT, 1);
						CON_printf(/*1DC6*/".\n");
					}
				} else {
					CON_printf(/*1FA6*/"\n\n%s\n", SpellMessages[1]);
				}
				matched = 1;
				break;
			}
		}
		if(!matched)
			CON_printf(/*1FA6*/"\n\n%s\n", SpellMessages[2]);
	} else {
		CON_printf(/*1D6F*/"\n");
	}
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();
}

/*use readied pick/shovel: dig in a direction. digging at one spot on
 the surface opens a hole (quality 0x13); on tiles TIL_06C..TIL_06F of
 dungeon levels 1-4 digging may find a fountain (1/32) or a gold nugget (1/10)*/
/*C_27A1_4FC6*/UseDiggingTool() {
	int dig_x, dig_y;
	int tile, hole;
	struct coord xyz;

	CON_printf(/*25E6*/"Direction:");
	MouseMode = 0;
	if(SelectMode != 0) {
		DrawSlotCursor(0);
		SelectMode = 1;
	}
	if(CON_getch() != CMD_DIRECTION) {
		CON_printf(/*1F7A*/"nowhere.\n");
		return;
	}
	CON_printf(/*1F84*/"%s.\n", DirNames[AdvanceDir]);
	dig_x = DirIncrX[AdvanceDir] + MapX;
	dig_y = DirIncrY[AdvanceDir] + MapY;
	tile = GetTileAtXYZ(dig_x, dig_y, MapZ);
	if(MapZ == 0 && dig_x == 0x2c3 && dig_y == 0x343 && FindTypeAt(dig_x, dig_y, MapZ, OBJ_HOLE) == -1) {
		/*Pirate Cave? (the hole's quality 0x13 is probably its
		destination/dungeon number)*/
		SetCoordXYZ(xyz, dig_x, dig_y, MapZ);
		hole = AddObj(0, xyz, OBJ_HOLE, 0, 0);
		SetQual(hole, 0x13);
		CON_printf(/*25F1*/"\nYou dig a hole.\n");
	} else if(MapZ >= 1 && MapZ <= 4 && tile >= TIL_06C && tile <= TIL_06F) {
		if(OSI_rand(0, 31) == 0) {
			SetCoordXYZ(xyz, dig_x, dig_y, MapZ);
			AddObj(0, xyz, OBJ_FOUNTAIN, 0, 0);
			CON_printf(/*2603*/"\nYou find a water fountain.\n");
		} else if(OSI_rand(0, 9) == 0) {
			SetCoordXYZ(xyz, dig_x, dig_y, MapZ);
			AddObj(0, xyz, OBJ_GOLD_NUGGET, 0, 1);
			CON_printf(/*2620*/"\nYou find a gold nugget.\n");
		} else {
			CON_printf(/*1F13*/"\n%s\n", SpellMessages[1]);
		}
	} else {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
	}
}

/*x/y step for the 8 directions (copy of DirIncrX/DirIncrY)*/
/*1D49*/char UnboardDirX[] = { 0, 1, 1, 1, 0,-1,-1,-1};
/*1D51*/char UnboardDirY[] = {-1,-1, 0, 1, 1, 1, 0,-1};

/*can the party leave vehicle at (x,y)? a ship needs a passable tile on
 its left or right side, other vehicles on any of the 8 neighbours*/
/*C_27A1_51C4*/static CanLeaveVehicleAt(int x, int y, int vehicle) {
	int dir, shipDir;

	if(GetType(vehicle) == OBJ_SHIP) {
		/*test the two directions perpendicular to the ship's axis*/
		shipDir = GetFrame(vehicle) & 2;
		if(CanMoveTo(Party[0], UnboardDirX[(shipDir + 2) & 7] + x, UnboardDirY[(shipDir + 2) & 7] + y))
			return 1;
		if(CanMoveTo(Party[0], UnboardDirX[(shipDir + 6) & 7] + x, UnboardDirY[(shipDir + 6) & 7] + y))
			return 1;
	} else {
		for(dir = 0; dir < 8; dir ++) {
			if(CanMoveTo(Party[0], UnboardDirX[dir] + x, UnboardDirY[dir] + y))
				return 1;
		}
	}
	return 0;
}

/*use ship/skiff/raft/balloon objNum: board it, or leave it when
 already aboard (only next to land, or onto the skiff of a ship)*/
/*C_27A1_5289*/static UseVehicle(int objNum) {
	int vehTyp, x, y, actor, actorTyp;

	objNum = COMBAT_getHead(objNum);
	vehTyp = GetType(objNum);
	actor = Party[Active];
	actorTyp = GetType(actor);
	x = GetX(objNum);
	y = GetY(objNum);
	if(SoloMember != -1) {
		CON_printf(/*263A*/"Not in solo mode!\n");
		return;
	}
	if(IN_VEHICLE && actor == objNum) {
		/*try to unboard*/
		if(CanLeaveVehicleAt(MapX, MapY, objNum)) {
			/*on land*/
			objNum = Unboard(1);
			if(vehTyp == OBJ_BALLOON_INFLATED) {
				ObjShapeType[objNum] = TypeFrame(OBJ_BALLOON_DEFLATED, 0);
				STAT_RefreshInventory(InvenOwner);
			}
		} else if(vehTyp == OBJ_SHIP && FindInvType(objNum, OBJ_SKIFF, 0) != -1) {
			/*at sea on skiff*/
			objNum = Unboard(0);
		} else {
			CON_printf(/*264D*/"\nOnly next to land.\n");
		}
	} else if(
		/*a ship/skiff with a quality needs the matching deed
		(OBJ_SHIP_DEED is the "ship deed") carried by the party*/
		(vehTyp == OBJ_SHIP || vehTyp == OBJ_SKIFF) &&
		GetQual(objNum) != 0 && FindPartyMemberWithObj(OBJ_SHIP_DEED, GetQual(objNum)) == -1
	) {
		CON_printf(/*2662*/"\nA deed is required.\n");
	} else if(!CLOSE_ENOUGH(1, MapX, MapY, x, y)) {
		CON_printf(/*1F24*/"\n%s!\n", SpellMessages[3]);
	} else {
		/*boarding a ship from a skiff: the skiff is stowed aboard*/
		if(vehTyp == OBJ_SHIP && actorTyp == OBJ_SKIFF)
			InsertObj(actor, objNum, INVEN);
		Board(objNum);
	}
}

/*put NPC objNum on a horse at (x,y) with frame frame: the NPC becomes
 the front half of a mounted horse; the rear half is the existing
 horse rear (reuseRear) or a new object*/
/*C_27A1_5431*/MountHorse(int objNum, int x, int y, int frame, int reuseRear) {
	int dir, rear;
	struct coord xyz;

	MoveObj(objNum, x, y, MapZ);
	ObjShapeType[objNum] = OBJ_MOUNTED_HORSE | (frame << 10);
	MapX = x;
	MapY = y;
	/*the rear half is one tile behind (frame & 6 = direction)*/
	dir = frame & 6;
	x -= DirIncrX[dir];
	y -= DirIncrY[dir];
	if(reuseRear) {
		rear = FindLoc(x, y, MapZ);
		frame = GetFrame(rear);
		ObjShapeType[rear] = OBJ_MOUNTED_HORSE | (frame << 10);
	} else {
		SetCoordXYZ(xyz, x, y, MapZ);
		AddObj(0, xyz, OBJ_MOUNTED_HORSE, frame + 8, 0);/*frames 8+ = rear half*/
	}
	MAP_LoadAround();
	DisplayTick();
}

/*use (unmounted) horse: the active character mounts it*/
/*C_27A1_5503*/static UseHorse(int horse/*bp06*/) {
	int actor;
	int horseX, horseY, horseFrm;

	horse = COMBAT_getHead(horse);
	/*NPCs 0..0xdf are named characters: their horse can't be taken*/
	if(horse < 0xe0) {
		CON_printf(/*2678*/"\nNot boardable!\n");
		return;
	}
	actor = Party[Active];
	if(actor == /*NPC_*/0x09) {
		CON_printf(/*2689*/"\nSherry says: \"Eeek!!! I'm afraid of horses!\"\n");
		return;
	}
	if(actor == /*NPC_*/0xA4) {
		CON_printf(/*26B8*/"\nBehLem says: \"Horses are for food!\"\n");
		return;
	}
	if(GetType(actor) == OBJ_MOUNTED_HORSE) {
		CON_printf(/*26DE*/"\nYou're already on a horse!\n");
		return;
	}
	horseX = GetX(horse);
	horseY = GetY(horse);
	horseFrm = GetFrame(horse);
	/*change the type first so that DeleteObj does not also remove the
	rear half (it is reused by MountHorse)*/
	SetType(horse, OBJ_GOLD);
	DeleteObj(horse);
	MountHorse(actor, horseX, horseY, horseFrm, 1);
}

/*dismount mounted horse objNum: restore the rider's shape, delete the
 rear half and generate a riderless horse next to him*/
/*C_27A1_55F0*/DismountHorse(int objNum) {
	int rear;
	int newFrm, objTyp, cur_x, cur_y, rearX, rearY;
	int dir, horse, i, j, placed;

	placed = 0;
	objNum = COMBAT_getHead(objNum);
	cur_x = GetX(objNum);
	cur_y = GetY(objNum);
	dir = GetFrame(objNum) & 6;
	/*the rider gets back his original shape, facing the same way*/
	objTyp = OrigShapeType[objNum] & 0x3ff;
	newFrm = OBJ_MakeDirFrame(objTyp, GetDirection(objNum));
	ObjShapeType[objNum] = objTyp | (newFrm << 10);

	rearX = cur_x - DirIncrX[dir];
	rearY = cur_y - DirIncrY[dir];
	for(rear = FindLoc(rearX, rearY, MapZ); rear >= 0 && GetType(rear) != OBJ_MOUNTED_HORSE; rear = NextLoc());
	if(rear >= 0)
		DeleteObj(rear);
	else
		CON_printf(/*26FB*/"\nRear horse???\n");

	/*a new (riderless) horse on the first free neighbouring tile*/
	horse = EGG_generate(OBJ_HORSE, 0, 0, MapZ);
	for(i = 0; i < 3; i ++) {
		for(j = 0; j < 3; j ++) {
			if(i == 1 && j == 1)
				continue;
			if(CanMoveTo(horse, cur_x + i - 1, cur_y + j - 1)) {
				MoveObj(horse, cur_x + i - 1, cur_y + j - 1, MapZ);
				placed = 1;
				break;
			}
		}
		if(placed)/*leave the outer loop too*/
			break;
	}
	if(!placed) {
		CON_printf(/*24D5*/"\nNo room!\n");
		DeleteObj(horse);
	}
	DisplayTick();
}

/*use orb of the moons (once Lord British explained it): create a red
 moongate within 2 tiles; its quality (from the relative position)
 selects the destination*/
/*C_27A1_5789*/UseOrbOfTheMoons() {
	int gate, dest;
	struct coord xyz;

	/*conversation flag 5 of NPC 5 (Lord British?)*/
	if(!((TalkFlags[5] >> 5) & 1)) {
		CON_printf(/*270B*/"\nYou can't figure out how to use it.\n");
		return;
	}
	CON_printf(/*2731*/"Where: ");
	MouseMode = 1;
	if(SelectMode != 0) {
		DrawSlotCursor(0);
		SelectMode = 1;
		SelectRange = 7;
	}
	CON_getch();
	if(
		MapX - 2 <= Selection.x && MapX + 2 >= Selection.x &&
		MapY - 2 <= Selection.y && MapY + 2 >= Selection.y
	) {
		/*destination from the position in the 5x5 square around the
		Avatar: the 3 central tiles of the middle row give 0, the
		others (row + 2) * 5 + (column + 3)*/
		if(Selection.y == MapY && MapX - 1 <= Selection.x && MapX + 1 >= Selection.x)
			dest = 0;
		else
			dest = (Selection.y - MapY + 2) * 5 + (Selection.x - MapX + 3);
		/*create the gate off-map first to test the destination tile*/
		SetCoordXYZ(xyz, 0, 0, MapZ);
		gate = AddObj(0, xyz, OBJ_RED_GATE, 1, QualQuan(dest, 0));
		if(
			CanMoveTo(gate, Selection.x, Selection.y) &&
			FindTypeAt(Selection.x, Selection.y, MapZ, OBJ_DOORWAY) == -1 &&
			FindTypeAt(Selection.x, Selection.y, MapZ, OBJ_ARCHWAY) == -1
		) {
			CON_printf(/*2739*/"a red moon gate appears.\n");
			MoveObj(gate, Selection.x, Selection.y, MapZ);
			ScreenFade = 1;
			VIS_UpdateView();
			DisplayTick();
		} else {
			CON_printf(/*1D6D*/"%s\n", SpellMessages[1]);
			DeleteObj(gate);
		}
	} else {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[1]);
	}
}

/*use crystal ball: enter a latitude/longitude and view that place
 (as with "wizard eye"); a failed intelligence check hurts*/
/*C_27A1_5935*/UseCrystalBall() {
	int lat, lon;
	char buf[4];
	int eyeX, eyeY, cursorY, snd_flag, actor;

	actor = GetPartyObjOrVehicle(Active);
	if(FailsIntCheck(INTEL[actor])) {
		LoseHP(actor, OSI_rand(1, 10));
		CON_putch('\n');
		COMBAT_displaySeverity(actor);
		return;
	}
	if(SelectMode != 0) {
		DrawSlotCursor(0);
		SelectMode = 0;
	}
	CON_printf(/*2753*/"\nEnter degrees followed by N, S, E or W.\n");
	CON_printf(/*277D*/"\nAt latitude=");
	cursorY = CurClip->curY;
	CON_gets(buf, 4);
	lat = ParseNumberInput(buf, 1);
	if(lat > 80 || lat < -44) {
		CON_gotoxy(12, cursorY);
		CON_printf(/*1F02*/"0   \n");
		return;
	}
	CON_printf(/*278B*/"\n  longitude=");
	cursorY = CurClip->curY;
	CON_gets(buf, 4);
	lon = ParseNumberInput(buf, 2);
	if(lon > 88 || lon < -37) {
		CON_gotoxy(12, cursorY);
		CON_printf(/*1F02*/"0   \n");
		return;
	}
	CON_putch('\n');
	snd_flag = SoundFlag;
	SoundFlag = 0;
	/*8 tiles per degree, origin at (0x130,0x168) on the surface;
	other levels are 4 times smaller*/
	eyeX = 0x130 + (lon << 3);
	eyeY = 0x168 + (lat << 3);
	if(MapZ != 0) {
		eyeX >>= 2;
		eyeY >>= 2;
	}
	SPELL_WizardEye(eyeX, eyeY, 0);
	SoundFlag = snd_flag;
}

/*use telescope objNum: scroll the view in a direction until a view
 blocking tile (>= TIL_0FC) or ~40 tiles*/
/*C_27A1_5A97*/UseTelescope(int objNum) {
	int steps, tile;
	int savedX, savedY;

	steps = 40;
	tile = 0;
	savedX = MapX;
	savedY = MapY;
	MapX = GetX(objNum);
	MapY = GetY(objNum);
	ShowObjects();
	MAP_LoadAround();
	DisplayTick();
	CON_printf(/*2799*/"Direction-");
	MouseMode = 0;
	if(CON_getch() != CMD_DIRECTION) {
		CON_printf(/*1F7A*/"nowhere.\n");
		return;
	}
	CON_printf(/*1F84*/"%s.\n", DirNames[AdvanceDir]);
	/*XRayView: see through walls/everything lit while scrolling*/
	XRayView = 1;
	ScreenFade = 1;
	ShowObjects();
	MAP_LoadAround();
	DisplayTick();
	while(steps >= 0 && tile < TIL_0FC) {
		MapX += DirIncrX[AdvanceDir];
		MapY += DirIncrY[AdvanceDir];
		ShowObjects();
		MAP_LoadAround();
		DisplayTick();
		steps --;
		tile = GetTileAtXYZ(MapX, MapY, MapZ);
	}
	XRayView = 0;
	MapX = savedX;
	MapY = savedY;
	ShowObjects();
	MAP_LoadAround();
	ScreenFade = 1;
	DisplayTick();
	CON_printf(/*1F13*/"\n%s\n", SpellMessages[7]);
}

/*use silver horn: summon up to 3 groups of silver serpents at the
 edges of the screen (not while one is already present)*/
/*C_27A1_5BCF*/UseSilverHorn() {
	int x, y;
	int idx, offs, tries, n, serpent, egg_numObject, generated;
	struct coord xyz;

	generated = 0;
	for(idx = 0xe0; idx < 0x100; idx ++) {
		if(ObjShapeType[idx] != 0 && GetType(idx) == OBJ_SILVER_SERPENT) {
			CON_printf(/*27A4*/"\nNot now!\n");
			return;
		}
	}
	/*temporary off-map serpent, only used for the passability tests*/
	SetCoordXYZ(xyz, 0, 0, MapZ);
	serpent = AddObj(0, xyz, OBJ_SILVER_SERPENT, 0, 1);
	for(n = 0; n < 3; n ++) {
		/*up to 8 tries for a free spot just outside one of the 4
		screen edges*/
		for(tries = 0; tries < 8; tries ++) {
			idx = OSI_rand(0, 3);
			offs = OSI_rand(0, 10);
			switch(idx) {
				case 0: x = MapX - 6; y = MapY + offs - 6; break;
				case 1: x = MapX + offs - 6; y = MapY - 7; break;
				case 2: x = MapX + 7; y = MapY + offs - 6; break;
				case 3: x = MapX + offs - 6; y = MapY + 6; break;
			}
			if(
				CanMoveTo(serpent, x,     y) &&
				CanMoveTo(serpent, x - 1, y) &&
				CanMoveTo(serpent, x,     y + 1) &&
				CanMoveTo(serpent, x - 1, y + 1) &&
				CanMoveTo(serpent, x + 1, y) &&
				CanMoveTo(serpent, x,     y - 1)
			) break;
		}
		if(tries == 8)
			continue;
		/*an egg (chaotic alignment, 100% hatching) containing a silver
		serpent (embryo quality = its AI mode), hatched at once*/
		SetCoordXYZ(xyz, x, y, MapZ);
		egg_numObject = AddObj(0, xyz, OBJ_EGG, 0, QualQuan(4, 100));
		AddInvObj(OBJ_SILVER_SERPENT, CONTAINED, QualQuan(8, OSI_rand(4, 8)), egg_numObject);
		EGG_hatches(egg_numObject);
		generated ++;
	}
	DeleteObj(serpent);
	if(!generated) {
		CON_printf(/*27AF*/"\nNone generated!\n");
		return;
	}
	CON_printf(/*27C1*/"\nSilver snakes are generated!\n");
}

/*use passthrough objNum: slide it open/closed by one tile*/
/*C_27A1_5DF2*/UsePassthrough(int objNum/*bp06*/) {
	int objX;
	int objFrm, objTyp, objY, blocker;

	objTyp = GetType(objNum);
	objFrm = GetFrame(objNum);
	objX = GetX(objNum);
	objY = GetY(objNum);
	/*OBJ_PASSTHROUGH_116 slides vertically, OBJ_PASSTHROUGH_118
	horizontally; frame bit 1 = open*/
	if(objFrm == 0) {
		if(objTyp == OBJ_PASSTHROUGH_116)
			MoveObj(objNum, objX, objY - 1, MapZ);
		else
			MoveObj(objNum, objX - 1, objY, MapZ);
		SetFrame(objNum, objFrm | 2);
		CON_printf(/*27E0*/"\nOpen the passthrough.\n");
	} else {
		if(objTyp == OBJ_PASSTHROUGH_116) {
			blocker = FindLoc(objX, objY + 1, MapZ);
			if(blocker != -1 && blocker < 0x100) {
				CON_printf(/*27A4*/"\nNot now!\n");
				return;
			}
			MoveObj(objNum, objX, objY + 1, MapZ);
		} else {
			blocker = FindLoc(objX + 1, objY, MapZ);
			if(blocker != -1 && blocker < 0x100) {
				CON_printf(/*27A4*/"\nNot now!\n");
				return;
			}
			MoveObj(objNum, objX + 1, objY, MapZ);
		}
		SetFrame(objNum, objFrm & 1);
		CON_printf(/*27F8*/"\nClose the passthrough.\n");
	}
}

/*eat food objNum (one of a stack)*/
/*C_27A1_5F43*/UseFood(int objNum/*bp06*/) {
	int inInven;

	inInven = 0;
	CON_printf(/*2811*/"\nYou eat the food.\n");
	if(GetCoordUse(objNum) != LOCXYZ)
		inInven = 1;
	if(QuanType(GetType(objNum)) && GetQuan(objNum) > 1)
		SubQuan(objNum, 1);
	else
		DeleteObj(objNum);
	if(inInven)
		STAT_DrawInventory();
}

/*use vortex cube objNum: if it holds the 8 moonstones and the two lenses
 and the Codex are in place (and the party is near the Codex), the
 Codex vanishes and the game ends. returns 0 if nothing happens*/
/*C_27A1_5FAC*/static UseVortexCube(int objNum) {
	int i, stone;
	int hasStone[8];
	int codex_objNum;

	/*all moonstones in the vortex cube?*/
	for(i = 0; i < 8; i ++)
		hasStone[i] = 0;
	for(stone = FindInv(objNum); stone >= 0; stone = NextInv())
		hasStone[GetFrame(stone)] = 1;
	for(i = 0; i < 8; i ++) {
		if(!hasStone[i])
			return 0;
	}
	/*Britannia lens?*/
	if(FindTypeAt(0x399, 0x353, 0, OBJ_BRITANNIA_LENS_BLUE) == -1)
		return 0;
	/*Gargoyle lens?*/
	if(FindTypeAt(0x39d, 0x353, 0, OBJ_GARGOYLE_LENS_VIOLET) == -1)
		return 0;
	/*codex?*/
	codex_objNum = FindTypeAt(0x39b, 0x353, 0, OBJ_CODEX);
	if(codex_objNum == -1)
		return 0;
	/*the Codex must be on screen*/
	if(
		MapX - 5 > 0x39b || MapX + 5 < 0x39b ||
		MapY - 5 > 0x353 || MapY + 5 < 0x353
	) return 0;
	/*all conditions are OK*/
	DeleteObj(codex_objNum);
	RedrawMapWindow();
	CON_printf(/*2825*/"\nThe Codex has vanished!\n");
	CON_getch();
	LaunchEndGame();/*go to ending?*/

	return 1;
}

/*use rubber ducky: squeak*/
/*C_27A1_60BD*/UseRubberDucky() {
	CON_printf(/*283F*/"\nSqueak!\n");
	MUS_PlaySfx(22, 0);
}

/*use fan: rotate the wind direction*/
/*C_27A1_60D6*/UseFan() {
	WindDir = (WindDir + 1) & 7;
	CON_printf(/*2849*/"\nYou feel a breeze.\n");
	PassTime(0);/*refresh the time-dependent state*/
}

/*types that can't be used while in a vehicle*/
/*1D59*/unsigned NoVehicleUseTypes[] = {OBJ_BALLOON_INFLATED,OBJ_BALLOON_PLANS,OBJ_CANNON,OBJ_BALLOON_DEFLATED,OBJ_HORSE,OBJ_LADDER,OBJ_RAFT,OBJ_SHIP,OBJ_SKIFF,OBJ_STAFF};

/*is objNum something that can't be used from a vehicle (NoVehicleUseTypes)?
 (using the vehicle itself, or a ship from its skiff, is allowed)*/
/*C_27A1_60F5*/IsNotUsableInVehicle(int objNum) {
		int i, objTyp;
		int found, actor;
		found = 0;
		objTyp = GetType(objNum);
		for(i = 0; i < 10; i ++) {
			if(NoVehicleUseTypes[i] == objTyp) {
				found = 1;
				break;
			}
		}
		if(found == 0)
			return 0;
		actor = Party[Active];
		if(
			(IN_VEHICLE && actor == objNum) ||
			(GetType(actor) == OBJ_SKIFF && objTyp == OBJ_SHIP)
		) return 0;
		return 1;
}

/*"use" command: dispatch on the selected object's type to its handler*/
/*C_27A1_6179*/CmdUse() {
	int objTyp, topObj;
	int objFrm, actorX, actorY, topTyp, dir;

	Selection.obj = COMBAT_getHead(Selection.obj);
	objTyp = GetType(Selection.obj);
	/*pointing at a person (except horses/cows, or from a vehicle) or
	at an "ignore" tile: use what is under it*/
	if(
		(Selection.obj <= 0xff && Selection.x != -1 &&
		objTyp != OBJ_HORSE && objTyp != OBJ_MOUNTED_HORSE && objTyp != OBJ_COW &&
		!IN_VEHICLE) ||
		(IsTileIg(TILE_FRAME(Selection.obj)))
	) Selection.obj = FindNonIgnoredObjAt(Selection.obj);
	if(Selection.obj == -1) {
		CON_printf(/*1D6D*/"%s\n", SpellMessages[5]);
		return;
	}
	Selection.obj = COMBAT_getHead(Selection.obj);
	if(objTyp == OBJ_MOUNTED_HORSE) {
		CON_printf(/*285E*/"horse\n");
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_putch('\n');
	}
	objTyp = GetType(Selection.obj);
	if(!IsUsableType(objTyp)) {
		CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
		return;
	}
	if(IN_VEHICLE && IsNotUsableInVehicle(Selection.obj)) {
		CON_printf(/*1D6F*/"\n");
		CON_printf(NotOnShipMsg);
		return;
	}
	if(GetCoordUse(Selection.obj) == LOCXYZ) {
		dir = MkDirection(PointerX, PointerY);
		if(dir != -1) {
			UpdateMoveFrame(Party[Active], dir);
			VIS_UpdateView();
		}
		actorX = GetX(Party[Active]);
		actorY = GetY(Party[Active]);
		if(!CLOSE_ENOUGH(1, Selection.x, Selection.y, actorX, actorY)) {
			CON_printf(/*1F24*/"\n%s!\n", SpellMessages[3]);
			return;
		}
	}
	/*a door (or secret door) can't be used while something lies in
	the doorway (on top of it)*/
	topObj = FindLoc(GetX(Selection.obj), GetY(Selection.obj), MapZ);
	topTyp = GetType(topObj);
	if(
		(IsDoorType(objTyp) && !IsDoorType(topTyp) && !IsTileIg(TILE_FRAME(topObj))) ||
		(objTyp == OBJ_SECRET_DOOR && topTyp != OBJ_SECRET_DOOR)
	) {
		CON_printf(/*27A4*/"\nNot now!\n");
		return;
	}
	/*a readied object can only be used by the member who holds it*/
	if(Active != StatusMember && GetCoordUse(Selection.obj) == EQUIP && objTyp != OBJ_TORCH) {
		CON_printf(/*2865*/"\nMust be in your hand.\n");
		return;
	}
	objFrm = GetFrame(Selection.obj);
	switch(objTyp) {
		case OBJ_WINE:
		case OBJ_MEAD:
		case OBJ_ALE: UseDrink(Selection.obj); break;
		case OBJ_SHIP:
		case OBJ_SKIFF:
		case OBJ_RAFT:
		case OBJ_BALLOON_INFLATED:
			if(GetCoordUse(Selection.obj) != LOCXYZ)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				UseVehicle(Selection.obj);
		break;
		case OBJ_BALLOON_PLANS: UseBalloonPlans(); break;
		case OBJ_OPEN_BARREL:
		case OBJ_OPEN_CRATE:
			/*frame 1 = closed: open it, then search it*/
			if(objFrm == 1)
				SetFrame(Selection.obj, 0);
			else
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			SearchObj(Selection.obj, Selection.x, Selection.y);
		break;
		case OBJ_BELL:
		case OBJ_PULL_CHAIN: UseBell(); break;
		case OBJ_BEEHIVE: UseBeehive(); break;
		case OBJ_CANDLE:
		case OBJ_CANDELABRA:
		case OBJ_FIREPLACE:
		case OBJ_BRAZIER:
		case OBJ_CAMPFIRE:
			/*(braziers with frame bit 1 set can't be lit/doused)*/
			if(objTyp == OBJ_BRAZIER && (objFrm & 2))
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				UseLightSource(Selection.obj);
		break;
		case OBJ_BUNCH_OF_GRAPES:
		case OBJ_BUTTER:
		case OBJ_LOAF_OF_BREAD:
		case OBJ_PORTION_OF_MEAT:
		case OBJ_ROLLS:
		case OBJ_CAKE:
		case OBJ_CHEESE:
		case OBJ_HAM:
		case OBJ_HORSE_CHOPS:
		case OBJ_BUCKET_OF_MILK:
		case OBJ_JAR_OF_HONEY:
		case OBJ_MEAT_RIB:
		case OBJ_RIBS:
		case OBJ_FISH: UseFood(Selection.obj); break;
		case OBJ_CANNON: UseCannon(Selection.obj); break;
		case OBJ_OAKEN_DOOR:
		case OBJ_WINDOWED_DOOR:
		case OBJ_CEDAR_DOOR:
		case OBJ_STEEL_DOOR: UseDoor(Selection.obj, 0, 0, 0); break;
		case OBJ_OPEN_CHEST:
			if(GetCoordUse(Selection.obj) != LOCXYZ)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				UseChest(Selection.obj, 0, 0, 0);
		break;
		case OBJ_CHURN: UseChurn(); break;
		case OBJ_COW: UseCow(); break;
		case OBJ_CRANK: UseCrank(Selection.obj); break;
		case OBJ_CRYSTAL_BALL: UseCrystalBall(); break;
		case OBJ_BALLOON_DEFLATED:
			/*only on the surface (0) or in the gargoyle land (5)*/
			if(MapZ != 0 && MapZ != 5)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				UseDeflatedBalloon(Selection.obj);
		break;
		case OBJ_FAN: UseFan(); break;
		case OBJ_FISHING_POLE: UseFishingPole(); break;
		case OBJ_FOUNTAIN: UseFountain(); break;
		case OBJ_GARGISH_VOCABULARY: UseGargishVocabulary(); break;
		case OBJ_GEM: UseGem(Selection.obj); break;
		case OBJ_HARP: PlayInstrument(2); break;/*play the instrument*/
		case OBJ_HARPSICHORD: PlayInstrument(3); break;/*play the instrument*/
		case OBJ_HORSE: UseHorse(Selection.obj); break;
		case OBJ_MOUNTED_HORSE: DismountHorse(Selection.obj); break;
		case OBJ_LOCKPICK:
		case OBJ_KEY: UseKeyOrLockpick(Selection.obj); break;
		case OBJ_LADDER:
			if(SoloMember == -1)
				PartyChangeLevel(Selection.obj);
			else
				CON_printf(/*287D*/"\nNot in solo mode.\n");
		break;
		case OBJ_LEVER: UseLever(Selection.obj); break;
		case OBJ_LUTE: PlayInstrument(0x12); break;/*play the instrument*/
		case OBJ_MOONSTONE: UseMoonstone(Selection.obj); break;
		case OBJ_ORB_OF_THE_MOONS:
			if(GetCoordUse(Selection.obj) == LOCXYZ)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				UseOrbOfTheMoons();
		break;
		case OBJ_PANPIPES: PlayInstrument(0x13); break;/*play the instrument*/
		case OBJ_PASSTHROUGH_116:
		case OBJ_PASSTHROUGH_118: UsePassthrough(Selection.obj); break;
		case OBJ_PICK:
		case OBJ_SHOVEL:
			if(GetCoordUse(Selection.obj) == EQUIP)
				UseDiggingTool();
			else
				CON_printf(/*2891*/"\nNot readied.\n");
		break;
		case OBJ_POTION: UsePotion(Selection.obj); break;
		case OBJ_POWDER_KEG: UsePowderKeg(Selection.obj); break;
		case OBJ_RUBBER_DUCKY: UseRubberDucky(); break;
		case OBJ_RUNE_OF_HONESTY:
		case OBJ_RUNE_OF_COMPASSION:
		case OBJ_RUNE_OF_VALOR:
		case OBJ_RUNE_OF_JUSTICE:
		case OBJ_RUNE_OF_SACRIFICE:
		case OBJ_RUNE_OF_HONOR:
		case OBJ_RUNE_OF_SPIRITUALITY:
		case OBJ_RUNE_OF_HUMILITY: UseRune(Selection.obj); break;
		case OBJ_SECRET_DOOR: UseSecretDoor(Selection.obj); break;
		case OBJ_SEXTANT:
			if(MapZ != 0 && MapZ != 5)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else
				SPELL_PrintLocation();
		break;
		case OBJ_SILVER_HORN: UseSilverHorn(); break;
		case OBJ_STAFF:
			if(GetCoordUse(Selection.obj) == LOCXYZ)
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[11]);
			else if(GetCoordUse(Selection.obj) == EQUIP)
				UseStaff(Selection.obj);
			else
				CON_printf(/*2891*/"\nNot readied.\n");
		break;
		case OBJ_SWITCH: UseSwitch(Selection.obj); break;
		case OBJ_TELESCOPE: UseTelescope(Selection.obj); break;
		case OBJ_TORCH: UseTorch(Selection.obj); break;
		case OBJ_VORTEX_CUBE:/*vortex cube: the endgame*/
			if(IsArmageddon || !UseVortexCube(Selection.obj))
				CON_printf(/*1F13*/"\n%s\n", SpellMessages[2]);
		break;
		case OBJ_WELL: UseWell(); break;
		case OBJ_XYLOPHONE: PlayInstrument(0x14); break;/*play the instrument*/
	}
	/*using costs 5 movement points*/
	SubMov(Party[Active], 5);
	VIS_UpdateView();
	DisplayTick();
}
