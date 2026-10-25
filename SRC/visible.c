/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	module "to compute only visible part of the area"

	VIS_UpdateView builds Tile_11x11, the 11x11 ground tiles actually shown
	around the party: it marks in AreaFlags what the objects of the area
	do to sight and light, then runs a breadth-first flood fill
	(VIS_FloodFill) from the party's square and from every visible light
	source, accumulating light in AreaLight. Squares never reached are
	drawn as TIL_HIDDEN, reached but unlit ones as TIL_DARKNESS, and wall
	tiles seen from one side only are replaced by a variant showing only
	their visible faces.

	AreaFlags bits (per area square):
	  0x03 light level of the brightest object on the square
	  0x04 a "background" object is there (does not block sight)
	  0x08 a wall object is there
	  0x10 a window object is there
	  0x20 an opaque object is there
	  0x40 already queued by the current flood fill
	  0x80 visible from the party
*/
#include "u6.h"

/*05E4*/int16 ViewX = 12;
/*05E6*/int16 ViewY = 12;
/*05E8*/int16 XRayView = 0;/*XRay vision/see through walls: show all tiles, fully lit (X-ray spell, wizard eye...)*/

/*rounded euclidean distance: DistanceTable[|dx|][|dy|] ~ sqrt(dx*dx + dy*dy)*/
/*05EA*/static char DistanceTable[][8] = {
	{0,1,2,3,4,5,6,7},
	{1,1,2,3,4,5,6,7},
	{2,2,3,4,5,6,6,7},
	{3,3,4,4,5,6,7,8},
	{4,4,5,5,6,7,7,8},
	{5,5,6,6,7,7,8,9},
	{6,6,6,7,7,8,8,9},
	{7,7,7,8,8,9,9,10}
};

/*flood fill queue (ring buffer of 256 in VisQueueX/VisQueueY)*/
/*062A*/static unsigned char visited_w = 0;
/*062B*/static unsigned char visited_r = 0;

/*cumulative steps that walk around the 8 neighbours of a square,
  starting from the center: N, NE, E, SE, S, SW, W, NW*/
/*062C*/static char NeighbourStepX[] = {      0, 1,      0,      0, -1, -1,       0,       0};/*delta_x*/
/*0634*/static char NeighbourStepY[] = {-     1, 0,      1,      1,  0,  0, -     1, -     1};/*delta_y*/
/*063C*/static char NeighbourStepOfs[] = {-AREA_W, 1, AREA_W, AREA_W, -1, -1, -AREA_W, -AREA_W};/*delta_y * AREA_W + delta_x*/

/*wall shapes: tile offset inside a group of 16 wall tiles, indexed by
  the wall's connections restricted to the visible sides
  (bit 8 = N, 4 = E, 2 = S, 1 = W); indexes 3 and 12 use the extra
  corner tiles at TIL_WALL_CORNERS (see VIS_UpdateView)*/
/*0644*/static unsigned char WallShapeOfs[] = {
	 0,/*----*/
	 0,/*---W*/
	 1,/*--S-*/
	10,/*--SW*/
	 0,/*-E--*/
	 0,/*-E-W*/
	 2,/*-ES-*/
	 2,/*-ESW*/
	 1,/*N---*/
	 5,/*N--W*/
	 1,/*N-S-*/
	 1,/*N-SW*/
	11,/*NE--*/
	 0,/*NE-W*/
	 2,/*NES-*/
	 2 /*NESW*/
};

/*can sight/light go through the square AreaOffset (x, y are its area
  coordinates)? opaque objects block, windows are only see-through
  from an orthogonally adjacent square of the party, opaque ground
  tiles block unless a background object is on them*/
static /*C_1100_000B*/VIS_IsSeeThrough(int16 x, int16 y) {
	int16 flag, tile;

	tile = AreaTiles[0][AreaOffset];
	flag = AreaFlags[0][AreaOffset];
	if(flag & 0x20)
		return 0;
	if(IsTileWin(tile) || (flag & 0x10)) {
		return
			(ViewX + 5 == x && abs(ViewY + 5 - y) < 2) ||
			(ViewY + 5 == y && abs(ViewX + 5 - x) < 2)
		;
	}
	if(!(flag & 0x04) && IsTileOpa(tile))
		return 0;
	return 1;
}

