/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Status, equipment, inventory module

	Draws the status panel (top right of the screen, TextWindows[1]):
	the party list, a character's portrait and statistics, and the
	equipment/inventory view (8 equipment slots around the character
	figure, a 4x3 grid of carried objects, scrollable by rows of 4).
	Also computes carried weights (in tenths of stones) and fills
	Equipment[] (object in each SLOT_xxx; 1 in SLOT_LHND means "taken by
	a two-handed weapon"), maps mouse clicks on the panel to Selection,
	and implements the "Ready" / "Unready" actions.
*/
#include  "u6.h"

/*07CE*/unsigned char InvenScroll = 0;

/*display fight mode (indexed by NPCComMode - AI_COMMAND)*/
/*07CF*/static char *ComModeNames[] = {
	/*0831*/"COMMAND",
	/*0839*/"FRONT",
	/*083F*/"REAR",
	/*0844*/"FLANK",
	/*084A*/"BERSERK",
	/*0852*/"RETREAT",
	/*085A*/"ASSAULT"
};

/*display party info: up to 5 members starting with party index
  "first" (picture, hit points, name), with scroll arrows*/
/*C_155D_000C*/STAT_ShowParty(int16 first) {
	int16 i;
	int16 mouseState, tile, objTyp, objNum;

	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(168, 6, 312, 103);
	DrawSkyBar();
	CON_setClip(&TextWindows[0]);
	/*up arrow*/
	if(first) {
		CON_gotoxy(21, 3);
		CON_putch(0x18);
	}
	/*down arrow*/
	if(first < 16 && PartySize > first + 5) {
		CON_gotoxy(21, 12);
		CON_putch(0x19);
	}
	CON_setClip(&TextWindows[1]);
	for(i = first; i < PartySize && i < first + 5; i ++) {
		objNum = Party[i];
		/*original (not current) shape, facing south*/
		objTyp = OrigShapeType[objNum] & 0x3ff;
		tile = OBJ_MakeDirFrame(objTyp, 4) + BaseTile[objTyp];
		DrawSlotTile(tile, TIL_SLOT_EMPTY, 176, TIL2SCR(i - first) + 24);
		CON_gotoxy(14, ((i - first) << 1) + 2);
		/*hit points: green if poisoned, red if low*/
		if(IsPoisoned(objNum))
			CurClip->fgColor = ColorGreen;
		else if(HitPoints[objNum] < 10)
			CurClip->fgColor = ColorRed;
		CON_printf(/*0862*/"%3d\n", HitPoints[objNum]);
		CurClip->fgColor = ColorText;
		CON_gotoxy(3, ((i - first) << 1) + 3);
		CON_printf(/*0867*/"%s\n", Names[i]);
	}
	CON_setClip(&TextWindows[3]);
	CON_mouseOn(mouseState);
}

/*character combat mode: (re)print the combat mode of the displayed
  party member (StatusMember) at the bottom right of the panel*/
/*C_155D_01BC*/STAT_refreshComMode() {
	int16 objNum, len;
	int16 mouseState;
	struct tClipInfo *savedClip;

	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	savedClip = CurClip;
	CON_setClip(&TextWindows[1]);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(256, 96, 311, 103);
	objNum = Party[StatusMember];
	len = strlen(ComModeNames[NPCComMode[objNum] - AI_COMMAND]);
	CON_gotoxy((8 - len) / 2 + 10, 11);
	CON_printf(/*086B*/"%s", ComModeNames[NPCComMode[objNum] - AI_COMMAND]);
	CON_setClip(savedClip);
	CON_mouseOn(mouseState);
}

/*character status view: portrait, attributes, magic, health,
  level/experience of party member #partyIdx, and the panel buttons*/
/*C_155D_028A*/STAT_ShowPortrait(int16 partyIdx/*bp06*/) {
	int16 objNum,mouseState;

	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(168, 6, 312, 103);
	CON_setClip(&TextWindows[1]);
	CON_gotoxy((17 - strlen(Names[partyIdx])) / 2, 0);
	CON_printf(/*0867*/"%s\n", Names[partyIdx]);
	DrawPortrait(Party[partyIdx], 176, 16);/*load portrait?*/
	objNum = Party[partyIdx];
	CON_gotoxy(10, 2); CON_printf(/*086E*/"STR:%d\n", STREN[objNum]);
	CON_gotoxy(10, 3); CON_printf(/*0876*/"DEX:%d\n", DEXTE[objNum]);
	CON_gotoxy(10, 4); CON_printf(/*087E*/"INT:%d\n", INTEL[objNum]);
	CON_gotoxy(10, 6); CON_printf(/*0886*/"Magic");
	CON_gotoxy(10, 7); CON_printf(/*088C*/"%2d%/%d", MAGIC[objNum], MaxMagic(objNum));
	CON_gotoxy(10, 8); CON_printf(/*0894*/"Health");
	CON_gotoxy(10, 9);
	if(IsPoisoned(objNum))
		CurClip->fgColor = ColorGreen;
	else if(HitPoints[objNum] < 10)
		CurClip->fgColor = ColorRed;
	CON_printf(/*089B*/"%3d", HitPoints[objNum]);
	CurClip->fgColor = ColorText;
	CON_printf(/*088F*/"%/%d", MaxHP(objNum));
	CON_gotoxy(10, 10); CON_printf(/*089F*/"Lev%/Exp");
	CON_gotoxy(10, 11); CON_printf(/*08A8*/"%d%/%d", GetLevel(objNum), ExpPoints[objNum]);
	if(StatusMember > 0)
		GR_PUT_TILE_SCREEN(TIL_BUTTON_NEXT_LEFT, 176, 88);
	GR_PUT_TILE_SCREEN(TIL_BUTTON_PARTY, 192, 88);
	GR_PUT_TILE_SCREEN(TIL_BUTTON_BAG, 208, 88);
	if(PartySize - 1 > StatusMember)
		GR_PUT_TILE_SCREEN(TIL_BUTTON_NEXT_RIGHT, 224, 88);
	CON_setClip(&TextWindows[3]);
	CON_mouseOn(mouseState);
}

/*tiles of the objects readied in one hand (SLOT_RHND)*/
/*07DD*/int16 OneHandedTiles[] = {
	TIL_SLING,TIL_CLUB,TIL_SPEAR,TIL_THROWING_AXE,TIL_DAGGER,TIL_MACE,TIL_MORNING_STAR,TIL_SWORD,
	TIL_GLASS_SWORD,TIL_BOOMERANG,TIL_SPELLBOOK,TIL_STAFF,TIL_FIRE_WAND,TIL_LIGHTNING_WAND,TIL_FLASK_OF_OIL,TIL_TORCH,
	TIL_TORCH_LIT,TIL_ZU_YLEM,TIL_SCYTHE,TIL_PITCHFORK,TIL_RAKE,TIL_PICK,TIL_SHOVEL,TIL_HOE,
	TIL_ROLLING_PIN,TIL_CLEAVER,TIL_KNIFE,TIL_KNIFE_1,TIL_WINE,TIL_MEAD,TIL_DECORATIVE_SWORD_0,TIL_DECORATIVE_SWORD_1,
	TIL_LUTE
};

/*equipment slot an object goes to (by its tile): SLOT_xxx,
  SLOT_2HND for two-handed weapons, SLOT_RING for rings (either hand),
  or -1 if it can't be readied*/
/*C_155D_0516*/STAT_GetEquipSlot(int16 objNum) {
	int16 tile, i;

	tile = TILE_FRAME(objNum);
	if(tile == TIL_LEATHER_BOOTS || tile == TIL_SWAMP_BOOTS)
		return SLOT_FEET;
	if(tile == TIL_RING || (tile >= TIL_PROTECTION_RING && tile <= TIL_INVISIBILITY_RING))
		return SLOT_RING;
	if(tile == TIL_GARGOYLE_BELT || (tile >= TIL_ANKH_AMULET && tile <= TIL_AMULET_OF_SUBMISSION) || tile == TIL_SPIKED_COLLAR || tile == TIL_HIT/*?? (ground tile)*/)
		return SLOT_NECK;
	if(tile >= TIL_LEATHER_HELM && tile <= TIL_MAGIC_HELM)
		return SLOT_HEAD;
	if((tile >= TIL_CLOTH_ARMOR && tile <= TIL_MAGIC_ARMOR) || tile == TIL_GUILD_BELT ||
		tile == TIL_GARGOYLE_BELT || tile == TIL_TUNIC || tile == TIL_DRESS || tile == TIL_PANTS ||
		tile == TIL_STORM_CLOAK)
		return SLOT_CHST;
	if(tile == TIL_BOW || tile == TIL_CROSSBOW || tile == TIL_TRIPLE_CROSSBOW || tile == TIL_MAGIC_BOW || (tile >= TIL_TWO_HANDED_HAMMER && tile <= TIL_HALBERD))
		return SLOT_2HND;
	if((tile >= TIL_WOODEN_SHIELD && tile <= TIL_MAGIC_SHIELD) || tile == TIL_MAIN_GAUCHE)
		return SLOT_LHND;
	for(i = 0; i < 33; i ++)
		if(OneHandedTiles[i] == tile)
			return SLOT_RHND;
	return -1;
}

/*types whose TypeWeight is for 10 units (gold coins, reagents)*/
/*081F*/int16 WeightPer10Types[] = {OBJ_GOLD,OBJ_BLACK_PEARL,OBJ_BLOOD_MOSS,OBJ_GARLIC,OBJ_GINSENG,OBJ_MANDRAKE,OBJ_NIGHTSHADE,OBJ_SPIDERSILK,OBJ_ASH};

/*is objType one of WeightPer10Types (weight counted per 10 units)?*/
/*C_155D_063A*/IsWeightPer10Type(int16 objType) {
	int16 i;

	for(i = 0; i < 9; i ++) {
		if(WeightPer10Types[i] == objType)
			return 1;
	}
	return 0;
}

/*weight of an object (not of its content), in tenths of stones;
  a weight of 255 means "immovable" (10000)*/
/*C_155D_0661*/GetWeight(int16 objNum) {
	int16 objTyp, qty, weight, quanType;

	objTyp = GetType(objNum);
	quanType = QuanType(objTyp);
	if(quanType == 4)
		/*quantity on 16 bits*/
		qty = Amount[objNum];
	else if(quanType == 2 && (objTyp != OBJ_TORCH || GetFrame(objNum) != 1))
		/*quantity on 8 bits (a lit torch counts for one)*/
		qty = GetQuan(objNum);
	else
		qty = 1;
	if(IsWeightPer10Type(objTyp))
		weight = (TypeWeight[objTyp] * qty) / 10;
	else
		weight = TypeWeight[objTyp] * qty;
	if(weight == 255)
		weight = 10000;
	return weight;
}

/*total weight of the objects contained (at any depth) in objNum:
  they follow it in the Link list*/
/*C_155D_0707*/Encumbrance(int16 objNum) {
	int16 item, weight;

	weight = 0;
	for(
		item = Link[objNum];
		item >= 0 && GetCoordUse(item) != LOCXYZ && OBJ_IsInside(item, objNum);
		item = Link[item]
	) weight += GetWeight(item);
	return weight;
}

/*compute InvenSize, WeightInven and (for an NPC) WeightEquip of
  objNum's inventory, without touching Equipment[]*/
/*C_155D_0748*/STAT_ComputeWeights(int16 objNum) {
	int16 item, partyIdx;
	int16 objTyp;

	InvenSize = 0;
	WeightInven = 0;
	if(objNum < 0x100)
		WeightEquip = 0;
	/*party index: computed but unused*/
	for(partyIdx = 0; partyIdx < 8; partyIdx ++)
		if(Party[partyIdx] == objNum)
			break;
	for(item = FindInv(objNum); item >= 0; item = NextInv()) {
		objTyp = GetType(item);
		if(GetCoordUse(item) == EQUIP) {
			WeightEquip += TypeWeight[objTyp];
		} else {
			WeightInven += GetWeight(item) + Encumbrance(item);
			InvenSize ++;
		}
	}
}

/*same as STAT_ComputeWeights, and also put each equipped object of objNum
  in its Equipment[] slot (Equipment[] must have been cleared)*/
/*C_155D_07E0*/STAT_ComputeEquipment(int16 objNum) {
	int16 slot, item;
	int16 objTyp, partyIdx;

	InvenSize = 0;
	WeightInven = 0;
	if(objNum < 0x100)
		WeightEquip = 0;
	for(partyIdx = 0; partyIdx < 8; partyIdx ++) {
		if(Party[partyIdx] == objNum)
			break;
	}
	for(item = FindInv(objNum); item >= 0; item = NextInv()) {
		objTyp = GetType(item);
		if(GetCoordUse(item) == EQUIP) {
			WeightEquip += TypeWeight[objTyp];
			slot = STAT_GetEquipSlot(item);
			if(slot == SLOT_2HND) {
				/*two-handed: right hand, left hand marked as taken*/
				slot = SLOT_RHND;
				Equipment[SLOT_LHND] = 1;
				if(!ShowNPCInven)
					Party_2Handed[partyIdx] = 1;
				else
					NPC_2Handed = 1;
			} else if(slot == SLOT_RHND && Equipment[SLOT_RHND]) {
				slot = SLOT_LHND;
			} else if(slot == SLOT_LHND && Equipment[SLOT_LHND]) {
				slot = SLOT_RHND;
			} else if(slot == SLOT_RING) {
				if(Equipment[SLOT_RFNG])
					slot = SLOT_LFNG;
				else
					slot = SLOT_RFNG;
			}
			Equipment[slot] = item;
		} else {
			WeightInven += GetWeight(item) + Encumbrance(item);
			InvenSize ++;
		}
	}
}

/*draw equipment: the 8 slots of Equipment[] (empty slot frame if
  nothing; left hand "taken" if a two-handed weapon is readied),
  shifted by (x, y)*/
/*C_155D_08F4*/STAT_DrawEquipment(register int16 x, register int16 y) {
	int16 slot, objNum;

	for(slot = 0; slot < 8; slot ++) {
		objNum = Equipment[slot];
		if(objNum) {
			switch(slot) {
				case SLOT_HEAD: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 200, y + 16); break;
				case SLOT_NECK: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 176, y + 24); break;
				case SLOT_CHST: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 224, y + 24); break;
				case SLOT_RHND: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 176, y + 40); break;
				case SLOT_LHND:
					if((!ShowNPCInven && Party_2Handed[StatusMember]) || (ShowNPCInven && NPC_2Handed))
						GR_PUT_TILE_SCREEN(TIL_SLOT_TWO_HANDED, x + 224, y + 40);
					else
						DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 224, y + 40);
				break;
				case SLOT_RFNG: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 176, y + 56); break;
				case SLOT_LFNG: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 224, y + 56); break;
				case SLOT_FEET: DrawSlotTile(TILE_FRAME(objNum), TIL_SLOT_EMPTY, x + 200, y + 64); break;
			}
		} else {
			switch(slot) {
				case SLOT_HEAD: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 200, y + 16); break;
				case SLOT_NECK: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 176, y + 24); break;
				case SLOT_CHST: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 224, y + 24); break;
				case SLOT_RHND: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 176, y + 40); break;
				case SLOT_LHND:
					if((!ShowNPCInven && Party_2Handed[StatusMember]) || (ShowNPCInven && NPC_2Handed))
						GR_PUT_TILE_SCREEN(TIL_SLOT_TWO_HANDED, x + 224, y + 40);
					else
						GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 224, y + 40);
				break;
				case SLOT_RFNG: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 176, y + 56); break;
				case SLOT_LFNG: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 224, y + 56); break;
				case SLOT_FEET: GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x + 200, y + 64); break;
			}
		}
	}
}

