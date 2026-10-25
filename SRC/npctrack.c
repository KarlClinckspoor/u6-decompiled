/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	NPC tracker module: movement and A.I. of every actor (objects 0..0xff).

	- passability test (CanMoveTo) shared by the party, NPCs and monsters,
	  facing/walk-animation update (UpdateMoveFrame), movement-point accounting
	  (SubMov, SubTerrainMov);
	- party movement: the leader's step (PartyAdvance), followers keeping a
	  formation behind him (MoveFollowers), boarding/leaving vehicles
	  (Board/Unboard), moongates, red gates and dungeon entrances (CheckGatesAndEntrances);
	- NPC schedules: hourly schedule switch (UpdateSchedules), arrival at the
	  scheduled spot (__AtDestination), off-screen teleport (TeleportToSchedule);
	- path finding (PTH_*): a bidirectional uniform-cost search on the 40x40
	  area around the player, using a bucket queue in ScratchBuf; up to 8
	  paths are stored run-length encoded in PathData (slot = Leader[npc]);
	- the per-NPC A.I. dispatcher (NPC_TakeTurn) and the turn scheduler that
	  lets every actor act in proportion to its dexterity until it is the
	  player's turn again (RunNPCTurns).

	NPCMode values are the AI_xxx codes of ai.h: < AI_SCHEDULE are combat
	or party modes, >= AI_SCHEDULE are schedule/path modes.
*/
#include "u6.h"

/*red gate destinations, indexed by (red gate quality - 1)*/
/*red gate X*/
/*171C*/int16 RedGateX[] = {
	0x383,0x3A7,0x1B3,0x1F7,0x093,
	0x397,0x044,0x133,0x0BC,0x09F,
	0x2E3,    0,    0,    0,0x0E3,
	0x017,0x080,0x06C,0x39B,0x127,
	0x04B,0x147,0x183,0x33F,0x29B
};
/*red gate Y*/
/*174E*/int16 RedGateY[] = {
	0x1F3,0x106,0x18B,0x166,0x373,
	0x3A6,0x02D,0x160,0x02D,0x3AE,
	0x2BB,    0,    0,    0,0x083,
	0x016,0x056,0x0DD,0x36C,0x026,
	0x1FB,0x336,0x313,0x0A6,0x043
};
/*red gate Z*/
/*1780*/unsigned char RedGateZ[] = {
	    0,    0,    0,    0,    0,
		0,    5,    0,    5,    0,
	    0,    0,    0,    0,    0,
		1,    5,    5,    0,    0,
	    0,    0,    0,    0,    0
};

/*furniture an NPC looks for, by "kind" (see FindFurnitureHere and AI_SEEKOBJ):
 0 chair (sit/play), 1 bed, 2 table = a chair next to a plate (eat), 3 bell pull chain*/
/*1799*/int16 FurnitureTypes[] = {OBJ_CHAIR,OBJ_BED,OBJ_TABLE_SQUARE_CORNER_0E4,OBJ_PULL_CHAIN};
/*17A1*/int16 AllowNPCTeleport = 0;
/*set before UpdateMoveFrame when the object really stepped (multi-tile parts follow)*/
/*17A3*/int16 StepMovesParts = 0;
/*number of schedule teleports done this turn (max 3)*/
/*17A5*/static int16 TeleportCount = 0;
/*number of path searches done this turn (max 3)*/
/*17A7*/static int16 PathSearchCount = 0;
/*output of CanMoveTo: the tile holds damaging terrain*/
/*17A9*/int16 HazardFound = 0;
/*never set: if non-zero TryMoveTo would not avoid damaging tiles*/
/*17AB*/int16 IgnoreHazards = 0;
/*print the "not upon a Sacred Quest" message (only once per party step)*/
/*17AD*/unsigned char ShowQuestBarrierMsg = 0;
/*the leader's last step was blocked (read by the mouse code to avoid repeats)*/
/*17AE*/int16 AdvanceBlocked = 0;
/*visible area must be recomputed (see OtherAnimations/FlushViewIfDirty)*/
/*17B0*/int16 VisibilityDirty = 0;
/*CanMoveTo: party members may walk through each other*/
/*17B2*/int16 PassThroughParty = 1;
/*CanMoveTo: refuse damaging terrain*/
/*17B4*/int16 AvoidHazards = 0;
/*"Shamino hears/senses enemies" message already given*/
/*17B6*/int16 EnemyWarningGiven = 0;

/*follower formation: position k is FormationSide[k] tiles sideways and
 FormationBack[k] tiles behind the leader (relative to his facing)*/
/*
 -3  -2  -1   0   1   2   3
+---+---+---+---+---+---+---+
|   |   |   | 0 |   |   |   | 0
+---+---+---+---+---+---+---+
|   |   | 1 |10 | 2 |   |   | 1
+---+---+---+---+---+---+---+
|   | 4 |   | 3 |   | 5 |   | 2
+---+---+---+---+---+---+---+
| 8 |   | 6 |   | 7 |   | 9 | 3
+---+---+---+---+---+---+---+
*/
/*17B8*/char FormationSide[] = { 0,-1, 1, 0,-2, 2,-1, 1,-3, 3, 0};
/*17C3*/char FormationBack[] = { 0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 1};

/*can actor objNum step onto (x, y, MapZ)?
 Checks the base tile terrain and every object on the destination, taking
 into account how the creature moves (walks, flies, swims, ethereal, boats).
 Returns 1 if passable. Side effect: HazardFound = 1 if the tile is damaging
 (fire, swamp...); with AvoidHazards set such tiles are refused.*/
/*C_1E0F_000F*/CanMoveTo(int16 objNum, int16 x, int16 y) {
	int16 otherObj, otherType;
	int16 terrainFlags, tileTerrain;
	int16 objTyp, retVal, flies, swims, walks, ethereal;
	int16 objCls, otherTile, keepFind, skiffOrRaft, leaderObj, is2x2;

	retVal = 0;
	HazardFound = 0;
	is2x2 = ((TileFlag[TILE_FRAME(objNum)] & (TILE_DOUBLE_WIDTH|TILE_DOUBLE_HEIGHT)) == (TILE_DOUBLE_WIDTH|TILE_DOUBLE_HEIGHT));
	leaderObj = Party[Active];
	objTyp = GetType(objNum);
	objCls = GetMonsterClass(objTyp);
	flies = IsMonsterFlying(objCls);
	skiffOrRaft = (objTyp == OBJ_SKIFF || objTyp == OBJ_RAFT);
	swims = (IsMonsterSwimming(objCls) || skiffOrRaft);
	ethereal = IsMonsterEthereal(objCls);
	walks = ((!swims && !flies) || IsMonsterAlligator(objCls));
	terrainFlags = TerrainType[GetTileAtXYZ(x, y, MapZ)];
	/*-- base tile --*/
	if(objTyp == OBJ_ROT_WORMS) {
		/*rot worms only live on damaging (swamp) tiles*/
		retVal = terrainFlags & TERRAIN_DAMAGING;
	} else {
		/*walkers: not impassable; swimmers: deep water (impassable+wet);
		boats: any water; fliers: anything but walls; ethereal: anything*/
		if(
			(walks && !(terrainFlags & TERRAIN_IMPASSABLE)) ||
			(swims && (terrainFlags & TERRAIN_IMPASSABLE) && (terrainFlags & TERRAIN_WET)) ||
			(skiffOrRaft && (terrainFlags & TERRAIN_WET)) ||
			(flies && !(terrainFlags & TERRAIN_WALL)) ||
			ethereal
		) retVal = 1;
		if(terrainFlags & TERRAIN_DAMAGING) {
			HazardFound = 1;
			if(AvoidHazards)
				return 0;
		}
	}
/*0198*/
	/*-- objects on the destination tile --*/
	keepFind = 1;
	for(otherObj = FindLoc(x, y, MapZ); otherObj >= 0; otherObj = NextLoc()) {
		if(otherObj == objNum)
			continue;
		otherType = GetType(otherObj);
		/*type 0 object = barrier that only lets the party pass
		while on the Sacred Quest (VarInt['Q'-0x37] = conversation var Q)*/
		if(otherType == OBJ_NOTHING && VarInt['Q' - 0x37] == 0) {
			if(ShowQuestBarrierMsg) {
				CON_printf(NoSacredQuestMsg);
				ShowQuestBarrierMsg = 0;
				PromptFlag = 1;
			}
			keepFind =
			retVal = 0;
			break;
		}
		/*multi-tile creature/ship: ignore its own body part (objects >= 0x100
		of the same type) lying on the side the edge flags N/E/S/W of
		its own tile point to*/
		if(
			(objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_SHIP) &&
			otherObj >= 0x100 && otherType == objTyp
		) {
			tileTerrain = TerrainType[TILE_FRAME(objNum)];
			if(
				(tileTerrain & TERRAIN_WALL_N) && GetY(objNum) > y ||
				(tileTerrain & TERRAIN_WALL_E) && GetX(objNum) < x ||
				(tileTerrain & TERRAIN_WALL_S) && GetY(objNum) < y ||
				(tileTerrain & TERRAIN_WALL_W) && GetX(objNum) > x
			) continue;
		}

		/*Sherry the mouse squeezes through mouse holes and bars*/
		if(
			objTyp == OBJ_MOUSE &&
			(otherType == OBJ_MOUSE_HOLE || otherType == OBJ_BARS || otherType == OBJ_PORTCULLIS)
		) {
			retVal = 1;
			continue;
		}

		/*tile of the found object (LocPartOffset = part of a multi-tile object hit by FindLoc)*/
		otherTile = TILE_FRAME(otherObj) - LocPartOffset;
		/*a 2x2 creature can't step onto another multi-tile object*/
		if(is2x2 && LocPartOffset == 0 && (TileFlag[otherTile] & (TILE_DOUBLE_WIDTH|TILE_DOUBLE_HEIGHT)))
			return 0;
		/*bridge-like object: walkable whatever the base tile*/
		if(walks && IsTileBr(otherTile)) {
			retVal = 1;
			if(!IsTileIg(otherTile))
				break;
		}

		tileTerrain = TerrainType[otherTile];
		if(tileTerrain & TERRAIN_DAMAGING) {
			HazardFound = 1;
			if(AvoidHazards)
				return 0;
		}

		/*silver serpent: may cross its body, except the segment right behind
		its head (segment quality = head quality + 1)*/
		if(objTyp == OBJ_SILVER_SERPENT && GetType(otherObj) == OBJ_SILVER_SERPENT) {
			if(GetQual(otherObj) == GetQual(objNum) + 1) {
				keepFind =
				retVal = 0;
			}
			goto c_04ed;
		}
		/*same movement rules as for the base tile; c_04ed = "this object
		does not block by its terrain"*/
		if(walks) {
			if(!(tileTerrain & TERRAIN_IMPASSABLE))
				goto c_04ed;
		}
		if(swims) {
			if((tileTerrain & TERRAIN_IMPASSABLE) && (tileTerrain & TERRAIN_WET))
				goto c_04ed;
			if(IsTileIg(otherTile))
				goto c_04ed;
		}
		if(skiffOrRaft) {
			if(tileTerrain & TERRAIN_WET)
				goto c_04ed;
			if(IsTileIg(otherTile))
				goto c_04ed;
		}
		if(flies) {
			if(!(tileTerrain & TERRAIN_WALL)) {
				if(otherObj >= 0x100)
					goto c_04ed;
				if(!IsMonsterFlying(GetMonsterClass(otherType)))
					goto c_04ed;
			}
		}
		if(ethereal) {
			if(otherObj >= 0x100)
				goto c_04ed;
		}

		if(IsPlrControl(objNum)) {
			/*party members may swap with an able party member (not the leader)*/
			if(
				otherObj != leaderObj &&
				PassThroughParty &&
				otherObj < 0x100 &&
				IsPlrControl(otherObj) &&
				!(IsIncapacitated(otherObj) || IsDraggedUnder(otherObj))
			) continue;
		} else if(objNum < 0x100) {
			/*NPCs of door-opening classes go through unlocked doors (frame < 8)*/
			if(
				(otherType >= OBJ_OAKEN_DOOR && otherType <= OBJ_STEEL_DOOR && GetFrame(otherObj) < 8) ||
				otherType == OBJ_PASSTHROUGH_116 || otherType == OBJ_PASSTHROUGH_118
			) {
				if(IsMonsterHuman(objCls))
					goto c_04ed;
			}
		}
/*c_04e5:*/
		keepFind =
		retVal = 0;

c_04ed:
		/*another actor blocks, unless it is a small critter (and not of our kind)*/
		if(otherObj < 0x100 && otherObj) {
			if(
				(otherType != OBJ_BIRD && otherType != OBJ_MOUSE && otherType != OBJ_INSECTS && otherType != OBJ_RABBIT && otherType != OBJ_CORPSER) ||
				otherType == objTyp
			) {
				keepFind =
				retVal = 0;
				break;
			}
		}
		/*big fliers can't pass over wall objects*/
		if(tileTerrain & TERRAIN_WALL) {
			if(objTyp == OBJ_WINGED_GARGOYLE || objTyp == OBJ_CYCLOPS || objTyp == OBJ_DRAGON || objTyp == OBJ_HYDRA_BODY) {
				keepFind =
				retVal = 0;
			}
		}

		if(!keepFind)
			break;
	}/*for(... */

	return retVal;
}

/*is the object outside of the 40x40 loaded area (or on another level)?*/
static /*C_1E0F_056E*/IsOutOfArea(int16 objNum) {
	return !(
		GetZ(objNum) == MapZ &&
		((GetX(objNum) - AreaX) & MapCoordMask) <= (AREA_W-1) &&
		((GetY(objNum) - AreaY) & MapCoordMask) <= (AREA_H-1)
	);
}

/*is (x, y, MapZ) blocked for a multi-tile animal turning diagonally?
 1 if the base tile or any object other than a party member is impassable*/
/*C_1E0F_05D8*/IsBlockedAt(int16 x, int16 y) {
	int16 obj;

	if(IsTerrainImpass(GetTileAtXYZ(x, y, MapZ)))
		return 1;
	for(obj = FindLoc(x, y, MapZ); obj >= 0; obj = NextLoc()) {
		if(obj >= 0x100 || !IsPlrControl(obj)) {
			if(IsTerrainImpass(TILE_FRAME(obj) - LocPartOffset))
				return 1;
		}
	}
	return 0;
}

