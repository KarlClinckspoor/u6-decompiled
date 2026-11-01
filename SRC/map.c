/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	map module

	Ground tiles are stored in two files:
	- "chunks": 8x8-tile blocks (64 bytes each), referenced by a 12-bit index;
	- "map": the chunk indexes, packed 2 per 3 bytes (12 bits each).
	  The surface (z=0, 1024x1024 tiles) is split in 8x8 "superchunks"
	  of 16x16 chunks (0x180 bytes each); only a 2x2 window of superchunks
	  is kept in ChunkIndexBuf. Levels 1..5 (256x256 tiles) are 32x32 chunks
	  (0x600 bytes) each and are loaded entirely.
	The 40x40 tiles around the party (AreaTiles, origin AreaX/AreaY) are
	rebuilt from a small LRU cache of 16 chunks (ChunkCache).
	This module also moves the whole party between places: gathering
	(PartyEnter), spreading out (PartyExit), teleport, moongates,
	ladders/dungeon entrances and resurrection at Lord British's castle.
*/
#include "u6.h"

/*05CA*/int16 MapCoordMask = 0x3FF;/*coordinate wrap mask: 0x3ff on surface, 0xff underground*/
/*05CC*/int16 ChunksFile = -1;/*file "chunks" handler*/
/*05CE*/int16 MapFile = -1;/*file "map" handler*/
/*05D0*/int16 LoadedMapZ = -1;/*level (z) currently loaded in ChunkIndexBuf*/
/*05D2*/int16 LoadedSuperChunks[4] = {-1, -1, -1, -1};/*surface: ids of the 2x2 superchunks loaded in ChunkIndexBuf (top-left, top-right, bottom-left, bottom-right)*/
/*05DA*/int16 BuiltAreaX = -1;/*AreaX of the AreaTiles currently built*/
/*05DC*/int16 BuiltAreaY = -1;/*AreaY of the AreaTiles currently built*/
/*05DE*/int16 BuiltAreaZ = -1;/*MapZ of the AreaTiles currently built*/

/*superchunk id = row * 8 + column (each superchunk is 128x128 tiles)*/
#define MK_MAP_ID(x,y) (((x) >> 7) + (((y) >> 4) & 0x38))

/*make sure chunk #chunkIdx is in the chunk cache (load it over the
  least recently used entry if needed); returns its cache slot*/
static /*C_101C_000F*/cacheChunk(int16 chunkIdx) {
	int16 i, cacheIdx;
	int16 oldestIdx;

	if(ChunksFile < 0)
		return 0;
	cacheIdx = -1;
	oldestIdx = 0;
	/*lookup chunk in cache*/
	for(i = 0; i < CHUNKS_CACHE_MAX; i ++) {
		if(ChunkCacheIdx[i] == chunkIdx) {
			cacheIdx = i;
			break;
		}
		if(ChunkCacheAge[i] > ChunkCacheAge[oldestIdx])
			oldestIdx = i;
	}
	/*load if not cached*/
	if(cacheIdx < 0) {
		cacheIdx = oldestIdx;
		ChunkCacheIdx[cacheIdx] = chunkIdx;
		DISK_confirm(DISK_MAP);
		OSI_read(ChunksFile, (int32)chunkIdx << 6, 0x40, ChunkCache[cacheIdx]);
	}
	/*update chunks "age"*/
	for(i = 0; i < CHUNKS_CACHE_MAX; i ++) {
		if(ChunkCacheAge[i] < ChunkCacheAge[cacheIdx])
			ChunkCacheAge[i] ++;
	}
	ChunkCacheAge[cacheIdx] = 0;

	return cacheIdx;
}

