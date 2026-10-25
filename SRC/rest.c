/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	module REST/REPAIR

	The "Rest" command (REST_Command) and its helpers.
	Before resting the game checks that the party is in the wilderness
	(no building walls in view, not wading), that no hostile NPC is in the
	loaded area, that no stranger stands within 5 tiles, that the party is
	not in solo mode and that there is free room around the leader.
	On a ship, "rest" repairs the hull instead (REST_RepairHull).
	Otherwise a campfire is lit, the party gathers around it, a musician
	(if any) plays a tune while everybody eats one piece of food from the
	edible list (EdibleTypes), then everybody except the chosen guard falls
	unconscious for 1-9 hours.  Every 10 game minutes there is a small
	chance of a night encounter (wolves, snakes, rats or gremlins spawned
	just outside the view).  An uninterrupted rest of 4+ hours, when
	NextSleep allows it, heals the party (HealPartyMember, also used by the
	inn keepers' OP_REST conversation opcode).
*/
#include "u6.h"

#undef abs
#include <math.h>

/*edible items*/
/*2CCE*/int16 EdibleTypes[] = {
	OBJ_LOAF_OF_BREAD,OBJ_PORTION_OF_MEAT,
	OBJ_ROLLS,OBJ_CAKE,
	OBJ_CHEESE,OBJ_HAM,
	OBJ_HORSE_CHOPS,OBJ_BUNCH_OF_GRAPES,
	OBJ_JAR_OF_HONEY,OBJ_BUTTER,
	OBJ_FISH,OBJ_BUCKET_OF_MILK,
	OBJ_MEAT_RIB,OBJ_RIBS
};

/*returns 1 if objType is one of the 14 edible types of EdibleTypes*/
static /*C_3200_000E*/IsEdible(int16 objType) {
	int16 i;

	for(i = 0; i < 14; i ++) {
		if(EdibleTypes[i] == objType)
			return 1;
	}
	return 0;
}

/*look for food in the inventory of party member objNum only;
  if found, one unit of that food type is consumed (TakeObj) and 1 is returned*/
static /*C_3200_0035*/TryEatExclusive(int16 objNum) {
	int16 item, itemType;

	/*walk the objects linked after objNum as long as they are
	  (recursively) contained in objNum's inventory*/
	for(item = Link[objNum]; item >= 0 && GetCoordUse(item) != LOCXYZ && OBJ_IsInside(item, objNum); item = Link[item]) {
		itemType = GetType(item);
		if(IsEdible(itemType)) {
			TakeObj(objNum, itemType, 1);
			return 1;
		}
	}
	return 0;
}

/*party member #partyNum eats: first from his own pack,
  else from the first party member who has some food.
  returns 1 if something was eaten*/
static /*C_3200_0094*/TryEat(int16 partyNum) {
	int16 i;

	if(TryEatExclusive(Party[partyNum]))
		return 1;
	for(i = 0; i < PartySize; i ++) {
		if(TryEatExclusive(Party[i]))
			return 1;
	}
	return 0;
}

/*check wilderness:
  returns 1 (=NOT in the wilderness) if a wall tile with a wall-side bit
  (TERRAIN_WALL_W..80, i.e. a building wall) is in the 11x11 view, or if
  the party stands on a wet tile without being in a vehicle*/
/*C_3200_00D3*/static REST_NotInWilderness() {
	int16 x, y, numTile;

	for(x = MapX - 5; x <= MapX + 5; x ++) {
		for(y = MapY - 5; y <= MapY + 5; y ++) {
			numTile = GetTileAtXYZ(x, y, MapZ);
			if(IsTerrainWall(numTile) && (TerrainType[numTile] & 0xf0))
				return 1;
		}
	}
	if(IsTerrainWet(GetTileAtXYZ(MapX, MapY, MapZ)) && !IN_VEHICLE)
		return 1;
	return 0;
}

/*check foes near:
  returns 1 if a hostile (EVIL or CHAOTIC) NPC is on this level inside the
  39x39 tiles square starting at the 8-aligned corner 16 tiles up-left of
  the party (roughly the loaded 40x40 area)*/
/*C_3200_0160*/static REST_FoesNear() {
	int16 objNum, coordMask;
	int16 areaX0, areaY0;

	/*map width: 256 underground, 1024 on the surface*/
	if(MapZ)
		coordMask = 0xff;
	else
		coordMask = 0x3ff;
	areaX0 = (MapX - 0x10) & 0x3f8;
	areaY0 = (MapY - 0x10) & 0x3f8;
	for(objNum = 0; objNum < 0x100; objNum ++) {
		if(
			ObjShapeType[objNum] != 0 &&
			(GetAlignment(objNum) == EVIL || GetAlignment(objNum) == CHAOTIC) &&
			((GetX(objNum) - areaX0) & coordMask) < 39 &&
			((GetY(objNum) - areaY0) & coordMask) < 39 &&
			GetZ(objNum) == MapZ
		) return 1;
	}
	return 0;
}

/*check others near:
  returns 1 if a non party NPC (horses excepted) is within 5 tiles*/
/*C_3200_0226*/static REST_OthersNear() {
	int16 objNum;

	for(objNum = 0; objNum < 0x100; objNum ++) {
		if(
			ObjShapeType[objNum] != 0 &&
			!IsPlrControl(objNum) &&
			CLOSE_ENOUGH(5, GetX(objNum), GetY(objNum), MapX, MapY) &&
			GetZ(objNum) == MapZ &&
			GetType(objNum) != OBJ_HORSE
		) return 1;
	}
	return 0;
}

/*check room:
  returns 1 (=NOT enough room) if one of the 3x3 tiles centered on the
  party is not passable for the active party member*/
/*C_3200_02C4*/static REST_NoRoom() {
	int16 dx, dy;

	for(dx = 0; dx < 3; dx ++) {
		for(dy = 0; dy < 3; dy ++) {
			if(!CanMoveTo(Party[Active], MapX + dx - 1, MapY + dy - 1))
				return 1;
		}
	}
	return 0;
}

/*spawn 1-8 hostile monsters of a random type (wolf, snake, giant rat or
  gremlin) just outside the 11x11 view, on a random side.
  returns 1 if at least one monster could be placed*/
static /*C_3200_0309*/HasNightEncounter() {
	int16 objNum, objType, numMonsters, i, tries;
	int16 pos, incr, x, y, ret;

	ret = 0;
	switch(OSI_rand(0, 3)) {
		case 0: objType = OBJ_WOLF; break;
		case 1: objType = OBJ_SNAKE; break;
		case 2: objType = OBJ_GIANT_RAT; break;
		case 3: objType = OBJ_GREMLIN; break;
	}
	numMonsters = OSI_rand(1, 8);
	for(i = 0; i < numMonsters; i ++) {
		objNum = EGG_generate(objType, 0, 0, MapZ);
		/*up to 8 tries to find a passable spot*/
		for(tries = 0; tries < 8; tries ++) {
			pos = OSI_rand(0, 3);
			incr = OSI_rand(0, 10);
			switch(pos) {
				case 0: x = MapX - 6;        y = MapY + incr - 6; break;/*left*/
				case 1: x = MapX + incr - 6; y = MapY - 7; break;/*up*/
				case 2: x = MapX + 7;        y = MapY + incr - 6; break;/*right*/
				case 3: x = MapX + incr - 6; y = MapY + 6; break;/*down*/
			}
			if(CanMoveTo(objNum, x, y))
				break;
		}
		if(tries != 8) {
			MoveObj(objNum, x, y, MapZ);
			SetAlignment(objNum, EVIL);
			NPCMode[objNum] = AI_ASSAULT;
			ret = 1;
		} else {
			DeleteObj(objNum);
		}
	}
	return ret;
}

/*restore health of party member #partyIdx:
  heals a random amount between 1/2 and all of the missing hit points
  (capped at MaxHP) and fully restores magic points.
  verbose: print "<name> has healed."
  (also called by the OP_REST conversation opcode, i.e. inns)*/
/*C_3200_0467*/HealPartyMember(int16 partyIdx, int16 verbose) {
	int16 npc, missingHP, healAmount;

	npc = Party[partyIdx];
	missingHP = MaxHP(npc) - HitPoints[npc];
	if(missingHP) {
		if(missingHP == 1)
			missingHP = 2;
		healAmount = OSI_rand(0, missingHP / 2) + (missingHP / 2);
		if(HitPoints[npc] + healAmount < MaxHP(npc))
			HitPoints[npc] += healAmount;
		else
			HitPoints[npc] = MaxHP(npc);
		if(verbose) {
			CON_printf(Names[partyIdx]);
			CON_printf(/*2D02*/" has healed.\n");
		}
	}
	MAGIC[npc] = MaxMagic(npc);
}

/*repair hull: "rest" aboard a ship adds 5% to the hull strength
  (the ship's HitPoints, max 100) and lets 5 minutes pass*/
/*C_3200_0514*/static REST_RepairHull() {
	int16 ship, unused;

	ship = Party[Active];
	if(HitPoints[ship] + 5 >= 100)
		HitPoints[ship] = 100;
	else
		HitPoints[ship] += 5;
	PassTime(5);
	CON_printf(/*2D10*/"\nHull Strength: %d%%\n", HitPoints[ship]);
}

/*places around the campfire for party members #0..#7:
  CampOfsX/CampOfsY = x/y offset from the fire, CampFacing = direction the member
  faces (0=N 2=E 4=S 6=W), i.e. everybody looks at the fire.
  layout (member index/direction, y grows downwards):
+---+---+---+
|7/2|0/4|4/4|
+---+---+---+
|3/2|   |2/6|
+---+---+---+
|5/0|1/0|6/6|
+---+---+---+*/
/*2CEA*/char CampOfsX[] = { 0, 0, 1,-1, 1,-1, 1,-1};
/*2CF2*/char CampOfsY[] = {-1, 1, 0, 0,-1, 1, 1,-1};
/*2CFA*/char CampFacing[] = { 4, 0, 6, 2, 4, 0, 6, 2};

/*"rest" command*/
/*C_3200_055D*/REST_Command() {
	int16 i, npc;
	struct coord xyz;
	int16 ch, sleepTime, campFire;
	int16 someoneCannotMove, cannotMove, gatherTries, noMeal, guardNum;
	int16 eaterIdx, guardMode, mouseState, wakeReason, wasSelectMode;

	someoneCannotMove = 1;
	gatherTries = 20;
	guardNum = 0x100;/*>= 0x100: no guard*/
	wakeReason = 0;/*0:none, 1:guard heard something, 2:ambushed without guard*/
	CON_printf(/*2D26*/"Rest");
	wasSelectMode = (char)SelectMode;
	if(InCombat) {
		CON_printf(/*2D2B*/"-Not while in Combat!\n");
		return;
	}
	if(IN_VEHICLE && GetType(Party[Active]) != OBJ_SHIP) {
		CON_printf(/*2D42*/"-Can not be repaired!\n");
		return;
	}
	if(REST_NotInWilderness()) {
		CON_printf(/*2D59*/"-Only in the wilderness!\n");
		return;
	}
	if(REST_FoesNear()) {
		CON_printf(/*2D73*/"-Not while foes are near!\n");
		return;
	}
	if(REST_OthersNear()) {
		CON_printf(/*2D8E*/"-Not while others are near!\n");
		return;
	}
	if(SoloMember != -1) {
		CON_printf(/*2DAB*/"-Not in solo mode!\n");
		return;
	}
	if(REST_NoRoom()) {
		CON_printf(/*2DBF*/"-Not enough room!\n");
		return;
	}
	if(IN_VEHICLE) {
		/*on a ship*/
		REST_RepairHull();
		return;
	}
	for(i = 0; i < PartySize; i ++) {
		if(GetType(Party[i]) == OBJ_MOUNTED_HORSE) {
			CON_printf(/*2DD2*/"-Dismount first!\n");
			return;
		}
	}

	CON_printf(/*2DE4*/"\nHow many hours? ");
	ch = CON_getch();
	if(ch < '1' || ch > '9') {
		CON_printf(/*2DF6*/"0\n");
		return;
	}
	CON_printf(/*2DF9*/"%c\n", ch);
	sleepTime = ch - '0';

	/*choose a guard: either by party number key, or with the cursor
	  (when the command was issued in cursor selection mode)*/
	if(PartySize != 1) {
		CON_printf(/*2DFD*/"Who will guard? ");
		MouseMode = 1;
		if(wasSelectMode) {
			SelectRange = 7;
			DrawSlotCursor(0);
			SelectMode = 1;
		}
		ch = CON_getch();
		if(!wasSelectMode && ch >= '1' && ch < PartySize + '0') {
			Selection.obj = Party[ch - '1'];
			ch = CMD_TARGET;
			Selection.x = GetX(Selection.obj);
		}
		SelectMode = 0;
		DisplayTick();
		if(ch != CMD_TARGET || Selection.obj == -1 || Selection.x == -1 || Selection.obj >= 0x100 || !IsPlrControl(Selection.obj)) {
			CON_printf(/*2E0E*/"none\n");
		} else {
			CON_printf((char *)GetObjectString(Selection.obj));
			CON_printf(/*2D0E*/"\n");
			guardNum = Selection.obj;
		}
	}

	mouseState = CON_mouseOffAt(0, 0, 311, 191);

	/*light a campfire on the party's tile*/
	SetCoordXYZ(xyz, MapX, MapY, MapZ);
	campFire = AddObj(0, xyz, OBJ_CAMPFIRE, 1, 1);
	SetLocal(campFire);
	ShowObjects();

	/*gather around the fire (at most 20 steps)*/
	while(someoneCannotMove && gatherTries) {
		someoneCannotMove = 0;
		gatherTries --;
		for(i = 0; i < PartySize; i ++) {
			npc = Party[i];
			cannotMove = TryMoveTo(npc, MapX + CampOfsX[i], MapY + CampOfsY[i]);
			if(!cannotMove)
				UpdateMoveFrame(npc, CampFacing[i]);
			if(cannotMove)
				someoneCannotMove = 1;
		}
		ShowObjects();
		DisplayTick();
	}

	CON_printf(/*2E14*/"\nMealtime!\n");

	/*a bard plays a tune*/
	for(i = 0; i < PartySize; i ++) {
		npc = Party[i];
		if(GetType(npc) == OBJ_MUSICIAN_LONG_CAPE) {
			SetType(npc, OBJ_MUSICIAN_SHORT_CAPE);
			UpdateMoveFrame(npc, CampFacing[i]);
			CON_printf(Names[i]);
			CON_printf(/*2E20*/" plays a tune.\n");
			break;
		}
	}
	ShowObjects();
	/*check food during melody: member #(i/4) eats every 4th iteration;
	  the loop lasts as long as the song plays (at least one iteration).
	  noMeal = index of the first member without food*/
	i = 0;
	MUS_SongIndex = 0;
	noMeal = -1;
	for(; MUS_SongIndex || (MUS_SongIndex == 0 && i == 0); i ++) {
		if(!(i & 3)) {
			eaterIdx = i / 4;
			if(eaterIdx < PartySize) {
				CON_printf(Names[eaterIdx]);
				if(TryEat(eaterIdx)) {
					CON_printf(/*2E30*/" has food.\n");
				} else {
					CON_printf(/*2E3C*/" has no food.\n");
					if(noMeal == -1)
						noMeal = eaterIdx;
				}
			}
		}
		IdleAnimTick = 1;
		DisplayTick();
		PaletteAnimation();
		ServeMouse
	}
	if(noMeal == -1)
		noMeal = PartySize;

	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_DrawInventory();

	/*the bard stops playing*/
	for(i = 0; i < PartySize; i ++) {
		npc = Party[i];
		if(GetType(npc) == OBJ_MUSICIAN_SHORT_CAPE) {
			SetType(npc, OBJ_MUSICIAN_LONG_CAPE);
			UpdateMoveFrame(npc, CampFacing[i]);
			break;
		}
	}

	/*everybody but the guard falls asleep*/
	for(i = 0; i < PartySize; i ++) {
		if(Party[i] != guardNum) {
			SetTypeUnconscious(Party[i]);
		} else {
			CON_printf(/*2D0E*/"\n");
			CON_printf(Names[i]);
			CON_printf(/*2E4B*/" stands guard while the party rests.\n");
		}
	}
	ShowObjects();
	/*the guard stays in place (AI_GRAZE) during the night*/
	if(guardNum < 0x100) {
		guardMode = NPCMode[guardNum];
		NPCMode[guardNum] = AI_GRAZE;
	}

	/*night loop - 10 minutes per iteration*/
	for(i = 0; i < sleepTime * 6; i ++) {
		/*"guard"'s ai*/
		if(guardNum < 0x100)
			NPC_TakeTurn(guardNum);

		PassTime(10);
		ShowObjects();
		IdleAnimTick = 1;
		DisplayTick();
		PaletteAnimation();
		ServeMouse
		/*encounter check: only when (i & 5) == 0, i.e. i%8 is 0 or 2,
		  then 1 chance in 64*/
		if(
			!(i & 5)/*strange*/ &&
			OSI_rand(1, 64) == 1 &&
			HasNightEncounter()
		) {
			if(guardNum < 0x100) {
				CON_printf(/*2E71*/"\nYou hear something!\n");
				wakeReason = 1;
			} else {
				wakeReason = 2;
			}
			break;
		}
	}

	/*put out the campfire*/
	SetFrame(campFire, 0);
	ShowObjects();

	/*wake up: everybody gets his shape back, unless ambushed without
	  a guard (the party then stays asleep); movement points are reset*/
	for(i = 0; i < PartySize; i ++) {
		if(wakeReason < 2) {
			if(Party[i] != guardNum)
				ObjShapeType[Party[i]] = OrigShapeType[Party[i]];
		} else {
			SetAsleep(Party[i]);
		}
		MovePts[Party[i]] = DEXTE[Party[i]];
	}

	if(guardNum < 0x100)
		NPCMode[guardNum] = guardMode;

	/*could rest? 4+ hours, NextSleep elapsed and not interrupted:
	  heal the members before the first one without food (note: members
	  after a hungry one are not healed either), except the guard*/
	if(sleepTime >= 4 && NextSleep == 0 && wakeReason == 0) {
		for(i = 0; i < noMeal; i ++) {
			npc = Party[i];
			if(npc != guardNum)
				HealPartyMember(i, 1);
		}
		NextSleep = 12;
	}

	MapX = GetX(Party[Active]);
	MapY = GetY(Party[Active]);
	ShowObjects();
	MAP_LoadAround();
	DisplayTick();
	CON_mouseOn(mouseState);
	if(StatusDisplay == CMD_STAT_PARTY)
		StatusDirty ++;
}