/*dragon: frame of each of the 4 extra body parts, 4 entries per facing
 (index = part + facing * 4, facing N/E/S/W)*/
/*17CE*/unsigned char DragonPartFrames[] = {
	0x08,0x20,0x10,0x18,
	0x1A,0x0A,0x22,0x12,
	0x14,0x1C,0x0C,0x24,
	0x26,0x16,0x1E,0x0E
};
/*ship: edge flag (N/E/S/W bits of TerrainType) the bow part must carry
 for a ship facing N/E/S/W, i.e. the direction pointing back to the hull*/
/*17DE*/unsigned char ShipBowEdgeFlag[] = {TERRAIN_WALL_S,TERRAIN_WALL_W,TERRAIN_WALL_N,TERRAIN_WALL_E};

/*turn the 4-way facing "frame" (0 N,1 E,2 S,3 W) toward the 8-way "dir":
 straight dirs set it directly; for a diagonal the facing is kept if it is
 one of the two components, otherwise it is reversed*/
#define FACE_TOWARD(dir,frame) {                        \
	if(!((dir) & 1))                                \
		(frame) = (dir) >> 1;                       \
	else if(((((dir) >> 1) - (frame) + 1) & 3) > 1) \
		(frame) = ((frame) + 2) & 3;                \
}

/*make objNum face the 8-way direction dir and update its shape frame.
 For walking humanoids/animals it also toggles the step animation (using the
 NPCFlag "walking" bit to alternate left/right foot). When StepMovesParts is set
 (the object really moved) multi-tile creatures and ships drag their other
 parts along. Also handles seated NPCs, the bird, cyclops (shakes screen) and
 the dragon. Always clears StepMovesParts.*/
/*C_1E0F_0664*/UpdateMoveFrame(int16 objNum, int16 dir) {
	int16 objFrm, dirFram;
	int16 partIdx, objTyp, tmpObj, isOnChair, seatType;
	int16 cur_x, cur_y;

	isOnChair = 0;
	/*sitting on a chair or a throne (throne part 2 = seat)?*/
	if(!IsOutOfArea(objNum)) {
		for(tmpObj = FindLoc(GetX(objNum), GetY(objNum), GetZ(objNum)); tmpObj >= 0; tmpObj = NextLoc()) {
			seatType = GetType(tmpObj);
			if(seatType == OBJ_CHAIR || (seatType == OBJ_THRONE && GetFrame(tmpObj) - LocPartOffset == 2)) {
				isOnChair = 1;
				break;
			}
		}
	}
	objTyp = GetType(objNum);
	objFrm = GetFrame(objNum);
	if(
		(objTyp >= OBJ_FIGHTER && objTyp <= OBJ_WOMAN) ||
		(objTyp >= OBJ_LORD_BRITISH && objTyp <= OBJ_AVATAR)
	) {
		/*humanoids: 4 frames per facing (1 standing, 0/2 steps, 3 sitting)*/
		if(isOnChair) {
			/*sit facing the chair's direction (throne: south)*/
			objFrm = 3;
			if(seatType == OBJ_CHAIR)
				dirFram = GetFrame(tmpObj);
			else
				dirFram = 2;
		} else {
			dirFram = objFrm >> 2;
			FACE_TOWARD(dir, dirFram);
			switch(objFrm & 3) {
				case 0: ClrWalking(objNum); objFrm = 1; break;
				case 1: objFrm = IsWalking(objNum)?0:2; break;
				case 2: SetWalking(objNum); objFrm = 1; break;
				default: objFrm = 1;
			}
		}
		objFrm = objFrm + (dirFram << 2);
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_WINGED_GARGOYLE) {
		/*12 frames per facing, the body frames are 3/7/11*/
		dirFram = objFrm / 12;
		FACE_TOWARD(dir, dirFram);
		switch(objFrm % 12) {
			case 3/*0*4+3*/: ClrWalking(objNum); objFrm = 7; break;
			case 7/*1*4+3*/: objFrm = IsWalking(objNum)?3:11; break;
			case 11/*2*4+3*/: SetWalking(objNum); objFrm = 7; break;
			default: objFrm = 7;
		}
		objFrm = objFrm + dirFram * 12;
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_BIRD) {
		objFrm = OSI_rand(0, 2);
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_GARGOYLE) {
		/*3 frames per facing*/
		dirFram = objFrm / 3;
		FACE_TOWARD(dir, dirFram);
		switch(objFrm % 3) {
			case 0: ClrWalking(objNum); objFrm = 1; break;
			case 1: objFrm = IsWalking(objNum)?0:2; break;
			case 2: SetWalking(objNum); objFrm = 1; break;
			default: objFrm =1;
		}
		objFrm = objFrm + dirFram * 3;
		SetFrame(objNum, objFrm);
	} else if(
		objTyp == OBJ_SHEEP ||
		objTyp == OBJ_DOG ||
		objTyp == OBJ_DEER ||
		objTyp == OBJ_GIANT_RAT ||
		objTyp == OBJ_SNAKE ||
		objTyp == OBJ_GIANT_SPIDER ||
		objTyp >= OBJ_GIANT_SCORPION ||
		objTyp == OBJ_WOLF ||
		objTyp == OBJ_SEA_SERPENT ||
		(objTyp >= OBJ_DAEMON && objTyp <= OBJ_MONGBAT) ||
		objTyp == OBJ_MUSICIAN_SHORT_CAPE
	) {
		/*2-frame animals (and 2-tile creatures >= OBJ_GIANT_SCORPION)*/
		dirFram = objFrm >> 1;
		if(objTyp < OBJ_GIANT_SCORPION || StepMovesParts) {
			/*diagonal: pick the straight facing whose tail square is free*/
			if(dir & 1) {
				cur_x = GetX(objNum);
				cur_y = GetY(objNum);
				if(IsBlockedAt(cur_x, cur_y - DirIncrY[dir]))
					dir = (dir & 4) + 2;
				else if(IsBlockedAt(cur_x - DirIncrX[dir], cur_y))
					dir = (dir + 1) & 4;
			}
			FACE_TOWARD(dir, dirFram);
			if(!(objFrm & 1))
				objFrm = 1;
			else
				objFrm = 0;
		}
		objFrm = (objFrm & 1) + (dirFram << 1);
		/*2-tile creature: put the tail part right behind the head*/
		if(objTyp >= OBJ_GIANT_SCORPION && StepMovesParts && (tmpObj = OBJ_getHead(objNum)) >= 0) {
			if(GetFrame(tmpObj) & 8)
				SetFrame(tmpObj, (objFrm & 7) + 8);
			else
				SetFrame(tmpObj, (objFrm & 7) ^ 4);
			MoveObj(tmpObj, GetX(objNum) - DirIncrX[dirFram << 1], GetY(objNum) - DirIncrY[dirFram << 1], MapZ);
		}
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_SHIP && StepMovesParts) {
		/*ship (3 tiles): middle frames 9/11/13/15, bow = middle - 8,
		stern = middle + 8; find bow and stern around the old facing and
		move them around the new one*/
		dirFram = (objFrm & 6) >> 1;
		FACE_TOWARD(dir, dirFram);
		objFrm = 9 + (dirFram << 1);
		cur_x = GetX(objNum);
		cur_y = GetY(objNum);
		dir = GetFrame(objNum) & 6;
		for(tmpObj = FindLoc(cur_x + DirIncrX[dir], cur_y + DirIncrY[dir], MapZ); tmpObj >= 0; tmpObj = NextLoc()) {
			if(
				tmpObj >= 0x100 &&
				GetType(tmpObj) == OBJ_SHIP &&
				GetFrame(tmpObj) < 8 &&
				TerrainType[TILE_FRAME(tmpObj)] & ShipBowEdgeFlag[dir >> 1]
			) {
				SetFrame(tmpObj, objFrm - 8);
				MoveObj(tmpObj, cur_x + DirIncrX[dirFram << 1], cur_y + DirIncrY[dirFram << 1], MapZ);
				break;
			}
		}
		for(tmpObj = FindLoc(cur_x - DirIncrX[dir], cur_y - DirIncrY[dir], MapZ); tmpObj >= 0; tmpObj = NextLoc()) {
			if(
				tmpObj >= 0x100 &&
				GetType(tmpObj) == OBJ_SHIP &&
				GetFrame(tmpObj) > 0xf &&
				TerrainType[TILE_FRAME(tmpObj)] & ShipBowEdgeFlag[(dir >> 1) ^ 2]
			) {
				SetFrame(tmpObj, objFrm + 8);
				MoveObj(tmpObj, cur_x - DirIncrX[dirFram << 1], cur_y - DirIncrY[dirFram << 1], MapZ);
				break;
			}
		}
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_MOUSE || objTyp == OBJ_RABBIT || objTyp == OBJ_SKIFF || objTyp == OBJ_CAT) {
		dirFram = objFrm;
		FACE_TOWARD(dir, dirFram);
		objFrm = dirFram;
		SetFrame(objNum, objFrm);
	} else if(objTyp == OBJ_CYCLOPS) {
		/*8 frames per facing; its steps shake the screen*/
		dirFram = objFrm >> 3;
		FACE_TOWARD(dir, dirFram);
		if(!(objFrm & 4))
			objFrm = 7;
		else
			objFrm = 3;
		objFrm = objFrm + (dirFram << 3);
		SetFrame(objNum, objFrm);
		if(!IsOutOfArea(objNum))
			CyclopsFlag = OSI_rand(2, 4);
	} else if(objTyp == OBJ_DRAGON && objFrm < 8) {
		/*dragon head: re-orient the 4 other parts (frame < 8 = head)*/
		cur_x = GetX(objNum);
		cur_y = GetY(objNum);
		for(partIdx = 0; partIdx < 4; partIdx ++) {
			tmpObj = OBJ_FindDragonPart(cur_x, cur_y, MapZ, partIdx << 1, objFrm);
			if(tmpObj >= 0)
				SetFrame(tmpObj, DragonPartFrames[partIdx + ((dir & 6) << 1)]);
		}
		SetFrame(objNum, dir & 6);
	}
	StepMovesParts = 0;
}
#undef FACE_TOWARD


/*spend pts movement points (MovePts) for objNum: halved for a mounted
 horse, halved for the party / doubled for hostiles under SpellFx[10] /
 SpellFx[7] (these two effects are never set elsewhere); at least 1*/
/*C_1E0F_0F54*/SubMov(int16 objNum, int16 pts) {
	if(GetType(objNum) == OBJ_MOUNTED_HORSE)
		pts >>= 1;
	if(SpellFx[10] && IsPlrControl(objNum))
		pts >>= 1;
	else if(SpellFx[7] && Is_ATKPLR(objNum))
		pts <<= 1;
	if(pts < 1)
		pts = 1;
	MovePts[objNum] -= pts;
}

/*actor held underground by a corpser tries to break free (STR vs 1d30).
 Returns 1 if it is still held (it is hurt and loses its turn), else 0.*/
/*C_1E0F_0FA9*/static StruggleDraggedUnder(int16 npc) {
	if(IsDraggedUnder(npc)) {
		if(NPCMode[npc] == AI_COMMAND)
			CON_printf(ArghMsg, (char *)GetObjectString(npc));
		if(OSI_rand(1, 30) < GetStr(npc)) {
			MUS_PlaySfx(10, 0);
			CON_printf((char *)GetObjectString(npc));
			CON_printf(RegurgitatedMsg);
			ClrDraggedUnder(npc);
			VisibilityDirty = 1;
		} else {
			LoseHP(npc, OSI_rand(1, 15));
			MovePts[npc] = 0;
			return 1;
		}
	}
	return 0;
}

/*formation helper: compare (x, y) with the first "count" members already
 placed in FormationObjs[] (leader first; a mounted horse counts for 2 tiles).
 Returns 0 if (x, y) is occupied by one of them (the mouse may share), else
 1 if (x, y) touches one of them (8-neighbourhood), else 0.*/
/*C_1E0F_1056*/static IsNextToFormation(int16 count, int16 x, int16 y) {
	int16 delta_x, delta_y;
	int16 k, placedObj, placedType, isAdjacent, isMouse;

	isAdjacent = 0;
	isMouse = (GetType(FormationObjs[count]) == OBJ_MOUSE);
	for(k = 0; k < count; k ++) {
		placedObj = FormationObjs[k];
		placedType = GetType(placedObj);
		delta_x = GetX(placedObj) - x;
		delta_y = GetY(placedObj) - y;
		if(delta_x == 0 && delta_y == 0 && placedType != OBJ_MOUSE && !isMouse)
			return 0;
		if(delta_x > -2 && delta_x < 2 && delta_y > -2 && delta_y < 2)
			isAdjacent = 1;
		if(GetType(placedObj) != OBJ_MOUNTED_HORSE)
			continue;
		delta_x -= DirIncrX[GetFrame(placedObj) & 6];
		delta_y -= DirIncrY[GetFrame(placedObj) & 6];
		if(delta_x == 0 && delta_y == 0 && placedType != OBJ_MOUSE && !isMouse)
			return 0;
		if(delta_x > -2 && delta_x < 2 && delta_y > -2 && delta_y < 2)
			isAdjacent = 1;
	}
	return isAdjacent;
}

/*move the AI_FOLLOW party members one step after the leader moved.
 Member k aims for formation slot k (FormationSide/FormationBack, relative to the leader's
 facing). Pass 0 only moves members that lost contact with the chain;
 pass 1 also moves those behind their slot (or, with regroup set - on
 "pass" or after entering a map - every member not exactly on its slot).
 Each move picks among the 8 neighbours the one that keeps contact and is
 closest to the slot, avoiding damaging tiles. Followers pay 5 points.*/
