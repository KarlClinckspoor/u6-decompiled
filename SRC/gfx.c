/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Module GraphicFX, missile, peer, ...

	- loading/initialisation of the graphic driver U6xxx.DRV (InitGraphicsDriver).
	  The driver is called through the GR_xx macros (gr.h): GrDriver holds
	  its far address, the offset part being patched with the entry number
	  before each call. GrInfo is the driver's info/state block.
	- projectile animation (Missiles): up to 16 sprites moving with a
	  Bresenham line algorithm from one source tile to several destination
	  tiles, with rotation, sound, collision with walls/objects and a list
	  of NPCs hit (MissileHitList). Explosion/MagicWind/web/hail storm build on it.
	  The animation is drawn in the 160x160 offscreen map buffer; a "dirty
	  rectangle" (DirtyRect) is accumulated and copied to screen (GR_COPY_TO_SCREEN).
	- misc. effects: magical halo, screen shake.
	- the sky bar (sun/moons) above the status area, portraits and
	  conversation scripts loading (portrait.?/converse.?), the paper-doll
	  background, and the "peer" (gem) overview map of the loaded area.

	Graphic driver sprite slots (as used here): GR_DEFINE_SPRITE defines a slot from a
	tile (with rotation and pivot), GR_SAVE_SPRITE_BG saves the background under a slot
	at x,y into that slot, GR_DRAW_SPRITE draws a slot at x,y. Slots 0-7 are used as
	saved backgrounds, slots 8-15 as missile images.
*/
#include "u6.h"

#undef abs
#include <math.h>

/*graphic drivers, indexed by video adapter type (GrAdapter)*/
/*2A34*/char *GrDriverFiles[] = {
	/*2C1F*/"U6MCGA.DRV",
	/*2C2A*/"U6EGA.DRV",
	/*2C34*/"U6TANDY.DRV",
	/*2C40*/"U6CGA.DRV",
	/*2C40*/"U6CGA.DRV"
};
/*driver init parameter, indexed by adapter type:
  BIOS video mode (MCGA/EGA/Tandy) or video segment (CGA/Hercules)*/
/*2A3E*/int16 GrDriverParam[] = {
	0x13,
	0xD,
	0x9,
	0xB800,
	0xB000
};
/*2A48*/int16 ScreenFade = 1;
/*colors (VGA/EGA palette indexes)*/
/*2A4A*/int16 ColorRed = 12;/*light red: low HP, conversation keywords, peer:damaging terrain*/
/*2A4C*/int16 ColorGreen = 10;/*light green: poisoned, peer:passable terrain*/
/*2A4E*/int16 ColorBlue = 9;/*light blue: default command underline, peer:deep water*/
/*2A50*/int16 ColorMagenta = 13;/*light magenta: set before map redraw (DrawMapWindow)*/
int16 ColorYellow = 14;/*yellow: unused*/
/*2A54*/int16 ColorPaper = 0x31;/*background ("paper") color*/
/*2A56*/int16 ColorText = 0x48;/*text color*/
/*2A58*/int16 ColorBlack = 0;/*black: clock hands*/
/*2A5A*/int16 ColorGray = 7;/*light gray: peer:impassable terrain*/
/*====---- ----====*/

/*init gr:
  load the graphic driver for adapter type "adapter" (0:MCGA 1:EGA 2:Tandy
  3:CGA 4:Hercules), initialise it and give it the callbacks it needs
  (floppy request, file loading/open/read/close)*/
/*C_2FC1_000A*/InitGraphicsDriver(int16 adapter/*bp06*/) {
	/*-- load graphic driver*/
	IsZFile = 1;
	GrDriver.p = LoadFile(GrDriverFiles[adapter], 0L);
	IsZFile = 0;
	GrInfo = GR_INIT(GrDriverParam[adapter] + GrDriverOption);
	GrInfo->pfnDiskConfirm = DISK_confirm;
	GrInfo->pfnLoadFile = LoadFileGr;
	GrInfo->pfnOpen = OSI_open;
	GrInfo->pfnRead = OSI_read;
	GrInfo->pfnClose = OSI_close;
	/*non VGA: paper is white and text brown*/
	if(adapter) {
		ColorPaper = 0xf;
		ColorText = 6;
	}
}

/*briefly flash object objNum (if on the map and in view) with a
  highlight effect: the high bits of the tile number (above 0x7ff) are
  drawing attributes for the driver; isTarget selects the effect
  (6<<10 for the target of a spell/item, 4<<10 for the caster)*/
/*C_2FC1_00B0*/MagicalHalo(int16 objNum, int16 isTarget) {
	int16 delta_x,delta_y,tileFrame;

	if(GetCoordUse(objNum) != LOCXYZ)
		return;
	delta_x = (GetX(objNum) - MapX + 5) & 0x3ff;
	delta_y = (GetY(objNum) - MapY + 5) & 0x3ff;
	tileFrame = TILE_FRAME(objNum) + (isTarget?(6 << 10):(4 << 10));
	if(delta_x < 11 && delta_y < 11) {
		ShowObject(tileFrame, delta_x, delta_y, 2);
		ScreenFade = 1;
		DrawMapWindow(1);
		ShowObjects();
		ScreenFade = 1;
		DrawMapWindow(1);
	}
}

/*redefineClipping:
  extend the dirty rectangle DirtyRect (left,right,top,bottom) with the
  16x16 square centered on x,y (map buffer coordinates), clamped to the
  map window (8..167)*/
/*C_2FC1_017F*/static ExtendDirtyRect(int16 x, int16 y) {
	if(x < 16)
		DirtyRect[0] = 8;
	else if(x - 8 < DirtyRect[0])
		DirtyRect[0] = x - 8;

	if(y < 16)
		DirtyRect[2] = 8;
	else if(y - 8 < DirtyRect[2])
		DirtyRect[2] = y - 8;

	if(x > 159)
		DirtyRect[1] = 167;
	else if(x + 8 > DirtyRect[1])
		DirtyRect[1] = x + 8;

	if(y > 159)
		DirtyRect[3] = 167;
	else if(y + 8 > DirtyRect[3])
		DirtyRect[3] = y + 8;
}

/*redraw frame if dirty:
  redraw (in the offscreen buffer) the tiles of the map window border
  that the rectangle x0,y0-x1,y1 overlaps*/
