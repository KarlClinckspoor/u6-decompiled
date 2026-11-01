/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	GAME LOOP / WORLD CLOCK module (gameloop.c)

	This is the heart of the game's turn handling:
	- GameLoop is the main command loop (called once from main.c after
	  initialisation): it reads a key, translates it into a CMD_xxx command,
	  executes it, then runs the end-of-turn processing (RunNPCTurns: NPC
	  movement/AI, eruptions, disk-full recovery, prompt display).
	- PassTime is the world clock: it advances time by N minutes and handles
	  everything that depends on elapsed time (spell durations, powder keg fuse,
	  torches burning, magic rings/cloak wearing out, status effects wearing
	  off, poison damage, terrain damage, hour/day/month/year rollover, NPC
	  schedules, mana regeneration, bells, sundial, moon phases & positions,
	  moongates, wind, ambient light, date/wind line).
	- OtherAnimations / AnimateTilesAndActors / DrawMapWindow are the per-frame display
	  update: animated tiles (water, fire...), idle animations of NPCs and
	  monsters, and redraw of the 11x11 map window (with clock faces, Negate
	  Magic static, target cursor, frame and mouse pointer).
	- TerrainDamage / ApplyTerrainEffects apply the effects of standing on a harmful
	  tile (web, swamp, poison/fire/sleep fields, lava, fires, traps...).
	- SetActive / SetPartyMode implement solo mode / party mode switching.
	- small helpers: karma, tile animation state, status display refresh.
*/
#include "u6.h"

#undef abs
#include <math.h>

/*0338*/int16 AnimFrameCounter = 0;/*animation frame counter (also paces the clock tic-tac in music.c)*/
/*033A*/unsigned char StatusDirty = 1;/*status area must be redrawn (may be incremented)*/
/*033B*/unsigned char PromptFlag = 0;/*a "<name>:\n>" prompt must be printed again*/
/*033C*/unsigned char CyclopsFlag = 0;/*pending screen shake (cyclops stomp), = intensity*/
/*033D*/unsigned char IsArmageddon = 0;
/*033E*/int16 IdleAnimTick = 0;/*set by the idle keyboard loop: allows one round of NPC idle animations*/
/*0340*/int16 MapAnimEnabled = 1;/*map animation/redraw enabled (cleared by DISK_full)*/
/*0342*/int16 Erupting = 0;/*volcano/fumarole: 1 = eruption pending, >1 = erupting object*/
/*same as DirConnFlag, but unused?*/
/*wall flag per direction: N,NE,E,SE,S,SW,W*/
unsigned char DirWallFlags[] = {TERRAIN_WALL_N,0,TERRAIN_WALL_E,0,TERRAIN_WALL_S,0,TERRAIN_WALL_W};

/*decrease karma by "amount", clamped at 0*/
/*C_0A33_0008*/SubKarma(int16 amount) {
	if(KARMA > amount)
		KARMA -= amount;
	else
		KARMA = 0;
}

/*increase karma by "amount", clamped at 99*/
/*never called?*/
/*C_0A33_0027*/AddKarma(int16 amount) {
	if(KARMA + amount < 100)
		KARMA += amount;
	else
		KARMA = 99;
}

/*set the animation state of an animated tile
param mode:
	0 = stop
	1 = normal
	2 = reverse*/
/*C_0A33_0049*/SetTileAnimation(int16 tileNum, int16 mode) {
	int16 i;

	for(i = 0; i < NumAnimData; i ++) {
		if(AnimTile[i] == tileNum) {
			StateAnimData[i] = mode;
			break;
		}
	}
}