/*C_1E0F_1193*/MoveFollowers(int16 leader, int16 regroup) {
	register int16 follower, follow_pos;
	int16 partyIdx, try_dir, pass;
	int16 follower_x, follower_y;
	int16 try_x, try_y, eager, new_dir;
	int16 max_eager, savedMovePts, facing_dir, leaderDir, x, y, target_x, target_y;
	int16 leaderType, wasNextTo;

	if(IN_VEHICLE)
		return;
	FormationObjs[0] = leader;
	x = GetX(leader);
	y = GetY(leader);
	follow_pos = 1;
	leaderDir = GetDirection(leader);
	leaderType = GetType(leader);
	/*the formation is oriented after the leader's facing (from his frame
	for humanoids, else his last move direction), rounded to N/E/S/W*/
	if((leaderType >= OBJ_FIGHTER && leaderType <= OBJ_AVATAR) || leaderType == OBJ_GARGOYLE)
		facing_dir = (GetFrame(leader) >> 1) & 6;
	else
		facing_dir = leaderDir & 6;
	/*two pass algorithm*/
	/*[see doc9.txt]*/
	for(pass = 0; pass < 2; pass ++) {
		follow_pos = 1;
		for(partyIdx = 0; partyIdx < PartySize; partyIdx ++) {
			follower = Party[partyIdx];
			FormationObjs[follow_pos] = follower;
			/*formation slot = leader - facing * back + perpendicular * side*/
			target_x = x
				- DirIncrX[facing_dir] * FormationBack[follow_pos]
				- DirIncrY[facing_dir] * FormationSide[follow_pos]
			;
			target_y = y
				+ DirIncrX[facing_dir] * FormationSide[follow_pos]
				- DirIncrY[facing_dir] * FormationBack[follow_pos]
			;
			if(follower == leader)
				continue;
			if(GetZ(follower) != MapZ)
				continue;
			if(NPCMode[follower] != AI_FOLLOW)
				continue;
			if(StruggleDraggedUnder(follower))
				continue;
			if(IsIncapacitated(follower) || IsDraggedUnder(follower))
				continue;
			savedMovePts = MovePts[follower];
			follower_x = GetX(follower);
			follower_y = GetY(follower);
			if(
				!IsNextToFormation(follow_pos, follower_x, follower_y)
				/*on second pass*/
				|| (pass == 1 && (
					(regroup && !(target_x == follower_x && target_y == follower_y)) ||
					(!regroup && (target_x - follower_x) * DirIncrX[facing_dir] + (target_y - follower_y) * DirIncrY[facing_dir] > 0)
				))
			) {
				wasNextTo = IsNextToFormation(follow_pos, follower_x, follower_y);
				new_dir = -1;
				max_eager = 0;
				for(try_dir = 0; try_dir < 8; try_dir ++) {
					try_x = follower_x + DirIncrX[try_dir];
					try_y = follower_y + DirIncrY[try_dir];
					/*score: 256 (128 on a damaging tile, refused if already in
					contact), +256 if the square keeps contact with the chain, 0 if
					it would break an existing contact, minus the distance to the slot*/
					if(CanMoveTo(follower, try_x, try_y)) {
						if(HazardFound) {
							if(wasNextTo)
								continue;
							eager = 128;
						} else {
							eager = 256;
						}
						if(IsNextToFormation(follow_pos, try_x, try_y))
							eager += 256;
						else if(wasNextTo)
							eager = 0;
						eager -= __abs__(try_x - target_x);
						eager -= __abs__(try_y - target_y);
						if(eager > max_eager) {
							max_eager = eager;
							new_dir = try_dir;
						}
					}
				}
				if(new_dir >= 0) {
					MoveObj(follower, follower_x + DirIncrX[new_dir], follower_y + DirIncrY[new_dir], MapZ);
					StepMovesParts = 1;
					UpdateMoveFrame(follower, new_dir);
				}
			}
			/*following always costs 5 points whatever the terrain*/
			MovePts[follower] = savedMovePts;
			SubMov(Party[partyIdx], 5);
			follow_pos ++;
		}
	}
}

/*board the vehicle map object objNum (ship, skiff, raft, balloon, horse...).
 Actor 0 becomes the vehicle: it gets the vehicle's shape, its hit points
 (= object quantity) and the Avatar's name; the party members (and a skiff
 or raft we were already in) are put into its inventory and
 Active = PartySize (see IN_VEHICLE).*/
/*C_1E0F_14BE*/Board(int16 objNum) {
	int16 i;
	int16 new_objNum, edgeFlags, partDir, carriedBoat;

	carriedBoat = -1;
	/*clicked on the bow or stern of a ship: its edge flag points to the
	middle part, which is the real ship object*/
	if(GetType(objNum) == OBJ_SHIP) {
		edgeFlags = TerrainType[TILE_FRAME(objNum)] & 0xf0;
		partDir = -1;
		if(edgeFlags == TERRAIN_WALL_N)
			partDir = 0;
		else if(edgeFlags == TERRAIN_WALL_E)
			partDir = 2;
		else if(edgeFlags == TERRAIN_WALL_S)
			partDir = 4;
		else if(edgeFlags == TERRAIN_WALL_W)
			partDir = 6;
		if(partDir >= 0) {
			for(objNum = FindLoc(GetX(objNum) + DirIncrX[partDir], GetY(objNum) + DirIncrY[partDir], MapZ); objNum >= 0; objNum = NextLoc()) {
				if(GetType(objNum) == OBJ_SHIP)
					break;
			}
		}
	}
	if(objNum < 0)
		return;
	COMBAT_breakOff();
	if(IN_VEHICLE) {
		carriedBoat = Unboard(1);
		if(GetType(carriedBoat) != OBJ_SKIFF && GetType(carriedBoat) != OBJ_RAFT)
			carriedBoat = -1;
	}
	MapX = GetX(objNum);
	MapY = GetY(objNum);
	PartyEnter(2);
	new_objNum = 0;
	CloneObject(Party[0], new_objNum);
	ClrLocal(new_objNum);
	ClrInvisible(new_objNum);
	MovePts[new_objNum] = DEXTE[new_objNum];
	MoveObj(new_objNum, MapX, MapY, MapZ);
	ObjShapeType[new_objNum] = ObjShapeType[objNum];
	HitPoints[new_objNum] = GetQuan(objNum);
	SetAlignment(new_objNum, GOOD);
	SetPlrControl(new_objNum);
	NPCMode[new_objNum] = AI_COMMAND;
	ObjShapeType[objNum] = TypeFrame(OBJ_GOLD, 0);/*why? (maybe so that DeleteObj doesn't handle it as a multi-tile object)*/
	DeleteObj(objNum);
	for(i = 0; i < PartySize; i ++)
		InsertObj(Party[i], new_objNum, INVEN);
	if(carriedBoat >= 0)
		InsertObj(carriedBoat, new_objNum, INVEN);
	Active = PartySize;
	Party[Active] = new_objNum;
	for(i = 0; i < 14; i ++)
		Names[Active][i] = Names[0][i];
	ShowObjects();
	VisibilityDirty = 1;
	MUS_PlayAreaSong();
}

/*leave the vehicle (actor 0 = Party[PartySize]): put back a map object of
 the vehicle's type with its hit points as quantity, park actor 0 in limbo
 (z = 14) and let the party out. Unless onLand, a skiff or raft carried
 in a ship is dropped and boarded. Returns the new map object.*/
/*C_1E0F_1745*/Unboard(int16 onLand) {
	int16 boat, vehicle;
	int16 mapObj;

	boat = -1;
	vehicle = Party[PartySize];
	mapObj = AddMapObj(GetType(vehicle), 1, HitPoints[vehicle], MapX, MapY, MapZ);
	if(!onLand) {
		/*look for skiff or raft*/
		for(boat = FindInv(vehicle); boat >= 0; boat = NextInv()) {
			if(GetType(boat) == OBJ_SKIFF || GetType(boat) == OBJ_RAFT) {
				MoveObj(boat, MapX, MapY, MapZ);
				break;
			}
		}
	}
	ClrLocal(mapObj);/*note: done before the mapObj >= 0 test*/
	if(mapObj >= 0)
		ObjShapeType[mapObj] = ObjShapeType[vehicle];
	ObjShapeType[vehicle] = TypeFrame(OBJ_GOLD, 0);
	NPCMode[vehicle] = AI_MOTIONLESS;
	MoveObj(vehicle, MapX, MapY, 14);
	Active = 0;
	PartyExit(MapX, MapY, MapZ, 2);
	if(boat >= 0)
		Board(boat);
	MUS_PlayAreaSong();
	return mapObj;
}

/*names used by Shamino's comment on dungeon/cave entrances,
 indexed by (entrance quality - 1): 1-7 dungeons, 8 Great Stygian Abyss,
 9-11 the shrines of the 3 principles, 12-20 other caves*/
/*17E2*/char *DungeonNames[] = {
	/*1872*/"Deceit",
	/*1879*/"Despise",
	/*1881*/"Destard",
	/*1889*/"Wrong",
	/*188F*/"Covetous",
	/*1898*/"Shame",
	/*189E*/"Hythloth",

	/*18A7*/"GSA",

	/*18AB*/"Control",
	/*18B3*/"Passion",
	/*18BB*/"Diligence",

	/*18C5*/"Tomb of Kings",
	/*18D3*/"Ant Mound",
	/*18DD*/"Swamp Cave",
	/*18E8*/"Spider Cave",
	/*18F4*/"Cyclops Cave",
	/*1901*/"Heftimus Cave",
	/*190F*/"Heroes' Hole",
	/*191C*/"Pirate Cave",
	/*1928*/"Buccaneer's Cave"
};

/*after the party leader moved: look at what he stands on and take
 moongates, red gates, mine shafts and holes (entering a dungeon). All of
 these are refused while in solo mode (SoloMember != -1).*/
/*C_1E0F_184D*/CheckGatesAndEntrances() {
	int16 objNum, gateIdx;
	int16 moonDiff, objType, needPartyMode;

	needPartyMode = 0;
	if(InCombat)
		return;
	if(!IN_VEHICLE) {
		for(objNum = FindLoc(MapX, MapY, MapZ); objNum >= 0; objNum = NextLoc()) {
			if(Party[Active] == objNum)
				continue;

			objType = GetType(objNum);
			/*moongate*/
			if(objType == OBJ_MOONGATE && LocPartOffset == 0) {
				if(SoloMember != -1) {
					needPartyMode = 1;
					break;
				}
				/*a moongate taken at midnight leads to the shrine of Spirituality*/
				if(Time_H == 0 && Time_M < 10) {
					/*shrine of spirituality*/
					PartyTeleport(0x018, 0x01d, 1, 1);
					MUS_PlaySong(8);
					PassTime(10);
					break;
				}
				/*the moon whose position (TrammelPos/FeluccaPos) is closest to 7 decides
				the destination (TrammelPhase/FeluccaPhase); on a tie it depends on the
				half hour*/
				moonDiff = __abs__(7 - TrammelPos) - __abs__(7 - FeluccaPos);
				if(moonDiff < 0) {
c_1922:
					GateTravel(TrammelPhase);
					break;
				}
				if(moonDiff <= 0 && (Time_M < 30) == (TrammelPos < FeluccaPos)) {
					goto c_1922;
				}
				{
					GateTravel(FeluccaPhase);
					break;
				}
			}
			/*red gate*/
			if(objType == OBJ_RED_GATE && LocPartOffset == 0) {
				if(SoloMember != -1) {
					needPartyMode = 1;
					break;
				}
				/*quality = 1 + index in RedGateX/RedGateY/RedGateZ; 0 = stay here*/
				gateIdx = GetQual(objNum);
				if(gateIdx) {
					gateIdx --;
					PartyTeleport(RedGateX[gateIdx], RedGateY[gateIdx], RedGateZ[gateIdx], 1);
					MUS_PlayAreaSong();
					break;
				}
				PartyTeleport(MapX, MapY, MapZ, 1);
				break;
			}
			/*dungeon/cave*/
			if(objType == OBJ_MINE_SHAFT || objType == OBJ_HOLE) {
				if(SHAMINO_COMMENT && GetQual(objNum) != 0 && GetQual(objNum) < 21) {
					CON_printf(ShaminoThisIsMsg);
					if(GetQual(objNum) < 8)
						CON_printf(/*1939*/"dungeon ");
					else if(GetQual(objNum) > 8 && GetQual(objNum) < 12)
						CON_printf(/*1942*/"shrine of ");
					CON_printf(DungeonNames[GetQual(objNum) - 1]);
					CON_printf(/*194D*/".\"\n");
					PromptFlag = 1;
				}
				/* */
				if(SoloMember != -1) {
					needPartyMode = 1;
					break;
				}
				/*change level*/
				PartyChangeLevel(objNum);
				break;
			}
		}
	}
	if(needPartyMode) {
		CON_printf(MustBePartyModeMsg);
		PromptFlag = 1;
	}
}

/*ship movement cost, index = (move dir - WindDir) & 7
 (0 = straight into the wind, 2/6 = wind from the side = fastest)*/
/*180A*/unsigned char ShipWindCost[] = {10,5,3,4, 5,4,3,5};
/*skiff movement cost, index = (move dir - current dir) & 7
 (0 = with the current, 4 = against it)*/
/*1812*/unsigned char SkiffCurrentCost[] = { 3,4,5,7,10,7,5,4};

/*raft drift: RaftDriftDir[current dir][dir the wind blows to] = resulting dir*/
/*181A*/unsigned char RaftDriftDir[][8] = {
	{0,0,1,1,0,7,7,0},
	{1,1,1,2,2,1,0,0},
	{1,2,2,2,3,3,2,1},
	{2,2,3,3,3,4,4,3},
	{4,3,3,4,4,4,5,5},
	{6,5,4,4,5,5,5,6},
	{7,7,6,5,5,6,6,6},
	{7,0,0,7,6,6,7,7}
};

/*[advance] the party leader (or vehicle) steps in direction dir.
 Handles drunkenness, raft/balloon drifting with wind and current, sliding
 along obstacles when moving diagonally with the mouse, swapping places with
 a party member standing in the way, movement costs (wind for ships,
 current for skiffs, terrain otherwise), then moves the followers and checks
 gates/entrances. Sets AdvanceBlocked when the step was blocked.*/