/*uncalled?*/
/*is (x, y) in the 11x11 view square around the party?*/
/*__1100_009F*/static VIS_IsInViewSquare(int16 x, int16 y) {
	return CLOSE_ENOUGH(5, x, y, MapX, MapY);
}

/*queue area square (x, y) for the flood fill and mark it 0x40
  (the queue silently drops squares when full)*/
static /*C_1100_00CF*/VIS_Enqueue(int16 x, int16 y) {
	if(x >= 0 && x < AREA_W && y >= 0 && y < AREA_H) {
		VisQueueX[visited_w] = x;
		VisQueueY[visited_w] = y;
		visited_w ++;
		if(visited_w == visited_r)
			visited_w --;
		else
			AreaFlags[y][x] |= 0x40;
	}
}

/*flood fill from area square (x, y): every square reached gets
  light = clamp(4 - distance + lightBase, 0..4) added to AreaLight;
  the fill spreads from see-through squares to their neighbours.
  mode 1: party's view (marks squares visible 0x80, limited to the
          11x11 view plus a 1-square border),
  mode 2: "peer" (marks visible, whole area; note DistanceTable is then read
          beyond its 8x8 bounds),
  mode 0: light source (spreads up to distance lightBase + 3).
  If (x, y) itself blocks sight (e.g. a torch on a wall), the fill
  starts from its east and south neighbours instead.*/
/*C_1100_0131*/VIS_FloodFill(int16 x, int16 y, int16 lightBase, int16 mode) {
	int16 curX, curY;
	int16 dir, light;

	/*AreaOffset = y * AREA_W + x*/
	AreaOffset = y << 3;
	AreaOffset += AreaOffset << 2;
	AreaOffset += x;
	if(VIS_IsSeeThrough(x, y)) {
		VIS_Enqueue(x, y);
	} else {
		AreaOffset ++;
		if(VIS_IsSeeThrough(x + 1, y))
			VIS_Enqueue(x + 1, y);
		AreaOffset += (AREA_W-1);
		if(VIS_IsSeeThrough(x, y + 1))
			VIS_Enqueue(x, y + 1);
	}
	while(visited_w != visited_r) {
		curX = VisQueueX[visited_r];
		curY = VisQueueY[visited_r];
		visited_r ++;
		AreaOffset = curY << 3;
		AreaOffset += AreaOffset << 2;
		AreaOffset += curX;

		light = 4 - DistanceTable[abs(curX - x)][abs(curY - y)] + lightBase;
		if(light < 0)
			light = 0;
		else if(light > 4)
			light = 4;
		AreaLight[0][AreaOffset] += light;

		if(mode)
			AreaFlags[0][AreaOffset] |= 0x80;
		if(VIS_IsSeeThrough(curX, curY)) {
			for(dir = 0; dir < 8; dir ++) {
				curX += NeighbourStepX[dir];
				curY += NeighbourStepY[dir];
				AreaOffset += NeighbourStepOfs[dir];
				if(AreaFlags[0][AreaOffset] & 0x40)
					continue;
				if(mode == 1) {
					if(
						ViewX - 1 > curX || ViewX + 11 < curX ||
						ViewY - 1 > curY || ViewY + 11 < curY
					) continue;
					VIS_Enqueue(curX, curY);
				} else if(mode == 2) {/*for peer*/
					VIS_Enqueue(curX, curY);
				} else if(DistanceTable[abs(curX - x)][abs(curY - y)] <= lightBase + 3) {
					VIS_Enqueue(curX, curY);
				}
			}
		}
	}
}

/*compute the visible 11x11 tiles (Tile_11x11) and the light of the
  area around the party, then redraw the map window*/