/*C_2FC1_0212*/RedrawMapBorder(int16 x0, int16 y0, int16 x1, int16 y1) {
	int16 i;

	if(x0 < 16 && y0 < 16)
		GR_PUT_TILE(TIL_FRAME_0, 0, 0);
	if(x1 > 159 && y0 < 16)
		GR_PUT_TILE(TIL_FRAME_2, 160, 0);
	if(x0 < 16 && y1 > 159)
		GR_PUT_TILE(TIL_FRAME_3, 0, 160);
	if(x1 > 159 && y1 > 159)
		GR_PUT_TILE(TIL_FRAME_5, 160, 160);
	for(i = 1; i < 10; i ++) {
		if(y0 < 16)
			GR_PUT_TILE(TIL_FRAME_1, TIL2SCR(i), 0);
		if(y1 > 159)
			GR_PUT_TILE(TIL_FRAME_4, TIL2SCR(i), 160);
		if(x0 < 16)
			GR_PUT_TILE(TIL_FRAME_6, 0, TIL2SCR(i));
		if(x1 > 159)
			GR_PUT_TILE(TIL_FRAME_7, 160, TIL2SCR(i));
	}
}

/*redraw, over the sprites, what must stay in front of them in the
  rectangle x0,y0-x1,y1 of the offscreen buffer: foreground tiles
  (TILE_FOREGROUND) of the objects, hidden/dark tiles and the window border*/
/*C_2FC1_032B*/static RedrawForeground(int16 x0, int16 y0, int16 x1, int16 y1) {
	int16 tx, ty;
	int16 entry, tile;

	for(tx = SCR2TIL(x0); tx <= SCR2TIL(x1); tx ++) {
		for(ty = SCR2TIL(y0); ty <= SCR2TIL(y1); ty ++) {
			/*objects' tiles of this cell: linked list CellObjNext, tiles in CellObjTile*/
			for(entry = Obj_11x11[ty][tx]; entry; entry = CellObjNext[entry]) {
				tile = CellObjTile[entry];
				if(tile && IsTileFor(tile & 0x7ff))
					GR_PUT_TILE(tile & 0x7ff, TIL2SCR(tx), TIL2SCR(ty));
			}
			if(Tile_11x11[ty][tx] == TIL_HIDDEN || Tile_11x11[ty][tx] == TIL_DARKNESS)
				GR_PUT_TILE(TIL_HIDDEN, TIL2SCR(tx), TIL2SCR(ty));
		}
	}
	RedrawMapBorder(x0, y0, x1, y1);
}

/*distance.rotation angles map:
  angle of the vector (dx,dy) indexed [dy + 7][dx + 7];
  0x80 is a full turn, 0x00=east, 0x20=north, 0x40=west, 0x60=south*/
/*2A5C*/unsigned char MissileAngles[][16] = {
	{0x30,0x2E,0x2D,0x2B,0x28,0x26,0x23,0x20,0x1D,0x1A,0x18,0x15,0x13,0x12,0x10,0x0F},
	{0x32,0x30,0x2E,0x2C,0x29,0x26,0x23,0x20,0x1D,0x1A,0x17,0x14,0x12,0x10,0x0E,0x0D},
	{0x34,0x32,0x30,0x2E,0x2B,0x27,0x24,0x20,0x1C,0x19,0x15,0x12,0x10,0x0E,0x0D,0x0B},
	{0x35,0x34,0x32,0x30,0x2D,0x29,0x25,0x20,0x1B,0x17,0x13,0x10,0x0E,0x0C,0x0B,0x09},
	{0x38,0x37,0x35,0x33,0x30,0x2C,0x26,0x20,0x1A,0x14,0x10,0x0D,0x0B,0x09,0x08,0x07},
	{0x3A,0x3A,0x39,0x37,0x34,0x30,0x29,0x20,0x17,0x10,0x0C,0x09,0x07,0x06,0x06,0x05},
	{0x3D,0x3D,0x3C,0x3B,0x3A,0x37,0x30,0x20,0x10,0x09,0x06,0x05,0x04,0x03,0x03,0x03},
	{0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x43,0x43,0x44,0x45,0x46,0x49,0x50,0x60,0x70,0x77,0x7A,0x7B,0x7C,0x7D,0x7D,0x7D},
	{0x46,0x46,0x47,0x49,0x4C,0x50,0x57,0x60,0x69,0x70,0x74,0x77,0x79,0x7A,0x7A,0x7B},
	{0x48,0x49,0x4B,0x4D,0x50,0x54,0x5A,0x60,0x66,0x6C,0x70,0x73,0x75,0x77,0x78,0x79},
	{0x4B,0x4C,0x4E,0x50,0x53,0x57,0x5B,0x60,0x65,0x69,0x6D,0x70,0x72,0x74,0x75,0x77},
	{0x4D,0x4E,0x50,0x52,0x55,0x59,0x5C,0x60,0x64,0x67,0x6B,0x6E,0x70,0x72,0x73,0x75},
	{0x4E,0x50,0x52,0x54,0x57,0x5A,0x5D,0x60,0x63,0x66,0x69,0x6C,0x6E,0x70,0x72,0x73},
	{0x50,0x52,0x53,0x55,0x58,0x5A,0x5D,0x60,0x63,0x66,0x68,0x6B,0x6D,0x6E,0x70,0x71},
	{0x51,0x53,0x55,0x57,0x59,0x5B,0x5D,0x60,0x63,0x65,0x67,0x69,0x6B,0x6D,0x6F,0x70}
};
/*missile tiles; the following tables are indexed by the position of the
  projectile tile in this list ("misType", 0 = unknown tile)*/
/*2B5C*/uint16 MissileTiles[] = {
	0,
	TIL_SPEAR,TIL_THROWING_AXE,TIL_DAGGER,TIL_BOOMERANG,TIL_ARROW,
	TIL_BOLT,TIL_MISSILE_0,TIL_MISSILE_1,TIL_MAGIC_BOW,TIL_MISSILE_2,
	TIL_MISSILE_3,TIL_MISSILE_4,TIL_MISSILE_5,TIL_POISON_MISSILE,TIL_17D,
	TIL_FIRE_MISSILE,TIL_17F
};
/*angle phase?: angle at which the tile is drawn (subtracted from the flight angle)*/
/*2B80*/unsigned char MissileTileAngle[] = {0x40,0x10,0x00,0x10,0x00,0x00,0x00,0x20,0x20,0x00,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0x00};
/*rotation increment?: spin added at each redraw (axe, dagger, boomerang, ...)*/
/*2B92*/unsigned char MissileSpin[] = {0x00,0x00,0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x0A,0x0A,0x0A};
/*start x?: rotation pivot x in the tile (0x7f = center)*/
/*2BA4*/unsigned char MissilePivotX[] = {0x7F,0x75,0x7F,0x86,0x7A,0x71,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F};
/*start y?: rotation pivot y in the tile (0x7f = center)*/
/*2BB6*/unsigned char MissilePivotY[] = {0x7F,0x7F,0x7F,0x8B,0x7F,0x46,0x3D,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F};
/*refresh rate?: the sprite is redrawn every N pixels of movement (higher = faster)*/
/*2BC8*/unsigned char MissileRefresh[] = {0x06,0x03,0x02,0x02,0x04,0x04,0x04,0x02,0x02,0x03,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02};