/*C_1E0F_1B0E*/PartyAdvance(int16 dir) {
	int16 obj, tmp_dir;
	int16 partyIdx, new_x, new_y, tileOrType, flow_dir;

	ShowQuestBarrierMsg = 1;
	obj = Party[Active];
	MapX = GetX(obj);
	MapY = GetY(obj);
	AdvanceBlocked = 0;
	if(DrunkCounter > 3 && OSI_rand(0, 1)) {
		CON_printf(/*1951*/"Hic!\n");
		dir = OSI_rand(0, 7);
	}
	/*raft and balloon can't be steered: they go where the wind blows
	(WindDir = where it comes from, -1 = no wind); a raft also follows
	the water current (tiles TIL_008..TIL_00F = current toward dir 0..7)*/
	if(GetType(obj) == OBJ_RAFT || GetType(obj) == OBJ_BALLOON_INFLATED) {
		flow_dir = dir;
		dir = WindDir ^ 4;
		if(GetType(obj) == OBJ_RAFT) {
			tileOrType = GetTileAtXYZ(MapX, MapY, MapZ);
			if(tileOrType >= TIL_008 && tileOrType < TIL_010)
				flow_dir = tileOrType - TIL_008;
			if(dir < 0)
				dir = flow_dir;
			else
				dir = RaftDriftDir[flow_dir][dir];
		}
		if(dir < 0) {
			CON_printf(NoWindMsg);
			PromptFlag = 1;
			MovePts[obj] = 0;
			return;
		}
	}
	tmp_dir = dir;
	new_x = (MapX + DirIncrX[tmp_dir]) & 0x3ff;
	new_y = (MapY + DirIncrY[tmp_dir]) & 0x3ff;
	if(!CanMoveTo(obj, new_x, new_y)) {
		AdvanceBlocked = 1;
		/*diagonal mouse move blocked: slide along the main axis of the mouse*/
		if(MousePress && DirIncrX[tmp_dir] && DirIncrY[tmp_dir]) {
			if(abs(MouseMapX - MapX) > abs(MouseMapY - MapY)) {
				if(CanMoveTo(obj, new_x, MapY)) {
					AdvanceBlocked = 0;
					new_y = MapY;
				} else if(CanMoveTo(obj, MapX, new_y)) {
					AdvanceBlocked = 0;
					new_x = MapX;
				}
			} else {
				if(CanMoveTo(obj, MapX, new_y)) {
					AdvanceBlocked = 0;
					new_x = MapX;
				} else if(CanMoveTo(obj, new_x, MapY)) {
					AdvanceBlocked = 0;
					new_y = MapY;
				}
			}
		}
	} else if(
		/*no squeezing diagonally between two obstacles*/
		DirIncrX[tmp_dir] && DirIncrY[tmp_dir] &&
		!CanMoveTo(obj, (MapX + DirIncrX[tmp_dir]) & 0x3ff, MapY) &&
		!CanMoveTo(obj, MapX, (MapY + DirIncrY[tmp_dir]) & 0x3ff)
	) {
		AdvanceBlocked = 1;
	}
	if(AdvanceBlocked) {
		/*"bump" sound*/
		MUS_PlaySfx(1, 0x137);
		MapX = GetX(obj);
		MapY = GetY(obj);
		ShowQuestBarrierMsg = 0;
		return;
	}
	/*a party member stands on the destination: it is pushed one square
	further (front-line fighters) or swaps places with the leader*/
	if(GetType(Party[Active]) != OBJ_MOUSE) {
		for(partyIdx = 0; partyIdx < PartySize; partyIdx ++) {
			if(Active != partyIdx) {
				obj = Party[partyIdx];
				if(
					!IsDead(obj) && GetType(obj) != OBJ_MOUSE &&
					GetX(obj) == new_x && GetY(obj) == new_y && GetZ(obj) == MapZ
				) {
					if(NPCMode[obj] != AI_FRONT || TryMoveTo(obj, new_x + new_x - MapX, new_y + new_y - MapY) == 0) {
						MoveObj(obj, MapX, MapY, MapZ);
						StepMovesParts = 1;
						UpdateMoveFrame(obj, (tmp_dir + 4) & 7);
					}
					break;
				}
			}
		}
	}

	/*movement cost; diagonal steps on foot cost 5 more*/
	obj = Party[Active];
	tileOrType = GetType(obj);
	if(tileOrType == OBJ_SHIP && WindDir >= 0) {
		SubMov(obj, ShipWindCost[(tmp_dir - WindDir) & 7]);
	} else if(tileOrType == OBJ_SKIFF) {
		flow_dir = tmp_dir;
		tileOrType = GetTileAtXYZ(MapX, MapY, MapZ);
		if(tileOrType >= TIL_008 && tileOrType < TIL_010)
			flow_dir = tileOrType - TIL_008;
		SubMov(obj, SkiffCurrentCost[(tmp_dir - flow_dir) & 7]);
	} else if(tileOrType == OBJ_RAFT || tileOrType == OBJ_BALLOON_INFLATED) {
		SubMov(obj, 5);
	} else if(DirIncrX[tmp_dir] && DirIncrY[tmp_dir]) {
		SubTerrainMov(obj);
		SubMov(obj, 5);
	} else {
		SubTerrainMov(obj);
	}
	MapX = new_x;
	MapY = new_y;
	MAP_LoadAround();
	if(MoveObj(obj, new_x, new_y, MapZ) >= 0) {
		SetDirection(obj, tmp_dir);
		StepMovesParts = 1;
		UpdateMoveFrame(obj, tmp_dir);
		VisibilityDirty = 1;
	} else {
		MapX = GetX(obj);
		MapY = GetY(obj);
		MAP_LoadAround();
		CON_printf(/*1957*/"Object mismatch\n");
	}
	/*the followers don't trigger the Sacred Quest message*/
	ShowQuestBarrierMsg = 0;
	MoveFollowers(obj, 0);
	CheckGatesAndEntrances();
	ShowQuestBarrierMsg = 0;
}

/*an NPC of a door-opening monster class (IsMonsterHuman, not the party)
 operates the door it stands on: closing = 1 when leaving the square (door
 frame 4..7), 0 when arriving (frame 0..3, open); a passthrough is toggled
 silently in both cases*/
/*C_1E0F_2001*/OperateDoorUnder(int16 objNum, int16 closing) {
	int16 obj, objTyp;
	int16 frame, objCls;

	objCls = GetMonsterClass(GetType(objNum));
	if(objCls < 0 || !IsMonsterHuman(objCls) || IsPlrControl(objNum))
		return;
	for(obj = FindLoc(GetX(objNum), GetY(objNum), GetZ(objNum)); obj >= 0; obj = NextLoc()) {
		objTyp = GetType(obj);
		if(objTyp == OBJ_OAKEN_DOOR || objTyp == OBJ_WINDOWED_DOOR || objTyp == OBJ_CEDAR_DOOR || objTyp == OBJ_STEEL_DOOR) {
			frame = GetFrame(obj);
			SetFrame(obj, (closing << 2) + (frame & 3));
			VIS_UpdateView();
			break;
		}
		if(objTyp == OBJ_PASSTHROUGH_116 || objTyp == OBJ_PASSTHROUGH_118) {
			TextOn = 0;
			UsePassthrough(obj);
			TextOn = 1;
		}
	}
}

/*look for a plate on the 4 squares next to {x, y, z} (N, E, S, W).
 Returns 1 + direction (1, 3, 5, 7) of the first plate found, 0 if none.*/
/*C_1E0F_2125*/static FindAdjacentPlate(int16 x, int16 y, int16 z) {
	int16 dir, obj;

	for(dir = 0; dir < 8; dir += 2) {
		for(obj = FindLoc(DirIncrX[dir] + x, DirIncrY[dir] + y, z); obj >= 0; obj = NextLoc()) {
			if(GetType(obj) == OBJ_PLATE)
				break;
		}
		if(obj >= 0)
			break;
	}
	if(dir < 8)
		return dir + 1;
	return 0;
}

/*look for the furniture FurnitureTypes[kind] on objNum's own square (a chair or a
 throne seat; for the table kind: a chair next to a plate).
 Returns the object found, or -1.*/
/*C_1E0F_2184*/static FindFurnitureHere(int16 objNum, int16 kind) {
	int16 obj, wantedType;
	int16 x, y, z;

	x = GetX(objNum);
	y = GetY(objNum);
	z = GetZ(objNum);
	wantedType = FurnitureTypes[kind];
	for(obj = FindLoc(x, y, z); obj >= 0; obj = NextLoc()) {
		if(wantedType == OBJ_TABLE_SQUARE_CORNER_0E4) {
			if(GetType(obj) == OBJ_CHAIR && FindAdjacentPlate(x, y, z))
				break;
		} else if(wantedType == OBJ_CHAIR) {
			if(
				GetType(obj) == OBJ_CHAIR ||
				(GetType(obj) == OBJ_THRONE && GetFrame(obj) - LocPartOffset == 2)
			) break;
		} else if(GetType(obj) == wantedType) {
			break;
		}
	}
	return obj;
}

/*start the current schedule entry's action: NPCMode = action, and set up
 the posture (sleeping in a bed, sitting, playing the lute, eating facing
 the plate, standing/guarding in a direction...). Sleep/sit/play/eat/ring
 count as "arrived" wherever the NPC is; while it is out of the loaded
 area NPCMode is left at AI_SCHEDULE so this is retried when it comes into
 view. For the other actions, if the NPC is not on its scheduled square
 NPCMode becomes AI_FINDPATH so it walks there.*/
static /*C_1E0F_2276*/StartScheduleAction(int16 objNum) {
	int16 obj;
	struct coord xyz;
	int16 action, isAtDest, plateDir;

	action = Schedule[SchedIndex[objNum] + SchedPointer[objNum]].action;
	xyz = Schedule[SchedIndex[objNum] + SchedPointer[objNum]].xyz;
	isAtDest = (
		xyz.raw[0] == ObjPos[objNum].raw[0] &&
		xyz.raw[1] == ObjPos[objNum].raw[1] &&
		xyz.raw[2] == ObjPos[objNum].raw[2]
	);
	NPCMode[objNum] = action;
	if(action == AI_SLEEP) {
		isAtDest = 1;
		SetAsleep(objNum);
		if(!IsOutOfArea(objNum)) {
			for(obj = FindLoc(GetX(objNum), GetY(objNum), GetZ(objNum)); obj >= 0; obj = NextLoc()) {
				/*pillow part of a bed (frame 0 or 6, two orientations)*/
				if(GetType(obj) == OBJ_BED && (GetFrame(obj) - LocPartOffset == 0 || GetFrame(obj) - LocPartOffset == 6)) {
					ObjShapeType[objNum] = TypeFrame(OBJ_PERSON_SLEEPING, 0);
					if(GetFrame(obj) - LocPartOffset != 0)
						SetFrame(objNum, 1);
					break;
				}
			}
			/*no bed: sleep on the floor*/
			if(obj < 0 || GetType(obj) != OBJ_BED)
				SetTypeUnconscious(objNum);
		} else {
			NPCMode[objNum] = AI_SCHEDULE;
		}
	} else {
		ClrAsleep(objNum);
		ObjShapeType[objNum] = OrigShapeType[objNum];
		if(action == AI_SIT || action == AI_PLAY) {
			isAtDest = 1;
			if(!IsOutOfArea(objNum)) {
				obj = FindFurnitureHere(objNum, 0/*chair*/);
				if(obj >= 0) {
					if(action == AI_PLAY) {
						/*the musician sits facing south and plays*/
						SetFrame(obj, 2);
						UpdateMoveFrame(objNum, 4);
						ObjShapeType[objNum] = TypeFrame(OBJ_MUSICIAN_SHORT_CAPE, 0);
						SetFrame(objNum, 4);
					} else {
						UpdateMoveFrame(objNum, 0);
					}
				}
			} else {
				NPCMode[objNum] = AI_SCHEDULE;
			}
		} else if(action == AI_EAT) {
			isAtDest = 1;
			if(!IsOutOfArea(objNum)) {
				obj = FindFurnitureHere(objNum, 2/*table*/);
				if(obj >= 0) {
					/*turn the chair and the NPC toward the plate*/
					plateDir = FindAdjacentPlate(GetX(obj), GetY(obj), GetZ(obj));
					SetFrame(obj, plateDir >> 1);
					UpdateMoveFrame(objNum, plateDir - 1);
				} else {
					UpdateMoveFrame(objNum, 4);
				}
			} else {
				NPCMode[objNum] = AI_SCHEDULE;
			}
		} else if(action == AI_RINGBELL) {
			isAtDest = 1;
			if(!IsOutOfArea(objNum)) {
				obj = FindFurnitureHere(objNum, 3/*pull chain*/);
			} else {
				NPCMode[objNum] = AI_SCHEDULE;
			}
		} else if(action >= AI_STAND_N && action <= AI_GUARD_W) {
			/*face N/E/S/W*/
			SetDirection(objNum, ((action - AI_STAND_N) & 3) << 1);
			UpdateMoveFrame(objNum, ((action - AI_STAND_N) & 3) << 1);
		}
	}
	if(!isAtDest) {
		NPCMode[objNum] = AI_FINDPATH;
		/*a musician stops playing to walk*/
		if(GetType(objNum) == OBJ_MUSICIAN_SHORT_CAPE) {
			ObjShapeType[objNum] = TypeFrame(OBJ_MUSICIAN_LONG_CAPE, 0);
			UpdateMoveFrame(objNum, 4);
		}
	}
}

/*====---- path finding ----====
 The search works on the 40x40 area whose corner is
 ((MapX - 0x10) & 0x3f8, (MapY - 0x10) & 0x3f8) and uses ScratchBuf as:
  PTH_map[y][x]   : 0xff = not reached, else cost (0..0x7f) | front flag
                    (0x00 = front grown from the NPC, 0x80 = from the goal);
  PTH_rank[cost]  : bucket queue, head node of the list of cells at that cost;
  PTH_link[node]  : next node in a bucket, or in the free list (PTH_Top);
  PTH_x/PTH_y     : cell of each node;
  PTH_resist[y][x]: cost of entering a cell (0xff = impassable).
 It is a uniform-cost (Dijkstra) search with buckets, run from both ends at
 once when the goal is a square; it stops when the fronts meet, when the
 seeked object type (PTH_object) is reached, or when the area edge in
 direction PTH_direct is reached (goal outside the area).
 A found path is stored in path slot PTH_Index of PathData (8 slots of 32
 bytes): each byte = direction (bits 0-1: 0 N, 1 E, 2 S, 3 W) + 4 * number
 of steps in that direction; 0 ends the path.*/
typedef struct {
	unsigned char /*_0000*/PTH_map[AREA_H][AREA_W];
	int16 /*_0640*/PTH_rank[0x100];
	int16 /*_0840*/PTH_link[0x100];
	unsigned char /*_0a40*/PTH_x[0x100],/*_0b40*/PTH_y[0x100];
	unsigned char /*_0c40*/PTH_resist[AREA_H][AREA_W];
} t40x40;