/*tenths of stones -> displayed stones: rounded to nearest,
  but anything below 1 stone shows as 1*/
/*C_155D_0CB6*/static WeightToStones(int16 tenths) {
	int16 stones, rest;

	stones = tenths / 10;
	rest = tenths % 10;
	if(stones)
		return stones + (rest >= 5);
	else
		return stones + (rest != 0);
}

/*draw the equipment and inventory of InvenOwner (12 visible objects from
  scroll position InvenScroll, recorded in InvenSlotObj), the scroll arrows,
  the owner's picture and the equipped/total weights vs capacity*/
/*C_155D_0CF5*/STAT_DrawInventory() {
	int16 slot, item;
	int16 mouseState, objTyp, tile, owner;
	struct tClipInfo *savedClip;

	if(StatusDisplay == CMD_STAT_PARTY && PickpocketActive == 0)
		return;
	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	savedClip = CurClip;
	CON_setClip(&TextWindows[1]);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(248, 16, 311, 79);
	STAT_DrawEquipment(0, 0);
	item = FindInv(InvenOwner);
	slot = 0;
	/*the last row became empty: scroll back one row*/
	if(InvenScroll >= 4 && InvenScroll + 8 >= InvenSize)
		InvenScroll -= 4;
	/*skip the scrolled-out objects*/
	while(InvenScroll > slot) {
		if(GetCoordUse(item) == INVEN || GetCoordUse(item) == CONTAINED)
			slot ++;
		item = NextInv();
	}
	/*owner's picture (an NPC facing south, or the container)*/
	objTyp = GetType(InvenOwner);
	if(InvenOwner < 0x100) {
		objTyp = OrigShapeType[InvenOwner] & 0x3ff;
		tile = OBJ_MakeDirFrame(objTyp, 4) + BaseTile[objTyp];
		DrawSlotTile(tile, TIL_SLOT_EMPTY, 272, 16);
	} else {
		DrawSlotTile(TILE_FRAME(InvenOwner), TIL_SLOT_EMPTY, 272, 16);
	}
	/*the 4x3 grid*/
	for(slot = 0; item >= 0 && slot < 12; item = NextInv()) {
		if(GetCoordUse(item) == INVEN || GetCoordUse(item) == CONTAINED) {
			DrawInvObject(item, 248 + TIL2SCR(slot & 3), 32 + TIL2SCR(slot >> 2));
			InvenSlotObj[slot++] = item;
		}
	}
	for( ;slot < 12; slot ++) {
		InvenSlotObj[slot] = 0;
		GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, 248 + TIL2SCR(slot & 3), 32 + TIL2SCR(slot >> 2));
	}
	/*scroll arrows*/
	if(InvenSize > InvenScroll + 12) {
		CON_gotoxy(8, 8);
		CON_putch(0x19);
	} else {
		CON_gotoxy(8, 8);
		CON_putch(' ');
	}
	if(InvenScroll == 0) {
		CON_gotoxy(8, 3);
		CON_putch(' ');
	} else {
		CON_gotoxy(8, 3);
		CON_putch(0x18);
	}
	/*the NPC carrying it all (InvenOwner may be a container)*/
	owner = InvenOwner;
	if(owner >= 0x100) {
		do
			owner = GetAssoc(owner);
		while(owner >= 0x100);
	}
	/*display equiped weight (capacity: strength)*/
	CON_gotoxy(2, 9);
	CON_printf(/*08AF*/"      ");
	CON_gotoxy(2, 9);
	CON_printf(/*08B6*/"%d%/%ds", WeightToStones(WeightEquip), STREN[owner]);
	/*display inventory weight (total, capacity: 2 x strength)*/
	CON_gotoxy(11, 9);
	CON_printf(/*08AF*/"      ");
	CON_gotoxy(11, 9);
	CON_printf(/*08B6*/"%d%/%ds", WeightToStones(WeightInven + WeightEquip), STREN[owner] << 1);
	/* */
	CON_setClip(savedClip);
	CON_mouseOn(mouseState);
}