#define MAX_MISSILES 16

/*animate num_dst projectiles (tile projTile, 0 = invisible) flying from
  tile src_x,src_y to the tiles pX_dst[],pY_dst[], one pixel per step
  (Bresenham). A projectile stops when it goes out of max_range (RangeTable
  distance), hits a tile that blocks missiles (TILE_BLOCKS_MISSILES; windows
  TILE_WINDOW are crossed when adjacent to the source) or an object
  whose tile blocks missiles.
  attr: 1 = boomerang return (flies from the destination back to the
            source; the destination is the thrower)
        2 = "trail" mode: the sprites are never erased (explosions,
            winds); slow fixed refresh rate
        4 = explosion-like sound
        8 = one non rotated sprite (slot 15) drawn at every position (web)
  Out:
   - pX_dst/pY_dst = tile where each projectile stopped,
   - MissileEnd = last tile reached by projectile #0,
   - MissileHitObj = object hit at the stop tile of #0 (if MissileShooter, the shooter, is set),
   - MissileHitList = list of the NPCs (< 0x100) crossed/hit, -1 terminated.
  returns 1 if every projectile reached its destination*/
/*C_2FC1_042D*/Missiles(
	int16 src_x, int16 src_y,
	int16 num_dst, int16 *pX_dst, int16 *pY_dst,
	int16 max_range, int16 projTile,
	int16 attr/*1=reverse,2=has trail,4=different sound,8=no rotation*/
) {
	int16 i;
	int16 dst;
	int16 objNum,refreshRate,temp,misType,delta_max;
	int16 tile_delta_x,tile_delta_y;
	int16 ret,whichDir;
	int16 spr_new_x,spr_new_y;
	int16 mustDisplay,numTile;
	int16 unused242;
	/*current position in world pixels (tile * 16 + 8)*/
	int16 cur_x[MAX_MISSILES],cur_y[MAX_MISSILES];
	/*Bresenham: major/minor deltas, increments [0]=straight [1]=diagonal step*/
	int16 delta_x[MAX_MISSILES],delta_y[MAX_MISSILES];
	int16 incr_x[MAX_MISSILES][2],incr_y[MAX_MISSILES][2];
	int16 b_error[MAX_MISSILES],berr_incr[MAX_MISSILES][2];

	/*last tile entered*/
	int16 tile_x[MAX_MISSILES],tile_y[MAX_MISSILES];
	int16 spr_rot[MAX_MISSILES];
	/*1 bit per NPC (0x100 NPCs)*/
	int16 collision_mask[0x100/8];
	/*sprite position in the map buffer*/
	int16 spr_x[MAX_MISSILES],spr_y[MAX_MISSILES];

#if 0
	CON_printf("Missiles %03x,%03x\n", src_x, src_y);
#endif
	unused242 = 0;/*unused?*/
	DrawMapWindow(0);

	for(i = 0; i < 0x100/8; i ++)
		collision_mask[i] = 0;

	HideMouse
	ret = 1;

	MissileEnd[0] =
	tile_x[0] = src_x;
	MissileEnd[1] =
	tile_y[0] = src_y;

	if(attr & 1) {
		temp = tile_x[0]; tile_x[0] = pX_dst[0]; pX_dst[0] = temp;
		temp = tile_y[0]; tile_y[0] = pY_dst[0]; pY_dst[0] = temp;
	}

	cur_x[0] = TIL2SCR(tile_x[0]) + 8;
	cur_y[0] = TIL2SCR(tile_y[0]) + 8;

	misType = 0;
	for(i = 0; i < 18; i ++) {
		if(MissileTiles[i] == projTile) {
			misType = i;
			break;
		}
	}

	if(attr & 2)
		refreshRate = 6;
	else
		refreshRate = MissileRefresh[misType];

	/*define the sprites: oriented toward each destination*/
	if(projTile) {
		if(attr & 8) {
			GR_DEFINE_SPRITE(15, projTile, 0, 0x7f, 0x7f);
		} else {
			for(dst = 0; dst < num_dst; dst ++) {
				spr_rot[dst] =
					MissileAngles[pY_dst[dst] - tile_y[0] + 7][pX_dst[dst] - tile_x[0] + 7] - MissileTileAngle[misType];
				if(attr & 2)
					GR_DEFINE_SPRITE(dst, projTile, spr_rot[dst], MissilePivotX[misType], MissilePivotY[misType]);
				else
					GR_DEFINE_SPRITE(dst + 8, projTile, spr_rot[dst], MissilePivotX[misType], MissilePivotY[misType]);
			}
		}
	}

	/*Bresenham setup*/
	delta_max = 0;
	for(dst = 0; dst < num_dst; dst ++) {
		cur_x[dst] = cur_x[0];
		cur_y[dst] = cur_y[0];
		tile_x[dst] = tile_x[0];
		tile_y[dst] = tile_y[0];
		delta_x[dst] = TIL2SCR(pX_dst[dst] - tile_x[0]);
		delta_y[dst] = TIL2SCR(pY_dst[dst] - tile_y[0]);
		incr_y[dst][0] = 0;
		if(delta_x[dst] < 0) {
			delta_x[dst] = -delta_x[dst];
			incr_x[dst][0] =
			incr_x[dst][1] = -1;
		} else {
			incr_x[dst][0] =
			incr_x[dst][1] = 1;
		}
		if(delta_y[dst] < 0) {
			delta_y[dst] = -delta_y[dst];

			incr_y[dst][1] = -1;
		} else {

			incr_y[dst][1] = 1;
		}
		/*y major: swap the roles*/
		if(delta_x[dst] < delta_y[dst]) {
			temp = delta_x[dst];
			delta_x[dst] = delta_y[dst];
			delta_y[dst] = temp;
			incr_y[dst][0] = incr_y[dst][1];
			incr_x[dst][1] = incr_x[dst][0];
			incr_x[dst][0] = 0;
		}
		if(delta_x[dst] > delta_max)
			delta_max = delta_x[dst];
		b_error[dst] = delta_y[dst] + delta_y[dst] - delta_x[dst];
		berr_incr[dst][0] = delta_y[dst] + delta_y[dst];
		berr_incr[dst][1] = (delta_y[dst] - delta_x[dst]) << 1;
	}

	/*one pixel per step; delta_x[dst] is the number of steps of
	  projectile dst (set to the current step when it stops)*/
	for(i = 0; i < delta_max; i ++) {
		mustDisplay = 0;
		/*empty dirty rectangle*/
		DirtyRect[0] = DirtyRect[2] = 168;
		DirtyRect[1] = DirtyRect[3] = 7;
		for(dst = 0; dst < num_dst; dst ++) {
			if(delta_x[dst] <= i) {
				/*arrived/stopped: its last place must still be refreshed*/
				if(i)
					ExtendDirtyRect(spr_x[dst], spr_y[dst]);
				continue;
			}
			whichDir = (b_error[dst] >= 0);
			b_error[dst] += berr_incr[dst][whichDir];
			cur_x[dst] += incr_x[dst][whichDir];
			cur_y[dst] += incr_y[dst][whichDir];

			if(projTile && refreshRate && (i % refreshRate) == 0) {
				spr_new_x = cur_x[dst] - TIL2SCR(MapX - 5);
				spr_new_y = cur_y[dst] - TIL2SCR(MapY - 5);
				if(spr_new_x >= 8 && spr_new_x < 168 && spr_new_y >= 8 && spr_new_y < 168) {
					if(!mustDisplay) {
						/*sound, pitch following the progression*/
						if(dst == 0) {
							if(attr & 4)
								MUS_PlaySfx(16, i);
							else if(attr & 1)
								MUS_PlaySfx(5, delta_max - i);
							else
								MUS_PlaySfx(5, i);
						}
						mustDisplay = 1;
					}
					ExtendDirtyRect(spr_new_x, spr_new_y);
					if(attr & 2) {
						/*trail: draw directly, never erased*/
						GR_DRAW_SPRITE(dst, spr_new_x, spr_new_y);
					} else {
						if(i)
							ExtendDirtyRect(spr_x[dst], spr_y[dst]);
						spr_x[dst] = spr_new_x;
						spr_y[dst] = spr_new_y;
					}
				}
				/*spinning projectile*/
				if(MissileSpin[misType]) {
					 spr_rot[dst] += MissileSpin[misType];
					 if(!(attr & 2) && !(attr & 8))
						 GR_DEFINE_SPRITE(dst + 8, projTile, spr_rot[dst], MissilePivotX[misType], MissilePivotY[misType]);
				}
			}

			/*still in the same tile?*/
			if(SCR2TIL(cur_x[dst]) == tile_x[dst] && SCR2TIL(cur_y[dst]) == tile_y[dst])
				continue;

			/*entering a new tile: check range and obstacles*/
			tile_delta_x = abs(SCR2TIL(cur_x[dst]) - src_x);
			tile_delta_y = abs(SCR2TIL(cur_y[dst]) - src_y);
			if(tile_delta_x <= 7 && tile_delta_y <= 7 && RangeTable[tile_delta_x][tile_delta_y] <= max_range) {
				tile_x[dst] = SCR2TIL(cur_x[dst]);
				tile_y[dst] = SCR2TIL(cur_y[dst]);
				numTile = GetTileAtXYZ(tile_x[dst], tile_y[dst], MapZ);
				if(
					(tile_delta_x <= 1 && tile_delta_y <= 1 && IsTileWin(numTile)) ||
					!IsTileNos(numTile)
				) {
					/*mark the NPCs of the tile (not a buried corpser, not dragged under);
					  stop at the first object blocking missiles*/
					for(objNum = FindLoc(tile_x[dst], tile_y[dst], MapZ); objNum >= 0; objNum = NextLoc()) {
						if(
							objNum < 0x100 &&
							(GetType(objNum) != OBJ_CORPSER || GetFrame(objNum) != 0) &&
							!IsDraggedUnder(objNum)
						) collision_mask[objNum >> 3] |= (1 << (objNum & 7));
						if(IsTileNos(TILE_FRAME(objNum) - LocPartOffset))
							break;
					}
					if(objNum < 0)
						goto c_0db6;
				}
			}
			/*blocked or out of range: this projectile stops here.
			  note: the break also leaves the loop for the remaining
			  projectiles of this step*/
			delta_x[dst] = i;
			break;
c_0db6:
			if(dst == 0) {
				MissileEnd[0] = tile_x[0];
				MissileEnd[1] = tile_y[0];
			}
		}

		if(!mustDisplay)
			continue;

		if(!(i & 3))
			PaletteAnimation();

		/*save the background under the sprites (slots 0-7)*/
		if(!(attr & 2)) {
			for(dst = 0; dst < num_dst; dst ++) {
				if(delta_x[dst] > i || (attr & 8))
					GR_SAVE_SPRITE_BG(dst, spr_x[dst], spr_y[dst]);
			}
		}
		/*draw the projectiles*/
		if(!(attr & 2)) {
			for(dst = 0; dst < num_dst; dst ++) {
				if(attr & 8)
					GR_DRAW_SPRITE(15, spr_x[dst], spr_y[dst]);
				else if(delta_x[dst] > i)
					GR_DRAW_SPRITE(dst + 8, spr_x[dst], spr_y[dst]);
			}
		}
		/*foreground + copy the dirty rectangle to screen*/
		if(DirtyRect[0] <= DirtyRect[1] && DirtyRect[2] <= DirtyRect[3]) {
			RedrawForeground(DirtyRect[0], DirtyRect[2], DirtyRect[1], DirtyRect[3]);
			GR_COPY_TO_SCREEN(DirtyRect[0], DirtyRect[2], DirtyRect[1], DirtyRect[3], DirtyRect[0], DirtyRect[2]);
		}
		/*restore the background in the offscreen buffer*/
		if(!(attr & 2)) {
			for(dst = 0; dst < num_dst; dst ++) {
				if(delta_x[dst] > i || (attr & 8))
					GR_DRAW_SPRITE(dst, spr_x[dst], spr_y[dst]);
			}
		}
	}

	MUS_PlaySfx(0, 0);
	if(projTile && !(attr & 2) && !(attr & 8))
		DisplayTick();
	/*report where the projectiles stopped*/
	for(dst = 0; dst < num_dst; dst ++) {
		if(
			tile_x[dst] == pX_dst[dst] &&
			(tile_y[dst] == pY_dst[dst] || (attr & 1))
		) continue;
		ret = 0;
		pX_dst[dst] = tile_x[dst];
		pY_dst[dst] = tile_y[dst];
		if(dst == 0 && MissileShooter >= 0)
			MissileHitObj = COMBAT_GetTargetAt(MissileShooter, tile_x[0], tile_y[0]);
	}
	ShowMouse

	/*collision bitmap -> list of NPCs (max 0x3f)*/
	num_dst = 0;
	for(i = 0; i < 0x100/8; i ++) {
		for(dst = 0; dst < 8; dst ++) {
			if(collision_mask[i] & (1 << dst)) {
				MissileHitList[num_dst] = (i << 3) + dst;
				num_dst ++;
			}
			if(num_dst > 0x3e)
				break;
		}
		if(num_dst > 0x3e)
			break;
	}
	MissileHitList[num_dst] = -1;
	return ret;
}