#define _ScratchBuf ((t40x40 far *)ScratchBuf)

/*x/y increments for the 4 path directions N, E, S, W*/
/*185A*/char PTH_DirX[] = { 0,1,0,-1};
/*185E*/char PTH_DirY[] = {-1,0,1, 0};

/*build the path once the search succeeded at cell (area_x, area_y):
 first walk down the costs back to the cost-0 start cell, prepending the
 reversed steps; then (two-ended search only) walk from the meeting cell
 down the goal front to the goal, appending the steps. Path longer than 31
 entries are truncated. The result goes to PathData->content[PTH_Index].*/
/*C_1E0F_25F9*/static PTH_BuildPath(int16 area_x, int16 area_y) {
	int16 i, dir;
	int16 pathLen, some_min, dir_min, back_x, back_y, new_x, new_y;

	back_x = area_x;
	back_y = area_y;

	pathLen =
	dir = 0;
	PathData->content[PTH_Index][0] = 0;

	while(_ScratchBuf->PTH_map[area_y][area_x]) {
		some_min = 0xff;
		for(i = 0; i < 4; i ++) {
			new_x = area_x + PTH_DirX[i];
			new_y = area_y + PTH_DirY[i];
			if(new_x >= 0 && new_x <= (AREA_W-1) && new_y >= 0 && new_y <= (AREA_H-1)) {
				if(

					_ScratchBuf->PTH_map[new_y][new_x] < some_min ||
					(_ScratchBuf->PTH_map[new_y][new_x] == some_min && dir == i)
				) {
					some_min = _ScratchBuf->PTH_map[new_y][new_x];
					dir_min = i;
				}
			}
		}
		dir = dir_min;
		area_x += PTH_DirX[dir];
		area_y += PTH_DirY[dir];


		/*new direction: shift the path and insert at the front; same
		direction: one more step in the first entry*/
		if(pathLen == 0 || (PathData->content[PTH_Index][0] & 3) != (dir ^ 2)) {
			if(pathLen < 0x1f)
				pathLen ++;
			for(i = pathLen; i > 0; i --)
				PathData->content[PTH_Index][i] = PathData->content[PTH_Index][i - 1];
			PathData->content[PTH_Index][0] = (dir ^ 2) + 4;
		} else {
			PathData->content[PTH_Index][0] += 4;
		}
	}

	if(PTH_object < 0 && PTH_direct < 0) {
		area_x = back_x;
		area_y = back_y;

		_ScratchBuf->PTH_map[area_y][area_x] = 0xff;

		/*follow the goal front (values >= 0x80) down to the goal (0x80)*/
		while(_ScratchBuf->PTH_map[area_y][area_x] > 0x80) {
			some_min = 0xff;
			for(i = 0; i < 4; i ++) {
				new_x = area_x + PTH_DirX[i];
				new_y = area_y + PTH_DirY[i];
				if(new_x >= 0 && new_x <= (AREA_W-1) && new_y >= 0 && new_y <= (AREA_H-1)) {
					if(
						_ScratchBuf->PTH_map[area_y + PTH_DirY[i]][area_x + PTH_DirX[i]] >= 0x80 && (
						_ScratchBuf->PTH_map[new_y][new_x] < some_min ||
						(_ScratchBuf->PTH_map[new_y][new_x] == some_min && dir == i)
					)) {
						some_min = _ScratchBuf->PTH_map[new_y][new_x];
						dir_min = i;
					}
				}
			}
			dir = dir_min;
			area_x += PTH_DirX[dir];
			area_y += PTH_DirY[dir];
			if(pathLen >= 0x1f)
				break;
			if(pathLen == 0 || (PathData->content[PTH_Index][pathLen - 1] & 3) != dir) {
				PathData->content[PTH_Index][pathLen] = dir + 4;
				pathLen ++;
			} else {
				PathData->content[PTH_Index][pathLen - 1] += 4;
			}
		}
	}
	PathData->content[PTH_Index][pathLen] = 0;
}

/*teleport an NPC directly to its scheduled location and start the action,
 unless it or the destination is within 5 squares of the player (on his
 level) - always done when AllowNPCTeleport is set. Frees its path slot.*/
/*C_1E0F_291C*/static TeleportToSchedule(int16 objNum) {
	struct coord sched_xyz;
	int16 x, y, z;

	x = GetX(objNum);
	y = GetY(objNum);
	z = GetZ(objNum);
	if(
		!AllowNPCTeleport &&
		z == MapZ &&
		x >= MapX - 5 && x <= MapX + 5 &&
		y >= MapY - 5 && y <= MapY + 5
	) return;
	sched_xyz = Schedule[SchedIndex[objNum] + SchedPointer[objNum]].xyz;
	x = GetCoordX(sched_xyz);
	y = GetCoordY(sched_xyz);
	/*note: tests the NPC's level, not the destination's*/
	if(
		!AllowNPCTeleport &&
		z == MapZ &&
		x >= MapX - 5 && x <= MapX + 5 &&
		y >= MapY - 5 && y <= MapY + 5
	) return;
	if(NPCMode[objNum] >= AI_ONPATH && NPCMode[objNum] < AI_PATH_FAILED)
		PathObject[Leader[objNum]] = 0;
	/*MoveAppliesTerrain = 0: no terrain-damage check in MoveObj*/
	MoveAppliesTerrain = 0;
	MoveObj(objNum, x, y, GetCoordZ(sched_xyz));
	MoveAppliesTerrain = 1;
	StartScheduleAction(objNum);
	SubMov(objNum, 10);
	TeleportCount ++;
}

/*path search: reach cell (x, y) with total cost "resist" from the front
 flag_80 (0 or 0x80). Detects success (other front met, seeked object found,
 area edge reached in direction PTH_direct); otherwise queues the cell in
 bucket PTH_rank[resist] if this is cheaper than what it had.*/
/*C_1E0F_2A74*/static PTH_Visit(int16 resist, int16 x, int16 y, int16 flag_80) {
	int16 newTop, obj;
	int16 origin_x, origin_y;

	if(
		PTH_Found ||
		resist > 0x7f ||
		!(x >= 0 && x <= (AREA_W-1) && y >= 0 && y <= (AREA_H-1))
	) return;

	/*reached a cell of the other front: the two searches met*/
	if(_ScratchBuf->PTH_map[y][x] != 0xff && (_ScratchBuf->PTH_map[y][x] & 0x80) != flag_80) {
		PTH_Found = 1;
		PTH_BuildPath(x, y);
		return;
	}

	/*seeking furniture: is it here?*/
	if(PTH_object >= 0) {
		origin_x = (MapX - 0x10) & 0x3f8;
		origin_y = (MapY - 0x10) & 0x3f8;
		for(obj = FindLoc(x + origin_x, y + origin_y, MapZ); obj >= 0; obj = NextLoc()) {
			if(
				(PTH_object == OBJ_TABLE_SQUARE_CORNER_0E4 && GetType(obj) == OBJ_CHAIR && FindAdjacentPlate(x + origin_x, y + origin_y, MapZ)) ||
				(PTH_object != OBJ_TABLE_SQUARE_CORNER_0E4 && GetType(obj) == PTH_object)
			) {
				PTH_Found = 1;
				PTH_BuildPath(x, y);
				return;
			}
		}
	}

	/*goal outside the area: reached the edge on its side?*/
	if(PTH_direct >= 0) {
		if(
			(x == 0 && PTH_direct == 3) || (x == (AREA_W-1) && PTH_direct == 1) ||
			(y == 0 && PTH_direct == 0) || (y == (AREA_H-1) && PTH_direct == 2)
		) {
			PTH_Found = 1;
			PTH_AnX = x;
			PTH_AnY = y;
			PTH_BuildPath(x, y);
			return;
		}
	}
	/*already reached at a lower or equal cost (0xff & 0x7f = 0x7f = never)*/
	if((_ScratchBuf->PTH_map[y][x] & 0x7f) <= resist)
		return;
	/*take a node from the free list (none left: give up on this cell)*/
	newTop = PTH_Top;
	if(newTop < 0)
		return;
	/*PTH_MaxCost is in fact the highest cost queued so far*/
	if(PTH_MaxCost < resist)
		PTH_MaxCost = resist;
	PTH_Top = _ScratchBuf->PTH_link[newTop];
	_ScratchBuf->PTH_link[newTop] = _ScratchBuf->PTH_rank[resist];
	_ScratchBuf->PTH_x[newTop] = x;
	_ScratchBuf->PTH_y[newTop] = y;
	_ScratchBuf->PTH_rank[resist] = newTop;
	_ScratchBuf->PTH_map[y][x] = resist | flag_80;
}

/*reverse path slot "index" in place: entries in reverse order and each
 direction flipped (^2: N<->S, E<->W). Used when the search was run from
 the goal toward the NPC.*/
static /*C_1E0F_2C9D*/PTH_ReversePath(int16 index) {
	int16 k, len;
	int16 tmp;

	for(len = 0; len < 0x20; len ++) {
		if(PathData->content[index][len] == 0)
			break;
	}
	for(k = 0; k < (len + 1) / 2; k ++) {
		tmp = PathData->content[index][k] ^ 2;
		PathData->content[index][k] = PathData->content[index][len - 1 - k] ^ 2;
		PathData->content[index][len - 1 - k] = tmp;
	}
}

/*find a path for objNum (stored in slot PTH_Index):
 - x >= 0: to the square (x, y). If the NPC or the goal is outside the
   area the NPC is teleported if possible (TeleportToSchedule), else the search
   goes toward the area edge on the far side (PTH_direct); when the NPC is
   the one outside, the search is run from the goal and the NPC is put on
   the edge square found ("swapped").
 - x < 0: to the nearest furniture of type y (AI_SEEKOBJ).
 Success: NPCMode = AI_ONPATH, Leader[npc] = path slot. Failure:
 AI_FINDPATH -> AI_PATH_FAILED (retry later), AI_SEEKOBJ -> AI_SCHEDULE.*/
/*C_1E0F_2D37*/static PTH_FindPath(int16 objNum, int16 x, int16 y) {
	int16 start_x, start_y, i, j, node;
	int16 area_x, area_y, cost, flag_80, tmp, swapped;

	PTH_Found = swapped = 0;
	PTH_direct = -1;
	area_x = (MapX - 0x10) & 0x3f8;
	area_y = (MapY - 0x10) & 0x3f8;
	/*coord "in area"*/
	start_x = GetX(objNum) - area_x;
	start_y = GetY(objNum) - area_y;
	if(x >= 0) {
		/*coord "in area"*/
		x -= area_x;
		y -= area_y;
		PTH_object = -1;
		if(start_x == x && start_y == y) {
			StartScheduleAction(objNum);
			return;
		}
		if(!(
			GetZ(objNum) == MapZ &&
			start_x >= 0 && start_x <= (AREA_W-1) &&
			start_y >= 0 && start_y <= (AREA_H-1) &&
			x >= 0 && x <= (AREA_W-1) &&
			y >= 0 && y <= (AREA_H-1)
		)) {
			/*at most 3 teleports per turn*/
			if(TeleportCount >= 3)
				return;
			TeleportToSchedule(objNum);
			if(NPCMode[objNum] != AI_FINDPATH)
				return;
			if(!(start_x >= 0 && start_x <= (AREA_W-1) && start_y >= 0 && start_y <= (AREA_H-1))) {
				tmp = x; x = start_x; start_x = tmp;
				tmp = y; y = start_y; start_y = tmp;
				swapped = 1;
			}
			/*edge to head for: the side of the area (seen from its center
			20,20) where the outside end lies; 0 N, 1 E, 2 S, 3 W*/
			if(__abs__(x - 20) > __abs__(y - 20))
				PTH_direct = (x < 20)?3:1;
			else
				PTH_direct = (y < 20)?0:2;
		}
	} else {
		/*seeked object*/
		PTH_object = y;
	}

	/*at most 3 searches per turn (see PTH_SearchPaths)*/
	PathSearchCount ++;
	__ComputeResistance(objNum);

	/*reset the map, the node free list and the buckets*/
	for(i = 0; i < AREA_H; i ++) {
		for(j = 0; j < AREA_W; j ++)
			_ScratchBuf->PTH_map[i][j] = 0xff;
	}
	for(i = 0; i < 0x100; i ++)
		_ScratchBuf->PTH_link[i] = i + 1;
	for(i = 0; i < 0x100; i ++)
		_ScratchBuf->PTH_rank[i] = -1;
	_ScratchBuf->PTH_link[0xff] = -1;
	PTH_Top = PTH_Found = 0;
	PTH_Cur = PTH_MaxCost = 0;

	/*seed the NPC's square (front 0) and, for a square goal, the goal
	(front 0x80)*/
	PTH_Visit(0, start_x, start_y, 0);
	if(PTH_object == -1 && PTH_direct == -1)
		PTH_Visit(0, x, y, 0x80);

	/*pop the cheapest cell, give its node back to the free list and
	expand its 4 neighbours*/
	while(_ScratchBuf->PTH_rank[PTH_Cur] >= 0 && !PTH_Found) {
		node = _ScratchBuf->PTH_rank[PTH_Cur];
		_ScratchBuf->PTH_rank[PTH_Cur] = _ScratchBuf->PTH_link[node];
		x = _ScratchBuf->PTH_x[node];
		y = _ScratchBuf->PTH_y[node];
		_ScratchBuf->PTH_link[node] = PTH_Top;
		PTH_Top = node;
		cost = _ScratchBuf->PTH_map[y][x];
		flag_80 = cost & 0x80;
		cost &= 0x7f;
		PTH_Visit(_ScratchBuf->PTH_resist[y][x - 1] + cost, x - 1, y, flag_80);
		PTH_Visit(_ScratchBuf->PTH_resist[y][x + 1] + cost, x + 1, y, flag_80);
		PTH_Visit(_ScratchBuf->PTH_resist[y - 1][x] + cost, x, y - 1, flag_80);
		PTH_Visit(_ScratchBuf->PTH_resist[y + 1][x] + cost, x, y + 1, flag_80);
		/*bucket empty: advance to the next non-empty one*/
		if(_ScratchBuf->PTH_rank[PTH_Cur] < 0) {
			while(PTH_Cur < PTH_MaxCost && _ScratchBuf->PTH_rank[PTH_Cur] < 0)
				PTH_Cur ++;
		}
	}
	if(PTH_Found) {
		/*searched from the goal: put the NPC on the edge square reached
		and turn the path around*/
		if(swapped) {
			MoveAppliesTerrain = 0;
			MoveObj(objNum, PTH_AnX + area_x, PTH_AnY + area_y, GetZ(objNum));
			MoveAppliesTerrain = 1;
			PTH_ReversePath(PTH_Index);
		}
		NPCMode[objNum] = AI_ONPATH;
		Leader[objNum] = PTH_Index;
		PathObject[PTH_Index] = objNum;
		PathCounter[PTH_Index] = 0;
		PathTries[PTH_Index] = 0;
	} else if(NPCMode[objNum] == AI_FINDPATH) {
		NPCMode[objNum] = AI_PATH_FAILED;
	} else if(NPCMode[objNum] == AI_SEEKOBJ) {
		NPCMode[objNum] = AI_SCHEDULE;
	}
}