/*recompute Equipment[] and weights of objNum and redraw the
  inventory panel*/
/*C_155D_1022*/STAT_RefreshInventory(int16 objNum) {
	int16 i, unused;

	for(i = 0; i < 12; i ++)
		InvenSlotObj[i] = 0;
	if(objNum < 0x100) {
		for(i = 0; i < 8; i ++)
			Equipment[i] = 0;
	}
	STAT_ComputeEquipment(objNum);
	STAT_DrawInventory();
}

/*character equipment view: of party member StatusMember, or of NPC
  ShowNPCInven if not 0 (the parameter is ignored)*/
/*C_155D_1065*/STAT_ShowInventory(int16 unusedArg) {
	int16 mouseState;

	InvenScroll = 0;
	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(168, 6, 312, 103);
	CON_setClip(&TextWindows[1]);
	if(!ShowNPCInven) {
		CON_gotoxy((17 - strlen(Names[StatusMember])) / 2, 0);
		CON_printf(/*0867*/"%s\n", Names[StatusMember]);
	}
	DrawPaperDoll(192, 32);
	CON_gotoxy(0, 9); CON_printf(/*08BE*/"E:");
	CON_gotoxy(9, 9); CON_printf(/*08C1*/"I:");
	if(!ShowNPCInven) {
		if(StatusMember > 0)
			GR_PUT_TILE_SCREEN(TIL_BUTTON_NEXT_LEFT, 176, 88);
		GR_PUT_TILE_SCREEN(TIL_BUTTON_PARTY, 192, 88);
		GR_PUT_TILE_SCREEN(TIL_BUTTON_PORTRAIT, 208, 88);
		if(PartySize - 1 > StatusMember)
			GR_PUT_TILE_SCREEN(TIL_BUTTON_NEXT_RIGHT, 224, 88);
		if(StatusMember)
			GR_PUT_TILE_SCREEN(TIL_BUTTON_COMBAT_MODE, 240, 88);
	}
	if(!ShowNPCInven)
		InvenOwner = Party[StatusMember];
	else
		InvenOwner = ShowNPCInven;
	Party_2Handed[StatusMember] = 0;
	STAT_RefreshInventory(InvenOwner);
	STAT_refreshComMode();
	CON_setClip(&TextWindows[3]);
	CON_mouseOn(mouseState);
}

