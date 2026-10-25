/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	OBJECT.C module -- object list management

	All objects live in parallel arrays indexed by an object number:
	ObjStatus[], ObjPos[] (packed x/y/z or container number), ObjShapeType[]
	(type | frame << 10), Amount[] (quantity | quality << 8) and Link[].
	  0x000-0x0FF  NPCs/monsters (0x00-0xDF fixed NPCs, 0xE0-0xFF temporary
	               monster slots, kept on their own free list FreeMonsterHead/FreeMonsterCount)
	  0x100        head of the main object list (never a real object)
	  0x101-0xBFF  ordinary objects (free list FreeObjHead/FreeObjCount)
	Link[] chains every object that is currently loaded into ONE singly linked
	list starting at Link[0x100]. Map objects are sorted by (y, x) (the 20 low
	bits of struct coord); the contents of an NPC or of a container follow
	their owner immediately in the chain (owner's "subtree"). MapObjPtr[][]
	caches, for every tile of the 40x40 area around the party, the first
	object of the chain at that tile so that searches need not walk the
	whole list (OBJ_FindListPos).

	Only the 2x2 outdoor areas (128x128 tiles each, 8x8 areas on the
	surface) near the party, or the current dungeon level, are kept in the
	list. The rest is stored in "savegame\objblkXY" files (X = 'A'+column,
	Y = 'A'+row; dungeon level z is "objblk<'A'+z-1>i"). Areas left behind
	are written to ".tmp" copies (OBJ_UnloadArea/OBJ_UnloadDungeon) and those are
	renamed over the real files when the game is saved (OBJ_CommitBlocks).

	The module also offers the location/inventory/area iterators
	(FindLoc/NextLoc, FindInv/NextInv, FindInvType/NextInvType,
	SearchArea/NextArea), object creation/deletion/moving, inventory
	give/take helpers, support for multi-object creatures (silver serpent,
	dragon, hydra, 2-tile monsters, ship), and the building of the 11x11
	per-tile object display stacks (ShowObject/ShowObjects).
*/
#include "u6.h"

/*musician was near the party at the previous ShowObjects()*/
/*0654*/static int16 MusicianWasNear = 0;
/*when set, NextInv/NextInvType skip equipped (EQUIP) items*/
/*0656*/int16 InvSkipEquipped = 0;
/*set by FindLoc/NextLoc: which part of a multi-tile object covers the
  searched tile (0 = the object's own tile, 1 = the tile left of/above it,
  2/3 = the upper/upper-left part of a 2x2 tile); callers use
  TILE_FRAME(obj) - LocPartOffset to get the tile actually covering the location*/
/*0658*/int16 LocPartOffset = 0;
/*set while the "Amulet of submission" may be moved (InsertObj refuses it otherwise)*/
/*065A*/static int16 AllowSubmissionMove = 0;
/*[0..63] outdoor areas, [64..68] dungeon levels 1..5: a ".tmp" objblk file
  newer than the saved one exists*/
/*065C*/int16 ObjBlkTmpPending[8*8 + 5] = {0};
/*when set, MoveObj applies terrain effects (ApplyTerrainEffects) to moved NPCs;
  cleared while teleporting the party etc.*/
/*06E6*/int16 MoveAppliesTerrain = 0;
/*ids (yyyxxx) of the up to 4 outdoor areas currently loaded, -1 = free slot*/
/*06E8*/int16 LoadedAreas[4] = {-1, -1, -1, -1};
/*id (yyyxxx) of the top-left area of the loaded 2x2 window, -1 = none*/
/*06F0*/int16 LoadedWindowArea = -1;
/*z level currently loaded (0 = surface), -1 = none*/
/*06F2*/int16 LoadedZ = -1;
/*FindLoc/NextLoc state: current list position, "already jumped to the
  next row" flag and the searched location*/
/*06F4*/static int16 LocCursor = -1;
/*06F6*/static int16 LocOnNextRow = 0;
/*06F8*/static struct coord LocTarget = {0, 0, 0};
/*FindInv/NextInv/FindInvType/NextInvType state: current list position,
  owner, type and quality (-1 = any)*/
/*06FB*/int16 InvCursor = -1;
/*06FD*/int16 InvOwner = -1;
/*06FF*/int16 InvType = -1;
/*0701*/int16 InvQuality = -1;/*Quality*/
/*SearchArea/NextArea state: current list position and end position*/
/*0703*/int16 AreaCursor = -1;
/*0705*/int16 AreaEnd = -1;
/*last content object moved out by DeleteObj (-1 if none); lets a caller
  that is walking Link[] continue from there*/
/*0707*/int16 LastSpilledObj = -1;

/*objblk file names; characters [15] and [16] are patched with the area letters*/
/*0709*/static char __Sav_file[] = "savegame\\objblkaa";
/*071B*/static char __Tmp_file[] = "savegame\\objblkaa.tmp";
/*0731*/static char __Bak_file[] = "savegame\\objblkaa.bak";

/*indexed by direction (0=N 2=E 4=S 6=W): masks out the connection flag
  of that direction (TERRAIN_WALL_N=N, _40=E, _20=S, _10=W)*/
/*0747*/unsigned char DirConnClearMask[] = {~TERRAIN_WALL_N,0,~TERRAIN_WALL_E,0,~TERRAIN_WALL_S,0,~TERRAIN_WALL_W};
/*silver serpent body frame from its connection flags (TerrainType >> 4,
  bit3=N bit2=E bit1=S bit0=W): 1/3/5/7 = tail ending N/E/S/W,
  8 = N-S, 9 = E-W, 0x0A = N-E, 0x0B = E-S, 0x0C = S-W, 0x0D = N-W*/
/*074E*/unsigned char SerpentBodyFrame[] = {0x00,0x07,0x05,0x0C,0x03,0x09,0x0B,0x00,0x01,0x0D,0x08,0x00,0x0A,0x00,0x00,0x00};
/*indexed by direction (0=N 2=E 4=S 6=W): connection flag of that direction*/
/*075E*/unsigned char DirConnFlag[] = {TERRAIN_WALL_N,0,TERRAIN_WALL_E,0,TERRAIN_WALL_S,0,TERRAIN_WALL_W};

/*uncalled?*/
/*is location (x, y, z) currently loaded in the object list?
  (dungeon: z is the current level; surface: its area is one of LoadedAreas[])*/
/*__1184_000E*/OBJ_IsLocLoaded(int16 x, int16 y, int16 z) {
	int16 isLoaded, i;
	int16 mapId;

	isLoaded = 0;
	if(z) {
		isLoaded = (z == MapZ);
	} else if(MapZ == 0) {
		mapId = (x >> 7) + ((y >> 4) & 0x38);
		for(i = 0; i < 4; i ++)
			if(LoadedAreas[i] == mapId) {
				isLoaded = 1;
				break;
			}
	}
	return isLoaded;
}

/*frame number of a creature of type objTyp facing direction dir (0=N 2=E
  4=S 6=W), i.e. dir times the number of frames per direction, plus the
  "standing" frame for the walking animations*/
/*C_1184_0073*/OBJ_MakeDirFrame(int16 objTyp, int16 dir) {
	if(
		objTyp == OBJ_CHAIR || objTyp == OBJ_CANNON ||
		objTyp == OBJ_MOUSE || objTyp == OBJ_RABBIT ||
		objTyp == OBJ_CAT
	) return dir >> 1;

	if(
		objTyp == OBJ_SHEEP || objTyp == OBJ_DOG || objTyp == OBJ_DEER || objTyp == OBJ_WOLF ||
		objTyp == OBJ_SNAKE || objTyp == OBJ_GIANT_SPIDER || objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_SILVER_SERPENT ||
		(objTyp >= OBJ_DAEMON && objTyp <= OBJ_MONGBAT) || objTyp == OBJ_GIANT_RAT || objTyp == OBJ_SEA_SERPENT ||
		objTyp == OBJ_MUSICIAN_SHORT_CAPE
	) return dir;

	if(objTyp == OBJ_WINGED_GARGOYLE)
		return  ((dir << 1) + (dir << 2)) + 7;

	if(objTyp == OBJ_CYCLOPS)
		return (dir << 2) + 3;

	if(objTyp == OBJ_GARGOYLE)
		return (dir >> 1) + dir + 1;

	if(
		(objTyp >= OBJ_FIGHTER && objTyp <= OBJ_WOMAN) ||
		(objTyp >= OBJ_LORD_BRITISH && objTyp <= OBJ_AVATAR)
	) return (dir << 1) + 1;

	return 0;
}

/*for a 2-tile creature/dragon/ship part, find the other part it is joined
  to (its TerrainType connection flag gives the side; the other part must
  carry the opposite flag); for a silver serpent body segment (>= 0x100)
  return the head (stored in its quantity). Returns objNum otherwise.*/
/*C_1184_0153*/OBJ_getHead(int16 objNum) {
	int16 i;
	int16 x, y, connFlag, backFlag;
	int16 objTyp, ret;

	if(objNum < 0)
		return -1;
	ret = objNum;
	objTyp = GetType(ret);
	if(objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_DRAGON || objTyp == OBJ_SHIP) {
		x = GetX(objNum);
		y = GetY(objNum);
		connFlag = TerrainType[TILE_FRAME(objNum)] & 0xf0;
		if(connFlag == TERRAIN_WALL_N) {
			y --;
			backFlag = TERRAIN_WALL_S;
		} else if(connFlag == TERRAIN_WALL_E) {
			x ++;
			backFlag = TERRAIN_WALL_W;
		} else if(connFlag == TERRAIN_WALL_S) {
			y ++;
			backFlag = TERRAIN_WALL_N;
		} else if(connFlag == TERRAIN_WALL_W) {
			x --;
			backFlag = TERRAIN_WALL_E;
		} else {
			return objNum;
		}
		for(i = FindLoc(x, y, MapZ); i >= 0; i = NextLoc()) {
			if(GetType(i) == objTyp && (TerrainType[TILE_FRAME(i)] & backFlag)) {
				ret = i;
				break;
			}
		}
	} else if(
		objTyp == OBJ_SILVER_SERPENT &&
		objNum >= 0x100 &&
		GetType(GetQuan(objNum)) == OBJ_SILVER_SERPENT
	) {
		ret = GetQuan(objNum);
	}
	return ret;
}

/*returns the next free object slot if at least num slots are free, else -1*/
/*C_1184_02E3*/static GetFreeObjSlot(int16 num) {
	if(FreeObjCount >= num)
		return FreeObjHead;
	return -1;
}

/*locate (x, y) in the sorted Link[] chain (z is ignored).
  wantPrev == 0: returns the first object whose key (y, x) is >= the
                 searched one (-1 if none; inside the 40x40 area only the
                 MapObjPtr[] cache is used);
  wantPrev != 0: returns the node after which an object at (x, y) must be
                 inserted (0x100 = list head), i.e. new objects go in front
                 of the objects already on that tile.*/
/*C_1184_02FA*/static OBJ_FindListPos(int16 x, int16 y, int16 wantPrev) {
	int16 i, j;
	struct coord xyz;
	int16 cur, prev, next, objY;
	int16 relX, relY;

	relX = (x - AreaX) & 0x3ff;
	relY = (y - AreaY) & 0x3ff;
	if(relX < AREA_W && relY < AREA_H) {
		j = AREA_W * relY + relX;
		if(wantPrev) {
			/*first non null object before j*/
			for(i = j - 1; i >= 0 && ((int16 *)MapObjPtr)[i] == -1; i --);
			/*first non null object after j*/
			for(; j < 0x63f && ((int16 *)MapObjPtr)[j] == -1; j ++);

			if(i >= 0)
				i = ((int16 *)MapObjPtr)[i];
			else
				i = 0x100;

			for(j = ((int16 *)MapObjPtr)[j]; (next = Link[i]) >= 0 && next != j; i = next) {
				if(GetCoordUse(next) == LOCXYZ) {
					objY = GetY(next);
					if(
						(objY > y) ||
						((objY == y) && (GetX(next) >= x))
					) break;
				}
			}
		} else {
			i = ((int16 *)MapObjPtr)[j];
			if(i < 0) {
				for(; j < 0x63f && ((int16 *)MapObjPtr)[j] == -1; j ++);
				i = ((int16 *)MapObjPtr)[j];
			}
		}
	} else {
		SetCoordXYZ(xyz, x, y, 0);
		if(
			((AreaY + (AREA_H-1)) & 0x3ff) < y || (
			((AreaY + (AREA_W-1)) & 0x3ff) == y/*AREA_W for AREA_H: same value*/ && ((AreaX + (AREA_W-1)) & 0x3ff) < x
		)) {
			for(i = 0x63f; i >= 0 && ((int16 *)MapObjPtr)[i] == -1; i --);
			if(i < 0)
				prev = 0x100;
			else
				prev = ((int16 *)MapObjPtr)[i];
		} else {
			prev = 0x100;
		}
		for(cur = Link[prev]; cur >= 0; prev = cur, cur = Link[cur]) {
			if(GetCoordUse(cur) == LOCXYZ) {
				/*compares the 20 low bits of the coordinates, i.e. the key (y << 10 | x)
				(raw[2] & 0xf are the 4 high bits of y):
				i.e if((ObjPos[cur].24bit & 0xfffff) >= xyz.24bit) break;*/
				if((ObjPos[cur].raw[2] & 0xf) > xyz.raw[2])
					break;
				if((ObjPos[cur].raw[2] & 0xf) == xyz.raw[2]) {
					if(ObjPos[cur].raw[1] > xyz.raw[1])
						break;
					if(ObjPos[cur].raw[1] == xyz.raw[1] && ObjPos[cur].raw[0] >= xyz.raw[0])
						break;
				}
			}
		}
		i = prev;
		if(!wantPrev)
			i = Link[i];
	}
	return i;
}

/*next object at the location set by FindLoc (same z), also returning the
  parts of double-width/double-height objects whose tile covers it, i.e.
  objects at x+1, y+1 or x+1/y+1 (LocPartOffset tells which). Since the list is
  sorted, the scan walks row y from x, then jumps once to row y+1.
  Returns -1 at the end.*/
/*C_1184_053F*/NextLoc() {
	struct coord *pos;
	int16 found;
	struct coord locDown, locRight, locDownRight;
	int16 tileNum, locX, locY, locZ;
	
	found = -1;
	locX = GetCoordX(LocTarget);
	locY = GetCoordY(LocTarget);
	locZ = GetCoordZ(LocTarget);

	locDownRight =
	locDown =
	locRight = LocTarget;

	SetCoordX(locRight, locX + 1);
	SetCoordY(locDown, locY + 1);
	SetCoordX(locDownRight, locX + 1);
	SetCoordY(locDownRight, locY + 1);
	while(LocCursor >= 0 && found < 0) {
		if(GetCoordUse(LocCursor) == LOCXYZ && GetZ(LocCursor) == locZ) {
			pos = &(ObjPos[LocCursor]);
			tileNum = TILE_FRAME(LocCursor);
			if(
				pos->raw[0] == LocTarget.raw[0] &&
				pos->raw[1] == LocTarget.raw[1] &&
				pos->raw[2] == LocTarget.raw[2]
			) {
				found = LocCursor;
			} else if(
				pos->raw[0] == locRight.raw[0] &&
				pos->raw[1] == locRight.raw[1] &&
				pos->raw[2] == locRight.raw[2]
			) {
				if(IsTileDoubleH(tileNum)) {
					found = LocCursor;
					LocPartOffset = 1;
				}
			} else if(
				pos->raw[0] == locDown.raw[0] &&
				pos->raw[1] == locDown.raw[1] &&
				pos->raw[2] == locDown.raw[2]
			) {
				if(IsTileDoubleV(tileNum)) {
					found = LocCursor;
					LocPartOffset = IsTileDoubleH(tileNum)?2:1;
				}
			} else if(
				pos->raw[0] == locDownRight.raw[0] &&
				pos->raw[1] == locDownRight.raw[1] &&
				pos->raw[2] == locDownRight.raw[2]
			) {
				if((TileFlag[tileNum] & (TILE_DOUBLE_WIDTH|TILE_DOUBLE_HEIGHT)) == (TILE_DOUBLE_WIDTH|TILE_DOUBLE_HEIGHT)) {
					found = LocCursor;
					LocPartOffset = 3;
				}
			} else if(!LocOnNextRow) {
				/*past the location on row y: continue once on row y+1*/
				LocCursor = OBJ_FindListPos(locX, locY + 1, 0);
				LocOnNextRow = 1;
				continue;
			} else {
				LocCursor = -1;
				break;
			}
		}
		LocCursor = Link[LocCursor];
	}
	return found;
}

/*start iterating over the objects at (x, y, z); returns the first one or -1*/
/*C_1184_075A*/FindLoc(int16 x, int16 y, int16 z) {
	if(z) {
		x &= 0xff;
		y &= 0xff;
	}
	LocPartOffset =
	LocOnNextRow = 0;
	LocCursor = OBJ_FindListPos(x, y, 0);
	SetCoordXYZ(LocTarget, x, y, z);
	return NextLoc();
}

/*find an object of type objTyp exactly at (x, y, MapZ) (multi-tile
  parts are not considered); -1 if none*/
/*C_1184_07A7*/OBJ_FindTypeAt(int16 objTyp, int16 x, int16 y) {
	int16 obj, found;

	found = -1;
	for(
		obj = OBJ_FindListPos(x, y, 0);
		obj >= 0 && (
			GetCoordUse(obj) != LOCXYZ ||
			(GetX(obj) == x && GetY(obj) == y)
		);
		obj = Link[obj]
	) {
		if(GetCoordUse(obj) == LOCXYZ && GetZ(obj) == MapZ && GetType(obj) == objTyp) {
			found = obj;
			break;
		}
	}

	return found;
}

/*start iterating over the direct contents of objNum (objects whose
  GetAssoc() is objNum); they directly follow it in the chain. Returns
  the first one or -1.*/
/*C_1184_0840*/FindInv(int16 objNum) {
	int16 first, ret;

	ret = -1;
	first = Link[objNum];
	if(GetCoordUse(first) != LOCXYZ && GetAssoc(first) == objNum) {
		ret = first;
		InvCursor = Link[first];
		InvOwner = objNum;
		InvType = GetType(ret);
		InvQuality = -1;
	}
	return ret;
}

/*next direct content of the object given to FindInv (skips the contents
  of sub-containers; skips equipped items if InvSkipEquipped is set); -1 at the end*/
/*C_1184_0899*/NextInv() {
	int16 ret;

	for(ret = -1; InvCursor >= 0 && ret < 0; InvCursor = Link[InvCursor]) {
		if(GetCoordUse(InvCursor) == LOCXYZ) {
			InvCursor = -1;
			break;
		}
		if(GetAssoc(InvCursor) == InvOwner) {
			if(InvSkipEquipped == 0 || GetCoordUse(InvCursor) == INVEN || GetCoordUse(InvCursor) == CONTAINED)
				ret = InvCursor;
		}
	}
	return ret;
}

/*next map object inside the rectangle given to SearchArea; -1 at the end*/
/*C_1184_090D*/NextArea() {
	int16 ret, x;
	int16 y, z;

	ret = -1;
	if(AreaCursor >= 0) {
		for(; AreaCursor != AreaEnd && AreaCursor >= 0 && ret < 0; AreaCursor = Link[AreaCursor]) {
			if(GetCoordUse(AreaCursor) == LOCXYZ) {
				x = GetX(AreaCursor) & MapCoordMask;
				y = GetY(AreaCursor) & MapCoordMask;
				z = GetZ(AreaCursor);
				if(
					x >= SearchRect[0] && x <= SearchRect[2] &&
					y >= SearchRect[1] && y <= SearchRect[3] &&
					z == SearchZ
				) ret = AreaCursor;
			}
		}
		if(AreaCursor == AreaEnd)
			AreaCursor = AreaEnd = -1;
	}
	return ret;
}

/*start iterating over the map objects inside the rectangle
  (x1, y1)-(x2, y2) at level MapZ; returns the first one or -1*/
/*C_1184_09DE*/SearchArea(int16 x1, int16 y1, int16 x2, int16 y2) {
	if(x1 < 0)
		x1 = 0;
	if(y1 < 0)
		y1 = 0;
	SearchRect[0] = x1;
	SearchRect[2] = x2;
	SearchRect[1] = y1;
	SearchRect[3] = y2;
	SearchZ = MapZ;
	AreaCursor = OBJ_FindListPos(x1, y1, 0);
	AreaEnd = OBJ_FindListPos(x2 + 1, y2, 0);
	return NextArea();
}

/*next direct content of the FindInvType owner with the wanted type
  (and quality, unless -1); -1 at the end*/
/*C_1184_0A37*/NextInvType() {
	int16 ret;

	for(ret = -1; InvCursor >= 0 && ret < 0; InvCursor = Link[InvCursor]) {
		if(GetCoordUse(InvCursor) == LOCXYZ) {
			InvCursor = -1;
			break;
		}
		if(GetAssoc(InvCursor) == InvOwner && GetType(InvCursor) == InvType) {
			if(
				(InvQuality < 0 || GetQual(InvCursor) == InvQuality) &&
				(InvSkipEquipped == 0 || GetCoordUse(InvCursor) == INVEN || GetCoordUse(InvCursor) == CONTAINED)
			) ret = InvCursor;
		}
	}
	return ret;
}


/*first direct content of objNum of type objType and the given quality
  (-1 = any quality); -1 if none*/
/*C_1184_0AE2*/FindInvType(int16 objNum, int16 objType, int16 quality) {
	int16 first, ret;

	if(objNum < 0)
		return -1;
	ret = -1;
	first = Link[objNum];
	if(GetCoordUse(first) != LOCXYZ && GetAssoc(first) == objNum) {
		ret = first;
		InvCursor = first;
		InvOwner = objNum;
		InvType = objType;
		InvQuality = quality;
		ret = NextInvType();
	}
	return ret;
}

/*find a living NPC/monster (slot startNum..0xFF, at least 1) standing at
  (x, y, MapZ); -1 if none*/
/*C_1184_0B3D*/OBJ_FindNPCAt(int16 startNum, int16 x, int16 y) {
	int16 i, ret;

	ret = -1;
	if(startNum < 1)
		startNum = 1;
	for(i = startNum; i < 0x100; i ++) {
		if(
			ObjShapeType[i] &&
			!IsDead(i) &&
			GetX(i) == x &&
			GetY(i) == y &&
			GetZ(i) == MapZ
		) {
			ret = i;
			break;
		}
	}
	return ret;
}

/*refresh the frame of every slime (temporary monster slots): bit mask
  of the neighbouring slimes, 1=N 2=E 4=S 8=W, so that they merge visually*/
/*C_1184_0BC1*/OBJ_UpdateSlimeFrames() {
	int16 slime, other;
	int16 dir, x, y, newX, newY, neighbours, dirBit;

	for(slime = 0xe0; slime < 0x100; slime ++) {
		if(GetType(slime) == OBJ_SLIME && !IsDead(slime)) {
			x = GetX(slime);
			y = GetY(slime);
			dirBit = 1;
			neighbours = 0;
			for(dir = 0; dir < 8; dirBit <<= 1, dir +=2) {
				newX = x + DirIncrX[dir];
				newY = y + DirIncrY[dir];
				for(
					other = OBJ_FindNPCAt(0xe0, newX, newY);
					other >= 0;
					other = OBJ_FindNPCAt(other + 1, newX, newY)
				) {
					if(GetType(other) == OBJ_SLIME && !IsDead(other)) {
						neighbours |= dirBit;
						break;
					}
				}
			}
			SetFrame(slime, neighbours);
		}
	}
}

/*silver serpent body. The segments are objects (>= 0x100) of type
  OBJ_SILVER_SERPENT whose quantity is the head and whose quality is the
  segment number (head's quality + 1, + 2, ...). Connections between
  segments are given by the TerrainType flags of their frame.
  deleteBody == 0: the head (objNum) has just moved away from
                   (oldX, oldY): renumber the segments, then move the tail
                   segment into the vacated tile (snake trick);
  deleteBody != 0: delete all the segments.*/
/*C_1184_0CBE*/static OBJ_MoveSerpentBody(int16 objNum, int16 oldX, int16 oldY, int16 deleteBody) {
	int16 part;
	int16 partX, partY, headX, headY, headZ;
	int16 partIndex;
	int16 tail, beforeTail, vacatedConn, backDir, headFrame;
	int16 partConn, partFrame;

	beforeTail = tail = objNum;
	partX = oldX;
	partY = oldY;
	vacatedConn = TerrainType[TILE_FRAME(objNum)];
	if(vacatedConn & TERRAIN_WALL_N)
		backDir = 4;
	else if(vacatedConn & TERRAIN_WALL_E)
		backDir = 6;
	else if(vacatedConn & TERRAIN_WALL_S)
		backDir = 0;
	else if(vacatedConn & TERRAIN_WALL_W)
		backDir = 2;
	headFrame = backDir;
	headX = GetX(objNum);
	headY = GetY(objNum);
	headZ = GetZ(objNum);
	if(headX > oldX) {
		headFrame = 2;
		vacatedConn |= TERRAIN_WALL_E;
	} else if(headX < oldX)  {
		headFrame = 6;
		vacatedConn |= TERRAIN_WALL_W;
	} else if(headY > oldY) {
		headFrame = 4;
		vacatedConn |= TERRAIN_WALL_S;
	} else if(headY < oldY) {
		headFrame = 0;
		vacatedConn |= TERRAIN_WALL_N;
	}
	SetFrame(objNum, headFrame);
	/*follow the body from the head to the tail*/
	partIndex = GetQual(objNum) + 1;
	do {
		for(
			part = FindLoc(partX - DirIncrX[backDir], partY - DirIncrY[backDir], headZ);
			part >= 0;
			part = NextLoc()
		) {
			if(
				part >= 0x100 &&
				GetType(part) == OBJ_SILVER_SERPENT &&
				GetQual(part) == partIndex &&
				GetQuan(part) == objNum
			) {
				beforeTail = tail;
				tail = part;
				partFrame = GetFrame(part);
				partIndex ++;
				SetQual(part, partIndex);
				partX = GetX(part);
				partY = GetY(part);
				headFrame = backDir;
				partConn = TerrainType[TILE_FRAME(part)] & DirConnClearMask[backDir];
				if(partConn & TERRAIN_WALL_N)
					backDir = 4;
				else if(partConn & TERRAIN_WALL_E)
					backDir = 6;
				else if(partConn & TERRAIN_WALL_S)
					backDir = 0;
				else if(partConn & TERRAIN_WALL_W)
					backDir = 2;
				if(deleteBody)
					DeleteObj(part);
				break;
			}
		}
	} while(part >= 0 && (partFrame & 0xf) >= 8);/*frames < 8 are tail ends*/
	if(deleteBody)
		return;

	/*the segment before the tail becomes the new tail end*/
	if(beforeTail != objNum) {
		partConn = TerrainType[TILE_FRAME(beforeTail)];
		partConn &= DirConnClearMask[(GetFrame(tail) & 0xf) ^ 5];
		if(partConn & TERRAIN_WALL_N)
			SetFrame(beforeTail, 1);
		else if(partConn & TERRAIN_WALL_E)
			SetFrame(beforeTail, 3);
		else if(partConn & TERRAIN_WALL_S)
			SetFrame(beforeTail, 5);
		else if(partConn & TERRAIN_WALL_W)
			SetFrame(beforeTail, 7);
	}
	if(tail == objNum)
		return;
	/*move the old tail into the tile left by the head*/
	if(beforeTail != objNum)
		SetFrame(tail, SerpentBodyFrame[vacatedConn >> 4]);
	else
		SetFrame(tail, GetFrame(objNum) + 1);
	MoveObj(tail, oldX, oldY, headZ);
	SetQual(tail, GetQual(objNum) + 1);
}

/*find the dragon part next to (x, y) in direction dir whose frame
  matches a dragon facing (headFrame & 6) (table DragonPartFrames); -1 if none*/
/*C_1184_10F1*/OBJ_FindDragonPart(int16 x, int16 y, int16 z, int16 dir, int16 headFrame) {
	int16 part;
	int16 partFrame;

	partFrame = DragonPartFrames[(dir >> 1) + ((headFrame & 6) << 1)];
	for(
		part = FindLoc(x + DirIncrX[dir], y + DirIncrY[dir], z);
		part >= 0;
		part = NextLoc()
	) {
		if(GetType(part) == OBJ_DRAGON && (GetFrame(part) & 0x3e) == partFrame)
			return part;
	}
	return -1;
}

/*multi-object creatures (2-tile monsters >= OBJ_GIANT_SCORPION, dragon,
  hydra, ship): objNum has moved from (oldX, oldY); move its other parts
  along, or delete them if deleteParts is set. The parts are found around
  the old position in every direction (hydra) or in the directions given
  by the TerrainType connection flags.*/
/*C_1184_116D*/OBJ_MoveCreatureParts(int16 objNum, int16 oldX, int16 oldY, int16 deleteParts) {
	int16 dir, part;
	int16 newX, newY, z, partType, connFlags, partConn, facing;

	newX = GetX(objNum);
	newY = GetY(objNum);
	z = GetZ(objNum);
	connFlags = TerrainType[TILE_FRAME(objNum)];
	partType = GetType(objNum);
	if(partType == OBJ_HYDRA)
		partType = OBJ_HYDRA_BODY;
	if(partType == OBJ_DRAGON)
		facing = (GetFrame(objNum) & 6) >> 1;
	for(dir = 0; dir < 8; dir ++) {
		if(
			partType == OBJ_HYDRA_BODY ||
			((dir & 1) == 0 && (connFlags & DirConnFlag[dir]))
		) {
			if(partType == OBJ_DRAGON) {
				part = OBJ_FindDragonPart(oldX, oldY, z, dir, facing << 1);
			} else {
				for(
					part = FindLoc(DirIncrX[dir] + oldX, DirIncrY[dir] + oldY, z);
					part >= 0;
					part = NextLoc()
				) {
					partConn = TerrainType[TILE_FRAME(part)];
					if(part >= 0x100 && GetType(part) == partType) {
						if(partType == OBJ_HYDRA_BODY && (GetFrame(part) >> 2) == dir)
							break;
						if(partType != OBJ_HYDRA_BODY && (partConn & DirConnFlag[dir ^ 4]))
							break;
					}
				}
			}
			if(part < 0)
				continue;
			if(deleteParts)
				DeleteObj(part);
			else
				MoveObj(part, DirIncrX[dir] + newX, DirIncrY[dir] + newY, z);
		}
	}
}

/*create an object from a status and a struct coord (map location or
  container depending on the status); does not return the new object*/
/*C_1184_134C*/AddObj(int16 status, struct coord location, int16 shapeType, int16 shapeNum, int16 amount) {
	if((status & 0x18) == LOCXYZ)
		AddMapObj(shapeType | (shapeNum << 10), status, amount, GetCoordX(location), GetCoordY(location), GetCoordZ(location));
	else
		AddInvObj(shapeType | (shapeNum << 10), status, amount, GetCoordAssoc(location));
}

/*create an object on the map at (x, y, z) (in front of the objects
  already there); returns the new object or -1 if no slot is free*/
/*C_1184_13A7*/AddMapObj(int16 shapeType, int16 status, int16 amount, int16 x, int16 y, int16 z) {
	int16 objNum;
	int16 prev, posMask;
	
	posMask = z?0xff:0x3ff;
	x &= posMask;
	y &= posMask;
	objNum = GetFreeObjSlot(1);
	if(objNum >= 0) {
		prev = OBJ_FindListPos(x, y, 1);
		FreeObjHead = Link[FreeObjHead];
		FreeObjCount --;
		ObjStatus[objNum] = status | LOCAL;
		SetXYZ(objNum, x, y, z);
		ObjShapeType[objNum] = shapeType;
		Amount[objNum] = amount;
		Link[objNum] = Link[prev];
		Link[prev] = objNum;
		x = (x - AreaX) & 0x3ff;
		y = (y - AreaY) & 0x3ff;
		if(x < AREA_W && y < AREA_H)
			MapObjPtr[y][x] = objNum;
	}
	return objNum;
}

/*create an object inside owner (an NPC or a container); returns the new
  object or -1 if no slot is free*/
/*C_1184_14A8*/AddInvObj(int16 shapeType, int16 status, int16 amount, int16 owner) {
	int16 objNum;

	if(StatusDisplay == CMD_STAT_EQUIP && Party[StatusMember] == owner)
		StatusDirty = 1;
	objNum = GetFreeObjSlot(1);
	if(objNum >= 0) {
		FreeObjHead = Link[FreeObjHead];
		FreeObjCount --;
		ObjStatus[objNum] = status;
		if(shapeType != OBJ_GARGOYLE_LENS_VIOLET)
			SetLocal(objNum);
		SetAssoc(objNum, owner);
		ObjShapeType[objNum] = shapeType;
		Amount[objNum] = amount;
		Link[objNum] = Link[owner];
		Link[owner] = objNum;
	}
	return objNum;
}

/*create a temporary monster of type objType at (x, y, z) in one of the
  slots 0xE0-0xFF; returns it or -1 if none is free*/
/*C_1184_1540*/AddMonster(int16 objType, int16 x, int16 y, int16 z) {
	int16 objNum;
	int16 prev, posMask;

	posMask = z?0xff:0x3ff;
	x &= posMask;
	y &= posMask;
	objNum = -1;
	if(FreeMonsterCount)
		objNum = FreeMonsterHead;
	if(objNum >= 0) {
		prev = OBJ_FindListPos(x, y, 1);
		FreeMonsterHead = Link[FreeMonsterHead];
		FreeMonsterCount --;
		ObjStatus[objNum] = LOCAL | OWNED;
		NPCStatus[objNum] = 0;
		NPCMode[objNum] = AI_ASSAULT;
		SetXYZ(objNum, x, y, z);
		ObjShapeType[objNum] =
		OrigShapeType[objNum] = TypeFrame(objType, 0);
		Amount[objNum] = 1;
		Link[objNum] = Link[prev];
		Link[prev] = objNum;
		x = (x - AreaX) & 0x3ff;
		y = (y - AreaY) & 0x3ff;
		if(x < AREA_W && y < AREA_H)
			MapObjPtr[y][x] = objNum;
		if(objType == OBJ_SLIME)
			OBJ_UpdateSlimeFrames();
	}
	return objNum;
}

/*the node preceding objNum in the Link[] chain (-1 if not found)*/
/*C_1184_1665*/static OBJ_FindPrevLink(int16 objNum) {
	int16 prev;

	if(GetCoordUse(objNum) == LOCXYZ)
		prev = OBJ_FindListPos(GetX(objNum), GetY(objNum), 1);
	else
		prev = GetAssoc(objNum);
	for(; prev >= 0 && Link[prev] != objNum; prev = Link[prev]);

	return prev;
}

/*objNum (at x, y, z) is being unlinked and nextObj now follows its
  predecessor: fix the MapObjPtr[] entry of the tile if it pointed to
  objNum (nextObj, or -1 if nextObj is on another tile)*/
/*C_1184_16D5*/static OBJ_FixMapObjPtr(int16 x, int16 y, int16 z, int16 objNum, int16 nextObj) {
	int16 relX, relY;
	int16 posMask;

	if(GetCoordUse(objNum) == LOCXYZ) {
		posMask = z?0xff:0x3ff;
		relX = (x - AreaX) & posMask;
		relY = (y - AreaY) & posMask;
		if(relX < AREA_W && relY < AREA_H && MapObjPtr[relY][relX] == objNum) {
			if(GetCoordUse(nextObj) == LOCXYZ && (GetX(nextObj) != x || GetY(nextObj) != y))
				MapObjPtr[relY][relX] = -1;
			else
				MapObjPtr[relY][relX] = nextObj;
		}
	}
}

/*delete an object: its contents are dropped where it was (or put in its
  owner), extra parts of big creatures are deleted too, and the slot goes
  back to the monster or object free list. Ships can't be deleted.*/
/*C_1184_17A5*/DeleteObj(int16 objNum) {
	int16 objTyp;
	int16 content, prev;

	if(objNum < 0)
		return;
	LastSpilledObj = -1;
	if(StatusDisplay == CMD_STAT_EQUIP && OBJ_IsInside(objNum, Party[StatusMember]))
		StatusDirty = 1;
	objTyp = GetType(objNum);
	if(objTyp == OBJ_SHIP)
		return;
	if(objNum < 0x100) {
		if(objTyp == OBJ_HYDRA || objTyp == OBJ_SHIP || objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_DRAGON)
			OBJ_MoveCreatureParts(objNum, GetX(objNum), GetY(objNum), 1);
		else if(objTyp == OBJ_SILVER_SERPENT)
			OBJ_MoveSerpentBody(objNum, GetX(objNum), GetY(objNum), 1);
	}
	/*spill the contents*/
	while((content = FindInv(objNum)) >= 0) {
		LastSpilledObj = content;
		if(GetCoordUse(objNum) == LOCXYZ)
			MoveObj(content, GetX(objNum), GetY(objNum), GetZ(objNum));
		else
			InsertObj(content, GetAssoc(objNum), GetCoordUse(objNum));
	}
	ObjShapeType[objNum] = 0;
	prev = OBJ_FindPrevLink(objNum);
	if(prev >= 0) {
		Link[prev] = Link[objNum];
		if(objNum < 0x100) {
			Link[objNum] = FreeMonsterHead;
			FreeMonsterHead = objNum;
			FreeMonsterCount ++;
		} else {
			Link[objNum] = FreeObjHead;
			FreeObjHead = objNum;
			FreeObjCount ++;
		}
	}
	OBJ_FixMapObjPtr(GetX(objNum), GetY(objNum), GetZ(objNum), objNum, Link[prev]);
	if(objTyp == OBJ_SLIME)
		OBJ_UpdateSlimeFrames();
}

/*the loaded area is now centered on (x, y, z): delete the temporary
  (LOCAL) monsters and objects farther than 20 tiles away, and let the
  eggs there hatch again*/
/*C_1184_19AA*/OBJ_PurgeFarLocals(int16 x, int16 y, int16 z) {
	int16 obj;
	int16 objTyp, next, objX, objY, objZ;

	x &= MapCoordMask;
	y &= MapCoordMask;
	/*delete local objects*/
	for(obj = 0xe0; obj < 0x100; obj ++) {
		if(ObjShapeType[obj]) {
			objX = GetX(obj);
			objY = GetY(obj);
			objZ = GetZ(obj);
			if(
				x - 20 > objX || x + 20 <= objX ||
				y - 20 > objY || y + 20 <= objY ||
				objZ != z
			) {
				if(IsLocal(obj))
					DeleteObj(obj);
			}
		}
	}
	/*reset eggs*/
	for(obj = Link[0x100]; obj >= 0; obj = next) {
		next = Link[obj];
		if(GetCoordUse(obj) == LOCXYZ) {
			objX = GetX(obj);
			objY = GetY(obj);
			objZ = GetZ(obj);
			if(
				x - 20 > objX || x + 20 <= objX ||
				y - 20 > objY || y + 20 <= objY ||
				objZ != z
			) {
				objTyp = GetType(obj);
				if(IsLocal(obj) || ObjShapeType[obj] == 0) {
					DeleteObj(obj);
					if(LastSpilledObj >= 0)
						next = LastSpilledObj;
				} else if(objTyp == OBJ_EGG && IsHatched(obj)) {
					ClrHatched(obj);
				}
			}
		}
	}
}

/*an NPC/monster dies: create its dead body (same shape, quality = the
  NPC number) at its location, put its inventory into the body, keep any
  blood on top and send the NPC to the limbo level 15. Returns the body.*/
/*C_1184_1B3A*/OBJ_MakeDeadBody(int16 objNum) {
	int16 body;
	int16 content, firstAtLoc, x, y, z;

	body = -1;
	if(objNum < 0)
		return -1;
	if(ObjShapeType[objNum]) {
		x = GetX(objNum);
		y = GetY(objNum);
		z = GetZ(objNum);
		firstAtLoc = FindLoc(x, y, z);

		AllowSubmissionMove = 1;
		body = AddMapObj(ObjShapeType[objNum], ObjStatus[objNum] | OWNED, QualQuan(objNum, 0), x, y, z);
		if(body >= 0) {
			if(objNum < 0xe0)
				ClrLocal(body);
			while((content = FindInv(objNum)) >= 0) {
				InsertObj(content, body, CONTAINED);
				SetOwned(content);
			}
			if(GetType(firstAtLoc) == OBJ_BLOOD)
				MoveObj(firstAtLoc, x, y, z);
			MoveObj(objNum, x, y, 15);
		}
		AllowSubmissionMove = 0;
	}

	return body;
}

/*resurrect the NPC whose dead body is given (the NPC number is the body's
  quality): put the NPC where the body (or whoever carries it) is, give it
  back the body's contents (Amulet of submission re-equipped) and delete
  the body. Returns the NPC.*/
/*C_1184_1C52*/OBJ_ReviveFromBody(int16 body) {
	int16 root, content;
	int16 npc;

	if(body < 0)
		return -1;

	AllowSubmissionMove = 1;
	npc = GetQual(body);
	for(root = body; GetCoordUse(root) != LOCXYZ; root = GetAssoc(root));
	MoveObj(npc, GetX(root), GetY(root), GetZ(root));
	for(content = FindInv(body); content >= 0; content = FindInv(body)) {
		if(GetType(content) == OBJ_AMULET_OF_SUBMISSION)
			InsertObj(content, npc, EQUIP);
		else
			InsertObj(content, npc, INVEN);
	}
	DeleteObj(body);
	AllowSubmissionMove = 0;

	return npc;
}

/*is objNum (directly or not) inside container?*/
/*C_1184_1D2E*/OBJ_IsInside(int16 objNum, int16 container) {
	int16 ret;

	ret = 0;
	while(GetCoordUse(objNum) != LOCXYZ) {
		objNum = GetAssoc(objNum);
		if(objNum == container) {
			ret = 1;
			break;
		}
	}
	return ret;
}

/*last node of objNum's subtree (its contents, recursively), objNum if empty*/
/*C_1184_1D60*/static OBJ_LastOfSubtree(int16 objNum) {
	int16 i, last;

	if(objNum < 0)
		return -1;
	for(i = last = objNum; Link[i] >= 0 && GetCoordUse(Link[i]) != LOCXYZ; ) {
		i = Link[i];
		if(OBJ_IsInside(i, objNum))
			last = i;
	}
	return last;
}

/*move an object (and its contents) to the map at (toX, toY, toZ); also
  moves the other parts of multi-object creatures, refreshes slimes,
  flags a redraw when the view is concerned and applies terrain effects
  to NPCs (if MoveAppliesTerrain). Returns objNum, or -1 if the move is refused
  (Amulet of submission, impossible silver serpent move).*/
/*C_1184_1DAF*/MoveObj(int16 objNum, int16 toX, int16 toY, int16 toZ) {
	int16 oldX;
	int16 insertAfter, prev, last, oldY, frame, objTyp, posMask;

	if(objNum < 0)
		return -1;
	if(StatusDisplay == CMD_STAT_EQUIP && OBJ_IsInside(objNum, Party[StatusMember]))
		StatusDirty = 1;
	objTyp = GetType(objNum);
	if(objTyp == OBJ_AMULET_OF_SUBMISSION)
		return -1;
	/*oldX/oldY hold the move delta (negated) for now*/
	oldX = GetX(objNum) - toX;
	oldY = GetY(objNum) - toY;
	if(objTyp == OBJ_SILVER_SERPENT && objNum < 0x100) {
		frame = GetFrame(objNum);
		if(oldX && oldY)
			return -1;
		if(oldX == 0 && oldY == 0)
			return -1;
		if(!CLOSE_ENOUGH(1, oldX, oldY, 0, 0))
			return -1;
		/*can't move backwards into its own body*/
		if(DirIncrX[frame] == oldX && DirIncrY[frame] == oldY)
			return -1;
	}

	posMask = toZ?0xff:0x3ff;
	toX &= posMask;
	toY &= posMask;
	oldX = GetX(objNum);
	oldY = GetY(objNum);
	prev = OBJ_FindPrevLink(objNum);
	if(prev < 0)
		return -1;
	if(
		((MapX - 5) <= oldX && (MapX + 5) >= oldX &&
		(MapY - 5) <= oldY && (MapY + 5) >= oldY) ||

		((MapX - 5) <= toX && (MapX + 5) >= toX &&
		(MapY - 5) <= toY && (MapY + 5) >= toY)
	) VisibilityDirty = 1;

	/*unlink the subtree prev->[objNum..last] and insert it at the new place*/
	last = OBJ_LastOfSubtree(objNum);
	Link[prev] = Link[last];
	OBJ_FixMapObjPtr(GetX(objNum), GetY(objNum), GetZ(objNum), objNum, Link[prev]);
	insertAfter = OBJ_FindListPos(toX, toY, 1);
	Link[last] = Link[insertAfter];
	Link[insertAfter] = objNum;
	SetCoordUse(objNum, LOCXYZ);
	SetXYZ(objNum, toX, toY, toZ);
	toX = (toX - AreaX) & 0x3ff;
	toY = (toY - AreaY) & 0x3ff;
	if(toX < AREA_W && toY < AREA_H)
		MapObjPtr[toY][toX] = objNum;
	if(objTyp == OBJ_SLIME)
		OBJ_UpdateSlimeFrames();
	else if(objTyp == OBJ_SILVER_SERPENT && objNum < 0x100)
		OBJ_MoveSerpentBody(objNum, oldX, oldY, 0);
	else if(objNum < 0x100 && (
		objTyp == OBJ_HYDRA || objTyp == OBJ_DRAGON ||objTyp == OBJ_SHIP || objTyp >= OBJ_GIANT_SCORPION
	)) OBJ_MoveCreatureParts(objNum, oldX, oldY, 0);
	else if(objTyp == OBJ_CORPSER)
		SetFrame(objNum, 0);

	if(objNum < 0x100 && MoveAppliesTerrain) {
		ClrHurtThisTurn(objNum);
		ApplyTerrainEffects(objNum);
	}

	return objNum;
}

/*put an object (and its contents) inside destNum with coord use objLoc
  (INVEN, EQUIP or CONTAINED); returns objNum or -1 if refused*/
/*C_1184_20DD*/InsertObj(int16 objNum, int16 destNum, int16 objLoc) {
	int16 prev, last, objTyp;

	if(objNum < 0 || destNum < 0)
		return -1;

	objTyp = GetType(objNum);
	if(objTyp == OBJ_AMULET_OF_SUBMISSION && !AllowSubmissionMove)
		return -1;

	if(objNum == destNum)
		return -1;
	if(OBJ_IsInside(destNum, objNum))
		return -1;
	prev = OBJ_FindPrevLink(objNum);
	last = OBJ_LastOfSubtree(objNum);
	Link[prev] = Link[last];
	Link[last] = Link[destNum];
	Link[destNum] = objNum;
	OBJ_FixMapObjPtr(GetX(objNum), GetY(objNum), GetZ(objNum), objNum, Link[prev]);
	SetCoordUse(objNum, objLoc);
	SetAssoc(objNum, destNum);
	return objNum;
}

/*object types counted by the quantity byte of Amount[]*/
/*0765*/int16 StackableTypes[] = {
	OBJ_TORCH,OBJ_LOCKPICK,OBJ_GEM,OBJ_ARROW,OBJ_BOLT,OBJ_BLACK_PEARL,OBJ_BLOOD_MOSS,
	OBJ_GARLIC,OBJ_GINSENG,OBJ_MANDRAKE,OBJ_NIGHTSHADE,OBJ_SPIDERSILK,OBJ_ASH,OBJ_EFFECT,
	OBJ_LOAF_OF_BREAD,OBJ_PORTION_OF_MEAT,OBJ_FLASK_OF_OIL,OBJ_EGG,OBJ_GOLD_NUGGET,OBJ_ZU_YLEM,OBJ_SILVER_SNAKE_VENOM
};

/*how an object type counts: 4 = whole Amount[] word (gold),
  2 = quantity byte (stackable, see StackableTypes), 0 = one per object*/
/*C_1184_21C7*/QuanType(int16 objTyp) {
	int16 i, ret;

	ret = 0;
	if(objTyp == OBJ_GOLD) {
		ret = 4;
	} else {
		for(i = 0; i < 21; i++)
			if(StackableTypes[i] == objTyp) {
				ret = 2;
				break;
			}
	}
	return ret;
}

/*number of items an object stands for (gold amount, stack quantity or 1)*/
/*C_1184_21FD*/static OBJ_GetCount(int16 objNum) {
	int16 quanType;

	quanType = QuanType(ObjShapeType[objNum]);
	if(quanType == 4)
		return Amount[objNum];
	if(quanType == 2)
		return GetQuan(objNum);
	return 1;
}

/*give owner (NPC or container) amount (quantity | quality << 8) items of
  objType; stacks are limited to 0x90 items. Returns the last object
  created/updated (or -1).*/
/*C_1184_2241*/GiveObj(int16 owner, int16 objType, int16 amount) {
	int16 obj;
	int16 i, quality, quantity, quanType, coordUse, slot;

	if(owner < 0)
		return -1;
	quality = amount >> 8;
	quantity = (unsigned char)amount;
	quanType = QuanType(objType);
	if(owner < 0x100) {
		coordUse = INVEN;
		if(objType == OBJ_AMULET_OF_SUBMISSION) {
			coordUse = EQUIP;
			for(obj = FindInv(owner); obj >= 0; obj = NextInv()) {
				if(GetCoordUse(obj) == EQUIP) {
					slot = STAT_GetEquipSlot(obj);
					if(slot == SLOT_NECK) {
						SetCoordUse(obj, INVEN);
						break;
					}
				}
			}
		}
	} else {
		coordUse = CONTAINED;
	}
	if(quanType == 4) {
		obj = FindInvType(owner, objType, -1);
		if(obj < 0)
			obj = AddInvObj(objType, coordUse | 1, 0, owner);
		if(obj >= 0)
			Amount[obj] += amount;
	} else if(quanType == 2) {
		InvSkipEquipped = 1;
		obj = FindInvType(owner, objType, quality);
		InvSkipEquipped = 0;
		while(quantity) {
			if(obj < 0)
				obj = AddInvObj(objType, coordUse | 1, QualQuan(quality, 0), owner);
			if(obj < 0)
				break;
			if(GetQuan(obj) + quantity > 0x90) {
				quantity -= 0x90 - GetQuan(obj);
				SetQuan(obj, 0x90);
				obj = -1;
			} else {
				AddQuan(obj, quantity);
				quantity = 0;
			}
		}
	} else {
		for(i = 0; i < quantity; i ++) {
			obj = AddInvObj(objType, coordUse | 1, QualQuan(quality, 0), owner);
			if(obj < 0)
				break;
		}
	}
	return obj;
}

/*remove amount (quantity | quality << 8, quality 0 = any) items of
  objType from owner's possessions (recursively)*/
/*C_1184_23F8*/TakeObj(int16 owner, int16 objType, int16 amount) {
	int16 obj, quanType;
	int16 count, next, quality, quantity, changed;

	changed = 0;
	if(owner < 0)
		return -1;
	quality = amount >> 8;
	quantity = (unsigned char)amount;
	quanType = QuanType(objType);
	if(quanType != 4)
		amount = quantity;
	obj = FindInv(owner);
	while(obj >= 0 && GetCoordUse(obj) != LOCXYZ && OBJ_IsInside(obj, owner)) {
		if(GetType(obj) == objType) {
			if(quanType == 4 || quality == 0 || GetQual(obj) == quality)  {
				count = OBJ_GetCount(obj);
				if(amount >= count) {
					amount -= count;
					next = Link[obj];
					DeleteObj(obj);
					obj = next;
					changed = 1;
					continue;
				}
				if(quanType == 4)
					Amount[obj] -= amount;
				else if(quanType == 2)
					SubQuan(obj, amount);
				amount = 0;
				changed = 1;
				/*always true here*/
				if(amount <= 0)
					break;
			}
		}
		obj = Link[obj];
	}
	if(changed && StatusDisplay == CMD_STAT_EQUIP && Party[StatusMember] == owner)
		StatusDirty = 1;
	return obj;
}

/*count the items of objType that objNum possesses (recursively)*/
/*C_1184_252D*/TestObj(int16 objNum, int16 objType) {
	int16 obj, total;

	total = 0;
	obj = FindInv(objNum);
	while(obj >= 0 && GetCoordUse(obj) != LOCXYZ && OBJ_IsInside(obj, objNum)) {
		if(GetType(obj) == objType)
			total += OBJ_GetCount(obj);
		obj = Link[obj];
	}
	return total;
}

/*uncalled?*/
/*C_1184_257D*/TransferObj(int16 oldOwner, int16 newOwner, int16 objType, int16 amount) {
	if(TakeObj(oldOwner, objType, amount) >= 0)
		GiveObj(newOwner, objType, amount);
}

/*copy object objNum to entry index of the objblk buffer and free it
  (ObjShapeType = 0). ScratchBuf->_6000[] maps buffer entries back to
  object numbers so that a CONTAINED object can store its container as a
  buffer index; NPC-held items keep the NPC number.*/
/*C_1184_25A2*/static __ObjSerialize(int16 objNum, int16 index) {
	struct coord containerPos;
	int16 containerIdx;

	if(GetCoordUse(objNum) != LOCXYZ) {
		if((int16)GetAssoc(objNum) < 0x100) {
			if(GetCoordUse(objNum) == CONTAINED)
				SetCoordUse(objNum, INVEN);
		} else {
			SetCoordUse(objNum, CONTAINED);
		}
	}
	ScratchBuf->objs[index].status = ObjStatus[objNum];
	ScratchBuf->objs[index].pos = ObjPos[objNum];
	ScratchBuf->objs[index].shapeType = ObjShapeType[objNum];
	ScratchBuf->objs[index].amount = Amount[objNum];
	if(GetCoordUse(objNum) == CONTAINED) {
		for(
			containerIdx = index;
			ScratchBuf->objNum[containerIdx] != GetAssoc(objNum);
			containerIdx --
		);
		SetCoordAssoc(containerPos, containerIdx);
		ScratchBuf->objs[index].pos = containerPos;
	}
	ObjShapeType[objNum] = 0;
	if(index + 1 >= 0xc00)
		CON_printf(/*078F*/"Error-too many objects in region!\n");
}

/*DeleteTempFiles: delete every "objblkXY.tmp" file flagged in ObjBlkTmpPending*/
/*C_1184_26E2*/OBJ_DeleteTmpBlocks() {
	int16 areaId;

	DISK_confirm(DISK_PLAYER);
	for(areaId = 0; areaId < (8*8 + 5); areaId ++) {
		if(ObjBlkTmpPending[areaId]) {
			__Tmp_file[15] = (areaId & 7) + 'A';
			__Tmp_file[16] = (areaId >> 3) + 'A';
			OSI_delete(__Tmp_file);
		}
	}
}

/*write the numObj entries of the objblk buffer to __Tmp_file (whose
  letters were set by the caller): a word count then 8 bytes per object.
  If the disk gets full (DISK_full sets DiskFullChoice): on 'R' (restore) nothing
  more is written; on 'S' (save) every pending .tmp file is renamed over
  the real file, this block is written directly to __Sav_file and a save
  game is requested (DiskFullChoice = 'G').*/
/*C_1184_2722*/static OBJ_WriteBlockFile(int16 numObj, int16 areaId) {
	int16 i, fd;
	char tmpLetters[2],savLetters[2];

	ObjBlkTmpPending[areaId] = 1;
	DISK_confirm(DISK_PLAYER);
	fd = OSI_create(__Tmp_file);
	OSI_write(fd, 0, 2, &numObj);
	if(DiskFullChoice != 'S' && DiskFullChoice != 'R')
		OSI_write(fd, -1, (int32)numObj << 3, ScratchBuf);
	if(DiskFullChoice == 'S') {
		tmpLetters[0] = __Tmp_file[15];
		tmpLetters[1] = __Tmp_file[16];
		savLetters[0] = __Sav_file[15];
		savLetters[1] = __Sav_file[16];
		OSI_close(fd);
		for(i = 0; i < (8*8 + 5); i ++) {
			if(ObjBlkTmpPending[i]) {
				__Sav_file[15] = __Tmp_file[15] = (i & 7) + 'A';
				__Sav_file[16] = __Tmp_file[16] = (i >> 3) + 'A';
				OSI_delete(__Sav_file);
				if(i != areaId)
					OSI_rename(__Tmp_file, __Sav_file);
				ObjBlkTmpPending[i] = 0;
			}
		}
		__Tmp_file[15] = __Sav_file[15] = tmpLetters[0];
		__Tmp_file[16] = __Sav_file[16] = tmpLetters[1];
		fd = OSI_create(__Sav_file);
		OSI_write(fd, 0, 2, &numObj);
		OSI_write(fd, -1, (int32)numObj << 3, ScratchBuf);
		__Sav_file[15] = savLetters[0];
		__Sav_file[16] = savLetters[1];
		DiskFullChoice = 'G';
		MapAnimEnabled = 1;
	}
	OSI_close(fd);
}

/*unload the outdoor area areaId (yyyxxx, 128x128 tiles at z 0): every
  object there and its contents, including the inventories of the NPCs
  standing there (the NPCs themselves stay), is serialized, removed from
  the chain and freed, then written to the area's .tmp file*/
/*C_1184_2888*/static OBJ_UnloadArea(int16 areaId) {
	int16 obj, numObj;
	int16 prev, last, baseX, baseY;

	if(areaId < 0 || DiskFullChoice == 'R')
		return;
	numObj = 0;
	baseX = (areaId & 7) << 7;
	baseY = (areaId & 0x38) << 4;
	prev = 0x100;
	obj = Link[0x100];
	while(numObj < 0xc00 && obj >= 0) {
		if(GetCoordUse(obj) == LOCXYZ && GetZ(obj) == 0 &&
			(GetX(obj) & 0x380) == baseX && (GetY(obj) & 0x380) == baseY
		) {
			if(obj < 0x100) {
				prev = obj;
			} else {
				ScratchBuf->objNum[numObj] = obj;
				__ObjSerialize(obj, numObj++);
				FreeObjCount ++;
			}
			last = obj;
			obj = Link[obj];
			while(obj >= 0 && GetCoordUse(obj) != LOCXYZ) {
				if(numObj < 0xc00) {
					ScratchBuf->objNum[numObj] = obj;
					__ObjSerialize(obj, numObj++);
				}
				FreeObjCount ++;
				last = obj;
				obj = Link[obj];
			}
			Link[last] = FreeObjHead;
			FreeObjHead = Link[prev];
			Link[prev] = obj;
		} else {
			prev = obj;
			obj = Link[obj];
		}
	}
	__Tmp_file[15] = (areaId & 7) + 'A';
	__Tmp_file[16] = (areaId >> 3) + 'A';
	OBJ_WriteBlockFile(numObj, areaId);
}

/*order of two objects in the chain for the merge: < 0 if obj0 must come
  first (-1 stands for the end of a list). An NPC-held object sorts
  before a map object (OBJ_RelinkHeldItems moves it behind its owner later);
  otherwise objects are compared by the location of their outermost
  container/owner: y, then x, then higher z first.*/
/*C_1184_29C4*/static OBJ_CompareOrder(int16 obj0, int16 obj1) {
	int16 root0, root1;
	int16 dist;

	if(obj0 == -1)
		return 1;
	if(obj1 == -1)
		return -1;
	root0 = obj0; root1 = obj1;
	while(GetCoordUse(root0) == CONTAINED)
		root0 = GetAssoc(root0);
	while(GetCoordUse(root1) == CONTAINED)
		root1 = GetAssoc(root1);
	if(GetCoordUse(root0) != LOCXYZ && GetCoordUse(obj1) == LOCXYZ)
		return -1;
	if(GetCoordUse(root1) != LOCXYZ && GetCoordUse(obj0) == LOCXYZ)
		return 1;
	if(IsHeldByNPC(root0))
		root0 = GetAssoc(root0);
	if(IsHeldByNPC(root1))
		root1 = GetAssoc(root1);
	dist = GetY(root0) - GetY(root1);
	if(dist == 0)
		dist = GetX(root0) - GetX(root1);
	if(dist == 0)
		dist = GetZ(root1) - GetZ(root0);
	if(root0 == root1) {/*second test can't be true: obj0/obj1 meant?*/
		if(GetCoordUse(root0) == LOCXYZ)
			dist = -1;
		else if(GetCoordUse(root1) == LOCXYZ)
			dist = 1;
	}
	return dist;
}

/*after loading blocks: move every NPC-held (INVEN/EQUIP) object that is
  not inside its owner's subtree (with its own contents) behind its owner*/
/*C_1184_2B27*/static OBJ_RelinkHeldItems() {
	int16 obj, walk;
	int16 prev, last, owner;

	prev = 0x100;
	while(Link[prev] >= 0) {
		obj = Link[prev];
		if(IsHeldByNPC(obj)) {
			owner = GetAssoc(obj);
			for(
				walk = Link[owner];
				walk >= 0 && GetCoordUse(walk) != LOCXYZ && walk != obj;
				walk = Link[walk]
			);
			if(walk != obj) {
				last = OBJ_LastOfSubtree(obj);
				owner = OBJ_LastOfSubtree(owner);
				Link[prev] = Link[last];
				Link[last] = Link[owner];
				Link[owner] = obj;
				continue;
			}
		}
		prev = obj;
	}
}

/*allocate numObj objects from the objblk buffer (as long as slots are
  free), translate the CONTAINED buffer indexes back to object numbers,
  then merge the new (sorted) chain into the main list*/
/*C_1184_2BD8*/static __ObjectsDeserialize(int16 numObj) {
	int16 cur, incoming;
	int16 i, tmp, restart;

	for(i = 0; i < numObj; i ++) {
		if(FreeObjCount == 0) {
			ScratchBuf->objNum[i] = -1;
			if(i)
				Link[ScratchBuf->objNum[i - 1]] = -1;
			break;
		}
		cur = FreeObjHead;
		ScratchBuf->objNum[i] = cur;
		FreeObjHead = Link[cur];
		FreeObjCount --;
		ObjStatus[cur] = ScratchBuf->objs[i].status;
		ObjPos[cur] =  ScratchBuf->objs[i].pos;
		ObjShapeType[cur] = ScratchBuf->objs[i].shapeType;
		Amount[cur] = ScratchBuf->objs[i].amount;
		if(GetCoordUse(cur) == CONTAINED)
			GetAssoc(cur) = ScratchBuf->objNum[GetAssoc(cur)];
	}
	if(i == numObj)
		Link[ScratchBuf->objNum[i - 1]] = -1;
	/*merge: cur walks the main list, incoming the new chain*/
	cur = 0x100;
	incoming = ScratchBuf->objNum[0];
	restart = 0;
	while(cur >= 0 && incoming >= 0) {
		if(OBJ_CompareOrder(Link[cur], incoming) < 0) {
			cur = Link[cur];
		} else {
			tmp = Link[cur];
			Link[cur] = incoming;
			cur = tmp;
			while(Link[incoming] >= 0 && OBJ_CompareOrder(cur, Link[incoming]) > 0) {
				if(OBJ_CompareOrder(incoming, Link[incoming]) <= 0) {
					incoming = Link[incoming];
				} else {
					restart = 1;
					break;
				}
			}
			tmp = Link[incoming];
			Link[incoming] = cur;
			incoming = tmp;
			if(restart) {
				cur = 0x100;
				restart = 0;
				continue;
			}
		}
		if(Link[cur] < 0) {
			Link[cur] = incoming;
			cur = -1;
		}
	}
}

/*load the outdoor area areaId from its objblk file (the .tmp one if any)*/
/*C_1184_2DEF*/static OBJ_LoadArea(int16 areaId) {
	int16 fd;
	int16 numObj;

	if(areaId < 0 || DiskFullChoice == 'R')
		return;
	areaId &= 0x3f;
	DISK_confirm(DISK_PLAYER);
	if(!ObjBlkTmpPending[areaId]) {
		__Sav_file[15] = 'A' + (areaId & 7);
		__Sav_file[16] = 'A' + (areaId >> 3);
		fd = OSI_open(__Sav_file);
	} else {
		__Tmp_file[15] = 'A' + (areaId & 7);
		__Tmp_file[16] = 'A' + (areaId >> 3);
		fd = OSI_open(__Tmp_file);
	}
	OSI_read(fd, 0, 2, (void far *)&numObj);
	if(numObj > 0xc00)
		numObj = 0xc00;
	if(numObj)
		OSI_read(fd, -1, (int32)numObj << 3, ScratchBuf);
	OSI_close(fd);
	if(numObj)
		__ObjectsDeserialize(numObj);
}

/*rebuild the MapObjPtr[][] cache: first chained object of every tile of
  the 40x40 area*/
/*C_1184_2ECC*/OBJ_RebuildMapObjPtr() {
	int16 obj;
	int16 relX, relY;
	int16 unusedMask;

	for(relY = 0; relY < AREA_H; relY ++) {
		for(relX = 0; relX < AREA_W; relX ++)
			MapObjPtr[relY][relX] = -1;
	}
	for(obj = Link[0x100]; obj >= 0; obj = Link[obj]) {
		if(GetCoordUse(obj) == LOCXYZ) {
			unusedMask = GetZ(obj)?0xff:0x3ff;
			relX = (GetX(obj) - AreaX) & 0x3ff;/*__unused__ => bug?*/
			relY = (GetY(obj) - AreaY) & 0x3ff;/*__unused__ => bug?*/
			if(relX < AREA_W && relY < AREA_H && MapObjPtr[relY][relX] == -1)
				MapObjPtr[relY][relX] = obj;
		}
	}
}

/*unload dungeon level z (1..5) like OBJ_UnloadArea does for an outdoor
  area; its file is "objblk<'A'+z-1>i", flag ObjBlkTmpPending[64 + z - 1]*/
/*C_1184_2FB3*/static OBJ_UnloadDungeon(int16 z) {
	int16 obj, numObj;
	int16 prev, last;

	if(z < 0 || z > 5 || DiskFullChoice == 'R')
		return;
	numObj = 0;
	prev = 0x100;


	obj = Link[0x100];
	while(numObj < 0xc00 && obj >= 0) {
		if(GetCoordUse(obj) == LOCXYZ && GetZ(obj) == z) {


			if(obj < 0x100) {
				prev = obj;
			} else {
				ScratchBuf->objNum[numObj] = obj;
				__ObjSerialize(obj, numObj++);
				FreeObjCount ++;
			}
			last = obj;
			obj = Link[obj];
			while(obj >= 0 && GetCoordUse(obj) != LOCXYZ) {
				if(numObj < 0xc00) {
					ScratchBuf->objNum[numObj] = obj;
					__ObjSerialize(obj, numObj++);
				}
				FreeObjCount ++;
				last = obj;
				obj = Link[obj];
			}
			Link[last] = FreeObjHead;
			FreeObjHead = Link[prev];
			Link[prev] = obj;
		} else {
			prev = obj;
			obj = Link[obj];
		}
	}
	__Tmp_file[15] = z - 1 + 'A';
	__Tmp_file[16] = 'I';
	OBJ_WriteBlockFile(numObj, z + 0x3f);
}

/*load dungeon level z (1..5) from its objblk file (the .tmp one if any)*/
/*C_1184_30A6*/static OBJ_LoadDungeon(int16 z) {
	int16 fd;
	int16 numObj;

	if(z < 0 || z > 5 || DiskFullChoice == 'R')
		return;
	DISK_confirm(DISK_PLAYER);
	if(!ObjBlkTmpPending[8*8 + z - 1]) {
		__Sav_file[15] = 'A' + z - 1;
		__Sav_file[16] = 'I';
		fd = OSI_open(__Sav_file);
	} else {
		__Tmp_file[15] = 'A' + z - 1;
		__Tmp_file[16] = 'I';
		fd = OSI_open(__Tmp_file);
	}
	OSI_read(fd, 0, 2, (void far *)&numObj);
	if(numObj > 0xc00)
		numObj = 0xc00;
	if(numObj)
		OSI_read(fd, -1, (int32)numObj << 3, ScratchBuf);
	OSI_close(fd);
	if(numObj)
		__ObjectsDeserialize(numObj);
}

/*make sure the objects around (x, y, z) are loaded. On the surface a
  2x2 window of areas is kept; it is only shifted when (x, y) comes
  within 5 tiles of its border. Areas leaving the window (or the level
  left) are unloaded, new ones loaded, then NPC inventories relinked.*/
/*C_1184_3176*/LoadNewRegions(int16 x, int16 y, int16 z) {
	int16 i, j;
	int16 shiftX, shiftY, relX, relY, numUnloaded, numLoaded;
	int16 mapIds[4];

	numUnloaded =
	numLoaded = 0;
	if(LoadedZ > 0 && LoadedZ != z) {
		OBJ_PurgeFarLocals(x, y, z);
		OBJ_UnloadDungeon(LoadedZ);
		numUnloaded ++;
	}
	if(z == 0) {
		/*mapIds[0] = top-left area of the window*/
		if(LoadedWindowArea == -1) {
			mapIds[0] = (((x - 0x40) >> 7) & 7) + (((y - 0x40) >> 4) & 0x38);
		} else {
			shiftX = 0;
			relX = x - ((LoadedWindowArea & 7) << 7);
			for(; relX < 5; relX += 0x80, shiftX --);
			for(; relX >= 0xfb; relX -= 0x80, shiftX ++);
			shiftY = 0;
			relY = y - ((LoadedWindowArea & 0x38) << 4);
			for(; relY < 5; relY += 0x80, shiftY -= 8);
			for(; relY >= 0xfb; relY -= 0x80, shiftY += 8);
			mapIds[0] = (((LoadedWindowArea & 7) + shiftX) & 7) + (((LoadedWindowArea & 0x38) + shiftY) & 0x38);
		}
		mapIds[1] = ((mapIds[0] + 1) & 7) + (mapIds[0] & 0x38);
		mapIds[2] = (mapIds[0] + 8) & 0x3f;
		mapIds[3] = (mapIds[1] + 8) & 0x3f;
		LoadedWindowArea = mapIds[0];
	} else {
		for(i = 0; i < 4; i ++)
			mapIds[i] = -1;
		LoadedWindowArea = -1;
	}
	/*unload the areas no longer needed*/
	if(LoadedZ == 0) {
		for(i = 0; i < 4; i ++) {
			for(j = 0; j < 4; j ++) {
				if(mapIds[j] == LoadedAreas[i])
					break;
			}
			if(j != 4 || LoadedAreas[i] == -1)
				continue;
			if(numUnloaded == 0)
				OBJ_PurgeFarLocals(x, y, z);
			OBJ_UnloadArea(LoadedAreas[i]);
			numUnloaded ++;
			LoadedAreas[i] = -1;
		}
	}
	/*load the new ones*/
	if(z == 0) {
		for(j = 0; j < 4; j ++) {
			for(i = 0; i < 4; i++) {
				if(mapIds[j] == LoadedAreas[i])
					break;
			}
			if(i != 4)
				continue;
			for(i = 0; i < 4; i ++) {
				if(LoadedAreas[i] == -1)
					break;
			}
			OBJ_LoadArea(mapIds[j]);
			numLoaded ++;
			LoadedAreas[i] = mapIds[j];
		}
	} else if(LoadedZ != z) {
		OBJ_LoadDungeon(z);
		numLoaded ++;
	}
	LoadedZ = z;
	if(numLoaded)
		OBJ_RelinkHeldItems();
}

/*"commit" obj files (save game): write the loaded blocks, rename every
  .tmp file over its objblk file, then reload the blocks*/
/*C_1184_33CA*/OBJ_CommitBlocks() {
	int16 i;

	if(MapZ) {
		OBJ_UnloadDungeon(MapZ);
	} else {
		for(i = 0; i < 4; i ++)
			OBJ_UnloadArea(LoadedAreas[i]);
	}
	for(i = 0; i < (8*8 + 5); i ++) {
		if(ObjBlkTmpPending[i]) {
			__Tmp_file[15] = __Sav_file[15] = __Bak_file[15] = (i & 7) + 'A';
			__Tmp_file[16] = __Sav_file[16] = __Bak_file[16] = (i >> 3) + 'A';/*.bak is never used*/
			OSI_delete(__Sav_file);
			OSI_rename(__Tmp_file, __Sav_file);
			ObjBlkTmpPending[i] = 0;
		}
	}
	for(i = 0; i < 4; i ++)
		LoadedAreas[i] = -1;
	LoadedWindowArea = LoadedZ = -1;
	LoadNewRegions(MapX, MapY, MapZ);
	OBJ_RebuildMapObjPtr();
}

/*add a tile to the display stack of the 11x11 view cell (x, y).
  Each cell's stack is a list of nodes: Obj_11x11[y][x] = first node,
  CellObjNext[node] = next node (CellObjNext[0] = free node list), CellObjTile[node] =
  tile | frame; it is drawn from the first node on (first = bottom).
  A "background" tile (IsTileBa) replaces the cell's map tile instead.
  Non-foreground tiles go first (below everything already there);
  foreground tiles (or mode 2) go:
    mode 0/2: just before the first foreground tile already there;
    mode 1:   last (on top) -- parts of multi-tile objects, hit effects;
    mode 3:   darkness: last, or with infravision right after the first
              non-warm tile.
  Nothing is shown on hidden cells nor, except warm tiles under
  infravision, on dark ones; egg objects are never shown.*/
/*C_1184_347C*/ShowObject(int16 tileFrame, int16 x, int16 y, int16 mode) {
	unsigned char *slot;
	int16 tile;
	int16 mapTile, onTop, node;
	unsigned char *insertAt;

	tile = tileFrame & 0x7ff;
	if(BaseTile[OBJ_EGG] == tile)
		return;
	mapTile = Tile_11x11[y][x];
	if(mapTile == TIL_HIDDEN)
		return;
	if(mapTile == TIL_DARKNESS) {
		if(SpellFx[1] == 0 || !IsTileWa(tile))
			return;
	}
	if(IsTileBa(tile)) {
		Tile_11x11[y][x] = tileFrame;
		return;
	}
	node = CellObjNext[0];
	if(node == 0)
		return;
	CellObjNext[0] = CellObjNext[node];
	CellObjTile[node] = tileFrame;
	slot = &(Obj_11x11[y][x]);
	if(IsTileFor(tile) || mode == 2) {
		onTop = 1;
		if(mode & 1) {
			if(SpellFx[1] && mode == 3) {
				insertAt = slot;
				for(; *slot; slot = CellObjNext + *slot) {
					if(!IsTileWa(CellObjTile[*slot] & 0x7ff)) {
						insertAt = CellObjNext + *slot;
						break;
					}
				}
				slot = insertAt;
			} else {
				while(*slot)
					slot = CellObjNext + *slot;
			}
		} else {
			while(*slot && !IsTileFor(CellObjTile[*slot] & 0x7ff))
				slot = CellObjNext + *slot;
		}
	}
	CellObjNext[node] = *slot;
	*slot = node;
}

/*show tile + frame at view cell (x, y) together with the left/upper parts
  of double-width/double-height tiles (x, y may be 11 for those)*/
/*C_1184_35EA*/static OBJ_ShowMultiTile(int16 tile, int16 frame, int16 x, int16 y) {
	int16 flags;

	if(x < 11 && y < 11)
		ShowObject(tile + frame, x, y, 0);

	flags = TileFlag[tile];
	if(flags & TILE_DOUBLE_WIDTH) {
		if(x != 0 && y < 11)
			ShowObject(tile + frame - 1, x - 1, y, 1);
		if(flags & TILE_DOUBLE_HEIGHT) {
			if(x < 11 && y != 0)
				ShowObject(tile + frame - 2, x, y - 1, 1);
			if(x != 0 && y != 0)
				ShowObject(tile + frame - 3, x - 1, y - 1, 1);
		}
	} else if(x < 11 && y != 0 && (flags & TILE_DOUBLE_HEIGHT)) {
		ShowObject(tile + frame - 1, x, y - 1, 1);
	}
}

/*build the display stacks of the 11x11 view from the objects around
  (MapX, MapY); also triggers the ambient sounds, the bard music and
  volcano eruptions, and draws the darkness of the badly lit cells*/
/*C_1184_36C2*/ShowObjects() {
	int16 objNum;
	int16 i, j;
	int16 tile, objX, objY, light;
	int16 newFrame, objTyp, stack, node, hasMusic;

	MUS_ClearAmbient();
	hasMusic = 0;
	for(i = 0; i < 0x100; i ++)
		CellObjNext[i] = i + 1;

	for(i = 0; i < 11; i ++)
		for(j = 0; j < 11; j ++)
			Obj_11x11[i][j] = 0;

	for(
		objNum = SearchArea(MapX - 5, MapY - 5, MapX + 6, MapY + 6);
		objNum >= 0;
		objNum = NextArea()
	) {
		newFrame = 0;
		objTyp = GetType(objNum);
		/*possible eruption*/
		if(Erupting == 1)
			if(objTyp == OBJ_VOLCANO || objTyp == OBJ_FUMAROLE)
				Erupting = objNum;
		/*bard*/
		if(objTyp == OBJ_MUSICIAN_SHORT_CAPE)
			hasMusic = 1;
		/*ambiant sound*/
		MUS_AddAmbientSource(objTyp, COMBAT_TileDistance(objNum, MapX, MapY));
		/* */
		if(IsInvisible(objNum) && (objNum >= 0x100 || !IsPlrControl(objNum)))
			continue;
		if(objNum < 0x100 && IsDraggedUnder(objNum))
			continue;
		/*mirror reflection*/
		if(objTyp == OBJ_MIRROR && GetFrame(objNum) < 2) {
			if(OBJ_FindNPCAt(0, GetX(objNum), GetY(objNum) + 1) >= 0)
				SetFrame(objNum, 1);
			else
				SetFrame(objNum, 0);
		}
		/* */
		tile = TILE_FRAME(objNum);
		/*outline drawn with the tile (bits 11-12 of the tile number, above the
		  0x7ff tile mask): 2 << 10 = invisible, 4 << 10 = protected, 6 << 10 = cursed*/
		if(IsInvisible(objNum) && objTyp < OBJ_GIANT_SCORPION)
			newFrame = (2 << 10);
		if(objNum < 0x100) {
			if(IsProtected(objNum))
				newFrame = (4 << 10);
			if(IsCursed(objNum))
				newFrame = (6 << 10);
		}
		objX = GetX(objNum) - MapX + 5;
		objY = GetY(objNum) - MapY + 5;
		if(newFrame)
			OBJ_ShowMultiTile(tile, newFrame, objX, objY);
		if(!IsInvisible(objNum))
			OBJ_ShowMultiTile(tile, 0, objX, objY);
		/*britannia lens in temple of singularity*/
		if(
			objTyp == OBJ_BRITANNIA_LENS_BLUE &&
			objX + MapX == 0x39e &&
			objY + MapY == 0x358 &&
			GetZ(objNum) == 0 &&
			objY < 11
		) {
			if(objX)
				ShowObject(TIL_1BA, objX + 1, objY, 1);
			if(objX < 10)
				ShowObject(TIL_1BB, objX - 1, objY, 1);
			continue;
		}
		/*gargoyle lens in temple of singularity*/
		if(
			objTyp == OBJ_GARGOYLE_LENS_VIOLET &&
			objX + MapX == 0x3a2 &&
			objY + MapY == 0x358 &&
			GetZ(objNum) == 0 &&
			objY < 11
		) {
			if(objX)
				ShowObject(TIL_1B8, objX + 1, objY, 1);
			if(objX < 10)
				ShowObject(TIL_1B9, objX - 1, objY, 1);
		}
	}
	/* */
	if(!XRayView) {
		for(i = 0; i < 11; i ++) {
			for(j = 0; j < 11; j ++) {
				if(IsTerrainWall(Tile_11x11[j][i])) {
					if(
						!(AreaFlags[j     + ViewY][i + 1 + ViewX] & 0x80) ||
						!(AreaFlags[j + 1 + ViewY][i     + ViewX] & 0x80)
					) {
						stack = Obj_11x11[j][i];
						/*wall not seen from the party side: hide what is on it*/
						if(stack) {
							Obj_11x11[j][i] = 0;
							for(node = stack; CellObjNext[node]; node = CellObjNext[node]);
							CellObjNext[node] = CellObjNext[0];
							CellObjNext[0] = stack;
						}
					}
				}
				/*draw obscurity*/
				light = AreaLight[j + ViewY][i + ViewX];
				if(light < 4 && light > 0) {
					tile = TIL_DARKNESS + light;
					ShowObject(tile, i, j, 3);
				}
			}
		}
	}
	/*no eruption this time*/
	if(Erupting == 1)
		Erupting = 0;
	/* */
	if(hasMusic && !MusicianWasNear) {
		MUS_SongIndex = 0;
		MUS_Song = OSI_rand(0, 1);
	}
	MusicianWasNear = hasMusic;
}

/*Initialization: empty object list, all slots free (monsters 0xE0-0xFF,
  objects 0x101-0xBFF), no area loaded*/
/*C_1184_3B1D*/OBJ_InitLists() {
	int16 i;

	for(i = 0; i < 0xbff; i ++)
		Link[i] = i + 1;
	Link[0xbff] = -1;
	Link[0xff] = -1;
	FreeMonsterHead = 0xe0;
	FreeMonsterCount = 0x20;
	FreeObjHead = 0x101;
	FreeObjCount = 0xaff;
	Link[0x100] = -1;
	for(i = 0; i < 4; i ++)
		LoadedAreas[i] = -1;
	LoadedWindowArea = LoadedZ = -1;
}

/*called after a restore: link the existing NPCs (0x00-0xFF) into the
  sorted object list and remove the used temporary monster slots from
  their free list (if slot 0xFF is used, its predecessor gets 0x100: bug?)*/
/*C_1184_3B7D*/OBJ_RelinkNPCs() {
	int16 npc, prev;
	int16 lastFree;

	lastFree = 0x100;
	for(npc = 0; npc < 0x100; npc ++) {
		if(ObjShapeType[npc]) {
			if(npc >= 0xe0) {
				if(lastFree == 0x100)
					FreeMonsterHead = npc + 1;
				else
					Link[lastFree] = npc + 1;
				FreeMonsterCount --;
			}
			for(prev = 0x100; Link[prev] >= 0 && OBJ_CompareOrder(Link[prev], npc) < 0; prev = Link[prev]);
			Link[npc] = Link[prev];
			Link[prev] = npc;
		} else {
			if(npc >= 0xe0)
				lastFree = npc;
		}
	}
}

/*load tile description from "look.lzd": entries are (word first tile,
  zero-terminated string) sorted by tile; the entry for numTile is the
  first one whose first tile is >= numTile. Returns "nothing" if empty.*/
/*C_1184_3C1B*/char *GetTileString(int16 numTile) {
	int16 tileNum;
	char *dst;
	int16 far *src;

	DISK_confirm(DISK_MAP);
	IsZFile = 1;
	LoadFile(/*07B2*/"look.lzd", ScratchBuf);
	IsZFile = 0;
	src = ScratchBuf;
	tileNum = *(src++);
	while(tileNum < numTile) {
		while(*CHARPTR_POSTINC(src));
		tileNum = *(src++);
	}
	dst = TileStringBuf;
	while(*dst++ = *CHARPTR_POSTINC(src));
	return TileStringBuf[0]?TileStringBuf:/*07BB*/"nothing";
}

/*name of an object: the party member's name, the original shape's name for
  a dead or sleeping NPC, otherwise its tile description*/
/*C_1184_3C9C*/char *GetObjectString(int16 objNum) {
	char *str;
	int16 i;

	if(objNum < 0) {
		str = /*07C3*/"Nothing";
	} else if(IsPlrControl(objNum)) {
		str = GetTileString(TILE_FRAME(objNum));
		for(i = 0; i < PartySize; i ++) {
			if(Party[i] == objNum) {
				strcpy(TileStringBuf, Names[i]);
				str = TileStringBuf;
				break;
			}
		}
	} else if(objNum < 0x100 && IsDeadOrAsleep(objNum)) {
		str = GetTileString(BaseTile[OrigShapeType[objNum] & 0x3ff] + (OrigShapeType[objNum] >> 10));
	} else {
		str = GetTileString(TILE_FRAME(objNum));
	}
	return str;
}