/*animate tiles and the idle animations of the NPCs/monsters near the party
(frozen during Time Stop)*/
/*C_0A33_0073*/static AnimateTilesAndActors() {
	int16 frm, i;
	int16 dir, obj_x, obj_y, frameBase, mustRefresh, objTyp, partObj;

	if(SpellFx[14])/*time stop*/
		return;

	AnimFrameCounter ++;

	/*-- animated tiles --*/
	/*each entry: tile AnimTile[i] displays tile
	  AnimFirstFrame[i] + ((counter & AnimFrameMask[i]) >> AnimFrameShift[i])*/
	for(i = 0; i < NumAnimData; i++) {
		frm = AnimFirstFrame[i];
		if(StateAnimData[i] == 1/*normal*/)
			frm += (AnimFrameCounter & AnimFrameMask[i]) >> AnimFrameShift[i];
		else if(StateAnimData[i] == 2/*reverse*/)
			frm += (~AnimFrameCounter & AnimFrameMask[i]) >> AnimFrameShift[i];
		GR_SET_ANIM_FRAME(AnimTile[i], frm);
	}

	if((AnimFrameCounter & 1) == 0)
		GR_ANIMATE_TILES();

	/*-- NPC/monster idle animations --*/
	/*most actors have 4 frames per direction: frameBase = direction part,
	  (frm & 3) = animation step*/
	mustRefresh = 0;
	for(i = 0; i < 0x100; i ++) {
		if(
			ObjShapeType[i] == 0 ||
			GetZ(i) != MapZ ||
			!CLOSE_ENOUGH_S(6, GetX(i), GetY(i), MapX, MapY)
		) continue;

		objTyp = GetType(i);

		if(!IdleAnimTick)/*why test inside loop?*/
			continue;

		if(IsIncapacitated(i))/*dead, asleep or paralyzed*/
			continue;

		frm = GetFrame(i);
		frameBase = frm & 0xfc;
		if(objTyp >= OBJ_FIGHTER && objTyp <= OBJ_AVATAR && objTyp != OBJ_CAT) {
			/*humans*/
			if(objTyp == OBJ_FARMER_2) {
				/*farmer working the field: 1/4 chance to swing the tool,
				  else 1/16 chance to become a plain farmer again*/
				if(OSI_rand(0, 3)) {
					if(OSI_rand(0, 15) == 0) {
						ObjShapeType[i] = TypeFrame(OBJ_FARMER, 0);
						frm = 0;
						frameBase = 8;
						mustRefresh = 1;
					}
				} else {
					frm ^= 1;
					mustRefresh = 1;
				}
			} else if(objTyp == OBJ_MUSICIAN_SHORT_CAPE) {
				/*playing musician*/
				if(OSI_rand(0, 1)) {
					frm ^= 1;
					mustRefresh = 1;
				}
			} else if(objTyp == OBJ_JESTER) {
				/*juggling: cycles frames 0,1,2*/
				frm &= 3;
				if(OSI_rand(0, 1) && frm < 3/*!?!*/) {
					if(++frm > 2)
						frm = 0;
					mustRefresh = 1;
				}
			} else {
				/*standing: step 1 is the neutral pose, 0/2 the alternate ones*/
				switch(frm & 3) {
					case 0: if(OSI_rand(0, 31) == 0) {
						frm = 1;
						mustRefresh = 1;
					} break;
					case 1: if(OSI_rand(0, 63) == 0) {
						frm = OSI_rand(0, 2);
						mustRefresh = 1;
					} break;
					case 2: if(OSI_rand(0, 31) == 0) {
						frm = 1;
						mustRefresh = 1;
					} break;
				}
				/*farmer on duty starts working the field*/
				if(objTyp == OBJ_FARMER && NPCMode[i] == AI_FARM && OSI_rand(0, 15) == 0) {
					ObjShapeType[i] = TypeFrame(OBJ_FARMER_2, 0);
					frm = 0;
					frameBase = frm;
					mustRefresh = 1;
				}
			}
		} else if(objTyp == OBJ_GARGOYLE) {
			/*3 frames per direction*/
			frameBase = frm - (frm % 3);
			frm %= 3;
			switch(frm) {
				case 0: if(OSI_rand(0, 31) == 0) {
					frm = 1;
					mustRefresh = 1;
				} break;
				case 1: if(OSI_rand(0, 63) == 0) {
					frm = OSI_rand(0, 2);
					mustRefresh = 1;
				} break;
				case 2: if(OSI_rand(0, 31) == 0) {
					frm = 1;
					mustRefresh = 1;
				} break;
			}
		} else if(objTyp == OBJ_WINGED_GARGOYLE) {
			/*12 frames per direction, idle poses are frames 3/7/11*/
			frameBase = frm - (frm % 12);
			frm %= 12;
			switch(frm) {
				case 3:
					if(OSI_rand(0, 31) == 0) {
						frm = 7;
						mustRefresh = 1;
					}
				break;
				case 7:
					if(OSI_rand(0, 63) == 0) {
						frm = OSI_rand(0, 1)?3:11;
						mustRefresh = 1;
					}
				break;
				case 0xb:
					if(OSI_rand(0, 31) == 0) {
						frm = 7;
						mustRefresh = 1;
					}
				break;
			}
			/*whole frame goes into frameBase (frm & 3 must add nothing)*/
			frameBase = frameBase + frm;
			frm = 0;
		} else if(objTyp >= OBJ_DAEMON && objTyp <= OBJ_MONGBAT) {
			if(OSI_rand(0, 7) == 0) {
				frm ^= 1;
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_GIANT_BAT || objTyp == OBJ_ACID_SLUG) {
			/*ping-pong 0..3, direction kept in the "walking" flag*/
			if(OSI_rand(0, 1)) {
				if(IsWalking(i)) {
					if(++frm >= 3) {
						frm = 3;
						ClrWalking(i);
					}
				} else {
					if(--frm <= 0) {
						frm = 0;
						SetWalking(i);
					}
				}
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_GAZER || objTyp == OBJ_REAPER || objTyp == OBJ_ROT_WORMS) {
			/*cycle 0..3*/
			if(OSI_rand(0, 1)) {
				if(++frm > 3)
					frm = 0;
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_INSECTS || objTyp == OBJ_GHOST || objTyp == OBJ_GREMLIN) {
			/*random frame (insects: every time)*/
			if(objTyp == OBJ_INSECTS || OSI_rand(0, 1)) {
				frm = OSI_rand(0, 3);
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_WISP || objTyp == OBJ_GIANT_SQUID) {
			/*random frame 0..2, rarely 3*/
			if(OSI_rand(0, 1)) {
				if(OSI_rand(0, 15) == 0)
					frm = 3;
				else
					frm = OSI_rand(0, 2);
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_CORPSER) {
			/*frame 0 = hidden underground; else ping-pong 1..3*/
			if(frm && OSI_rand(0, 1)) {
				IsWalking(i)?frm --:frm ++;
				if(frm > 3) {
					frm = 2;
					SetWalking(i);
				}
				if(frm < 1) {
					frm = 2;
					ClrWalking(i);
				}
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_BIRD) {
			/*flapping 0..2, rarely lands (frame 3)*/
			if(frm < 3) {
				if(OSI_rand(0, 31) == 0) {
					frm = 3;
					mustRefresh = 1;
				} else if(OSI_rand(0, 1)) {
					if(++frm > 2)
						frm = 0;
					mustRefresh = 1;
				}
			}
		} else if(objTyp == OBJ_TANGLE_VINE_POD) {
			if(OSI_rand(0, 7) == 0) {
				frm ^= 1;
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_HYDRA) {
			/*animate the 8 heads around the body:
			  head frame = (dir << 2) | step*/
			obj_x = GetX(i);
			obj_y = GetY(i);
			for(dir = 0; dir < 8; dir ++) {
				for(
					partObj = FindLoc(DirIncrX[dir] + obj_x, DirIncrY[dir] + obj_y, MapZ);
					partObj >= 0;
					partObj = NextLoc()
				) {
					if(GetType(partObj) == OBJ_HYDRA_BODY && (GetFrame(partObj) >> 2) == dir) {
						if(OSI_rand(0, 1)) {
							frm = GetFrame(partObj);
							SetFrame(partObj, (frm & 0x1c) + ((frm + 1) & 3));
							mustRefresh = 1;
						}
						break;
					}
				}
			}
			/*central body: ping-pong 0..3*/
			frm = GetFrame(i);
			if(OSI_rand(0, 1)) {
				if(frm == 0 || (frm < 3 && OSI_rand(0, 4) == 0))
					SetWalking(i);
				if(frm == 3 || (frm > 0 && OSI_rand(0, 4) == 0))
					ClrWalking(i);
				if(IsWalking(i))
					frm ++;
				else
					frm --;
				mustRefresh = 1;
			}
		} else if(objTyp == OBJ_DRAGON) {
			if(OSI_rand(0, 1)) {
				frm ^= 1;
				mustRefresh = 1;
			}
			/*the other dragon parts (head, wings, tail)*/
			obj_x = GetX(i);
			obj_y = GetY(i);
			for(dir = 0; dir < 8; dir += 2) {
				partObj = OBJ_FindDragonPart(obj_x, obj_y, MapZ, dir, GetFrame(i));
				if(partObj >= 0) {
					if(GetFrame(partObj) >= 0x18) {
						SetFrame(partObj, (GetFrame(partObj) & 0x38) | (frm & 7));
					} else if(OSI_rand(0, 7) == 0) {
						SetFrame(partObj, GetFrame(partObj) ^ 1);
						mustRefresh = 1;
					}
				}
			}
		} else if(
			objTyp == OBJ_SHEEP || objTyp == OBJ_DOG || objTyp == OBJ_DEER ||
			objTyp == OBJ_GIANT_RAT || objTyp == OBJ_SNAKE || objTyp == OBJ_GIANT_SPIDER ||
			objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_WOLF || objTyp == OBJ_SEA_SERPENT ||
			(objTyp >= OBJ_DAEMON && objTyp <= OBJ_MONGBAT)
		) {
			/*animals: toggle frame, also on the head/tail part of
			  2-tile creatures (cows only 1/4 as often)*/
			if(OSI_rand(0, 7) == 0) {
				if(objTyp != OBJ_COW || OSI_rand(0, 3) == 0) {
					frm ^= 1;
					mustRefresh = 1;
					partObj = OBJ_getHead(i);
					if(partObj >= 0)
						SetFrame(partObj, GetFrame(partObj) ^ 1);
				}
			}
		}

		frm = (frm & 3) + frameBase;
		SetFrame(i, frm);
	}/*end for*/

	IdleAnimTick = 0;

	if(mustRefresh)
		ShowObjects();
}

/*hour (mod 12) -> direction index for the clock's short hand*/
/*034B*/unsigned char ClockHourToDir[] = {0,1,1,2,3,3,4,5,5,6,7,7};

/*draw the hands of a clock tile located at screen (x,y) of the map window
(pixels, with the 8 direction increments as hand vectors)*/
/*C_0A33_0931*/static DrawClockHands(int16 x, int16 y) {
	int16 dirIdx;

	x += 4;
	y += 14;
	GR_SET_COLOR(ColorBlack);
	GrInfo->toBackBuffer = 1;
	/*centre*/
	GR_PUT_PIXEL(x, y);
	/*short hand*/
	dirIdx = ClockHourToDir[Time_H % 12];
	GR_PUT_PIXEL(DirIncrX[dirIdx] + x, DirIncrY[dirIdx] + y);
	/*long hand (60 minutes -> 8 directions)*/
	dirIdx = (Time_M * 2) / 15;
	GR_PUT_PIXEL(DirIncrX[dirIdx] + x, DirIncrY[dirIdx] + y);
	GrInfo->toBackBuffer = 0;
}

/*draw the 11x11 map window into the back buffer (tiles + objects).
If showOnScreen: also draw the target cursor, the window frame, the mouse
pointer, and copy (or fade in) the window to the screen.*/
/*C_0A33_09CE*/DrawMapWindow(int16 showOnScreen/*0:offScreen,1:onScreen*/) {
	int16 i, j;
	int16 cellObj, scr_x, scr_y, tile, mouseInMap;

	GR_SET_COLOR(ColorMagenta);
	for(j = 0; j < 11; j++) {
		for(i = 0; i < 11; i++) {
			scr_x = TIL2SCR(i);
			scr_y = TIL2SCR(j);
			ServeMouse
			/*tiles*/
			GR_PUT_TILE(Tile_11x11[j][i], scr_x, scr_y);
			/*objects: linked list of tiles to draw in this cell*/
			cellObj = Obj_11x11[j][i];
			while(cellObj) {
				tile = CellObjTile[cellObj];
				GR_PUT_TILE(tile, scr_x, scr_y);
				if(BaseTile[OBJ_CLOCK] == (tile & 0x7ff))
					DrawClockHands(scr_x, scr_y);
				cellObj = CellObjNext[cellObj];
			}
		}
	}
	/*negate magic/storm cloak effect: 100 random "static" pixels*/
	if(SpellFx[13]) {
		GrInfo->xorMode = 1;
		GrInfo->toBackBuffer = 1;
		GR_SET_COLOR(8);
		for(i = 0; i < 100; i++)
			GR_PUT_PIXEL(OSI_rand(8, 167), OSI_rand(8, 167));
		GrInfo->xorMode = 0;
		GrInfo->toBackBuffer = 0;
	}
	/* */
	if(!showOnScreen)
		return;
	if(MouseMode == 0 && !FreeAimMode) {
		DrawSlotCursor(0);
		SelectMode = 0;
	}
	if(SelectMode == 1) {
		/*target cursor*/
		if(SelectRange < 0)
			tile = TIL_CURSOR_DIRECTION;/*direction*/
		else
			tile = TIL_CURSOR_SELECT;/*select*/
		GR_PUT_TILE(tile, AimX * 16, AimY * 16);
	}
	/*draw frame*/
	GR_PUT_TILE(TIL_FRAME_0,   0,   0);
	GR_PUT_TILE(TIL_FRAME_2, 160,   0);
	GR_PUT_TILE(TIL_FRAME_3,   0, 160);
	GR_PUT_TILE(TIL_FRAME_5, 160, 160);
	for(i = 1; i < 10; i ++) {
		GR_PUT_TILE(TIL_FRAME_1, TIL2SCR(i),          0);
		GR_PUT_TILE(TIL_FRAME_4, TIL2SCR(i),        160);
		GR_PUT_TILE(TIL_FRAME_6,          0, TIL2SCR(i));
		GR_PUT_TILE(TIL_FRAME_7,        160, TIL2SCR(i));
	}
	/*mouse pointer over the map: draw it in the back buffer too*/
	if(MouseOn) {
		MouseArea.left = 8;
		MouseArea.right = 167;
		MouseArea.top = 8;
		MouseArea.bottom = 167;
		mouseInMap = MOUSE_IN_AREA(&MouseArea);
		if(mouseInMap) {
			Mouse.surface = &MapSurface;
			Mouse.posX -= 8;
			Mouse.posY -= 8;
			MOUSE_SAVE_BG();
			MOUSE_DRAW();
			Mouse.posX += 8;
			Mouse.posY += 8;
			Mouse.surface = &Screen;
		}
	}
	/*display the map part*/
	if(ScreenFade) {
		GR_FADE_TO_SCREEN(8, 8, 167, 167);
		ScreenFade = 0;
	} else {
		GR_COPY_TO_SCREEN(8, 8, 167, 167, 8, 8);
	}
}

/*one display "tick": recompute the view if needed (VisibilityDirty), animate
tiles/NPCs, redraw the map window, music tick, cyclops screen shake.
Called from every waiting loop of the game.*/
/*C_0A33_0CBC*/DisplayTick() {
	int16 shake;

	if(VisibilityDirty) {
		VIS_UpdateView();
		VisibilityDirty = 0;
	}
	if(MapAnimEnabled) {
		AnimateTilesAndActors();
		DrawMapWindow(1);
		MUS_PlayAmbient();
	}
	/*cyclops ground shake*/
	if(CyclopsFlag) {
		shake = CyclopsFlag;
		CyclopsFlag = 0;
		ShakeScreen(shake, 1);
	}
}

/*if the view is dirty, recompute it and redisplay the map now
(used before showing a hit/damage so the screen is up to date)*/
/*C_0A33_0D06*/FlushViewIfDirty() {
	if(VisibilityDirty) {
		VIS_UpdateView();
		VisibilityDirty = 0;
		DisplayTick();
	}
}

/*apply the effect of harmful tile "tileNum" to the creature objNum*/
/*C_0A33_0D1D*/TerrainDamage(int16 objNum, int16 tileNum) {
	int16 isImune;
	int16 bootsObj, objCls, dmg;

	isImune = 0;
	SetHurtThisTurn(objNum);/*terrain handled for this turn*/
	if(IsDead(objNum))
		return;
	objCls = GetMonsterClass(GetType(objNum));
	switch(tileNum) {
		case TIL_WEB:/*web: stuck unless STR >= 1d30*/
			if(IsProtected(objNum))
				break;
			if(GetStr(objNum) < OSI_rand(1, 30) && GetType(objNum) != OBJ_GIANT_SPIDER) {
				SetParalyzed(objNum);
				CON_printf(StuckInWebMsg, GetObjectString(objNum));
				PromptFlag = 1;
			}
		break;
		case TIL_SWAMP_0: case TIL_003: case TIL_004: case TIL_005:/*swamps?*/
			/*equipped swamp boots protect, otherwise same as poison field*/
			for(bootsObj = FindInvType(objNum, OBJ_SWAMP_BOOTS, -1); bootsObj >= 0; bootsObj = NextInvType()) {
				if(GetCoordUse(bootsObj) == EQUIP)
					break;
			}
			if(bootsObj >= 0)
				break;
		/*fall through*/
		case TIL_POISON_FIELD:/*poison field?*/
			if(IsProtected(objNum))
				isImune = 1;
			if(IsMonsterImmunePoison(objCls))
				isImune = 1;
			if(!isImune && !IsPoisoned(objNum)) {
				CON_printf(PoisonedMsg, GetObjectString(objNum));
				PromptFlag = 1;
				FlushViewIfDirty();
				COMBAT_showHit(objNum);
				SetPoisoned(objNum);
				StatusDirty = 1;
			}
		break;
		case TIL_LAVA_0: case TIL_0DD: case TIL_0DE: case TIL_0DF:/*lava?*/
		case TIL_CAMPFIRE: case TIL_COOK_FIRE_2: case TIL_COOK_FIRE_3: case TIL_COOK_FIRE_8:
		case TIL_COOK_FIRE_9: case TIL_FIRE_FIELD: case TIL_BOOKSTAND_1:/*fire related?*/
			/*fire: 1d30 for a fire field, 1d8 otherwise; doubled for
			  fire-sensitive monsters*/
			if(IsProtected(objNum))
				isImune = 1;
			if(IsMonsterFireImmune(objCls))
				isImune = 1;
			if(!isImune) {
				if(tileNum == TIL_FIRE_FIELD)
					dmg = OSI_rand(1, 30);
				else
					dmg = OSI_rand(1, 8);
				if(IsMonsterFireVulnerable(objCls))
					dmg <<= 1;
				FlushViewIfDirty();
				LoseHP(objNum, dmg);
			}
		break;
		case TIL_TANGLE_VINE_0: case TIL_TANGLE_VINE_1: case TIL_TANGLE_VINE_2:
		case TIL_TANGLE_VINE_3: case TIL_TANGLE_VINE_4: case TIL_TANGLE_VINE_5:
			/*tangle vine does not hurt itself*/
			if(GetType(objNum) == OBJ_TANGLE_VINE_VINE)
				break;
		/*fall through*/
		case TIL_FORCE_FIELD: case TIL_SPIKES:
			/*1d8*/
			if(IsProtected(objNum))
				break;
			FlushViewIfDirty();
			LoseHP(objNum, OSI_rand(1, 8));
		break;
		case TIL_TRAP: case TIL_LOG_SAW:
			/*1d30*/
			if(IsProtected(objNum))
				break;
			FlushViewIfDirty();
			LoseHP(objNum, OSI_rand(1, 30));
		break;
		case TIL_SLEEP_FIELD:/*sleep field*/
			if(IsMonsterImmuneSleep(objCls))
				isImune = 1;
			if(IsProtected(objNum))
				isImune = 1;
			if(!isImune && !IsAsleep(objNum)) {
				FlushViewIfDirty();
				COMBAT_showHit(objNum);
				SetAsleep(objNum);
				SetTypeUnconscious(objNum);
			}
		break;
	}
}

/*do terrain effects:
check whether creature objNum stands on a damaging tile (map tile or object
in the same cell) and apply it. Vehicles, flying monsters, creatures already
handled this turn (SkipSomeTest flag), on another level or outside the
loaded 40x40 area are skipped.*/
/*C_0A33_0FEB*/ApplyTerrainEffects(int16 objNum) {
	int16 otherObj;
	int16 tileNum, x, y, area_x, area_y, wrapMask, objTyp, objCls;

	objTyp = GetType(objNum);
	objCls = GetMonsterClass(objTyp);
	if(
		objTyp == OBJ_BALLOON_INFLATED ||
		objTyp == OBJ_SKIFF ||
		objTyp == OBJ_RAFT ||
		objTyp == OBJ_SHIP
	) return;
	if(objCls >= 0 && IsMonsterFlying(objCls))/*flies*/
		return;
	if(IsHurtThisTurn(objNum))
		return;
	if(GetZ(objNum) != MapZ)
		return;

	/*coordinates wrap at 1024 on the surface, 256 underground*/
	if(MapZ)
		wrapMask = 0xff;
	else
		wrapMask = 0x3ff;
	area_x = (MapX - 0x10) & 0x3f8;
	area_y = (MapY - 0x10) & 0x3f8;
	x = GetX(objNum);
	y = GetY(objNum);
	if(((x - area_x) & wrapMask) < AREA_W && ((y - area_y) & wrapMask) < AREA_H) {
		tileNum = GetTileAtXYZ(x, y, MapZ);
		if(IsTerrainDamage(tileNum)) {
			TerrainDamage(objNum, tileNum);
			return;
		}
		for(otherObj = FindLoc(x, y, MapZ); otherObj >= 0; otherObj = NextLoc()) {
			if(otherObj == objNum)
				continue;
			tileNum = TILE_FRAME(otherObj);
			if(IsTerrainDamage(tileNum)) {
				ClrInvisible(otherObj);/*reveal the trap/field*/
				TerrainDamage(objNum, tileNum);
				break;
			}
		}
	}
}

/*eruption management:
Erupting is set to 1 at random by PassTime, then to the first volcano or
fumarole found when the area objects are listed (object.c).
Volcano: screen shake; fumarole: shake or explosion.*/
/*C_0A33_117A*/static ProcessEruption() {
	int16 objTyp, objNum;

	if(Erupting <= 1)
		return;
	objTyp = GetType(Erupting);
	if(objTyp == OBJ_VOLCANO) {
		ShakeScreen(10, 2);
	} else if(objTyp == OBJ_FUMAROLE) {
		if(OSI_rand(0, 1)) {
			ShakeScreen(10, 2);
		} else {
			objNum = AddMapObj(OBJ_FUMAROLE, 0, 0, GetX(Erupting), GetY(Erupting), GetZ(Erupting));
			Detonate(objNum);
		}
	}
	Erupting = 0;
}

/*moongates management:
for each of the 8 moonstone locations inside the loaded area, show a
moongate while at least one moon is above the horizon (position < 15),
remove it otherwise.*/
/*C_0A33_121A*/UpdateMoongates() {
	int16 i, x, y, z, objNum, changed;

	changed = 0;
	for(i = 0; i < 8; i ++) {
		x = MoonstonePos[i][0];
		y = MoonstonePos[i][1];
		z = MoonstonePos[i][2];
		if(
			z == MapZ &&
			x >= AreaX && x < AreaX + AREA_W &&
			y >= AreaY && y < AreaY + AREA_H
		) {
			objNum = OBJ_FindTypeAt(OBJ_MOONGATE, x, y);
			if(TrammelPos < 15 || FeluccaPos < 15) {
				if(objNum < 0) {
					AddMapObj(TypeFrame(OBJ_MOONGATE, 1), 0, 0, x, y, z);
					changed = 1;
				}
			} else {
				if(objNum >= 0) {
					DeleteObj(objNum);
					changed = 1;
				}
			}
		}
	}
	if(changed) {
		VIS_UpdateView();
		VisibilityDirty = 1;
	}
}

/*wind update: no wind (-1) in dungeons (z 1..4); outdoors (z 0 or 5),
unless noChange, 1 chance in 64 to pick a new random wind (-1..7)*/
/*C_0A33_12F6*/UpdateWind(int16 noChange) {
	if(MapZ > 0 && MapZ < 5 && WindDir >= 0) {
		WindDir = -1;
		noChange = 1;
	}
	if(noChange)
		return;
	if(OSI_rand(0, 63))
		return;
	if(MapZ == 0 || MapZ == 5) {
		WindDir = OSI_rand(0, 8) - 1;
		noChange = 1;/*???*/
	}
}

/*wind direction names, indexed by WindDir + 1 ("C " = calm)*/
/*0357*/char *WindDirNames[] = {
	/*03B6*/"C ",
	/*03B9*/"N ",
	/*03BC*/"NE",
	/*03BF*/"E ",
	/*03C2*/"SE",
	/*03C5*/"S ",
	/*03C8*/"SW",
	/*03CB*/"W ",
	/*03CE*/"NW"
};
/*0369*/unsigned char LastHourUpdate = -1;/*hour of the last hourly update (-1: none yet)*/
/*phases of the two moons for each day of the 28-day month
({moon 1, moon 2}, 0..7)*/
/*036A*/unsigned char MoonPhaseByDay[][2] = {
	{0,0},/*day 1*/
	{7,0},/*day 2*/
	{7,7},/*day 3*/
	{6,6},/*day 4*/
	{6,5},/*day 5*/
	{5,4},/*day 6*/
	{5,3},/*day 7*/
	{4,2},/*day 8*/
	{3,1},/*day 9*/
	{3,0},/*day 10*/
	{2,0},/*day 11*/
	{2,7},/*day 12*/
	{1,6},/*day 13*/
	{1,5},/*day 14*/
	{0,4},/*day 15*/
	{7,3},/*day 16*/
	{7,2},/*day 17*/
	{6,1},/*day 18*/
	{6,0},/*day 19*/
	{5,0},/*day 20*/
	{5,7},/*day 21*/
	{4,6},/*day 22*/
	{3,5},/*day 23*/
	{3,4},/*day 24*/
	{2,3},/*day 25*/
	{2,2},/*day 26*/
	{1,1},/*day 27*/
	{1,0} /*day 28*/
};

/*world clock: advance the game time by "minutes" (0 = just refresh the
time-dependent state) and process everything that depends on it*/
/*C_0A33_1355*/PassTime(int16 minutes) {
	int16 i, invObj;
	int16 minute, newMinutes, sunTileOrLight, isEclipse;
	int16 torchLit, objTyp, wearsInvisRing, itemVanished, wearsStormCloak;
	char *vanishedName;

	/*time stop counter (nothing else happens while time is stopped)*/
	if(SpellFx[14]) {
		if(SpellFx[14] <= minutes) {
			GrInfo->animOn = 1;
			SpellFx[14] = 0;
		} else {
			GrInfo->animOn = 0;
			SpellFx[14] -= minutes;
			return;
		}
	}
	/*other spells*/
	for(i = 0; i < 16; i ++) {
		if(SpellFx[i]) {
			if(SpellFx[i] <= minutes)
				SpellFx[i] = 0;
			else
				SpellFx[i] -= minutes;
		}
	}
	/*powder keg*/
	if(KegFuse > minutes) {
		KegFuse -= minutes;
	} else if(KegFuse) {
		KegFuse = 0;
		Detonate(KegObject);
	}
	/*possible eruption*/
	if(OSI_rand(0, 7) == 0)
		Erupting = 1;
	/* */
	torchLit =
	wearsStormCloak = 0;
	for(i = 0; i < 0x100; i++) {
		if(ObjShapeType[i] == 0)
			continue;

		/*-- equipped items of party members --*/
		if(IsPlrControl(i)) {
			wearsInvisRing =
			itemVanished = 0;
			vanishedName = /*03D1*/"ring";
			for(invObj = FindInv(i); invObj >= 0; invObj = NextInv()) {
				if(GetCoordUse(invObj) == INVEN)
					continue;
				/*lit torch: half of the calls, its quantity (= remaining
				  minutes) is decreased by "minutes"*/
				if(GetType(invObj) == OBJ_TORCH && GetFrame(invObj) == 1) {
					if(OSI_rand(0, 1)) {
						if(GetQuan(invObj) <= minutes) {
							CON_printf(BurnOutMsg);
							DeleteObj(invObj);
							PromptFlag = 1;
						} else {
							torchLit = 1;
							SubQuan(invObj, minutes);
						}
					} else {
						torchLit = 1;
					}
				} else
				/*invisibility ring (1/1001 chance to vanish)*/
				if(GetType(invObj) == OBJ_INVISIBILITY_RING) {
					if(OSI_rand(0, 1000) == 0x1ad/*429*/) {
						itemVanished = 1;
						ClrInvisible(i);
						ScreenFade = 1;
						DrawMapWindow(1);
					} else {
						wearsInvisRing = 1;
					}
				} else
				/*regeneration ring: +1 HP per minute (1/1001 chance to vanish)*/
				if(GetType(invObj) == OBJ_REGENERATION_RING) {
					if(StatusDisplay == CMD_STAT_PARTY || (StatusDisplay == CMD_STAT_PORTRAIT && Party[StatusMember] == i)) {
						if(HitPoints[i] < MaxHP(i))
							StatusDirty ++;
					}
					if(HitPoints[i] + minutes < MaxHP(i)) {
						HitPoints[i] += minutes;
					} else {
						HitPoints[i] = MaxHP(i);
					}
					if(OSI_rand(0, 1000) == 0x2de/*734*/)
						itemVanished = 1;
				} else
				/*storm cloak: keeps the "negate magic" effect on
				  (1/1001 chance to vanish)*/
				if(GetType(invObj) == OBJ_STORM_CLOAK) {
					if(OSI_rand(0, 1000) == 0x24c/*588*/) {
						vanishedName = /*03D6*/"cloak";
						itemVanished = 1;
						SpellFx[13] = 0;
					} else {
						wearsStormCloak = 1;
					}
				}
				/* */
				if(itemVanished) {
					CON_printf(RingVanishedMsg, vanishedName);
					MUS_PlaySfx(12, 0);
					DeleteObj(invObj);
					PromptFlag = 1;
					itemVanished = 0;
				}
			}
		}

		/*-- status effects wearing off, one roll per minute --*/
		for(minute = 0; minute < minutes; minute ++) {
			if(IsInvisible(i) && !wearsInvisRing && OSI_rand(0, 63) == 0 && GetInt(i) <= OSI_rand(1, 30))
				ClrInvisible(i);
			if(IsProtected(i) && OSI_rand(0, 63) == 0 && GetInt(i) <= OSI_rand(1, 30))
				ClrProtected(i);
			if(IsCursed(i) && OSI_rand(0, 15) == 0 && GetInt(i) >= OSI_rand(1, 30))
				ClrCursed(i);
			if(IsCharmed(i) && OSI_rand(0, 7) == 0 && GetInt(i) >= OSI_rand(1, 30))
				COMBAT_stopCharm(i);
			if(IsParalyzed(i) && OSI_rand(0, 3) == 0 && GetStr(i) >= OSI_rand(1, 30))
				ClrParalyzed(i);
			if(IsAsleep(i) && NPCMode[i] != AI_SLEEP && OSI_rand(0, 15) == 0) {
				ClrAsleep(i);
				SetHurtThisTurn(i);
				ObjShapeType[i] = OrigShapeType[i];
			}
			if(IsPoisoned(i) && !IsProtected(i) && OSI_rand(0, 7) == 0)
				LoseHP(i, 1);
		}

		/*-- damaging terrain --*/
		ApplyTerrainEffects(i);
		ClrHurtThisTurn(i);
	}

	if(wearsStormCloak)
		SpellFx[13] = 1;
	/*-- calendar: 60 minutes, 24 hours, 28 days, 12 months --*/
	for(newMinutes = Time_M + minutes; newMinutes > 59; ) {
		newMinutes -= 60;
		/*-- +1 hour --*/
		if(++Time_H > 23) {
			Time_H = 0;
			/*-- +1 day --*/
			if(++Date_D > 28) {
				Date_D = 1;
				/*-- +1 month*/
				if(++Date_M > 12) {
					Date_M = 1;
					/*-- +1 year*/
					Date_Y ++;
				}
			}
		}

		if(NextSleep)
			NextSleep --;
		if(DrunkCounter)
			DrunkCounter --;

		/*NPC schedules*/
		UpdateSchedules();

		/*spell casters regain "level" magic points per hour*/
		for(i = 0; i < PartySize; i ++) {
			objTyp = GetType(Party[i]);
			if(objTyp == OBJ_AVATAR || objTyp == OBJ_MAGE || objTyp == OBJ_SWASHBUCKLER || objTyp == OBJ_MUSICIAN_LONG_CAPE) {
				MAGIC[Party[i]] += Level[Party[i]];
				if(MaxMagic(Party[i]) < MAGIC[Party[i]])
					MAGIC[Party[i]] = MaxMagic(Party[i]);
				StatusDirty ++;
			}
		}
	}
	/*hourly updates?*/
	if(LastHourUpdate != Time_H) {
		/*some flags (not on the very first call)*/
		if(LastHourUpdate != (unsigned char)-1) {
			HourlyUnused = /*unused?*/
			CaughtFish = 0;
			MustRingBell = 1;
			/*number of chimes*/
			MUS_Clock = Time_H % 12;
			if(MUS_Clock == 0)
				MUS_Clock = 12;
		}

		LastHourUpdate = Time_H;
		/*sundial: one frame per 2 hours from 5:00 to 20:59*/
		if(Time_H > 20 || Time_H < 5)
			sunTileOrLight = TIL_SUNDIAL_NIGHT;
		else
			sunTileOrLight = TIL_SUNDIAL + ((Time_H - 5) >> 1);
		GR_SET_ANIM_FRAME(BaseTile[OBJ_SUNDIAL], sunTileOrLight);
		/*moons: phase from the day of the month, position in the
		  sky = hours until moonset (like the sun's 19 - Time_H);
		  above the horizon if 0..14 (negative wraps to >= 15)*/
		TrammelPhase = MoonPhaseByDay[Date_D - 1][0];
		TrammelPos = (TrammelPhase * 3 + 18 - Time_H) % 24;
		FeluccaPhase = MoonPhaseByDay[Date_D - 1][1];
		FeluccaPos = (FeluccaPhase * 3 + 20 - Time_H) % 24;
		DrawSkyBar();
		UpdateMoongates();
	}
	/*wind*/
	UpdateWind(0);
	/*ambiant light: 0 at night, underground or during an eclipse,
	  ramps 1..6 at dawn (5h) and dusk (19h), 7 in daytime,
	  at least 4 with a lit torch or the light spell*/
	Time_M = newMinutes;
	sunTileOrLight = AmbientLight;
	isEclipse = ((Date_D == 1 && (Date_M % 3) == 0) || SpellFx[15]);
	if(isEclipse || !(Time_H >= 5 && Time_H <= 19) || (MapZ > 0 && MapZ < 5))
		AmbientLight = 0;
	else if(Time_H == 5)
		AmbientLight = Time_M / 10 + 1;
	else if(Time_H == 19)
		AmbientLight = (59 - Time_M) / 10 + 1;
	else
		AmbientLight = 7;
	if(AmbientLight < 4 && (torchLit || SpellFx[0]))
		AmbientLight = 4;
	if(AmbientLight != sunTileOrLight)
		VIS_UpdateView();
	/*display time/wind infos*/
	CON_setClip(&(TextWindows[0]));
	CON_gotoxy(2, 21);
	CON_printf(DateMsg, Date_M, Date_D, Date_Y, WindDirNames[WindDir + 1]);
	CON_setClip(&(TextWindows[3]));
}

/*redraw the status area (party list, portrait, equipment or the
conversation portrait) if StatusDirty; in solo mode the displayed member is
forced to the solo character*/
/*C_0A33_1AB7*/RefreshStatus() {
	if(StatusDirty) {
		if(SoloMember >= 0)
			StatusMember = SoloMember;
		switch(StatusDisplay) {
			case CMD_STAT_PARTY: STAT_ShowParty(PartyListTop); break;
			case CMD_STAT_PORTRAIT: STAT_ShowPortrait(StatusMember); break;
			case CMD_STAT_EQUIP: STAT_ShowInventory(StatusMember); break;
			case CMD_STAT_TALK: ShowPortraitAndInventory(Party[StatusMember]); break;
		}
		DrawSlotCursor(1);
	}
	StatusDirty = 0;
}

/*make party member #partyIdx the controlled character and centre the map
on him (partyIdx == PartySize: the vehicle)*/
/*C_0A33_1B36*/SetActive(int16 partyIdx) {
	if(Active == partyIdx)
		return;
	Active = partyIdx;
	MapX = GetX(Party[partyIdx]);
	MapY = GetY(Party[partyIdx]);
	MapZ = GetZ(Party[partyIdx]);
	MAP_LoadAround();
	MapAnimEnabled = 1;
	PromptFlag = 1;
}

/*back to party mode: restore the AI mode saved for the solo character,
the leader (Party[0]) is commanded, the others follow*/
/*C_0A33_1BB2*/SetPartyMode() {
	int16 i, objNum;

	if(SoloMember < 0)
		NPCMode[Party[0]] = SavedNPCMode;
	else
		NPCMode[Party[SoloMember]] = SavedNPCMode;
	SavedNPCMode = NPCMode[Party[0]];
	NPCMode[Party[0]] = AI_COMMAND;
	for(i = 1; i < PartySize; i ++) {
		objNum = Party[i];
		if(!IsCharmed(objNum))
			NPCMode[objNum] = AI_FOLLOW;
	}
	SetActive(0);
	SoloMember = 0xff;/*i.e. -1: party mode*/
	DrawSlotCursor(0);
	LastActiveIdx = 0;
}

/*read an hexadecimal number of up to "len" (max 4) digits from the
keyboard (debug commands); no validation of the digits*/
static /*C_0A33_1C4C*/ReadHexNumber(int16 len) {
	int16 i, value;

	if(len > 4)
		len = 4;
	CON_gets(HexInputBuf, len);
	value = 0;
	for(i = 0; i < len; i++) {
		if(HexInputBuf[i] == 0)
			break;
		value <<= 4;
		if(HexInputBuf[i] >= '0' && HexInputBuf[i] <= '9')
			value += HexInputBuf[i] - '0';
		else
			value += toupper(HexInputBuf[i]) - ('A' - 10);
	}
	return value;
}

/*commands names, indexed by CMD_xxx - CMD_ATTACK*/
/*03A2*/static char *CommandNames[] = {
	/*03DC*/"Attack",
	/*03E3*/"Cast",
	/*03E8*/"Talk",
	/*03ED*/"Look",
	/*03F2*/"Get",
	/*03F6*/"Drop",
	/*03FB*/"Move",
	/*0400*/"Use",
	/*0404*/"Rest",
	/*0409*/"Save"
};

/*game loop:
one iteration per player turn: read a key (CON_getch also returns mouse
generated CMD_xxx codes and runs the animations while waiting), translate
letter keys to commands, execute, then do the end of turn processing.
Exits on <ALT+X>/<CTRL+Q>/<CTRL+E> confirmed (ch == 0x12D).*/
/*C_0A33_1CB4*/GameLoop() {
	int16 ch, key;
	int16 goto_x, goto_y, goto_z, member;

	CON_printf(/*040E*/"%s:\n%>", Names[Active]);
	do {
		RefreshStatus();

		if(MouseMode == 0)
			AllowMouseMov = 1;
		ch = CON_getch();
		AllowMouseMov = 0;

		/*keyboard commands: start a target selection (cursor at the
		  centre of the map window, SelectRange < 0 = direction only)*/
		switch(ch) {
			case 'A'/*ttack*/:
				ch = CMD_ATTACK;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = 7;
				/*aim at the previous target if still on screen*/
				if(LastAttackTarget >= 0) {
					AimX = (GetX(LastAttackTarget) & 0xff) - MapX + 5;
					AimY = (GetY(LastAttackTarget) & 0xff) - MapY + 5;
					if(AimX >= 0 && AimX <= 10 && AimY >= 0 && AimY <= 10)
						break;
					AimX =
					AimY = 5;
				}
			break;
			case 'C'/*ast*/:
				ch = CMD_CAST;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 3;
				AimX =
				AimY = 5;
				SelectRange = 7;
			break;
			case 'T'/*alk*/:
				ch = CMD_TALK;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = 7;
			break;
			case 'L'/*ook*/:
				ch = CMD_LOOK;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = 7;
			break;
			case 'G'/*et*/:
				ch = CMD_GET;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = -1;
			break;
			case 'D'/*rop*/:
				/*selection in the inventory: show the equipment of the
				  active character*/
				ch = CMD_DROP;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 2;
				AimX =
				AimY = 5;
				SelectRange = 7;
				SlotCursorCol = 3;
				SlotCursorRow = 0;
				if(!IN_VEHICLE && (StatusDisplay != CMD_STAT_EQUIP || Active != StatusMember)) {
					StatusDisplay = CMD_STAT_EQUIP;
					StatusMember = Active;
					StatusDirty = 1;
					RefreshStatus();
				}
				DrawSlotCursor(1);
			break;
			case 'M'/*ove*/:
				ch = CMD_MOVE;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = -1;
			break;
			case 'U'/*se*/:
				ch = CMD_USE;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = -1;
			break;
			case 'R'/*est*/:
				ch = CMD_REST;
				DrawSlotCursor(0);
				FreeAimMode = 0;
				SelectMode = 1;
				AimX =
				AimY = 5;
				SelectRange = 7;
			break;
			case 19:/*<CTRL+S>*/
				ch = CMD_SAVE;
			break;
			case 'B'/*egin/break combat*/:
				ch = CMD_COMBAT;
			break;
		}

		/*character switch: '0' = party mode, '1'..'9' = solo mode*/
		if(ch >= '0' && ch <= '9' && ch < PartySize + '1') {
			if(ch == '0') {/*Party Mode*/
				if(IN_VEHICLE) {
					CON_printf(NotOnShipMsg);
					goto C_262D;
				}
				if(InCombat) {
					CON_printf(NotInCombatMsg);
					goto C_262D;
				}
				/*everybody must be within 8 tiles*/
				for(member = 0; member < PartySize; member ++) {
					if(!CLOSE_ENOUGH(8, MapX, MapY, GetX(Party[member]), GetY(Party[member]))) {
						CON_printf(NotAllHereMsg);
						break;
					}
				}
				if(PartySize != member) {
					goto C_262D;
				}
				CON_printf(PartyModeMsg);
				SetPartyMode();
				MouseMode = 0;
			} else {/*Solo Mode*/
				if(IsIncapacitated(Party[ch - '1']) || IsDraggedUnder(Party[ch - '1']) || IsCharmed(Party[ch - '1'])) {
					CON_printf(FailsToRespondMsg, Names[ch - '1']);
					goto C_262D;
				}
				if(IN_VEHICLE) {
					CON_printf(NotOnShipMsg);
					goto C_262D;
				}
				if(InCombat) {
					CON_printf(NotInCombatMsg);
					goto C_262D;
				}
				/*restore the AI mode of the previously commanded
				  character and save the one of the new solo character*/
				if(SoloMember < 0)
					NPCMode[Party[Active]] = SavedNPCMode;
				else
					NPCMode[Party[SoloMember]] = SavedNPCMode;
				SavedNPCMode = NPCMode[Party[ch - '1']];
				/*followers stay where they are*/
				for(member = 0; member < PartySize; member ++) {
					if(NPCMode[Party[member]] == AI_FOLLOW)
						NPCMode[Party[member]] = AI_MOTIONLESS;
				}
				SetActive(ch - '1');
				NPCMode[Party[Active]] = AI_COMMAND;
				SoloMember = Active;
				if(StatusDisplay != CMD_STAT_PARTY)
					StatusDirty = 1;
				CON_printf(SoloModeMsg);
				MouseMode = 0;
				DrawSlotCursor(0);
			}
			SelectMode = 0;
			PendingCommand = CMD_PASS;
			CON_UpdateMousePointer();
			goto C_262D;
		}

		switch(ch) {
			case CMD_DIRECTION:/*[advance]*/
				PartyAdvance(AdvanceDir);
			break;
			case 5: case 17: case 0x12D:/*<CTRL+E> <CTRL+Q> <ALT+X> --"exit to dos?"*/
				CON_printf(ExitToDOSMsg);
				while((key = CON_getch()) != 'Y' && key != 'N');
				CON_printf(/*0415*/"%c\n", key);
				if(key == 'N')
					ch = 0;
				else
					ch = 0x12D;
			break;
			case CMD_ATTACK:/*"attack"*/
				COMBAT_attack();
				PendingCommand = CMD_PASS;
			break;
			case CMD_COMBAT:/*begin/break combat*/
				if(SoloMember >= 0) {
					CON_printf(NotInSoloMsg);
				} else if(InCombat) {/*break off*/
					CON_printf(BreakOffMsg);
					COMBAT_breakOff();
				} else if(IN_VEHICLE) {
					CON_printf(NotOnShipMsg);
				} else {
					CON_printf(CombatMsg);
					COMBAT_begin();
				}
			break;
			case CMD_CAST:
			case CMD_GET:
			case CMD_DROP:
			case CMD_MOVE:
				if(ch == CMD_CAST) {
					/*reset the incantation*/
					IncantMask =
					IncantLen = 0;
				}
				/*not allowed aboard a vehicle*/
				if(IN_VEHICLE) {
					CON_printf(/*0419*/"%s-", CommandNames[ch - CMD_ATTACK]);
					CON_printf(NotOnShipMsg);
					break;
				}
			/*break;*/
			case CMD_TALK:
			case CMD_LOOK:
			case CMD_USE:
				/*wait for the target (CMD_TARGET = selection done)*/
				MouseMode = 1;
				PendingCommand = ch;
				CON_UpdateMousePointer();
				CON_printf(/*0419*/"%s-", CommandNames[ch - CMD_ATTACK]);
				if(ch == CMD_CAST) {
					CMD_CastSpell();/*"cast"?*/
				} else {
					key = CON_getch();
					if(key == CMD_TARGET) {
						switch(PendingCommand) {
							case CMD_LOOK: CmdLook(); break;/*"look"*/
							case CMD_DROP: CmdDrop(); break;/*"drop"*/
							case CMD_GET: CmdGet(); break;/*"get"*/
							case CMD_MOVE: CmdMove(); break;/*"move"*/
							case CMD_USE: CmdUse(); break;/*"use"*/
							case CMD_TALK:/*"talk"*/
								TALK_talkTo(Active, Selection.obj, 1);
								MUS_PlayAreaSong();
							break;
						}
					} else {
						CON_printf(WhatMsg);
					}
				}
				MouseMode = 0;

				PendingCommand = CMD_PASS;
				DrawSlotCursor(0);
				SelectMode = 0;
				CON_UpdateMousePointer();
			break;
			case CMD_REST:/*"rest"*/
				if(!IN_VEHICLE)
					MUS_PlaySong(8);
				REST_Command();
				if(!IN_VEHICLE)
					MUS_PlayAreaSong();
				MouseMode = 0;
				DrawSlotCursor(0);
				SelectMode = 0;
				CON_UpdateMousePointer();
			break;
			case CMD_SAVE:/*"save game"*/
				if(IsArmageddon)
					break;
				CON_printf(SaveMsg);
				while((key = CON_getch()) != 'Y' && key != 'N');
				CON_printf(/*0415*/"%c\n", key);
				if(key == 'Y') {
					GAME_Save();
					DrawSlotCursor(0);
					MouseMode = 0;
					SelectMode = 0;
					CON_UpdateMousePointer();
				}
			break;
			case 18:/*<CTRL+R> "restore game"*/
				CON_printf(/*041D*/"Restore game? ");
				while((key = CON_getch()) != 'Y' && key != 'N');
				CON_printf(/*0415*/"%c\n", key);
				if(key == 'Y')
					GAME_Restore();
			break;
			case 27: case ' ': case CMD_PASS:/*"pass"*/
				CON_printf(PassMsg);
				MovePts[Party[Active]] = 0;
				DrawSlotCursor(0);
				MouseMode = 0;
				SelectMode = 0;
				CON_UpdateMousePointer();
				/*raft/balloon drift with the wind*/
				if(
					(GetType(Party[Active]) == OBJ_RAFT || GetType(Party[Active]) == OBJ_BALLOON_INFLATED) &&
					WindDir >= 0
				) PartyAdvance(WindDir);
				MoveFollowers(Party[Active], 1);
			break;
			case 26:/*<CTRL+Z>*/
				if(SoundFlag) {
					MUS_PlaySong(0);
					CON_printf(SoundOffMsg);
					SoundFlag = 0;
				} else {
					SoundFlag = 1;
					MUS_PlayAreaSong();
					CON_printf(SoundOnMsg);
				}
			break;
			case 8:/*<CTRL+H>*/
				CON_printf(/*042C*/"Help ");
				HelpOn = !HelpOn;
				CON_printf(HelpOn?/*0432*/"on":/*0435*/"off");
				CON_printf(/*0439*/"!\n");
			break;
			case 0x119:/*<ALT+P> palette related*/
				SetVGAPalette();
			break;
			case 22:/*<CTRL+V> version string*/
				CON_printf(VersionMsg);
			break;
			case 213:/*<alt 2 1 3> peer+info (debug: karma, time, position)*/
				CON_printf(/*043C*/"%02d%02d%02d%03x%03x%1x\n",
					KARMA, Time_H, Time_M, MapX, MapY, MapZ);
				PeerMap();/*peer?*/
			break;
			case 214:/*<alt 2 1 4> go to coordinate (debug, hexadecimal x,y,z)*/
				if(IN_VEHICLE) {
					CON_printf(/*0455*/"<nat uail abord wip!>\n");
					break;
				}
				CON_printf(/*046C*/"<gotu eks>: ");
				goto_x = ReadHexNumber(3);
				CON_printf(/*0479*/"\n<uai>: ");
				goto_y = ReadHexNumber(3);
				CON_printf(/*0482*/"\n<zi>: ");
				goto_z = ReadHexNumber(1);
				if(goto_z) {
					goto_x &= 0xff;
					goto_y &= 0xff;
				} else {
					goto_x &= 0x3ff;
					goto_y &= 0x3ff;
				}
				if(goto_z > 5)
					goto_z = 5;
				PartyTeleport(goto_x, goto_y, goto_z, 0);
				MUS_PlayAreaSong();
			break;
			case 215:/*<alt 2 1 5>time += 60 minutes (debug)*/
				PassTime(60);
				AllowNPCTeleport = 1;
				PTH_SearchPaths();
				AllowNPCTeleport = 0;
			break;
			default:
				CON_printf(WhatMsg);
				DrawSlotCursor(0);
				MouseMode = 0;
				SelectMode = 0;
				CON_UpdateMousePointer();
		}

	/*-- end of turn --*/
C_262D:
		if(ch != 0x12D) {
			/*"disk full" recovery choice*/
			if(DiskFullChoice == 'R')
				GAME_Restore();/*Restore game?*/
			else if(DiskFullChoice == 'G')
				GAME_Save();/*Save game?*/
			/*NPCs and the rest of the world take their turn*/
			RunNPCTurns();
			ProcessEruption();
			if(ch != CMD_DIRECTION)
				PromptFlag = 1;
			if(PromptFlag) {
				CON_printf(/*048A*/"\n%s:\n%>", Names[Active]);
				PromptFlag = 0;
			}
		}
	} while(ch != 0x12D);
}