/*map
level 0 is divided in 8x8 = 64 superchunks of 16x16 chunks x 1.5 bytes
(ids are row * 8 + column, written below as "row column"):
+--+--+--+--+--+--+--+--+
|00|01|02|03|04|05|06|07|
+--+--+--+--+--+--+--+--+
|10|11|12|13|14|15|16|17|
+--+--+--+--+--+--+--+--+
|20|21|22|23|24|25|26|27|
+--+--+--+--+--+--+--+--+
|30|31|32|33|34|35|36|37|
+--+--+--+--+--+--+--+--+
|40|41|42|43|44|45|46|47|
+--+--+--+--+--+--+--+--+
|50|51|52|53|54|55|56|57|
+--+--+--+--+--+--+--+--+
|60|61|62|63|64|65|66|67|
+--+--+--+--+--+--+--+--+
|70|71|72|73|74|75|76|77|
+--+--+--+--+--+--+--+--+
(file offsets 0x0000-0x5fff)

levels 1~5 are in 4 16x16x(1.5) parts, i.e. 32x32 chunks = 0x600 bytes
each, at file offset 0x6000 + (z - 1) * 0x600
*/
/*load into ChunkIndexBuf the chunk indexes needed for (x, y, z):
  the whole level if z != 0, else a 2x2 window of superchunks
  containing (x, y); the window is only moved when (x, y) leaves it*/
/*C_101C_00D7*/MAP_LoadChunkIndexes(int16 x, int16 y, int16 z) {
	int16 mapId, i;
	int16 mustLoad, colDiff, rowDiff, bufOfs;
	int32 fileOfs;
	unsigned char far *pSlot;

	mustLoad = (LoadedMapZ != z);
	if(!mustLoad && z == 0) {
		mapId = MK_MAP_ID(x, y);
		mustLoad = 1;
		for(i = 0; i < 4; i ++) {
			if(LoadedSuperChunks[i] == mapId) {
				mustLoad = 0;
				break;
			}
		}
	}
	if(!mustLoad)
		return;
	if(z != 0) {
		DISK_confirm(DISK_PLAYER);
		MapFile = OSI_open(/*05E0*/"map");
		/*z * 0x600 + 0x5a00 = 0x6000 + (z - 1) * 0x600*/
		OSI_read(MapFile, (int32)((z + z + z) << 9) + 0x5a00, 4 * 0x180, ChunkIndexBuf);
		OSI_close(MapFile);
		LoadedMapZ = z;
		for(i = 0; i < 4; i ++)
			LoadedSuperChunks[i] = -1;
	} else {
		mapId = MK_MAP_ID(x, y);
		if(LoadedSuperChunks[0] >= 0) {
			/*move the current window just enough to contain mapId
			  (column/row differences are modulo 8, the world wraps)*/
			colDiff = (mapId - LoadedSuperChunks[0]) & 7;
			rowDiff = (mapId - LoadedSuperChunks[0]) & 0x38;
			mapId = LoadedSuperChunks[0];
			if(colDiff > 1 && colDiff < 5)
				mapId = (mapId & 0x38) + ((mapId + colDiff - 1) & 7);
			else if(colDiff > 4)
				mapId = (mapId & 0x38) + ((mapId + colDiff) & 7);
			/*note: "- 1" here only lowers the row when the column is 0
			  ("- 8" was probably meant); the window still contains the target*/
			if(rowDiff > 0x08 && rowDiff < 0x28)
				mapId = (mapId & 7) + ((mapId + rowDiff - 1) & 0x38);
			else if(rowDiff > 0x20)
				mapId = (mapId & 7) + ((mapId + rowDiff) & 0x38);
		}
		if(mapId != LoadedSuperChunks[0]) {
			LoadedSuperChunks[0] = mapId;
			LoadedSuperChunks[1] = (mapId + 1) & 0x3f;
			LoadedSuperChunks[2] = (mapId + 8) & 0x3f;
			LoadedSuperChunks[3] = (mapId + 9) & 0x3f;
			DISK_confirm(DISK_PLAYER);
			MapFile = OSI_open(/*05E0*/"map");
			for(i = 0; i < 4; i ++) {
				/*superchunk id * 0x180*/
				fileOfs = (int32)LoadedSuperChunks[i] << 7; fileOfs += fileOfs << 1;
				/*window slot * 0x180*/
				bufOfs = i << 7; bufOfs += bufOfs << 1;
				pSlot = ChunkIndexBuf + bufOfs;
				OSI_read(MapFile, fileOfs, 0x180, pSlot);
			}
			OSI_close(MapFile);
		}
		LoadedMapZ = 0;
	}
}