/*scroll the inventory down one row*/
/*C_155D_120A*/STAT_InvenScrollDown(int16 unusedArg) {
	CON_setClip(&TextWindows[1]);
	if(InvenScroll + 4 < InvenSize) {
		InvenScroll += 4;
		STAT_DrawInventory();
		CON_setClip(&TextWindows[3]);
	}
}

/*scroll the inventory up one row*/
/*C_155D_123E*/STAT_InvenScrollUp(int16 unusedArg) {
	CON_setClip(&TextWindows[1]);
	if(InvenScroll >= 4) {
		InvenScroll -= 4;
		STAT_DrawInventory();
		CON_setClip(&TextWindows[3]);
	}
}

/*mouse on the inventory grid or the owner's picture?
  sets Selection (obj: object or -1, x: grid slot + 1 / 0 for a
  contained owner / map position for an NPC owner); returns 1 if hit*/
/*C_155D_1267*/STAT_PickInvenSlot() {
	int16 slot;
	if(
		PointerX >= 248 && PointerX < 312 &&
		PointerY >= 32 && PointerY < 80
	) {
		slot = SCR2TIL(PointerX - 248);
		slot += SCR2TIL(PointerY - 32) << 2;
		Selection.obj = InvenSlotObj[slot];
		if(Selection.obj == 0)
			Selection.obj = -1;
		Selection.x = slot + 1;
		return 1;
	}
	if(PointerX >= 272 && PointerX < 288 && PointerY >= 16 && PointerY < 32) {
		if(InvenOwner >= 0x100) {
			Selection.x = 0;
		} else {
			Selection.x = MapX;
			Selection.y = MapY;
		}
		Selection.obj = InvenOwner;
		return 1;
	}
	Selection.obj = -1;
	return 0;
}