/* explosion pattern
  +---+---+---+---+---+
  |   | 4 | 8 | 1 |   |
  +---+---+---+---+---+
  | b | 7 |   | e | c |
  +---+---+---+---+---+
  | 6 |   | o |   | d |
  +---+---+---+---+---+
  | a | 5 |   | 3 | 0 |
  +---+---+---+---+---+
  |   | 9 | 2 | f |   |
  +---+---+---+---+---+
*/
/*2BDA*/char ExplosionDX[] = { 2, 1, 0, 1,-1,-1,-2,-1, 0,-1,-2,-2, 2, 2, 1, 1};
/*2BEA*/char ExplosionDY[] = {-1, 2,-2,-1, 2,-1, 0, 1, 2,-2,-1, 1, 1, 0, 1,-2};

/*explosion centered on x,y: 16 trails of missileTile toward the pattern
  above (range 3); returns the -1 terminated list (MissileHitList) of the NPCs
  hit, including those on the center tile*/
int16 far /*C_2FC1_10D2*/*Explosion(int16 x, int16 y, int16 missileTile) {
	int16 count, objNum;
	int16 x_dst[MAX_MISSILES], y_dst[MAX_MISSILES];

	for(count = 0; count < MAX_MISSILES; count ++) {
		x_dst[count] = ExplosionDX[count] + x;
		y_dst[count] = ExplosionDY[count] + y;
	}
	Missiles(x, y, MAX_MISSILES, x_dst, y_dst, 3, missileTile, 4|2);
	/*append the NPCs of the center tile*/
	for(count = 0; count < 0x3e; count ++)
		if(MissileHitList[count] < 0)
			break;
	for(objNum = FindLoc(x, y, MapZ); count < 0x3f && objNum >= 0; objNum = NextLoc()) {
		if(
			objNum < 0x100 &&
			(GetType(objNum) != OBJ_CORPSER || GetFrame(objNum) != 0) &&
			!IsDraggedUnder(objNum)
		) MissileHitList[count ++] = objNum;
		if(IsTileNos(TILE_FRAME(objNum) - LocPartOffset))
			break;
	}
	MissileHitList[count] = -1;
	return MissileHitList;
}