/*random signed offset with a geometric distribution: 0 (50%), +-1 (25%)...*/
/*C_1E0F_31C7*/static RandomSpread() {
	int16 n;

	for(n = 0; OSI_rand(0, 1); n ++);
	if(OSI_rand(0, 1))
		n = -n;
	return n;
}

/*consume movement points according to terrain: 5 + the upper nibble of
 TerrainType (movement cost) of the base tile and of every object on the
 object's square (impassable tiles excluded)*/
static /*C_1E0F_31F7*/SubTerrainMov(int16 numObject) {
	int16 numTile, obj;
	int16 x, y, z, pts;

	pts = 5;
	x = GetX(numObject);
	y = GetY(numObject);
	z = GetZ(numObject);
	numTile = GetTileAtXYZ(x, y, z);
	if(!IsTerrainImpass(numTile))
		pts += TerrainType[numTile] >> 4;
	for(obj = FindLoc(x, y, z); obj >= 0; obj = NextLoc()) {
		numTile = TILE_FRAME(obj);
		if(!IsTerrainImpass(numTile))
			pts += TerrainType[numTile] >> 4;
	}
	SubMov(numObject, pts);
}

/*try to step objNum one square in direction dir (refusing damaging tiles if
 avoidHazard). Pays the terrain cost even when blocked. A party member in
 combat won't leave the 17x17 square around the leader. Door-opening NPCs
 open/close doors on the way. Returns 1 if moved.*/
/*C_1E0F_32D6*/TryStraightMove(int16 objNum, int16 dir, int16 avoidHazard) {
	int16 new_x, new_y;

	dir &= 7;
	SubTerrainMov(objNum);
	new_x = GetX(objNum) + DirIncrX[dir];
	new_y = GetY(objNum) + DirIncrY[dir];
	if(
		!IsPlrControl(objNum) || !InCombat ||
		CLOSE_ENOUGH0(8, MapX, MapY, new_x, new_y)
	) {
		AvoidHazards = avoidHazard;
		if(CanMoveTo(objNum, new_x, new_y)) {
				AvoidHazards = 0;
			OperateDoorUnder(objNum, 1);
			MoveObj(objNum, new_x, new_y, MapZ);
			SetDirection(objNum, dir);
			StepMovesParts = 1;
			UpdateMoveFrame(objNum, dir);
			OperateDoorUnder(objNum, 0);
			return 1;
		}
		AvoidHazards = 0;
	}
	return 0;
}

/*loiter around (x, y): 1 chance in 8 to take a random straight step,
 biased toward (x, y) the farther the NPC is; otherwise just wait*/
/*C_1E0F_33C4*/static LoiterNear(int16 objNum, int16 x, int16 y) {
	int16 delta_x, delta_y, dir;

	if(OSI_rand(0, 7) == 0) {
		delta_x = GetX(objNum) - x;
		delta_y = GetY(objNum) - y;
		if(__abs__(delta_y) - __abs__(delta_x) < RandomSpread())
			dir = (RandomSpread() > delta_x)?2:6;
		else
			dir = (RandomSpread() > delta_y)?4:0;
		TryStraightMove(objNum, dir, 1);
	} else {
		SubMov(objNum, 5);
	}
}

/*try a diagonal step combining h_dir (2/6) and v_dir (0/4); at least one of
 the two orthogonal squares must be free too (no corner cutting). Never
 for the silver serpent. Returns 1 if moved.*/
static /*C_1E0F_3470*/__TryDiagMove(int16 objNum, int16 h_dir, int16 v_dir) {
	int16 dir;
	int16 sideFree, new_x, new_y;

	if(GetType(objNum) == OBJ_SILVER_SERPENT)
		return 0;

	if(v_dir == 0 && h_dir == 6)
		dir = 7;
	else
		dir = (v_dir + h_dir) >> 1;
	SubTerrainMov(objNum);
	new_x = GetX(objNum) + DirIncrX[dir];
	new_y = GetY(objNum) + DirIncrY[dir];
	PassThroughParty = 1;
	AvoidHazards = 1;
	sideFree = CanMoveTo(objNum, new_x, GetY(objNum));
	if(!sideFree)
		sideFree = CanMoveTo(objNum, GetX(objNum), new_y);
	PassThroughParty = 0;
	if(sideFree && CanMoveTo(objNum, new_x, new_y)) {
		AvoidHazards = 0;
		OperateDoorUnder(objNum, 1);
		MoveObj(objNum, new_x, new_y, MapZ);
		SetDirection(objNum, dir);
		StepMovesParts = 1;
		UpdateMoveFrame(objNum, dir);
		OperateDoorUnder(objNum, 0);
		return 1;
	}
	AvoidHazards = 0;

	return 0;
}

/*take one step from objNum's position toward (new_x, new_y) (not further).
 Chooses the main axis (the larger delta; randomly weighted when far), then
 tries in order: straight on the main axis, diagonal, straight on the other
 axis, and (1 chance in 2) straight back on the other axis. Party members
 don't walk through each other meanwhile (PassThroughParty = 0).
 Returns 1 if a step was made.*/
/*C_1E0F_35A7*/TryMoveTo(int16 objNum, int16 new_x, int16 new_y) {
	int16 delta_x, delta_y;
	int16 h_dir, v_dir, moved, isVMove, avoidHazard;

	avoidHazard = !IgnoreHazards;
	moved = 1;
	delta_x = new_x - GetX(objNum);
	delta_y = new_y - GetY(objNum);
	if((delta_x == 0 && delta_y == 0) || NPCMode[objNum] == AI_IMMOBILE) {
		SubMov(objNum, 5);
		return 0;
	}
	if(delta_x == 0)
		h_dir = OSI_rand(0, 1)?2:6;
	else if(delta_x < 0)
		h_dir = 6;
	else
		h_dir = 2;
	if(delta_y == 0)
		v_dir = OSI_rand(0, 1)?0:4;
	else if(delta_y < 0)
		v_dir = 0;
	else
		v_dir = 4;
	PassThroughParty = 0;
	if(__abs__(delta_x) < 4 && __abs__(delta_y) < 4) {
		isVMove = 
			(__abs__(delta_x) < __abs__(delta_y)) ||
			(__abs__(delta_x) == __abs__(delta_y) && OSI_rand(0, 1))
		;
	} else {
		isVMove = OSI_rand(1, __abs__(delta_x) + __abs__(delta_y)) > __abs__(delta_x);
	}
	if(isVMove) {
		if(
			!TryStraightMove(objNum, v_dir, avoidHazard) &&
			!__TryDiagMove(objNum, h_dir, v_dir) &&
			!TryStraightMove(objNum, h_dir, avoidHazard) &&
			(OSI_rand(0, 1) || !TryStraightMove(objNum, h_dir ^ 4, 1))
		) {
			SubTerrainMov(objNum);
			moved = 0;
		}
	} else {
		if(
			!TryStraightMove(objNum, h_dir, avoidHazard) &&
			!__TryDiagMove(objNum, h_dir, v_dir) &&
			!TryStraightMove(objNum, v_dir, avoidHazard) &&
			(OSI_rand(0, 1) || !TryStraightMove(objNum, v_dir ^ 4, 1))
		) {
			SubTerrainMov(objNum);
			moved = 0;
		}
	}
	PassThroughParty = 1;

	return moved;
}

/*wander at random: a party member sometimes walks back to the leader,
 others take a random straight step 1 time in 8; otherwise wait*/
/*C_1E0F_37DB*/WanderRandomly(int16 objNum) {
	if(NPCMode[objNum] != AI_IMMOBILE) {
		if(
			IsPlrControl(objNum) &&
			(GetX(objNum) != MapX || GetY(objNum) != MapY) &&
			OSI_rand(0, 3) == 0
		) {
			TryMoveTo(objNum, MapX, MapY);
			return;
		}
		if(OSI_rand(0, 7) == 0) {
			TryStraightMove(objNum, OSI_rand(0, 3) << 1, 1);
			return;
		}
	}
	SubMov(objNum, 5);
}

/*follow the stored path one step (AI_ONPATH): slot = Leader[npc],
 PathCounter[slot] = current entry, PathTries[slot] = steps already done
 in it. At the end of the path the schedule action starts if the NPC is
 within 1 square of (x, y) (its scheduled spot), else it searches again.
 When blocked it waits up to 3 turns (AI_ONPATH -> AI_PATH_BLOCKED1 -> AI_PATH_BLOCKED2 -> AI_PATH_FAILED),
 then gives up the path (AI_PATH_FAILED = to be searched again later).*/
static /*C_1E0F_387D*/__DoOnPath(int16 objNum, int16 x, int16 y) {
	int16 index;
	int16 info;

	index = Leader[objNum];
	info = PathData->content[index][PathCounter[index]];
	if(info == 0) {
		PathObject[index] = 0;
		if(COMBAT_TileDistance(objNum, x, y) < 2)
			StartScheduleAction(objNum);
		else
			NPCMode[objNum] = AI_FINDPATH;
		SubMov(objNum, 5);
	} else if(TryStraightMove(objNum, (info & 3) << 1, 1)) {
		if((info >> 2) <= ++PathTries[index]) {
			PathCounter[index] ++;
			PathTries[index] = 0;
		}
		NPCMode[objNum] = AI_ONPATH;
	} else if(NPCMode[objNum] == AI_ONPATH) {
		NPCMode[objNum] = AI_PATH_BLOCKED1;
		MovePts[objNum] = 0;
	} else if(NPCMode[objNum] == AI_PATH_BLOCKED1) {
		NPCMode[objNum] = AI_PATH_BLOCKED2;
		MovePts[objNum] = 0;
	} else if(NPCMode[objNum] == AI_PATH_BLOCKED2) {
		NPCMode[objNum] = AI_PATH_FAILED;
		MovePts[objNum] = 0;
		PathObject[index] = 0;
		/*blocked right next to the goal: good enough
		(note: DirIncrX[info] uses the raw path byte, not (info & 3) << 1)*/
		if(GetX(objNum) + DirIncrX[info] == x && GetY(objNum) + DirIncrY[info] == y)
			StartScheduleAction(objNum);
	}
}

/*walk toward the nearest visible party member (or the party's center when
 none is visible). Returns that member if now adjacent, else -1.*/
/*C_1E0F_39B3*/static ApproachNearestMember(int16 objNum) {
	int16 nearest, member;
	int16 partyIdx, x, y, x_dest, y_dest, dist, dist_min;

	nearest = -1;
	x = GetX(objNum);
	y = GetY(objNum);
	dist_min = 50;
	for(partyIdx = 0; partyIdx < PartySize; partyIdx ++) {
		member = Party[partyIdx];
		if(ObjShapeType[member] && !IsDead(member) && COMBAT_canSee(objNum, member)) {
			x_dest = GetX(member);
			y_dest = GetY(member);
			dist = (x_dest - x) * (x_dest - x) + (y_dest - y) * (y_dest - y);
			if(
				(dist < dist_min) ||
				(dist == dist_min && OSI_rand(0, 1))
			) {
				dist_min = dist;
				nearest = member;
			}
		}
	}
	if(nearest < 0) {
		TryMoveTo(objNum, PartyGravityX, PartyGravityY);
	} else {
		TryMoveTo(objNum, GetX(nearest), GetY(nearest));
		if(!CLOSE_ENOUGH0(1, GetX(nearest), GetY(nearest), GetX(objNum), GetY(objNum)))
			nearest = -1;
	}
	return nearest;
}

/*a guard (AI_ARREST) caught the party: "wilt thou come quietly?".
 Only once Lord British's (NPC 5) talk flag 0x80 is set; before that the
 guard just goes back to its schedule.
 Yes: the party wakes up in the Britain jail at 8 o'clock, lockpicks are
 destroyed, the jail key (key quality 9) goes into the desk and the cell
 door gets locked. No: the guard and every other arresting NPC turn hostile.*/
/*C_1E0F_3B60*/static GuardArrest(int16 guard) {
	int16 obj, i;
	int16 answer, item, nextItem;

	if(!(TalkFlags[5] & 0x80)) {
		NPCMode[guard] = AI_FINDPATH;
		return;
	}
	CON_printf(ArrestMsg);
	do
		answer = CON_getch();
	while(answer != 'N' && answer != 'Y');
	NPCMode[guard] = AI_FINDPATH;
	if(answer == 'Y') {
		CON_printf(GotoJailMsg);
		ResetMapView();
		if(InCombat)
			COMBAT_breakOff();
		PartyTeleport(0x0e7, 0x0ba, 0, 0);
		while(Time_H != 8)
			PassTime(60);
		/*go through each member's inventory (the objects following him in
		the Link list while carried by him)*/
		for(i = 0; i < PartySize; i ++) {
			for(item = Link[Party[i]]; item >= 0 && GetCoordUse(item) != LOCXYZ && OBJ_IsInside(item, Party[i]); item = nextItem) {
				nextItem = Link[item];
				if(GetType(item) == OBJ_LOCKPICK)
					DeleteObj(item);
				if(GetType(item) == OBJ_KEY && GetQual(item) == 9) {
					for(obj = FindLoc(0x0eb, 0x0b7, 0); obj >= 0 && GetType(obj) != OBJ_DESK; obj = NextLoc());
					if(obj >= 0)
						InsertObj(item, obj, CONTAINED);
				}
				/*lock the cell door (done for every item, harmless)*/
				for(obj = FindLoc(0x0e7, 0x0b8, 0); obj >= 0 && GetType(obj) != OBJ_STEEL_DOOR; obj = NextLoc());
				if(obj >= 0)
					SetFrame(obj, 9);
			}
		}
		ScreenFade = 1;
		ShowObjects();
		MUS_PlayAreaSong();
		DisplayTick();
	} else {
		CON_printf(DefendThyselfMsg);
		COMBAT_RaiseAlarm();
		NPCMode[guard] = AI_ASSAULT;
		SetAlignment(guard, EVIL);
		for(i = 0; i < 0x100; i ++) {
			if(NPCMode[i] == AI_ARREST) {
				NPCMode[i] = AI_ASSAULT;
				SetAlignment(i, EVIL);
			}
		}
	}
	PromptFlag = 1;
}