/*C_1100_0306*/VIS_UpdateView() {
	unsigned char *pTiles,*pFlags,*pLight;
	int16 *pView;
	int16 cellX,cellY,x,y;
	int16 tile,terrain,wallBase,visibleSides,wallSides,nbTile,nbTerrain,objNum;
	int16 light;

	if(XRayView) {
		/*X-ray: everything visible and lit*/
		for(cellY = 0; cellY < 11; cellY ++) {
			for(cellX = 0; cellX < 11; cellX ++)
				Tile_11x11[cellY][cellX] = AreaTiles[cellY + ViewY][cellX + ViewX];
		}
		for(cellX = 8; cellX < 32; cellX ++) {
			for(cellY = 8; cellY < 32; cellY ++)
				AreaLight[cellX][cellY] = 4;
		}
		ShowObjects();
		return;
	}

	/*clear flags and light of the central 24x24 squares*/
	pFlags = &(AreaFlags[8][8]);
	pLight = &(AreaLight[8][8]);
	for(cellX = 8; cellX < 32; cellX ++) {
		for(cellY = 8; cellY < 32; cellY ++) {
			*pFlags++ =
			*pLight++ = 0;
		}
		pFlags += 16;
		pLight += 16;
	}
	/*cellX, cellY = area origin (same as AreaX, AreaY)*/
	cellX = (MapX - 0x10) & 0xfff8;
	cellY = (MapY - 0x10) & 0xfff8;
	/*flag what the objects of the central squares do to sight and light*/
	for(
		objNum = SearchArea(cellX + 8, cellY + 8, cellX + 31, cellY + 31);
		objNum >= 0;
		objNum = NextArea()
	) {
		x = GetX(objNum) - cellX;
		y = GetY(objNum) - cellY;
		pFlags = &(AreaFlags[y][x]);
		tile = TILE_FRAME(objNum);

		if(IsTileWin(tile))
			*pFlags |= 0x10;
		else if(IsTileOpa(tile))
			*pFlags |= 0x20;
		else if(IsTileBa(tile))
			*pFlags |= 0x04;

		/*double-size objects also cover the square above/left*/
		if(IsTileOpa(tile - 1)) {
			if(IsTileDoubleV(tile) && y)
				*(pFlags - AREA_W) |= 0x20;
			if(IsTileDoubleH(tile) && x)
				*(pFlags - 1) |= 0x20;
		}

		if(IsTerrainWall(tile)) {
			*pFlags |= 0x08;
			if(IsTileDoubleV(tile) && y)
				*(pFlags - AREA_W) |= 0x08;
			if(IsTileDoubleH(tile) && x)
				*(pFlags - 1) |= 0x08;
		}

		if(GetTileLight(tile)) {
			light = GetTileLight(tile);
			if((*pFlags & 0x03) < light)
				*pFlags = (*pFlags & ~0x03) | light;
		}
	}

	/*what the party sees, lit by the ambient light*/
	VIS_FloodFill(ViewX + 5, ViewY + 5, AmbientLight, 1);

	/*light sources(if night)?*/
	/*every visible light source adds its own light (radius = its level)*/
	if(AmbientLight < 7) {
		pFlags = &(AreaFlags[8][8]);
		pTiles = &(AreaTiles[8][8]);
		for(cellY = 8; cellY < 32; cellY ++) {
			for(cellX = 8; cellX < 32; cellX ++) {
				if((*pFlags & 0x03) && (*pFlags & 0x80)) {
					/*a light on a wall only counts if the square south of it is visible*/
					if(
						IsTerrainWall(*pTiles) &&
						!((*(pFlags + AREA_W) & 0x80) && (*(pFlags + AREA_W) & 0x80))/*bug?why twice?*/
					) continue;
					/*forget the "queued" marks around it*/
					for(x = cellX - 3; x <= cellX + 3; x ++) {
						for(y = cellY - 3; y <= cellY + 3; y ++)
							AreaFlags[y][x] &= ~0x40;
					}
					VIS_FloodFill(cellX, cellY, (*pFlags & 0x03) - 4, 0);
				}
				pFlags ++;
				pTiles ++;
			}
			pFlags += 16;
			pTiles += 16;
		}
	}

	/*build the 11x11 view*/
	pView = &(Tile_11x11[0][0]);
	pLight = &(AreaLight[ViewY][ViewX]);
	pFlags = &(AreaFlags[ViewY][ViewX]);
	pTiles = &(AreaTiles[ViewY][ViewX]);
	for(cellY = 0; cellY < 11; cellY ++) {
		for(cellX = 0; cellX < 11; cellX ++) {
			x = cellX + ViewX;
			y = cellY + ViewY;
			if(!(*pFlags & 0x80)) {
				tile = TIL_HIDDEN;/*hidden*/
			} else if(*pLight == 0) {
				tile = TIL_DARKNESS;/*darkness*/
			} else {
				tile = *pTiles;
				/*"hide inside wall" patch?*/
				/*a plain wall tile (wall + impassable) not seen from all
				  4 sides is replaced by the wall shape showing only the
				  sides that are visible*/
				terrain = TerrainType[tile] & 0x0f;
				if(terrain == (TERRAIN_WALL|TERRAIN_IMPASSABLE)) {
					/*first tile of its group of 16 wall tiles*/
					wallBase = tile & 0x0f0;
					if(wallBase < TIL_WALL_FIRST)
						wallBase = TIL_WALL_FIRST;
					/*which neighbours are visible (8 N, 4 E, 2 S, 1 W)*/
					visibleSides = 0;
					if(*(pFlags - AREA_W) & 0x80)
						visibleSides |= 8;
					if(*(pFlags + 1) & 0x80)
						visibleSides |= 4;
					if(*(pFlags + AREA_W) & 0x80)
						visibleSides |= 2;
					if(*(pFlags - 1) & 0x80)
						visibleSides |= 1;
					if(visibleSides != 0xf) {
						/*the wall's connections: upper nibble of TerrainType
						  (TERRAIN_WALL_N..10 -> 8..1)*/
						wallSides = TerrainType[tile] >> 4;
						/*with bit 4, the N connection is dropped unless the
						  north square has a wall object or is a wall tile
						  with TERRAIN_WALL_S*/
						if(wallSides & 4) {
							nbTile = *(pTiles - AREA_W);
							nbTerrain = TerrainType[nbTile] & 0x0f;
							if(
								!(*(pFlags - AREA_W) & 0x08) &&
								(!(nbTerrain & TERRAIN_WALL) || !(TerrainType[nbTile] & TERRAIN_WALL_S))
							) wallSides &= ~8;
						}
						/*with bit 2, the same for the W connection with the
						  west square and TERRAIN_WALL_E*/
						if(wallSides & 2) {
							nbTile = *(pTiles - 1);
							nbTerrain = TerrainType[nbTile] & 0x0f;
							if(
								!(*(pFlags - 1) & 0x08) &&
								(!(nbTerrain & TERRAIN_WALL) || !(TerrainType[nbTile] & TERRAIN_WALL_E))
							) wallSides &= ~1;
						}
						if(wallSides == (4|2|1) || wallSides == (8|2|1) || wallSides > (8|4) || wallSides == visibleSides) {
							wallSides &= visibleSides;
							if(wallSides == (2|1) || wallSides == (8|4))
								/*corner: 2 extra tiles per wall group at TIL_WALL_CORNERS*/
								tile = TIL_WALL_CORNERS + (wallBase >> 3) - (TIL_WALL_FIRST >> 3) + WallShapeOfs[wallSides];
							else
								tile = wallBase + WallShapeOfs[wallSides];
						}
					}
				}

			}
			*pView++ = tile;
			pFlags ++;
			pTiles ++;
			pLight ++;
		}
		pFlags += 29;/*AREA_W - 11*/
		pTiles += 29;
		pLight += 29;
	}
	ShowObjects();
}