/*"{flame|poison|energy|death} wind" from objNum toward x,y:
  5 trails of missileTile (range 7), one aimed at x,y and two pairs going
  1 and 2 tiles to each side (the two side directions come from SpreadDirTable,
  high/low nibble). returns the list of NPCs hit (MissileHitList)*/
int16 far /*C_2FC1_11EB*/*MagicWind(int16 objNum, int16 x, int16 y, int16 missileTile) {
	int16 sideDirs;
	int16 x_dst[5], y_dst[5];

	sideDirs = SpreadDirTable[y - GetY(objNum) + 5][x - GetX(objNum) + 5];
	x_dst[0] = x;
	x_dst[1] = x + DirIncrX[sideDirs >> 4];
	x_dst[2] = x + DirIncrX[sideDirs & 0xf];
	x_dst[3] = x_dst[1] + DirIncrX[sideDirs >> 4];
	x_dst[4] = x_dst[2] + DirIncrX[sideDirs & 0xf];
	y_dst[0] = y;
	y_dst[1] = y + DirIncrY[sideDirs >> 4];
	y_dst[2] = y + DirIncrY[sideDirs & 0xf];
	y_dst[3] = y_dst[1] + DirIncrY[sideDirs >> 4];
	y_dst[4] = y_dst[2] + DirIncrY[sideDirs & 0xf];
	Missiles(GetX(objNum), GetY(objNum), 5, x_dst, y_dst, 7, missileTile, 4|2);
	return MissileHitList;
}

/*"Web" spell animation: 9 web tiles thrown by objNum to the 3x3 square
  centered on x,y; returns the Missiles() result
	+---+---+---+
	| 7 | 0 | 1 |
	+---+---+---+
	| 6 | 8 | 2 |
	+---+---+---+
	| 5 | 4 | 3 |
	+---+---+---+*/
/*C_2FC1_131E*/AnimWebs(int16 objNum, int16 x, int16 y) {
	int16 i;
	int16 dst_x[9], dst_y[9];

	for(i = 0; i < 8; i ++) {
		dst_x[i] = x + DirIncrX[i];
		dst_y[i] = y + DirIncrY[i];
	}
	dst_x[8] = x;
	dst_y[8] = y;
	return Missiles(GetX(objNum), GetY(objNum), 9, dst_x, dst_y, 7, BaseTile[OBJ_WEB], 8|4);
}

/*"hail storm" spell around tile x,y:
  10 to 30 hailstones (up to 6 at a time) fall diagonally (from 50 pixels
  up-left) to random points around x,y; each NPC on an impact tile loses
  1 HP (with a hit tile flash). returns the total experience earned*/