/*AI_THIEF: steal 10-30 gold from the party member victim (an object
 number, as returned by ApproachNearestMember); the theft is noticed depending on
 the victim's INT against the thief's DEX.
 note: never called in practice, see the AI_CONVERSE/AI_THIEF case of
 NPC_TakeTurn.*/
/*C_1E0F_3D8D*/static StealGold(int16 thief, int16 victim) {
	if(!TestObj(victim, OBJ_GOLD))
		return;
	TakeObj(victim, OBJ_GOLD, OSI_rand(10, 30));
	if((GetInt(victim) + 30 - GetDex(thief)) / 2 <= OSI_rand(1, 30))
		CON_printf(StealMsg, GetObjectString(thief));/*"The %s stole some gold!\n"*/
}

/*AI_CONVERSE: the NPC starts a conversation with the party.
 note: TALK_talkTo() expects a party index as first parameter but gets
 the member's object number here*/
/*C_1E0F_3E08*/static TalkToParty(int16 npc, int16 member) {
	TALK_talkTo(member, npc, 0);
	MUS_PlayAreaSong();
	PromptFlag = 1;
}

/*find a free path slot (0..7), freeing the slots whose NPC is no longer
 following a path. Returns 8 if none.*/
static /*C_1E0F_3E27*/__NewPathIndex() {
	int16 slot, mode;

	for(slot = 0; slot < 8; slot ++) {
		if(PathObject[slot]) {
			mode = NPCMode[PathObject[slot]];
			if(!(mode >= AI_ONPATH  && mode <= AI_PATH_FAILED))
				PathObject[slot] = 0;
		}
		if(PathObject[slot] == 0)
			break;
	}
	return slot;
}

/*A.I. engine: let actor objNum act once according to NPCMode (combat
 strategies, schedule behaviours, path following...). Nothing happens
 during Time Stop (SpellFx[14]) or on another level. An actor that did
 not spend any movement point loses 10.*/
/*C_1E0F_3E6A*/NPC_TakeTurn(int16 objNum) {
	int16 other;
	int16 dir, sched_x, sched_y, savedMovePts, startMovePts;
	struct coord sched_xyz;

	if(IsPlrControl(objNum) && !IsCharmed(objNum))
		COMBAT_stopCharm(objNum);
	startMovePts = MovePts[objNum];
	if(SpellFx[14] || GetZ(objNum) != MapZ) {
		MovePts[objNum] = 0;
		return;
	}
	sched_xyz = Schedule[SchedIndex[objNum] + SchedPointer[objNum]].xyz;
	sched_x = GetCoordX(sched_xyz);
	sched_y = GetCoordY(sched_xyz);
	switch(NPCMode[objNum]) {
		case AI_FRONT: COMBAT_AI_Front(objNum); break;
		case AI_REAR: COMBAT_AI_Rear(objNum); break;
		case AI_FLANK: COMBAT_AI_Flank(objNum); break;
		case AI_BERSEK: COMBAT_AI_Berserk(objNum); break;
		case AI_ASSAULT:
		case AI_VIGILANTE: COMBAT_AI_Assault(objNum); break;
		case AI_IMMOBILE: COMBAT_AI_Immobile(objNum); break;
		case AI_TANGLE: COMBAT_AI_Tangle(objNum); break;
		case AI_RETREAT:
		case AI_SHY:
		case AI_FEAR:
		case AI_SCHED_SHY: COMBAT_AI_Retreat(objNum); break;
		case AI_LIKE: COMBAT_AI_Like(objNum); break;
		case AI_UNFRIENDLY: COMBAT_AI_Unfriendly(objNum); break;
		case AI_ARREST:
			/*chase the party; arrest when adjacent*/
			if((other = ApproachNearestMember(objNum)) >= 0)
				GuardArrest(objNum);
		break;
		case AI_BRAWL:
			/*tavern brawl: from time to time go and (harmlessly) hit an
			opponent near the brawl spot, then come back*/
			if(OSI_rand(0, 3) == 0) {
				other = COMBAT_pickOpponent(objNum);
				if(other >= 0) {
					if(COMBAT_TileDistance(other, sched_x, sched_y) < 5) {
						if(COMBAT_TileDistance(objNum, GetX(other), GetY(other)) < 2) {
							COMBAT_showHit(other);
							TryMoveTo(objNum, sched_x, sched_y);
						} else {
							TryMoveTo(objNum, GetX(other), GetY(other));
						}
						break;
					}
				}
			}
			LoiterNear(objNum, sched_x, sched_y);
		break;
		case AI_CONVERSE:
		case AI_THIEF:
			if(CLOSE_ENOUGH0_S(5, sched_x, sched_y, MapX, MapY)) {
				if((other = ApproachNearestMember(objNum)) >= 0) {
					/*bug: the mode is overwritten before being tested, so
					thieves only talk*/
					NPCMode[objNum] = AI_LOITER;/*bug?*/
					if(NPCMode[objNum] == AI_THIEF)
						StealGold(objNum, other);/*steal gold*/
					else
						TalkToParty(objNum, other);/*talk to*/
				}
			} else {
				LoiterNear(objNum, sched_x, sched_y);
			}
		break;
		case AI_ONPATH:
		case AI_PATH_BLOCKED1:
		case AI_PATH_BLOCKED2: __DoOnPath(objNum, sched_x, sched_y); break;
		case AI_SEEKOBJ:
			/*Leader[] holds the furniture kind (index in FurnitureTypes)*/
			PTH_Index = __NewPathIndex();
			if(PTH_Index < 8) {
				PTH_FindPath(objNum, -1, FurnitureTypes[Leader[objNum]]);
				SubMov(objNum, 5);
			} else {
				MovePts[objNum] = 0;
			}
		break;
		case AI_GRAZE:
		case AI_WANDER: WanderRandomly(objNum); break;
		case AI_LOITER:
		case AI_FARM: LoiterNear(objNum, sched_x, sched_y); break;
		case AI_PATROL_EW:
		case AI_PATROL_NS:
		case AI_GUARD_N:
		case AI_GUARD_E:
		case AI_GUARD_S:
		case AI_GUARD_W:
			/*patrol: walk straight on (on its spot a guard faces its
			direction) and turn back when blocked*/
			if(OSI_rand(0, 1) == 0) {
				SubMov(objNum, 5);
			} else {
				if(NPCMode[objNum] >= AI_GUARD_N && GetX(objNum) == sched_x && GetY(objNum) == sched_y)
					dir = (NPCMode[objNum] - AI_GUARD_N) << 1;
				else
					dir = GetDirection(objNum);
				savedMovePts = MovePts[objNum];
				if(TryStraightMove(objNum, dir, 1) == 0) {
					dir ^= 4;
					MovePts[objNum] = savedMovePts;
					TryStraightMove(objNum, dir, 1);
				}
			}
		break;
		case AI_RINGBELL:
			/*on the hour: ring the bell (hour % 12 strokes)*/
			if(MustRingBell) {
				MustRingBell = 0;
				MUS_Bell = Time_H % 12;
				if(MUS_Bell == 0)
					MUS_Bell = 12;
				SetTileAnimation(BaseTile[OBJ_BELL], 1);
			}
			/*fall through*/
		default:
			SubMov(objNum, 5);
	}
	if(MovePts[objNum] == startMovePts)
		SubMov(objNum, 10);
}

/*add the terrain of an object tile to the path resistance of a cell:
 a non-impassable object on impassable water makes it passable (bridge),
 an impassable one blocks it, others add their movement cost (max 0xff)*/
/*C_1E0F_4265*/static PTH_AddObjResist(int16 tile, int16 area_x, int16 area_y) {
	int16 sum;

	if(_ScratchBuf->PTH_resist[area_y][area_x] == 0xff) {
		if(IsTerrainWet(AreaTiles[area_y][area_x]) && !IsTerrainImpass(tile))
			_ScratchBuf->PTH_resist[area_y][area_x] = (TerrainType[tile] >> 4) + 1;
	} else if(IsTerrainImpass(tile)) {
		_ScratchBuf->PTH_resist[area_y][area_x] = 0xff;
	} else if(!IsTerrainWall(tile)) {
		sum = _ScratchBuf->PTH_resist[area_y][area_x] + (TerrainType[tile] >> 4);
		if(sum > 0xff)
			sum = 0xff;
		_ScratchBuf->PTH_resist[area_y][area_x] = sum;
	}
}

/*fill PTH_resist for the 40x40 area: 1 + movement cost of the base tile
 (0xff if impassable), then objects: unlocked doors and passthroughs cost 1
 more, locked doors block, doors also block the square beside them (the
 frame), other objects (all their tiles for multi-tile ones) go through
 PTH_AddObjResist*/
static /*C_1E0F_436A*/__ComputeResistance(int16 unusedObj) {
	int16 area_y, area_x;
	int16 row, col, objNum, tile;
	int16 origin_x, origin_y, objTyp, tileFlags;

	origin_x = (MapX - 0x10) & 0x3f8;
	origin_y = (MapY - 0x10) & 0x3f8;

	for(row = 0; row < AREA_H; row ++) {
		for(col = 0; col < AREA_W; col ++) {
			tile = AreaTiles[row][col];
			if(IsTerrainImpass(tile))
				_ScratchBuf->PTH_resist[row][col] = 0xff;
			else
				_ScratchBuf->PTH_resist[row][col] = (TerrainType[tile] >> 4) + 1;
		}
	}

	for(objNum = SearchArea(origin_x, origin_y, origin_x + (AREA_W-1), origin_y + (AREA_H-1)); objNum >= 0; objNum = NextArea()) {
		area_x = GetX(objNum) - origin_x;
		area_y = GetY(objNum) - origin_y;
		objTyp = GetType(objNum);
		tileFlags = TileFlag[TILE_FRAME(objNum)];
		if(objTyp >= OBJ_OAKEN_DOOR && objTyp <= OBJ_STEEL_DOOR) {/*doors*/
			if(GetFrame(objNum) < 8 && _ScratchBuf->PTH_resist[area_y][area_x] < 0xff)
				_ScratchBuf->PTH_resist[area_y][area_x] ++;
			else
				_ScratchBuf->PTH_resist[area_y][area_x] = 0xff;
			if(tileFlags & TILE_DOUBLE_HEIGHT) {
				if(area_y)
					_ScratchBuf->PTH_resist[area_y - 1][area_x] = 0xff;
			} else {
				if(area_x)
					_ScratchBuf->PTH_resist[area_y][area_x - 1] = 0xff;
			}
		} else if(objTyp == OBJ_PASSTHROUGH_116 || objTyp == OBJ_PASSTHROUGH_118) {/*passthrough*/
			if(_ScratchBuf->PTH_resist[area_y][area_x] < 0xff)
				_ScratchBuf->PTH_resist[area_y][area_x] ++;
		} else if(objNum >= 0x100) {
			tile = TILE_FRAME(objNum);
			PTH_AddObjResist(tile, area_x, area_y);
			if(tileFlags & TILE_DOUBLE_WIDTH) {
				if(area_x)
					PTH_AddObjResist(tile - 1, area_x - 1, area_y);
				if(tileFlags & TILE_DOUBLE_HEIGHT) {
					if(area_y)
						PTH_AddObjResist(tile - 2, area_x, area_y - 1);
					if(area_x && area_y)
						PTH_AddObjResist(tile - 3, area_x - 1, area_y - 1);
				}
			} else if(tileFlags & TILE_DOUBLE_HEIGHT) {
				if(area_y)
					PTH_AddObjResist(tile - 1, area_x, area_y - 1);
			}
		}
	}
}

/*once per turn: path searches for the NPCs (2..0xdf) waiting in AI_FINDPATH,
 at most 3 teleports and 3 searches per turn while free path slots
 remain. If no search was needed, the NPCs whose path failed (AI_PATH_FAILED) are
 put back to AI_FINDPATH for another try.*/
/*C_1E0F_464A*/PTH_SearchPaths() {
	int16 objNum, noneSearched;
	int16 schedEntry;
	struct coord xyz;

	noneSearched = 1;
	PTH_Index = __NewPathIndex();
	if(PTH_Index == 8)
		return;
	TeleportCount =
	PathSearchCount = 0;
	for(objNum = 2; objNum < 0xe0; objNum ++) {
		if(NPCMode[objNum] == AI_FINDPATH) {
			if(TeleportCount < 3)
				TeleportToSchedule(objNum);
			if(NPCMode[objNum] == AI_FINDPATH && PathSearchCount < 3) {
				noneSearched = 0;
				schedEntry = SchedIndex[objNum] + SchedPointer[objNum];
				xyz = Schedule[schedEntry].xyz;
				PTH_FindPath(objNum, GetCoordX(xyz), GetCoordY(xyz));
				while(PathObject[PTH_Index] && PTH_Index < 8)
					PTH_Index ++;
				if(PTH_Index == 8)
					break;
			}
		}
	}
	if(noneSearched) {
		for(objNum = 2; objNum < 0xe0; objNum ++) {
			if(NPCMode[objNum] == AI_PATH_FAILED) {
				NPCMode[objNum] = AI_FINDPATH;
				if(TeleportCount < 3)
					TeleportToSchedule(objNum);
			}
		}
	}
}

/*combat geometry, once per turn:
 - PartyGravityX/Y: center of the party members not in flank/berserk mode;
 - EnemiesGravityX/Y, EnemiesNum: center/number of the hostiles (Is_ATKPLR)
   within 24 squares (retreating ones only when within 5);
 - along the axis party center -> enemy center: the party's front-most
   fighter (PartyFrontMember) and the two outermost ones on each side (PartyFlankPos,
   PartyFlankNeg), the same sides for the non-party front fighters (EnemyFlankPos,
   EnemyFlankNeg) and the first of them (EnemyFrontFirst). Used by the flank/front A.I.
 Without enemies the "enemy center" is the square ahead of the Avatar.*/