/*recompute AreaX/AreaY (40x40 area aligned on chunks, party around the
  middle) and ViewX/ViewY (top-left of the 11x11 view inside the area);
  if the area moved, re-place the NPCs, rebuild AreaTiles from the chunks
  and hatch the eggs; then update the moongates and ask for a view redraw*/
/*C_101C_0306*/MAP_BuildArea() {
	unsigned char *pMap, *pChunk;
	int16 i, j;
	int16 anX, anY, cacheIdx;

	AreaX = anX = (MapX - 0x10) & 0x3f8;
	ViewX = (MapX - anX - 5) & 0x1f;
	AreaY = anY = (MapY - 0x10) & 0x3f8;
	ViewY = (MapY - anY - 5) & 0x1f;
	if(MapZ)
		MapCoordMask = 0xff;
	else
		MapCoordMask = 0x3ff;
	/*redundant?*/
	anX = AreaX;
	anY = AreaY;
	/* */
	if(anX != BuiltAreaX || anY != BuiltAreaY || MapZ != BuiltAreaZ) {
		OBJ_RebuildMapObjPtr();
		/*NPCs inside the new area but missing from MapObjPtr: put them back*/
		MoveAppliesTerrain = 0;
		for(i = 0; i < 0x100; i ++) {
			anX = (GetX(i) - AreaX) & MapCoordMask;
			anY = (GetY(i) - AreaY) & MapCoordMask;
			if(ObjShapeType[i])
			if(GetCoordUse(i) == LOCXYZ)
			if(GetZ(i) == MapZ)
			if(anX < 40 && anY < 40)
			if(MapObjPtr[anY][anX] != i)
				MoveObj(i, GetX(i), GetY(i), MapZ);
		}
		MoveAppliesTerrain = 1;

		anX = AreaX;
		anY = AreaY;
		OBJ_PurgeFarLocals(anX + 20, AreaY + 20, MapZ);

		BuiltAreaX = anX;
		BuiltAreaY = anY;
		BuiltAreaZ = MapZ;
		/*copy the 5x5 chunks of the area (i = x, j = y)*/
		for(i = 0; i < 40; i += 8) {
			for(j = 0; j < 40; j += 8) {
				cacheIdx = cacheChunk(getChunkIdxAt((BuiltAreaX + i) & 0x3ff, (BuiltAreaY + j) & 0x3ff, MapZ));
				pChunk = ChunkCache[cacheIdx];
				pMap = &(AreaTiles[j][i]);
				for(anY = 0; anY < 8; anY ++) {
					for(anX = 0; anX < 8; anX ++)
						*pMap++ = *pChunk++;
					pMap += 32;/*next line: AREA_W - 8*/
				}
			}
		}
		EGG_hatchArea();
	}
	UpdateMoongates();
	VisibilityDirty = 1;
}

/*empty the chunk cache*/
/*C_101C_052D*/MAP_InitChunkCache() {
	int16 i;

	for(i = 0; i < 0x10; i++) {
		ChunkCacheIdx[i] = -1;
		ChunkCacheAge[i] = i;
	}
}

/*load everything (objects regions, chunk indexes, area tiles)
  around MapX, MapY, MapZ*/
/*C_101C_054C*/MAP_LoadAround() {
	LoadNewRegions(MapX, MapY, MapZ);
	MAP_LoadChunkIndexes(MapX, MapY, MapZ);
	MAP_BuildArea();
}

/*gather the party on the leader's square (MapX, MapY) and take
  everybody off the map (z = 15), e.g. before a teleport or a level change.
  withAnimation: 0 = instant; else members walk to the leader for up to 30
  steps, with 1 the screen fades when a member disappears*/