/*mouse on an equipment slot? sets Selection.x = slot and
  Selection.obj = object (-1 if empty or taken by a two-handed weapon);
  returns 1 if hit*/
/*C_155D_130E*/STAT_PickEquipSlot() {
	if(PointerX >= 200 && PointerX < 216 && PointerY >= 16 && PointerY < 32) {
		Selection.x = 0;
		Selection.obj = Equipment[SLOT_HEAD];
	} else if(PointerX >= 176 && PointerX < 192 && PointerY >= 24 && PointerY < 72) {
		/*left column: SLOT_NECK, SLOT_RHND, SLOT_RFNG*/
		Selection.x = SCR2TIL(PointerY - 24) + 1;
		Selection.obj = Equipment[Selection.x];
	} else if(PointerX >= 224 && PointerX < 240 && PointerY >= 24 && PointerY < 72) {
		/*right column: SLOT_CHST, SLOT_LHND, SLOT_LFNG*/
		Selection.x = SCR2TIL(PointerY - 24) + 4;
		Selection.obj = Equipment[Selection.x];
	} else if(PointerX >= 200 && PointerX < 216 && PointerY >= 64 && PointerY < 80) {
		Selection.x = SLOT_FEET;
		Selection.obj = Equipment[SLOT_FEET];
	} else {
		Selection.obj = -1;
		return 0;
	}
	if(Selection.obj == 0 || Selection.obj == 1)
		Selection.obj = -1;
	return 1;
}