/*C_2FC1_13B0*/AnimHailStorm(int16 x, int16 y) {
	int16 i;
	int16 stonesLeft;
	int16 stone_x[6], stone_y[6];/*current position (world pixels), <0: free slot*/
	int16 impact_x[6], impact_y[6];/*impact point (world pixels)*/
	int16 scr_x, scr_y;
	int16 objNum, anyFalling, frameCnt, soundArg, totalExp;

	anyFalling = 0;
	frameCnt = 0;
	totalExp = 0;
	for(i = 0; i < 6; i ++) {
		stone_x[i] =
		stone_y[i] =
		impact_x[i] =
		impact_y[i] = -1;
	}
	stonesLeft = OSI_rand(10, 30);
	GR_DEFINE_SPRITE(7, TIL_MISSILE_6, 0, 0x7f, 0x7f);
	DirtyRect[0] = DirtyRect[2] = 168;
	DirtyRect[1] = DirtyRect[3] = 7;
	while(stonesLeft || anyFalling) {
		soundArg = 0;
		/*maybe launch a new hailstone in a free slot*/
		for(i = 0; i < 6; i ++)
			if(stone_x[i] < 0)
				break;
		if(stonesLeft && i < 6 && OSI_rand(0, 7) == 0) {
			impact_x[i] = OSI_rand(1, 30) + TIL2SCR(x) + OSI_rand(1, 30) - 23;
			impact_y[i] = OSI_rand(1, 30) + TIL2SCR(y) + OSI_rand(1, 30) - 23;
			stone_x[i] = impact_x[i] - 50;
			stone_y[i] = impact_y[i] - 50;
			stonesLeft --;
		}
		anyFalling = 0;
		/*redraw every 4 pixels: save bg (slot i), draw stone (slot 7),
		  copy to screen, restore bg*/
		if(++frameCnt >= 4) {
			for(i = 0; i < 6; i ++) {
				if(stone_x[i] >= 0) {
					scr_x = stone_x[i] - TIL2SCR(MapX - 5);
					scr_y = stone_y[i] - TIL2SCR(MapY - 5);
					if(scr_x > 15 && scr_x < 152 && scr_y > 15 && scr_y < 152) {
						GR_SAVE_SPRITE_BG(i, scr_x, scr_y);
						ExtendDirtyRect(scr_x, scr_y);
					}
				}
			}
			for(i = 0; i < 6; i ++) {
				if(stone_x[i] >= 0) {
					scr_x = stone_x[i] - TIL2SCR(MapX - 5);
					scr_y = stone_y[i] - TIL2SCR(MapY - 5);
					if(scr_x > 15 && scr_x < 152 && scr_y > 15 && scr_y < 152) {
						GR_DRAW_SPRITE(7, scr_x, scr_y);
						ExtendDirtyRect(scr_x, scr_y);
					}
				}
			}
			if(DirtyRect[0] <= DirtyRect[1] && DirtyRect[2] <= DirtyRect[3])
				GR_COPY_TO_SCREEN(DirtyRect[0], DirtyRect[2], DirtyRect[1], DirtyRect[3], DirtyRect[0], DirtyRect[2]);
			DirtyRect[0] = DirtyRect[2] = 168;
			DirtyRect[1] = DirtyRect[3] = 7;
			for(i = 0; i < 6; i ++) {
				if(stone_x[i] >= 0) {
					scr_x = stone_x[i] - TIL2SCR(MapX - 5);
					scr_y = stone_y[i] - TIL2SCR(MapY - 5);
					if(scr_x > 15 && scr_x < 152 && scr_y > 15 && scr_y < 152) {
						GR_DRAW_SPRITE(i, scr_x, scr_y);
						ExtendDirtyRect(scr_x, scr_y);
					}
				}
			}
			frameCnt = 0;
		}
		/*move the stones one pixel down-right; handle impacts*/
		for(i = 0; i < 6; i ++) {
			if(stone_x[i] >= 0) {
				stone_x[i] ++;
				stone_y[i] ++;
				if(impact_x[i] == stone_x[i] && impact_y[i] == stone_y[i]) {
					stone_x[i] = -1;
					soundArg = OSI_rand(400, 500);
					scr_x = SCR2TIL(impact_x[i]);
					scr_y = SCR2TIL(impact_y[i]);
					for(objNum = FindLoc(scr_x, scr_y, MapZ); objNum >= 0; objNum = NextLoc()) {
						if(objNum >= 0x100)
							continue;
						ShowHitFlash = 0;/*no standard hit display*/
						totalExp += LoseHP(objNum, 1);
						soundArg = -1;
						/*hit flash (scr_x/scr_y: tile -> map buffer center).
						  note: converted in place, so a 2nd NPC on the same
						  tile gets a wrong flash position*/
						scr_x = TIL2SCR(scr_x - (MapX - 5)) + 8;
						scr_y = TIL2SCR(scr_y - (MapY - 5)) + 8;
						if(scr_x >= 8 && scr_x < 168 && scr_y >= 8 && scr_y < 168) {
							GR_SAVE_SPRITE_BG(6, scr_x, scr_y);
							GR_PUT_TILE(TIL_HIT, scr_x - 8, scr_y - 8);
							GR_COPY_TO_SCREEN(scr_x - 8, scr_y - 8, scr_x + 7, scr_y + 7, scr_x - 8, scr_y - 8);
							GR_DRAW_SPRITE(6, scr_x, scr_y);
							ExtendDirtyRect(scr_x, scr_y);
						}
					}
				} else {
					anyFalling = 1;
				}
			}
		}
		/*soundArg: 0=nothing, >0 impact on the ground, -1 NPC hit*/
		MUS_PlaySfx(7, soundArg);
	}
	DisplayTick();
	return totalExp;
}

/*shake the map window "duration" times: the 144x144 inner part of the
  offscreen map is copied to screen at a random offset of +/-ampl pixels*/
/*C_2FC1_18BD*/ShakeScreen(int16 duration, int16 ampl) {
	int16 i;
	int16 ofs_x, ofs_y;

	DrawMapWindow(0);
	for(i = 0; i < duration; i ++) {
		ofs_x = OSI_rand(16 - ampl, ampl + 16);
		ofs_y = OSI_rand(16 - ampl, ampl + 16);
		GR_COPY_TO_SCREEN(ofs_x, ofs_y, ofs_x + 144, ofs_y + 144, 16, 16);
		MUS_PlaySfx(8, 0);
	}
	DisplayTick();
}

/*clear the 11x11 view: all tiles hidden, no object, and rebuild the
  free list of the object tile entries (CellObjNext[i] = i + 1)*/
/*C_2FC1_193C*/static ClearMapView() {
	int16 i, j;

	for(i = 0; i < 11; i ++) {
		for(j = 0; j < 11; j ++)
			Tile_11x11[i][j] = TIL_HIDDEN;
	}
	for(i = 0; i < 0x100; i++)
		CellObjNext[i] = i + 1;
	for(i = 0; i < 11; i ++) {
		for(j = 0; j < 11; j ++)
			Obj_11x11[i][j] = 0;
	}
}

/*blank the map window (displayed), then rebuild the view
  (tile map and visible tiles/objects) without displaying it*/
/*C_2FC1_19A1*/ResetMapView() {
	ClearMapView();
	VisibilityDirty = 0;
	ScreenFade = 1;
	DrawMapWindow(1);
	MAP_BuildArea();
	VIS_UpdateView();
}

/*sun, moons'y: height in the sky bar for positions 0..14 (an arc)*/
/*2BFA*/unsigned char SkyArcY[] = {10,7,5,3,2,1,0,0,0,1,2,3,5,7,10};

/*display sky info:
  the strip above the party/status area (only when the party or
  CMD_STAT_TALK view is displayed): outside, the sun (5h-19h, moving from right
  to left) and the two moons (sky position TrammelPos/FeluccaPos 0..14, phase
  TrammelPhase/FeluccaPhase) over a mountain skyline, none of the moons during an
  eclipse (1st day of every 3rd month, or eclipse spell SpellFx[15]);
  underground, a cave ceiling. Also redraws the wind indicator*/