/*C_101C_0572*/PartyEnter(int16 withAnimation) {
	int16 i;

	/*dismount horses*/
	for(i = 0; i < PartySize; i ++) {
		if(GetType(Party[i]) == OBJ_MOUNTED_HORSE)
			DismountHorse(Party[i]);
	}

	MAP_LoadAround();

	if(withAnimation) {
		int16 j, allArrived;
		for(j = 0; j < 30; j ++) {
			allArrived = 1;
			for(i = 0; i < PartySize; i ++) {
				int16 objNum = Party[i];
				/*wake up*/
				if(IsAsleep(objNum))
					ObjShapeType[objNum] = OrigShapeType[objNum];
				ClrAsleepParalyzed(objNum);
				if(GetZ(objNum) == MapZ && !(IsIncapacitated(objNum) || IsDraggedUnder(objNum))) {
					if(GetX(objNum) == MapX && GetY(objNum) == MapY) {
						/*arrived: remove from map*/
						if(withAnimation) {
							ShowObjects();
							DisplayTick();
						}
						MoveAppliesTerrain = 0;
						MoveObj(objNum, MapX, MapY, 15);
						MoveAppliesTerrain = 1;
						if(withAnimation == 1) {
							ScreenFade = 1;
							ShowObjects();
							DisplayTick();
						}
					} else if(!TryMoveTo(objNum, MapX, MapY)) {
						/*blocked: jump if adjacent, else random step*/
						if(COMBAT_TileDistance(objNum, MapX, MapY) < 2)
							MoveObj(objNum, MapX, MapY, MapZ);
						else
							TryStraightMove(objNum, OSI_rand(0, 3) << 1, 1);
					} else {
						allArrived = 0;
					}
				}
				MovePts[objNum] = DEXTE[objNum];
			}
			if(withAnimation) {
				ShowObjects();
				DisplayTick();
			}
			if(allArrived)
				break;
		}
	}

	/*whoever is left (asleep, paralyzed, other level...) is taken anyway*/
	MoveAppliesTerrain = 0;
	for(i = 0; i < PartySize; i ++) {
		if(GetZ(Party[i]) != 15)
			MoveObj(Party[i], MapX, MapY, 15);
	}
	MoveAppliesTerrain = 1;
}

/*put the whole party back on the map at (x, y, z) and let the
  followers step out around the leader (5 steps)*/
/*C_101C_077E*/PartyExit(int16 x, int16 y, int16 z, int16 withAnimation) {
	int16 i;

	MoveAppliesTerrain = 0;
	for(i = 0; i < PartySize; i ++)
		MoveObj(Party[i], x, y, z);
	MoveAppliesTerrain = 1;

	SetPartyMode();

	if(withAnimation == 1) {
		ScreenFade = 1;
		ShowObjects();
		DisplayTick();
	}

	for(i = 0; i < 5; i ++) {
		MoveFollowers(Party[Active], 1);
		if(withAnimation) {
			ShowObjects();
			DisplayTick();
		}
	}
	for(i = 0; i < PartySize; i ++)
		MovePts[Party[i]] = DEXTE[Party[i]];
}

/*move the whole party to (x, y, z), removing the red gate it stood on*/
/*C_101C_0828*/PartyTeleport(int16 x, int16 y, int16 z, int16 withAnimation) {
	int16 gateObj;

	PartyEnter(withAnimation);

	gateObj = OBJ_FindTypeAt(OBJ_RED_GATE, MapX, MapY);
	if(gateObj >= 0)
		DeleteObj(gateObj);

	MapX = x;
	MapY = y;
	MapZ = z;
	MAP_LoadAround();

	ForceHatching = 1;
	EGG_hatchArea();
	ForceHatching = 0;

	VIS_UpdateView();
	DrawSkyBar();
	PassTime(0);
	PartyExit(x, y, z, withAnimation);
}

/*enter dungeon/use ladder: move the party one level up or down
  through objNum (ladder, hole, dungeon entrance...)*/