/*slot where objNum is equipped, or -1*/
/*C_155D_13F4*/STAT_FindEquipSlot(int16 objNum) {
	int16 i;

	for(i = 0; i < 8; i++) {
		if(Equipment[i] == objNum)
			return i;
	}

	return -1;
}

/*uncalled?*/
/*slot where an object of type objTyp is equipped, or -1*/
/*__155D_141B*/STAT_FindEquipSlotByType(int16 objTyp) {
	int16 i;

	for(i = 0; i < 8; i++) {
		if(GetType(Equipment[i]) == objTyp)
			return i;
	}

	return -1;
}

/*"Ready": equip Selection.obj on the displayed party member
  (equipment weight limit: strength, in stones)*/
/*C_155D_144B*/STAT_Ready() {
	int16 slot, owner;
	int16 objTyp, strength, npcNum;

	npcNum = Party[StatusMember];
	objTyp = GetType(Selection.obj);
	strength = STREN[npcNum];
	CON_printf(/*08C4*/"Ready-");
	if(objTyp == OBJ_TORCH)
		CON_printf(/*08CB*/"torch\n");
	else
		CON_printf(/*0867*/"%s\n", GetObjectString(Selection.obj));
	if((slot = STAT_GetEquipSlot(Selection.obj)) == -1) {
		CON_printf(/*08D2*/"\nCan't be readied!\n");
	} else if(TypeWeight[objTyp] + WeightEquip > strength * 10) {
		CON_printf(/*08E6*/"\nToo heavy!\n");
	} else {
		if(slot == SLOT_2HND) {
			if(Equipment[SLOT_RHND]) {
				slot = SLOT_RHND;
			} else if(Equipment[SLOT_LHND]) {
				slot = SLOT_LHND;
			} else {
				slot = SLOT_RHND;
				Equipment[SLOT_LHND] = 1;
				Party_2Handed[StatusMember] = 1;
			}
		} else if(slot == SLOT_RHND && Equipment[SLOT_RHND] && Equipment[SLOT_LHND] == 0) {
			slot = SLOT_LHND;
		} else if(slot == SLOT_LHND && Equipment[SLOT_LHND] && Equipment[SLOT_RHND] == 0) {
			slot = SLOT_RHND;
		} else if(slot == SLOT_RING) {
			if(Equipment[SLOT_RFNG])
				slot = SLOT_LFNG;
			else
				slot = SLOT_RFNG;
		}
		if(Equipment[slot]) {
			CON_printf(/*08F3*/"\nNo place to put!\n");
		} else {
			/*the NPC carrying the object*/
			owner = Selection.obj;
			do
				owner = GetAssoc(owner);
			while(owner >= 0x100);
			if(objTyp == OBJ_TORCH) {
				/*take one torch from the stack*/
				if(SubQuan(Selection.obj, 1) == 0)
					DeleteObj(Selection.obj);
				Equipment[slot] = AddInvObj(OBJ_TORCH, EQUIP, 1, owner);
			} else {
				Equipment[slot] = InsertObj(Selection.obj, owner, EQUIP);
				InvenSize --;
			}
			STAT_RefreshInventory(Party[StatusMember]);
			if(GetType(Selection.obj) == OBJ_INVISIBILITY_RING) {
				SetInvisible(npcNum);
				MagicalHalo(npcNum, 1);
			} else if(GetType(Selection.obj) == OBJ_STORM_CLOAK) {
				SpellFx[13] = 20;
				MagicalHalo(npcNum, 1);
			}
		}
	}
	CON_printf(/*0906*/"\n%s:\n%>", Names[Active]);
}