/*C_2FC1_19C5*/DrawSkyBar() {
	int16 i, sunTile;
	int16 isEclipse;

	if(StatusDisplay != CMD_STAT_PARTY && StatusDisplay != CMD_STAT_TALK)
		return;
	isEclipse = ((Date_D == 1 && (Date_M % 3) == 0) || SpellFx[15]);
	for(i = 0; i < 9; i ++)
		GR_PUT_TILE(TIL_SLOT_EMPTY, TIL2SCR(i), 4);
	if(MapZ == 0 || MapZ == 5) {
		/*outside: brittania or gargoyles'realm*/
		/*the sun*/
		if(Time_H > 4 && Time_H < 20) {
			if(isEclipse)
				sunTile = TIL_SUN_2;
			else if(Time_H == 5 || Time_H == 19)
				sunTile = TIL_SUN_0;
			else
				sunTile = TIL_SUN_1;
			GR_PUT_TILE(sunTile, (19 - Time_H) << 3, SkyArcY[19 - Time_H]);
		}
		/*the moons*/
		if(!isEclipse) {
			if(TrammelPos >= 0 && TrammelPos <= 14)
				GR_PUT_TILE(BaseTile[OBJ_MOONSTONE] + TrammelPhase, TrammelPos << 3, SkyArcY[TrammelPos]);
			if(FeluccaPos >= 0 && FeluccaPos <= 14)
				GR_PUT_TILE(BaseTile[OBJ_MOONSTONE] + FeluccaPhase, FeluccaPos << 3, SkyArcY[FeluccaPos]);
		}
		/*the mountain*/
		for(i = 0; i < 9; i ++)
			GR_PUT_TILE(TIL_OUTSIDE + i, TIL2SCR(i), 4);
	} else {
		/*in a cave*/
		GR_PUT_TILE(TIL_UNDERGROUND_LEFT, 0, 4);
		for(i = 1; i < 8; i ++)
			GR_PUT_TILE(TIL_UNDERGROUND_MIDDLE, TIL2SCR(i), 4);
		GR_PUT_TILE(TIL_UNDERGROUND_RIGHT, 128, 4);
	}
	/*copy the 136x16 strip from the offscreen buffer to screen 176,6*/
	GR_COPY_TO_SCREEN(0, 4, 135, 19, 176, 6);
	UpdateWind(1);
}

/*full redraw of the map window (cancel equipment selection and cursor
  mode, blank, rebuild the view and display it)*/
/*C_2FC1_1BCE*/RedrawMapWindow() {
	DrawSlotCursor(0);
	SelectMode = 0;
	HideMouse
	DrawMapWindow(0);
	GR_CODEX_EFFECT();
	ResetMapView();
	ScreenFade = 1;
	DrawMapWindow(1);
	ShowMouse
}

/*name of the portrait file; the last letter (index 9) is patched*/
/*2C09*/char PortraitFileName[] = "portrait.a";

/*load portrait?:
  draw the 56x64 portrait of NPC objNum at screen x,y.
  portrait.a holds NPCs 1..0x62, portrait.b NPCs 0x63..
  (generic wisp/guard/gargoyle use 0xc0..0xc2), portrait.z the avatar
  portraits (objNum 1, number AvatarPortrait). Each file starts with a table of
  4-byte offsets to LZW-compressed pictures*/
/*C_2FC1_1C19*/DrawPortrait(int16 objNum, int16 x, int16 y) {
	int16 fd;
	int16 objType;
	int32 offset;

	objType = GetType(objNum);
	/*temporary NPCs: generic portraits*/
	if(objNum >= 0xe0) {
		if(objType == OBJ_WISP) {
			DISK_confirm(DISK_POPULACE_B);
			objNum = 0xc0;
		} else if(objType == OBJ_GUARD) {
			DISK_confirm(DISK_POPULACE_B);
			objNum = 0xc1;
		} else if(objType == OBJ_GARGOYLE) {
			DISK_confirm(DISK_POPULACE_B);
			objNum = 0xc2;
		}
	}
	/*from here on objNum is the portrait index in the file*/
	if(objNum == 1) {
		DISK_confirm(DISK_POPULACE_A);
		PortraitFileName[9] = 'z';
		objNum = AvatarPortrait - 1;
		if(objNum < 0)
			return;
	} else {
		if(objNum)
			objNum --;
		if(objNum >= 0x62) {
			DISK_confirm(DISK_POPULACE_B);
			PortraitFileName[9] = 'b';
			objNum -= 0x62;
		} else {
			DISK_confirm(DISK_POPULACE_A);
			PortraitFileName[9] = 'a';
		}
	}
	fd = OSI_open(PortraitFileName);
	if(fd < 0)
		return;
	OSI_read(fd, (int32)(objNum << 2), 4, &offset);
	decompress(fd, offset, ((struct tPixmap far *)ScratchBuf)->data, FP_SEG(ScratchBuf) + 0x100);
	OSI_close(fd);
	((struct tPixmap far *)ScratchBuf)->width = 56;
	((struct tPixmap far *)ScratchBuf)->height = 64;
	GR_DRAW_PIXMAP(ScratchBuf, x, y);
}

/*name of the conversation file; the last letter (index 9) is patched*/
/*2C14*/char ConverseFileName[] = "converse.a";

/*load the conversation script of NPC objNum into dest.
  converse.a holds NPCs 0..0x62, converse.b NPCs 0x63.. and the generic
  wisp/guard/gargoyle scripts (0x66..0x68 of converse.b). The file starts
  with a table of 4-byte offsets; at each offset a 4-byte unpacked size,
  followed by the LZW data (size 0 or >= 0x2800: stored uncompressed).
  returns objNum, or 0 if the NPC has no conversation*/
/*C_2FC1_1D40*/LoadConversation(int16 objNum/*di,bp0a*/, void far *dest) {
	int16 fd;
	int32 offset, size;
	int16 objType, convIndex;

	objType = GetType(objNum);
	if(objNum >= 0xe0) {
		/*generic conversations?*/
		if(objType == OBJ_WISP) {/*Wisp*/
			DISK_confirm(DISK_POPULACE_B);
			convIndex = 0x66;
			ConverseFileName[9] = 'b';
		} else if(objType == OBJ_GUARD) {/*Guard*/
			DISK_confirm(DISK_POPULACE_B);
			convIndex = 0x67;
			ConverseFileName[9] = 'b';
		} else if(objType == OBJ_GARGOYLE) {/*Gargoyle*/
			DISK_confirm(DISK_POPULACE_B);
			convIndex = 0x68;
			ConverseFileName[9] = 'b';
		} else {
			return 0;
		}
	} else if(objNum > 0x62) {
		DISK_confirm(DISK_POPULACE_B);
		ConverseFileName[9] = 'b';
		convIndex = objNum - 0x63;
	} else {
		DISK_confirm(DISK_POPULACE_A);
		ConverseFileName[9] = 'a';
		convIndex = objNum;
	}
	fd = OSI_open(ConverseFileName);
	if(fd >= 0) {
		/*read the NPC's data offset*/
		OSI_read(fd, (int32)(convIndex << 2), 4, &offset);
		if(offset) {
			/*loading the real conversation: stop the music*/
			if(dest == TalkBuf)
				MUS_PlaySong(0);
			/*read the inflated data size*/
			OSI_read(fd, offset, 4, &size);
			if(size && size < 0x2800L)
				decompress(fd, offset, dest, FP_SEG(ScratchBuf) + 0x400);
			else
				OSI_read(fd, offset + 4, 0x2800L, dest);
		} else {
			objNum = 0;
		}
		OSI_close(fd);
	}
	return objNum;
}