/*C_101C_089E*/PartyChangeLevel(int16 objNum) {
	int16 z_incr;

	if(InCombat)
		COMBAT_breakOff();
	/*up from the gargoyle land (5) or with an "up" ladder*/
	if(MapZ && (MapZ == 5 || ObjShapeType[objNum] == TypeFrame(OBJ_LADDER, 1)))
		z_incr = -1;
	else
		z_incr = 1;
	MapX = GetX(objNum);
	MapY = GetY(objNum);
	PartyEnter(2);
	MapZ += z_incr;
	if(MapZ == 1 && z_incr == 1) {
		/*surface -> level 1: chunk coordinates / 4, same tile in chunk*/
		MapX = ((MapX >> 2) & 0xf8) + (MapX & 7);
		MapY = ((MapY >> 2) & 0xf8) + (MapY & 7);
	} else if(MapZ == 0 && z_incr == -1) {
		/*level 1 -> surface: chunk coordinates * 4, the ladder's quality
		  says which of the 4x4 surface chunks is the exit*/
		MapX = ((MapX << 2) & 0x3e0) + (MapX & 7);
		MapY = ((MapY << 2) & 0x3e0) + (MapY & 7);
		if(GetQual(objNum) & 1)
			MapX += 8;
		if(GetQual(objNum) & 2)
			MapX += 0x10;
		if(GetQual(objNum) & 4)
			MapY += 8;
		if(GetQual(objNum) & 8)
			MapY += 0x10;
	}
	MAP_LoadAround();
	VIS_UpdateView();
	/*going between outside and inside: refresh sky and light*/
	if(
		MapZ == 0 || MapZ == 5 ||
		(MapZ == 1 && z_incr == 1) ||
		(MapZ == 4 && z_incr == -1)
	) {
		DrawSkyBar();
		PassTime(0);
	}
	PartyExit(MapX, MapY, MapZ, 2);
	MUS_PlayAreaSong();
}

/*travel through a moongate to the place of moonstone #gateIdx
  (stay in place if that moonstone has no position)*/
/*C_101C_0A3A*/GateTravel(int16 gateIdx/*bp06*/) {
	if(InCombat)
		COMBAT_breakOff();
	if(MoonstonePos[gateIdx][0] || MoonstonePos[gateIdx][1] || MoonstonePos[gateIdx][2])
		PartyTeleport(MoonstonePos[gateIdx][0], MoonstonePos[gateIdx][1], MoonstonePos[gateIdx][2], 1);
	else
		PartyTeleport(MapX, MapY, MapZ, 1);
	MUS_PlayAreaSong();
}

/*revive/help: after the party's death (or the "help" spell),
  resurrect the dead party members, heal everybody and bring the party
  to Lord British's castle, then wait for the next audience hours*/
/*C_101C_0ACC*/PartyResurrect() {
	int16 i, memberObj;
	int16 bodyObj;

	Active = -1;
	for(i = 0; i < 0xe0; i ++) {
		if(IsPlrControl(i) && IsDead(i)) {
			/*look for the NPC's dead body (OBJ_MOUSE: Sherry?)*/
			for(bodyObj = Link[0x100]; bodyObj >= 0; bodyObj = Link[bodyObj]) {
				if(GetType(bodyObj) == OBJ_DEAD_BODY || GetType(bodyObj) == OBJ_MOUSE)
				if(GetQual(bodyObj) == i) {
					Selection.obj = bodyObj;
					if(i == 1) {
						/*the avatar: full magic, karma-based experience loss*/
						OBJ_ReviveFromBody(bodyObj);
						ClrDead(1);
						ObjShapeType[1] = OrigShapeType[1];
						MAGIC[1] = INTEL[1];
						ClrAilmentsProtected(1);
						if(KARMA < 98)
							ExpPoints[1] = ((int32)ExpPoints[1] * KARMA) / 100L;
					} else {
						MoveAppliesTerrain = 0;
						SPELL_ReviveNPC(bodyObj);
					}
					break;
				}
			}
		}
		if(NPCMode[i] == AI_VIGILANTE)
			NPCMode[i] = AI_FINDPATH;
	}
	for(i = 0; i < PartySize; i ++) {
		memberObj = Party[i];
		if(IsAsleep(memberObj)) {
			ClrAsleep(memberObj);
			ObjShapeType[memberObj] = OrigShapeType[memberObj];
		}
		SetAlignment(memberObj, GOOD);
		ClrCursedCharmedInvis(memberObj);
		ClrAilments(memberObj);
		HitPoints[memberObj] = MaxHP(memberObj);
		MovePts[memberObj] = DEXTE[memberObj];
	}
	/*no more guards after the party*/
	for(i = 0xe0; i < 0x100; i ++) {
		if(GetType(i) == OBJ_GUARD)
			DeleteObj(i);
	}
	/*cancel all spell effects*/
	for(i = 0; i < 16; i ++)
		SpellFx[i] = 0;
	for(i = 0; i < 0x40; i ++)
		MissileHitList[i] = -1;
	SetPartyMode();
	COMBAT_breakOff();
	PartyTeleport(0x133, 0x160, 0, 0);
	/*let time pass until 8-11h or 14-17h*/
	AllowNPCTeleport = 1;
	while(
		Time_H < 8 ||
		(Time_H > 11 && Time_H < 14) ||
		Time_H > 17
	) {
		PassTime(15);
		PTH_SearchPaths();
	}
	AllowNPCTeleport = 0;
	ScreenFade = 1;
	ShowObjects();
	MUS_PlaySfx(21, 0);
	DisplayTick();
	MUS_PlayAreaSong();
	StatusDirty = 1;
	MoveAppliesTerrain = 1;
}