/*can equipped objNum be removed? returns 1 if not (amulet of
  submission), 2 if it was a lit torch (now burnt out and deleted),
  0 otherwise (npcNum is ignored)*/
/*C_155D_1666*/STAT_CheckUnready(int16 objNum/*bp08*/, int16 npcNum) {
	int16 objTyp;

	if(GetCoordUse(objNum) == EQUIP) {
		objTyp = GetType(objNum);
		if(objTyp == OBJ_AMULET_OF_SUBMISSION) {
			CON_printf(/*090E*/"\n%s\n", SpellMessages[12]);
			return 1;
		}
		if(objTyp == OBJ_TORCH && GetFrame(objNum) == 1) {
			DeleteObj(objNum);
			CON_printf(/*0862 + 3*/"\n");
			CON_printf(BurnOutMsg);
			return 2;
		}
	}
	return 0;
}

/*stop equiped ring/cloak effect of objNum worn by npcNum*/
/*(the return value is not used)*/
/*C_155D_16E7*/STAT_StopEquipEffect(int16 objNum, int16 npcNum/*bp06*/) {
	int16 objTyp;

	if(GetCoordUse(objNum) == EQUIP) {
		objTyp = GetType(objNum);
		if(objTyp == OBJ_INVISIBILITY_RING) {
			ClrInvisible(npcNum);
			MagicalHalo(npcNum, 1);
			return 3;
		}
		if(objTyp == OBJ_STORM_CLOAK) {
			SpellFx[13] = 0;
			MagicalHalo(npcNum, 1);
			return 3;
		}
	}
	return 0;
}