/*C_1E0F_4746*/static ComputeCombatGeometry() {
	int16 i, num;
	int16 enemy_x, enemy_y, maxFwd, partyMaxSide, partyMinSide, foeMaxSide, foeMinSide, fwd, side;
	int32 sum_x, sum_y;

	/*compute party's gravity center?*/
	sum_x =
	sum_y =
	num = 0;
	for(i = 0; i < PartySize; i ++) {
		if(NPCMode[Party[i]] != AI_FLANK && NPCMode[Party[i]] != AI_BERSEK) {
			num ++;
			sum_x += GetX(Party[i]);
			sum_y += GetY(Party[i]);
		}
	}
	if(num) {
		PartyGravityX = ((PartySize >> 1) + sum_x) / num;
		PartyGravityY = ((PartySize >> 1) + sum_y) / num;
	} else {
		PartyGravityX = MapX;
		PartyGravityY = MapY;
	}
	/*compute enemies's gravity center?*/
	num =
	sum_x =
	sum_y = 0;
	for(i = 1; i < 0x100; i ++) {
		if(ObjShapeType[i] && !IsDead(i) && Is_ATKPLR(i)) {
			enemy_x = GetX(i);
			enemy_y = GetY(i);
			if(NPCMode[i] != AI_RETREAT || CLOSE_ENOUGH0(5, enemy_x, enemy_y, MapX, MapY)) {
				if(CLOSE_ENOUGH0_S(24, enemy_x, enemy_y, MapX, MapY)) {
					num ++;
					sum_x += enemy_x;
					sum_y += enemy_y;
				}
			}
		}
	}
	EnemiesNum = num;
	if(num) {
		EnemiesGravityX = ((num >> 1) + sum_x) / num;
		EnemiesGravityY = ((num >> 1) + sum_y) / num;
		/*enemy_x/enemy_y now = axis vector from the party to the enemies*/
		enemy_x = EnemiesGravityX - PartyGravityX;
		enemy_y = EnemiesGravityY - PartyGravityY;
		maxFwd = 0x8000;/*-32768*/

		EnemyFrontFirst =
		EnemyFlankPos =
		EnemyFlankNeg = -1;

		partyMinSide =
		partyMaxSide =
		foeMinSide =
		foeMaxSide = 0;
		for(i = 0; i < 0x100; i ++) {
			if(IsDead(i))
				continue;
			if(NPCMode[i] == AI_FRONT || NPCMode[i] == AI_COMMAND) {
				/*fwd = dot product (how far ahead), side = cross product*/
				fwd = (GetX(i) - PartyGravityX) * enemy_x + (GetY(i) - PartyGravityY) * enemy_y;
				side = (GetX(i) - PartyGravityX) * enemy_y - (GetY(i) - PartyGravityY) * enemy_x;
				if(IsPlrControl(i)) {
					if(fwd > maxFwd) {
						maxFwd = fwd;
						PartyFrontMember = i;
					}
					if(side > partyMaxSide) {
						partyMaxSide = side;
						PartyFlankPos = i;
					}
					if(side < partyMinSide) {
						partyMinSide = side;
						PartyFlankNeg = i;
					}
				} else {
					if(EnemyFrontFirst < 0)
						EnemyFrontFirst = i;
					if(side > foeMaxSide) {
						foeMaxSide = side;
						EnemyFlankPos = i;
					}
					if(side < foeMinSide) {
						foeMinSide = side;
						EnemyFlankNeg = i;
					}
				}
			}
		}
	} else {
		EnemyWarningGiven = 0;
		EnemiesGravityX = MapX + DirIncrX[GetDirection(Party[0])];
		EnemiesGravityY = MapY + DirIncrY[GetDirection(Party[0])];
	}
	/*keep the two centers distinct*/
	if(EnemiesGravityX == PartyGravityX && EnemiesGravityY == PartyGravityY) {
		EnemiesGravityX += DirIncrX[GetDirection(Party[0])];
		EnemiesGravityY += DirIncrY[GetDirection(Party[0])];
	}
}

/*requiem: the 8 note frequencies played when the Avatar dies*/
/*1862*/int16 RequiemNotes[] = {0x12C,0x119,0x12C,0x0FA,0x119,0x0DE,0x0FA,0x0FA};

/*the Avatar died: requiem, "An unending darkness engulfs thee...",
 "KAL LOR" and the party is resurrected (PartyResurrect)*/
/*C_1E0F_4AFA*/static PartyDeath() {
	int16 i;

	if(Active == 0x10)
		return;
	MUS_PlaySong(0);
	CON_printf(DarknessEngulfsMsg);
	for(i = 0; i < 8; i ++)
		OSI_playWavedNote(RequiemNotes[i], 1, 20000, 1, 3);
	MapAnimEnabled = 1;
	ResetMapView();
	OSI_delay(30);
	CON_printf(KalLorMsg);
	PartyResurrect();
}

/*check who the player controls:
 - in a vehicle, actor 0 (the vehicle) is kept healthy;
 - the Avatar (object 1) dead -> PartyDeath;
 - if the active member is asleep/paralyzed/dead/dragged under/charmed,
   control goes to the next able member (in solo mode: back to party mode);
 - out of combat, control goes back to the Avatar as soon as he is able
   (in combat the former active member resumes its combat mode).
 LastActiveIdx receives the new active index (it is never read).*/
/*C_1E0F_4B6A*/CheckActiveMember() {
	int16 i;

	if(Active > PartySize)
		return;
	if(IN_VEHICLE) {
		ClrCursedCharmed(0);
		ClrAilments(0);
		ClrDraggedUnder(0);
		SetAlignment(0, GOOD);
	}
	if(IsDead(1))
		PartyDeath();
	if(SoloMember >= 0) {
		if(IsIncapacitated(Party[Active]) || IsDraggedUnder(Party[Active]) || IsCharmed(Party[Active]))
			SetPartyMode();
	}
	if(SoloMember >= 0)
		return;
	if(
		IsIncapacitated(Party[Active]) ||
		IsDraggedUnder(Party[Active]) ||
		IsCharmed(Party[Active])
	) {
		for(i = 0; i < PartySize; i ++) {
			if(
				IsIncapacitated(Party[i]) ||
				IsDraggedUnder(Party[i]) ||
				IsCharmed(Party[i])
			) continue;
			if(NPCMode[Party[i]] != AI_MOTIONLESS) {
				NPCMode[Party[Active]] = AI_FOLLOW;
				NPCMode[Party[i]] = AI_COMMAND;
				Active =
				LastActiveIdx = i;
				PromptFlag = 1;
				MapX = GetX(Party[i]);
				MapY = GetY(Party[i]);
				MAP_LoadAround();
				break;
			}
		}
	}
	if(Active < PartySize && Active && SoloMember < 0) {
		if(
			IsIncapacitated(Party[0]) ||
			IsDraggedUnder(Party[0]) ||
			IsCharmed(Party[0])
		) return;
		if(InCombat) {
			NPCMode[Party[Active]] = NPCComMode[Party[Active]];
		} else {
			NPCMode[Party[Active]] = AI_FOLLOW;
			Active =
			LastActiveIdx = 0;
			PromptFlag = 1;
			MapX = GetX(Party[0]);
			MapY = GetY(Party[0]);
			MAP_LoadAround();
		}
		NPCMode[Party[0]] = AI_COMMAND;
	}
}

/*the turn scheduler, called after each player command: run the A.I. of all
 the actors of the area until a player-controlled one (AI_COMMAND) has
 to act. The next actor is the one with the highest MovePts/DEXTE ratio
 (one with MovePts >= DEXTE is taken at once); when nobody has points left
 everybody gets DEXTE more points and the clock advances one minute.
 Actors walking off the area are frozen (path NPCs teleported/re-pathed).
 AI_FOLLOW members move only with the leader (MoveFollowers); the Avatar
 can't act while a follower has no points left.*/
/*C_1E0F_4E0A*/RunNPCTurns() {
	int16 i, bestPts;
	int16 area_x, area_y, actor, bestDex, cmp, coordMask;
	
	CheckActiveMember();
	ComputeCombatGeometry();
	PTH_SearchPaths();
	/*in combat-type modes Leader[] is the actor's current target/leader;
	reset it to itself*/
	actor = Party[Active];
	if(NPCMode[actor] > AI_FOLLOW && NPCMode[actor] < AI_PATROL_NS)
		Leader[actor] = actor;
	/*coordinates wrap at 0x400 on the surface, at 0x100 underground*/
	if(MapZ)
		coordMask = 0xff;
	else
		coordMask = 0x3ff;

	do {
		actor = -1;
		bestPts = 0;
		bestDex = 1;

		do {
			area_x = (MapX - 0x10) & 0x3f8;
			area_y = (MapY - 0x10) & 0x3f8;

			for(i = 1; i < PartySize; i ++) {
				if(NPCMode[Party[i]] == AI_FOLLOW && MovePts[Party[i]] <= 0)
					MovePts[Party[0]] = 0;
			}

			for(i = 0; i < 0x100; i ++) {
				if(
					ObjShapeType[i] &&
					NPCMode[i] != AI_MOTIONLESS &&
					!IsParalyzed(i) &&
					(!IsAsleep(i) || NPCMode[i] == AI_SCHEDULE) &&
					!IsDead(i) &&
					MovePts[i] > 0 &&
					GetCoordUse(i) == LOCXYZ && GetZ(i) == MapZ
				) {
					if(
						((GetX(i) - area_x) & coordMask) > (AREA_W-1) ||
						((GetY(i) - area_y) & coordMask) > (AREA_H-1)
					) {
						if(NPCMode[i] >= AI_ONPATH && NPCMode[i] <= AI_PATH_FAILED)
							TeleportToSchedule(i);
						if(NPCMode[i] >= AI_ONPATH && NPCMode[i] < AI_PATH_FAILED) {
							PathObject[Leader[i]] = 0;
							NPCMode[i] = AI_FINDPATH;
						}
						MovePts[i] = 0;
					} else if(NPCMode[i] != AI_FOLLOW) {
						if(NPCMode[i] == AI_SCHEDULE)
							StartScheduleAction(i);
						/*cmp > 0 <=> MovePts[i]/DEXTE[i] > bestPts/bestDex*/
						cmp = MovePts[i] * bestDex - DEXTE[i] * bestPts;
						if(
							MovePts[i] >= DEXTE[i] || cmp > 0 ||
							(cmp == 0 && DEXTE[i] > bestDex)
						) {
							actor = i;
							bestPts = MovePts[i];
							bestDex = DEXTE[i];
						}
						if(MovePts[i] >= DEXTE[i])
							break;
					}
				}
			}

			/*new round: give everybody its dexterity in points (debts are
			carried over) and let a minute pass*/
			if(bestPts <= 0) {
				for(i = 0; i < 0x100; i ++) {
					MovePts[i] = (MovePts[i] < 0)?(MovePts[i] + DEXTE[i]):DEXTE[i];
				}
				PassTime(1);
			}

		} while(bestPts <= 0);

		if(!StruggleDraggedUnder(actor)) {
			if(NPCMode[actor] != AI_COMMAND && NPCMode[actor] != AI_FOLLOW) {
				NPC_TakeTurn(actor);
				CheckActiveMember();
				if(NPCMode[actor] > AI_FOLLOW && NPCMode[actor] < AI_PATROL_NS)
					Leader[actor] = actor;
			}
			FlushViewIfDirty();
		}

	} while(NPCMode[actor] != AI_COMMAND || IsDraggedUnder(actor));

	/*give control to the member whose turn it is*/
	if(IN_VEHICLE)
		actor = 0;
	if(Party[Active] != actor) {
		for(i = 0; i < PartySize; i ++) {
			if(Party[i] == actor) {
				SetActive(i);
				PromptFlag = 1;
				break;
			}
		}
	}
}

/*initialization after a game is loaded/started: no actor with 0 dexterity,
 actor 0 (vehicle) not in a schedule mode, combat geometry computed*/
/*C_1E0F_512A*/InitNPCTracker() {
	int16 i;

	for(i = 0; i < 0x100; i ++) {
		if(DEXTE[i] == 0)
			DEXTE[i] = 1;
	}
	if(NPCMode[0] >= AI_SCHEDULE)
		NPCMode[0] = AI_MOTIONLESS;
	if(SoloMember < 0)
		LastActiveIdx = Active;
	ComputeCombatGeometry();
}

/*NPC update, every hour: for each scheduled NPC (2..0xdf) not in party or
 combat (except retreating ones), find the schedule entry for this hour
 and day and make it walk there (AI_FINDPATH). Schedule entries of NPC n are
 Schedule[SchedPointer[n] .. SchedPointer[n+1]-1]; SchedIndex[n] = current
 entry relative to SchedPointer[n]. Entry time byte: bits 0-4 hour,
 bits 5-7 day of the week (0 = every day). Vigilantes calm down.*/
/*C_1E0F_5165*/UpdateSchedules() {
	int16 npc, entry;
	int16 first, last, when;

	for(npc = 2; npc < 0xe0; npc ++) {
		if(NPCMode[npc] == AI_VIGILANTE)
			NPCMode[npc] = AI_FINDPATH;
		if(ObjShapeType[npc] == 0)
			continue;
		if(NPCMode[npc] < AI_SCHEDULE && NPCMode[npc] != AI_RETREAT)
			continue;
		if(IsPlrControl(npc))
			continue;
		first = SchedPointer[npc];
		last = SchedPointer[npc + 1] - 1;
		if(last < first)
			continue;
		for(entry = last; entry >= first; entry --) {
			when = Schedule[entry].time;
			/*searched backward, so the last matching entry wins*/
			if((when & 0x1f) == Time_H)
			if((when >> 5) == ((Date_D - 1) % 7 + 1) || (when & 0xe0) == 0) {
				SchedIndex[npc] = entry - first;
				NPCMode[npc] = AI_FINDPATH;
				if(GetType(npc) == OBJ_MUSICIAN_SHORT_CAPE && Schedule[entry].action != AI_PLAY) {
					ObjShapeType[npc] = OrigShapeType[npc];
					UpdateMoveFrame(npc, 4);
				}
				break;
			}
		}
	}
}