/*12-bit index of the chunk containing tile (x, y, z)
  (loads the needed part of the map first)*/
static /*C_101C_0CF7*/getChunkIdxAt(int16 x, int16 y, int16 z) {
	int16 bufPos;
	int16 chunkIdx, mapId, quadOfs;

	x &= 0x3ff;
	y &= 0x3ff;
	MAP_LoadChunkIndexes(x, y, z);
	if(z != 0) {
		/*32x32 chunks: (chunkY * 32 + chunkX) * 1.5*/
		bufPos = (x >> 3) & 0x1f;
		bufPos += (y << 2) & 0x3e0;
		bufPos += bufPos >> 1;
		/*odd chunk: upper 12 bits, even chunk: lower 12 bits*/
		if(x & 8)
			chunkIdx = *((int16 far *)(ChunkIndexBuf + bufPos)) >> 4;
		else
			chunkIdx = *((int16 far *)(ChunkIndexBuf + bufPos)) & 0xfff;
	} else {
		/*which superchunk of the 2x2 window (x 0x100 chunk entries)*/
		mapId = MK_MAP_ID(x, y);
		quadOfs = 0;
		if((mapId - LoadedSuperChunks[0]) & 1)
			quadOfs = 0x100;
		if((mapId - LoadedSuperChunks[0]) & 8)
			quadOfs += 0x200;
		/*16x16 chunks: (quadOfs + chunkY * 16 + chunkX) * 1.5*/
		bufPos = ((x >> 3) & 0xf) + quadOfs;
		bufPos += (y << 1) & 0xf0;
		bufPos += bufPos >> 1;
		if(x & 8)
			chunkIdx = *((int16 far *)(ChunkIndexBuf + bufPos)) >> 4;
		else
			chunkIdx = *((int16 far *)(ChunkIndexBuf + bufPos)) & 0xfff;
	}

	return chunkIdx;
}

/*ground tile at (x, y, z): from AreaTiles if inside the current area,
  else from the chunks*/
unsigned char /*C_101C_0DC9*/GetTileAtXYZ(int16 x, int16 y, int16 z) {
	int16 cacheIdx;
	int16 relX, relY;

	x &= MapCoordMask;
	y &= MapCoordMask;
	relX = (x - AreaX) & MapCoordMask;
	relY = (y - AreaY) & MapCoordMask;
	if(relX < 40 && relY < 40)
		return AreaTiles[relY][relX];

	cacheIdx = cacheChunk(getChunkIdxAt(x, y, z));
	return ChunkCache[cacheIdx][y & 7][x & 7];
}