/*"Unready": put equipped Selection.obj of the displayed party
  member back in the inventory*/
/*C_155D_1738*/STAT_Unready() {
	int16 npcNum, result;
	int16 objTyp;

	result = 0;
	if(Selection.obj == 1)
		return;
	CON_printf(/*0913*/"Unready-%s\n", GetObjectString(Selection.obj));
	objTyp = GetType(Selection.obj);
	npcNum = Party[StatusMember];
	result = STAT_CheckUnready(Selection.obj, npcNum);
	if(result != 1) {
		Equipment[STAT_FindEquipSlot(Selection.obj)] = 0;
		if(result != 2) {
			STAT_StopEquipEffect(Selection.obj, npcNum);
			if(STAT_GetEquipSlot(Selection.obj) == SLOT_2HND) {
				Party_2Handed[StatusMember] = 0;
				Equipment[SLOT_LHND] = 0;
			}
			/*a torch goes back on the stack of torches if any*/
			if(objTyp == OBJ_TORCH && IsHeldByNPC(FindInvType(npcNum, OBJ_TORCH, 0))) {
				DeleteObj(Selection.obj);
				GiveObj(npcNum, OBJ_TORCH, 1);
			} else {
				SetCoordUse(Selection.obj, INVEN);
				InvenSize ++;
			}
		}
	}
	STAT_RefreshInventory(Party[StatusMember]);
	CON_printf(/*0906*/"\n%s:\n%>", Names[Active]);
}