/*display equipment bg chara: the 2x2 tiles paper doll at screen x,y*/
/*C_2FC1_1EAF*/DrawPaperDoll(int16 x/*si,bp08*/, int16 y/*di,bp06*/) {
	GR_PUT_TILE_SCREEN(TIL_EQUIP_UP_LEFT, x,      y);
	GR_PUT_TILE_SCREEN(TIL_EQUIP_UP_RIGHT, x + 16, y);
	GR_PUT_TILE_SCREEN(TIL_EQUIP_DOWN_LEFT, x,      y + 16);
	GR_PUT_TILE_SCREEN(TIL_EQUIP_DOWN_RIGHT, x + 16, y + 16);
}

/*peer: draw one map tile as a 4x4 checkered dot at x,y:
  tileNum < 0 = an NPC (white), TIL_HIDDEN = black, damaging = red,
  impassable = blue (wet) or gray, other terrain (< 0x100) = green;
  passable object tiles are not drawn*/
static /*C_2FC1_1F14*/peer_tile(int16 tileNum, int16 x, int16 y) {
	int16 color;
	int16 i, j, pattern;

	pattern = 1;
	if(tileNum < 0) {
		color = 15;
		pattern = 0;
	} else if(tileNum == TIL_HIDDEN) {
		color = 0;
	} else if(IsTerrainDamage(tileNum)) {
		color = ColorRed;
	} else if(IsTerrainImpass(tileNum)) {
		if(IsTerrainWet(tileNum))
			color = ColorBlue;
		else
			color = ColorGray;
	} else {
		color = -1;
	}
	if(color < 0 && tileNum >= TIL_WALL_CORNERS)
		return;
	if(color < 0)
		color = ColorGreen;
	GR_SET_COLOR(color);
	for(i = 0; i < 4; i ++) {
		for(j = 0; j < 4; j ++) {
			if(((i + j) & 1) == pattern)
				GR_PUT_PIXEL(i + x, j + y);
		}
	}
}

/*peer:
  overview of the whole loaded 40x40 area, 4x4 pixels per tile, drawn
  over the map window (gem of peer / debug key). Underground only the
  tiles reachable from the party (AreaFlags bit 0x80, computed by
  VIS_FloodFill) are shown. The party is a white square. Waits for a key
  (without animations), then recomputes the view*/
/*C_2FC1_1FD6*/PeerMap() {
	int16 row, col;
	int16 rowStart, rowEnd, colStart, colEnd;
	int16 tile, objNum, mouseState, isUnderground;

	isUnderground = (MapZ > 0 && MapZ < 5);
	for(row = 0; row < AREA_H; row ++) {
		for(col = 0; col < AREA_W; col ++)
			AreaFlags[row][col] =
			AreaLight[row][col] = 0;
	}
	rowStart = colStart = 0;
	rowEnd = colEnd = 40;
	if(isUnderground) {
		VIS_FloodFill(ViewX + 5, ViewY + 5, 40, 2);
	} else if(MapZ == 0) {
		/*surface: special cases near the world edges (0x400 wrap)*/
		if(AreaX == 0x3f0) {
			colStart = 8;
		} else if(AreaX == 0x3e0) {
			colEnd = 0x20;
		} else if(AreaY >= 0x3f0) {
			rowStart = 0x400 - AreaY;
		} else if(AreaY == 0x3e0 || AreaY == 0x3e8) {
			rowEnd = 0x400 - AreaY;
		}
	}
	GrInfo->toBackBuffer = 1;
	GR_SET_COLOR(0);
	GR_FILL_RECT(8, 8, 167, 167);
	/*terrain*/
	for(row = rowStart; row < rowEnd; row ++) {
		for(col = colStart; col < colEnd; col ++) {
			tile = AreaTiles[row][col];
			if(isUnderground && !(AreaFlags[row][col] & 0x80))
				tile = TIL_HIDDEN;
			peer_tile(tile, (col << 2) + 8, (row << 2) + 8);
		}
	}
	/*objects (NPC #0 only if under player control)*/
	for(objNum = SearchArea(AreaX, AreaY, AreaX + (AREA_W-1), AreaY + (AREA_H-1)); objNum >= 0; objNum = NextArea()) {
		if(objNum == 0 && !IsPlrControl(objNum))
			continue;
		col = GetX(objNum) - AreaX;
		row = GetY(objNum) - AreaY;
		if(isUnderground && !(AreaFlags[row][col] & 0x80))
			continue;
		if(objNum < 0x100) {
			peer_tile(-1, (col << 2) + 8, (row << 2) + 8);
		} else {
			tile = TILE_FRAME(objNum);
			peer_tile(tile, (col << 2) + 8, (row << 2) + 8);
			/*multi-tile objects extend to the left/up*/
			if(IsTileDoubleH(tile) && col) {
				if(IsTileDoubleV(tile) && row) {
					peer_tile(tile - 2, (col << 2) + 8, (row << 2) + 4);
					peer_tile(tile - 3, (col << 2) + 4, (row << 2) + 4);
				}
				peer_tile(tile - 1, (col << 2) + 4, (row << 2) + 8);
			} else if(IsTileDoubleV(tile) && row) {
				peer_tile(tile - 1, (col << 2) + 8, (row << 2) + 4);
			}
		}
	}
	/*the party (center of the view)*/
	GR_SET_COLOR(15);
	GR_FILL_RECT((ViewX << 2) + 28, (ViewY << 2) + 28, (ViewX << 2) + 31, (ViewY << 2) + 31);
	GR_SET_COLOR(0);
	GrInfo->toBackBuffer = 0;
	RedrawMapBorder(8, 8, 167, 167);
	mouseState = CON_mouseOffAt(8, 8, 167, 167);
	GR_COPY_TO_SCREEN(8, 8, 167, 167, 8, 8);
	CON_mouseOn(mouseState);
	AnimateAtPrompt = 0;
	CON_getch();
	AnimateAtPrompt = 1;
	VIS_UpdateView();
}
