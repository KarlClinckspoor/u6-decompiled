/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __U6_H__
#define __U6_H__

union tHandler {
	struct {
		unsigned ofs, seg;
	} iii;
	void far *p;
	void far *(*f)();
};

#include <dos.h>
#include <stdlib.h>
#include <alloc.h>
#include <process.h>
#include <ctype.h>
#include <string.h>

#include "gr.h"
#include "mouse.h"
#include "osi_file.h"
#include "monsdata.h"
#include "msgs.h"

#include "ai.h"
#include "tile.h"
#include "obj.h"
#include "cmd.h"
#include "spells.h"

#define SCR2TIL(i) ((i) >> 4)
#define TIL2SCR(i) ((i) << 4)

/* */

#define DELTA(a,b) (abs((a)-(b)))
#define CLOSE_ENOUGH(d,x1,y1,x2,y2) (DELTA(x1,x2) <= (d) && DELTA(y1,y2) <= (d))
#define CLOSE_ENOUGH_S(d,x1,y1,x2,y2) (DELTA(x1,x2) < (d) && DELTA(y1,y2) < (d))

#define DELTA0(a,b) (__abs__((a)-(b)))
#define CLOSE_ENOUGH0(d,x1,y1,x2,y2) (DELTA0(x1,x2) <= (d) && DELTA0(y1,y2) <= (d))
#define CLOSE_ENOUGH0_S(d,x1,y1,x2,y2) (DELTA0(x1,x2) < (d) && DELTA0(y1,y2) < (d))

/* */

#define __Amount_ptr(o, i) ((unsigned char far *)(Amount + (o)))[i]

#define GetQuan(o) (__Amount_ptr(o, 0))
#define GetQual(o) (__Amount_ptr(o, 1))
#define SetQuan(o, v) (__Amount_ptr(o, 0) = (v))
#define SetQual(o, v) (__Amount_ptr(o, 1) = (v))
#define AddQuan(o, v) (__Amount_ptr(o, 0) += (v))
#define AddQual(o, v) (__Amount_ptr(o, 1) += (v))
#define SubQuan(o, v) (__Amount_ptr(o, 0) -= (v))
#define SubQual(o, v) (__Amount_ptr(o, 1) -= (v))
#define QualQuan(quality, quantity) ((quality) << 8 | (quantity))

/*----------------------------------------------------*/
/*these macros test the simple Bit fields in ObjStatus*/
/*----------------------------------------------------*/
#define LOCXYZ 0
#define CONTAINED 8
#define INVEN 0x10
#define EQUIP 0x18

#define SetCoordUse(i, v) (ObjStatus[i] = (ObjStatus[i] & ~0x18) | (v))
#define GetCoordUse(i) (ObjStatus[i] & 0x18)
/*coord use is INVEN or EQUIP (owner is an NPC)*/
#define IsHeldByNPC(i) (ObjStatus[i] & 0x10)

/*bit 0 is really "ok to take": taking an object without it (on the surface) is stealing, see IsOkToGet*/
#define OWNED     0x01
#define INVISIBLE 0x02
#define CHARMED   0x04
#define LOCAL     0x20
#define CURSED    0x40
#define MUTANT    0x40
#define HATCHED   0x40
#define LIT       0x80

/*clear CURSED and CHARMED*/
#define ClrCursedCharmed(i) (ObjStatus[i] &= ~(CURSED|CHARMED))
/*clear CURSED, CHARMED and INVISIBLE*/
#define ClrCursedCharmedInvis(i) (ObjStatus[i] &= ~(CURSED|CHARMED|INVISIBLE))

#define IsOwned(i)      (ObjStatus[i] & OWNED)
#define IsInvisible(i)  (ObjStatus[i] & INVISIBLE)
#define IsCharmed(i)    (ObjStatus[i] & CHARMED)
#define IsLocal(i)      (ObjStatus[i] & LOCAL)
#define IsCursed(i)     (ObjStatus[i] & CURSED)
#define IsHatched(i)    (ObjStatus[i] & HATCHED)
#define IsMutant(i)     (ObjStatus[i] & MUTANT)
#define IsLit(i)        (ObjStatus[i] & LIT)
#define SetOwned(i)     ObjStatus[i] |= OWNED
#define SetInvisible(i) ObjStatus[i] |= INVISIBLE
#define SetCharmed(i)   ObjStatus[i] |= CHARMED
#define SetLocal(i)     ObjStatus[i] |= LOCAL
#define SetCursed(i)    ObjStatus[i] |= CURSED
#define SetHatched(i)   ObjStatus[i] |= HATCHED
#define SetLit(i)       ObjStatus[i] |= LIT
#define ClrInvisible(i) ObjStatus[i] &= ~INVISIBLE
#define ClrCharmed(i)   ObjStatus[i] &= ~CHARMED
#define ClrLocal(i)     ObjStatus[i] &= ~LOCAL
#define ClrCursed(i)    ObjStatus[i] &= ~CURSED
#define ClrHatched(i)   ObjStatus[i] &= ~HATCHED
#define ClrLit(i)       ObjStatus[i] &= ~LIT

/*----------------------------------------------------*/
/*these macros test the simple Bit fields in NPCStatus*/
/*----------------------------------------------------*/
#define NEUTRAL 0
#define EVIL 0x20
#define GOOD 0x40
#define CHAOTIC 0x60

#define SetAlignment(i, v) (NPCStatus[i] = (NPCStatus[i] & ~0x60) | (v))
#define GetAlignment(i) (NPCStatus[i] & 0x60)
#define Is_ATKPLR(i) (GetAlignment(i) & 0x20)
#define Is_ATKMON(i) (GetAlignment(i) & 0x40)

#define PROTECTED  0x01
#define PARALYZED  0x02
#define ASLEEP     0x04
#define POISONED   0x08
#define DEAD       0x10
#define PLRCONTROL 0x80

/*NPCStatus DEAD or ASLEEP (GetObjectString then names the original shape)*/
#define IsDeadOrAsleep(i) (NPCStatus[i] & (DEAD|ASLEEP))
/*dead, asleep or paralyzed*/
#define IsIncapacitated(i) (NPCStatus[i] & (DEAD|ASLEEP|PARALYZED))
/*clear ASLEEP and PARALYZED*/
#define ClrAsleepParalyzed(i) (NPCStatus[i] &= ~(ASLEEP|PARALYZED))
/*clear DEAD, POISONED, ASLEEP, PARALYZED (resurrection)*/
#define ClrAilments(i) (NPCStatus[i] &= ~(DEAD|POISONED|ASLEEP|PARALYZED))
/*clear DEAD, POISONED, ASLEEP, PARALYZED and PROTECTED (resurrection)*/
#define ClrAilmentsProtected(i) (NPCStatus[i] &= ~(DEAD|POISONED|ASLEEP|PARALYZED|PROTECTED))

#define IsProtected(i)   (NPCStatus[i] & PROTECTED)
#define IsParalyzed(i)   (NPCStatus[i] & PARALYZED)
#define IsAsleep(i)      (NPCStatus[i] & ASLEEP)
#define IsPoisoned(i)    (NPCStatus[i] & POISONED)
#define IsDead(i)        (NPCStatus[i] & DEAD)
#define IsPlrControl(i)  (NPCStatus[i] & PLRCONTROL)
#define SetProtected(i)  (NPCStatus[i] |= PROTECTED)
#define SetParalyzed(i)  (NPCStatus[i] |= PARALYZED)
#define SetAsleep(i)     (NPCStatus[i] |= ASLEEP)
#define SetPoisoned(i)   (NPCStatus[i] |= POISONED)
#define SetDead(i)       (NPCStatus[i] |= DEAD)
#define SetPlrControl(i) (NPCStatus[i] |= PLRCONTROL)
#define ClrProtected(i)  (NPCStatus[i] &= ~PROTECTED)
#define ClrParalyzed(i)  (NPCStatus[i] &= ~PARALYZED)
#define ClrAsleep(i)     (NPCStatus[i] &= ~ASLEEP)
#define ClrPoisoned(i)   (NPCStatus[i] &= ~POISONED)
#define ClrDead(i)       (NPCStatus[i] &= ~DEAD)
#define ClrPlrControl(i) (NPCStatus[i] &= ~PLRCONTROL)

/*--------------------------------------------------*/
/*these macros test the simple Bit fields in NPCFlag*/
/*--------------------------------------------------*/
#define GetDirection(i)      (NPCFlag[i] & 7)
#define SetDirection(i, v)   (NPCFlag[i] = (NPCFlag[i] & ~7) | (v))

#define GetBKAlignment(i)    (NPCFlag[i] & 0x60)
#define SetBKAlignment(i, v) (NPCFlag[i] = (NPCFlag[i] & ~0x60) | (v))

/*NPCFlag bit 8: set by TerrainDamage, a combat hit or waking; ApplyTerrainEffects skips terrain damage while set; cleared after MoveObj and each time step*/
#define IsHurtThisTurn(i)    (NPCFlag[i] & 8)
#define IsDraggedUnder(i)    (NPCFlag[i] & 0x10)
#define IsWalking(i)         (NPCFlag[i] & 0x80)
/*see IsHurtThisTurn*/
#define SetHurtThisTurn(i)   (NPCFlag[i] |= 8)
#define SetDraggedUnder(i)   (NPCFlag[i] |= 0x10)
#define SetWalking(i)        (NPCFlag[i] |= 0x80)
/*see IsHurtThisTurn*/
#define ClrHurtThisTurn(i)   (NPCFlag[i] &= ~8)
#define ClrDraggedUnder(i)   (NPCFlag[i] &= ~0x10)
#define ClrWalking(i)        (NPCFlag[i] &= ~0x80)

/*------------------------------------------------------*/
/*                                                      */
/*------------------------------------------------------*/
/*====---- TerrainType ----====*/
/*[Wet]*/
#define TERRAIN_WET 0x01
/*[Impass]able*/
#define TERRAIN_IMPASSABLE 0x02
/*[Wall]*/
#define TERRAIN_WALL 0x04
/*[Damag]ing*/
#define TERRAIN_DAMAGING 0x08
/*[W]*/
#define TERRAIN_WALL_W 0x10
/*[S]*/
#define TERRAIN_WALL_S 0x20
/*[E]*/
#define TERRAIN_WALL_E 0x40
/*[N]*/
#define TERRAIN_WALL_N 0x80

#define IsTerrainWet(tile)    (TerrainType[tile] & TERRAIN_WET)
#define IsTerrainImpass(tile) (TerrainType[tile] & TERRAIN_IMPASSABLE)
#define IsTerrainWall(tile)   (TerrainType[tile] & TERRAIN_WALL)
#define IsTerrainDamage(tile) (TerrainType[tile] & TERRAIN_DAMAGING)

/*====---- TileFlag ----====*/
/*[Light] level*/
#define TILE_LIGHT_MASK 0x03
/*[Opa]que*/
#define TILE_OPAQUE 0x04
/*[Win]dow*/
#define TILE_WINDOW 0x08
/*[For]eground*/
#define TILE_FOREGROUND 0x10
/*[Nos]hooting through*/
#define TILE_BLOCKS_MISSILES 0x20
/*[Double] height: the object also covers the tile above (y-1)*/
#define TILE_DOUBLE_HEIGHT 0x40
/*[Double] width: the object also covers the tile to the left (x-1)*/
#define TILE_DOUBLE_WIDTH 0x80

#define GetTileLight(tile)  (TileFlag[tile] & TILE_LIGHT_MASK)
#define IsTileOpa(tile)     (TileFlag[tile] & TILE_OPAQUE)
#define IsTileWin(tile)     (TileFlag[tile] & TILE_WINDOW)
#define IsTileFor(tile)     (TileFlag[tile] & TILE_FOREGROUND)
#define IsTileNos(tile)     (TileFlag[tile] & TILE_BLOCKS_MISSILES)
#define IsTileDoubleV(tile) (TileFlag[tile] & TILE_DOUBLE_HEIGHT)
#define IsTileDoubleH(tile) (TileFlag[tile] & TILE_DOUBLE_WIDTH)

/*====---- TileFlag2 ----====*/
/*[Wa]rm*/
#define TILE_FLAG2_01 0x01
/*[Su]pporting*/
#define TILE_FLAG2_02 0x02
/*[Br]eakthrough*/
#define TILE_FLAG2_04 0x04
/*[Ge]neric -- "Replicate"-able and "Vanish"-able*/
#define TILE_FLAG2_08 0x08
/*[Ig]nore*/
#define TILE_FLAG2_10 0x10
/*[Ba]ckground*/
#define TILE_FLAG2_20 0x20
/*x a an the*/
#define TILE_FLAG2_ARTICLE 0xC0

#define IsTileWa(tile) (TileFlag2[tile] & TILE_FLAG2_01)
#define IsTileSu(tile) (TileFlag2[tile] & TILE_FLAG2_02)
#define IsTileBr(tile) (TileFlag2[tile] & TILE_FLAG2_04)
#define IsTileGe(tile) (TileFlag2[tile] & TILE_FLAG2_08)
#define IsTileIg(tile) (TileFlag2[tile] & TILE_FLAG2_10)
#define IsTileBa(tile) (TileFlag2[tile] & TILE_FLAG2_20)
/*------------------------------------------------------*/
/* */
#define TypeFrame(t, s) ((t) + ((s) << 10))
#define GetType(i) (ObjShapeType[i] & 0x3ff)
#define GetFrame(i) (ObjShapeType[i] >> 10)
#define SetType(i, v) (ObjShapeType[i] = (ObjShapeType[i] & ~0x3ff) + (v))
#define SetFrame(i, v) (ObjShapeType[i] = TypeFrame(GetType(i),(v)))

#define GetCoordX(o) ((*(int *)&((o).raw[0]))&0x3ff)
#define GetCoordY(o) (((*(int *)&((o).raw[1]))>>2)&0x3ff)
#define GetCoordZ(o) ((o).raw[2] >> 4)
#define GetCoordAssoc(i) *(unsigned int *)(&(i))

#define SetCoordX(o, v) { (*(int *)&((o).raw[0])) &= ~0x3ff; (*(int *)&((o).raw[0])) |= (v); }
#define SetCoordY(o, v) { (*(int *)&((o).raw[1])) &= ~(0x3ff << 2); (*(int *)&((o).raw[1])) |= (v) << 2; }

#define SetCoordXYZ(o, x, y, z) {      \
	(o).raw[2] = ((z) << 4);           \
	*(int *)&((o).raw[0]) = (x);       \
	*(int *)&((o).raw[1]) |= (y) << 2; \
}

#define SetCoordAssoc(i, j) *(unsigned int *)(&(i)) = (j)

#define SetXYZ(i, x, y, z) SetCoordXYZ(ObjPos[i], x, y, z)
#define SetAssoc(i, j) SetCoordAssoc(ObjPos[i], j)

#define GetX(i) GetCoordX(ObjPos[i])
#define GetY(i) GetCoordY(ObjPos[i])
#define GetZ(i) GetCoordZ(ObjPos[i])
#define GetAssoc(i) GetCoordAssoc(ObjPos[i])

/*------------------------------------------------------*/
#define AREA_H 40
#define AREA_W 40
#define SHAMINO_COMMENT (IsPlrControl(3) && !IsDead(3) && COMBAT_TileDistance(3, MapX, MapY) < 6)
#define IN_VEHICLE (Active == PartySize)
#define TILE_FRAME(objNum) (BaseTile[GetType(objNum)] + GetFrame(objNum))

#define MAX_ANIM 0x20

/*for Equipment[]*/
#define SLOT_HEAD 0
#define SLOT_NECK 1
#define SLOT_RHND 2
#define SLOT_RFNG 3
#define SLOT_CHST 4
#define SLOT_LHND 5
#define SLOT_LFNG 6
#define SLOT_FEET 7
#define SLOT_2HND 8
#define SLOT_RING 9
/* */

struct coord {
	unsigned char raw[3];
};

struct tScratchBuf {
	struct {
		unsigned char status;/*_00*//*ObjStatus*/
		struct coord pos;/*ObjPos (for CONTAINED: index of the container within the block)*/
		int shapeType;/*ObjShapeType*/
		int amount;/*_06*//*Amount*/
	} objs[0xc00];/*objblk record buffer (0xC00 entries of 8 bytes, the on-disk format)*/
	int objNum[0xc00];/*block entry -> object number map used while (de)serializing*/
	unsigned char spare[0x100];/*unused by object.c; pads the buffer to 0x7900 (size allocated in main.c)*/
};

struct tClipInfo/*size:9*/ {
	unsigned char originX, originY, cols, rows;
	unsigned char curX, curY;
	unsigned char fgColor, bgColor;
	unsigned char attrFlags;/*CLIP_xxx bits: 1 wrap, 2 center, 4 '`' capitalize, 8 runes, 0x10 paginate, 0x20 scroll*/
};

/*C_0903_00DE*/extern void far *LoadFile(char *, void far *);
/*OSI file error callback (OSI_ErrorHandler): ignores EOF/missing-delete, disk full -> DISK_full, else prints DISK ERROR and waits key; clears OSI_ErrorCode to retry unless Ctrl-C/Esc*/
/*C_0903_0430*/extern void FileErrorHandler();
/*C_0903_033A*/extern LoadFileGr(char far *, void far *, int);


/*C_0C9C_01FB*/extern CON_UpdateMousePointer();/*mouse callback: choose pointer shape (arrow / target / 8 direction arrows) from position and MouseMode*/
/*C_0C9C_331A*/extern CON_itoa(unsigned, char far *, int, int);/*converts an int to a string, not the other way*/
/*C_0C9C_3643*/extern cdecl CON_printf(unsigned char far *, ...);

/*C_101C_0DC9*/extern unsigned char GetTileAtXYZ(int, int, int);

/*C_2FC1_10D2*/extern int far *Explosion(int, int, int);
/*C_2FC1_11EB*/extern int far *MagicWind(int, int, int, int);

/*DiskNames names; ULTIMA6.EXE*/
#define DISK_PROGRAM 1
/*music files*/
#define DISK_INTRO 2
/*END.EXE*/
#define DISK_ENDGAME 3
/*drivers, tiles, palette, assumed in current drive at start*/
#define DISK_GAME 4
/*map*/
#define DISK_MAP 5
/*portraits, newmagic.bmp*/
#define DISK_POPULACE_A 6
/*monster/gargoyle portraits*/
#define DISK_POPULACE_B 7
/*savegame/temp files*/
#define DISK_PLAYER 8
/*C_32C3_0097*/extern DISK_confirm(int);/*request floppy*/

/*C_32FD_0000*/extern cdecl decompress(int, unsigned long, void far *, unsigned);


/*0194*/extern int IsZFile;
/*0198*/extern char *BitmapFiles[];/*full screen bitmap names: paper, newmagic, u6face0, stats, worldmap*/
/*01A2*/extern char DirIncrX[];
/*01AA*/extern char DirIncrY[];
/*01B2*/extern char *DirNames[];
/*01CA*/extern int MusicAdapter;/*0 none, 1 AdLib, 2 CMS, 3 Tandy, 4 Roland, 5 Covox, 6 Innovation (driver list MUS_DriverFiles)*/
/*01CE*/extern int GrDriverOption;/*'$' config flag, added to the driver init parameter; meaning unknown*/
/*01D0*/extern unsigned char MouseOn;

/*0338*/extern int AnimFrameCounter;/*incremented each animation tick; drives tile animation and clock tic-tac*/
/*033A*/extern unsigned char StatusDirty;
/*033B*/extern unsigned char PromptFlag;
/*033C*/extern unsigned char CyclopsFlag;
/*033D*/extern unsigned char IsArmageddon;
/*033E*/extern int IdleAnimTick;/*set by idle keyboard/rest loops; allows one pass of NPC idle animation, then cleared*/
/*0340*/extern int MapAnimEnabled;/*enables animation+map redraw in OtherAnimations; cleared by DISK_full, set on save/restore/SetActive*/
/*0342*/extern int Erupting;
/*0492*/extern unsigned char SelectMode;
/*0493,0494*/extern char AimX,AimY;
/*0495*/extern char SelectRange;
/*0497*/extern unsigned char FreeAimMode;/*aiming cursor started with TAB without a command (ENTER then runs the default command)*/
/*0499*/extern char SlotCursorCol;/*keyboard slot cursor column in equipment/portrait view*/
/*049A*/extern char SlotCursorRow;/*keyboard slot cursor row (-1 container slot, 0~2 grid, 3 buttons)*/
/*049C*/extern unsigned char AnimateAtPrompt;/*run tile/palette animations while waiting for a key*/
/*049D*/extern int LastNumberPlural;/*last number printed != 1, drives the '/' and '\' singular/plural escapes*/
/*049F*/extern int EndPagingAtPrompt;/*clear the paginate attribute (0x10) at the next prompt (set after books/look/talk)*/
/*04A1*/extern int HelpOn;/*<CTRL+H> help flag: highlight '@' keywords*/
/*04A5*/extern int TextOn;
/*04A7*/extern int LastAttackTarget;/*object last attacked; the attack cursor starts on it*/
/*04A9*/extern int SkipNewlines;/*ignore '\n' in scrolling windows until the next printable char (set by attack)*/
/*04AD*/extern int GetsAborted;/*last CON_gets ended with ESC*/
/*04B3*/extern int StatusMember;/*party index shown in portrait/equipment view*/
/*04B5*/extern unsigned char IncantLen;/*number of syllables typed for Cast*/
/*04B6*/extern unsigned long IncantMask;/*typed incantation as a set of syllable letters (bit n = 'A'+n)*/
/*04BA*/extern int MousePress;
/*04BC*/extern int MouseTrigger;
/*04BE*/extern int MouseMode;
/*04C0*/extern int StatusDisplay;
/*04C2*/extern int PendingCommand;/*command being executed / waiting for a target (CMD_PASS when none)*/
/*04C4*/extern int AllowMouseMov;
/*04C6,04C8*/extern int MouseMapX,MouseMapY;
/*04CA*/extern unsigned char DirOriginX;/*screen x origin for MkDirection (88 = leader; moved for some spells)*/
/*04CB*/extern unsigned char DirOriginY;/*screen y origin for MkDirection*/
/*04D4*/extern int PromptCh;
/*04DF*/extern struct tClipInfo *CurClip;/*current text window*/
/*05CA*/extern int MapCoordMask;/*coordinate wrap mask, 0x3ff on surface, 0xff underground*/
/*05CC*/extern int ChunksFile;/*file handle of 'chunks'*/
/*05DA*/extern int BuiltAreaX;/*AreaX the AreaTiles were built for*/
/*05DC*/extern int BuiltAreaY;/*AreaY the AreaTiles were built for*/
/*05DE*/extern int BuiltAreaZ;/*MapZ the AreaTiles were built for*/
/*05E4,05E6*/extern int ViewX,ViewY;
/*05E8*/extern int XRayView;/*show all tiles fully lit, skip visibility (X-ray spell, wizard eye, view gem...)*/
/*0658*/extern int LocPartOffset;/*FindLoc/NextLoc: which part (0-3) of a multi-tile object covers the searched tile; TILE_FRAME(o)-LocPartOffset*/
/*065C*/extern int ObjBlkTmpPending[];/*[69] per outdoor area (64) / dungeon level (5): a .tmp objblk file exists*/
/*06E6*/extern int MoveAppliesTerrain;/*MoveObj applies terrain effects to moved NPCs; cleared during party relocation*/
/*07CC*/extern unsigned char PartyListTop;/*first party index shown in the party list (scroll)*/
/*07CE*/extern unsigned char InvenScroll;/*index of first shown inventory object (multiple of 4)*/
/*098B*/extern unsigned char IsInConversation;
/*0AF0*/extern unsigned char *SpellCode[];
/*0BF0*/extern unsigned char *MagicWords[];/*words of power An..Zu, indexed by incantation letter - 'A'*/
/*0C24*/extern unsigned char *SpellName[];
/*0D24*/extern char *Reagents_name[];
/*0D44*/extern unsigned char Reagents_needed[];
/*0DDC*/extern char *SpellMessages[];/*Success/Failed/No effect/Out of range/Blocked/nothing/Not possible/Done/prompts/...*/
/*17A1*/extern int AllowNPCTeleport;
/*17AE*/extern int AdvanceBlocked;/*leader's step was blocked (also used by wizard eye / mouse repeat)*/
/*17B0*/extern int VisibilityDirty;/*recompute visible area (VIS_UpdateView) on next refresh*/
/*17B2*/extern int PassThroughParty;/*CanMoveTo: party members may walk through each other*/
/*17B6*/extern int EnemyWarningGiven;/*Shamino's 'hears something' message already given*/
/*17CE*/extern unsigned char DragonPartFrames[];/*frames of the 4 dragon parts per facing*/
/*1968*/extern int ShowHitFlash;/*when 0, the next COMBAT_showHit skips the hit flash (cleared by area damage in gfx.c)*/
/*196A*/extern int MissileShooter;/*actor firing the current missile (-1 none); Missiles() then sets MissileHitObj*/
/*196C*/extern unsigned char RangeTable[][8];/*[|dx|][|dy|] combat distance table (approx max+min/2), dx,dy < 8*/
/*19AC*/extern unsigned char SpreadDirTable[][11];/*[dy+5][dx+5] two side directions (nibbles) for triple crossbow / wind spells*/
/*28A0*/extern int ForceHatching;

/*28BA*/extern unsigned char MUS_SongIndex;
/*28BB*/extern unsigned char MUS_Song;
/*28BC*/extern unsigned char MUS_Clock;
/*28BD*/extern unsigned char MUS_Bell;
/*28C0*/extern int MUS_MIndex;

/*2A48*/extern int ScreenFade;
/*2A4A*/extern int ColorRed;/*light red (12): HP<10, conversation keywords, peer damaging terrain*/
/*2A4C*/extern int ColorGreen;/*light green (10): poisoned HP, peer passable terrain*/
/*2A4E*/extern int ColorBlue;/*light blue (9): default command underline, peer deep water*/
/*2A50*/extern int ColorMagenta;/*light magenta (13): current color set before drawing the map view; role unclear*/
/*2A54*/extern int ColorPaper;/*background color of status/text areas (0x31 VGA, 0xF otherwise)*/
/*2A56*/extern int ColorText;/*default text color (0x48 VGA, 6 otherwise)*/
/*2A58*/extern int ColorBlack;/*black (0): clock hands*/
#include "gamestat.h"
/*2E9E*/extern int DiskFullChoice;/*'S'ave/'R'estore answer to disk full, 'G' after the save, ' ' none*/
/*2FFC*/extern int OSI_Started;/*set to 1 by main before loading the game, never read*/
/*2FFE*/extern int TextIdle;/*0 while console text output draws chars, 1 otherwise/while scrolling; not read anywhere in the code*/
/*3004*/extern void (*DriveErrorCallback)();/*called by OSI_selectDrive when selection fails (DISK_NullCallback)*/

/*====---- uninitialized ----====*/
/*module main.c*/
/*322E*/extern char far *Mem_BP;
/*3232*/extern long Mem_Remain;
/*3236*/extern char Names[][14];/*[savegame\objlist]*/
/*3316*/extern unsigned char DEXTE[];/*[savegame\objlist]*/
/*3416*/extern struct tMouse Mouse;
/*342F*/extern unsigned char far *Leader;/*[savegame\objlist]*/
/*3433*/extern unsigned char INTEL[];/*[savegame\objlist]*/
/*3533*/extern unsigned char Party[];/*[savegame\objlist]*/
/*3544*/extern unsigned char far *NPCMode;/*[savegame\objlist]*/
/*3548*/extern unsigned ObjShapeType[];
/*4D48*/extern void huge *GrMemory;/*tGr._20 after GR_INIT_MEMORY (0 = driver init failed)*/
/*4D4C*/extern unsigned char far *NPCFlag;/*[savegame\objlist]*/
/*4D50*/extern unsigned char far *TalkBuf;
/*4D54*/extern unsigned ScreenScanlines[];/*200 scanline offsets of the screen surface*/
/*4EE4*/extern int Amount[];
/*66E4*/extern unsigned char HitPoints[];/*[savegame\objlist]*/
/*67E4*/extern unsigned int AnimTile[MAX_ANIM];/*animdata: tile number of each animated tile*/
/*6824*/extern int far *BaseTile;
/*6828*/extern struct coord ObjPos[];

struct tSchedule {
	unsigned char time;/*_00*/
	unsigned char action;/*_01*/
	struct coord xyz;/*_02*/
};
/*8C28*/extern struct tSchedule far *Schedule;
extern struct {
	unsigned char content[3];
/*8C2C*/} far *VGAPalette;/*256 RGB entries from u6pal*/
/*8C30*/extern int far *OrigShapeType;/*[savegame\objlist]*/
/*8C34*/extern struct tSurface Screen;
extern struct {
	unsigned char content[8][32];
/*8C42*/} far *PathData;/*8 path slots x 32 bytes, (dir&3)+4*count, 0-terminated; saved in objlist*/
/*8C46*/extern unsigned char far *TileFlag;
/*8C4A*/extern unsigned char STREN[];/*[savegame\objlist]*/
/*8D4A*/extern char MovePts[];/*[savegame\objlist]*/
/*8E4A*/extern unsigned GrMemorySeg;/*normalized segment of GrMemory, never used*/
/*8E4C*/extern unsigned char far *Level;/*[savegame\objlist]*/
/*8E50*/extern unsigned char PartySize;/*[savegame\objlist]*/
/*8E51*/extern unsigned char AreaTiles[AREA_H][AREA_W];/*current tile map?*/
/*9491*/extern unsigned char far *SchedIndex;/*[savegame\objlist]*/
/*9495*/extern unsigned char AnimFrameMask[MAX_ANIM];/*animdata: counter mask*/
/*94B5*/extern unsigned char AreaFlags[AREA_H][AREA_W];
/*9AF5*/extern unsigned char MAGIC[];/*[savegame\objlist]*/
/*9BF5*/extern unsigned ExpPoints[];/*[savegame\objlist]*/
/*9DF5*/extern int AnimFirstFrame[MAX_ANIM];/*animdata: first frame tile*/
/*9E35*/extern unsigned char far *NPCComMode;/*[savegame\objlist]*/
/*9E39*/extern struct tScratchBuf far *ScratchBuf;/*type of ScratchBuf (laid out as an objblk buffer here, reused by pathfinding/graphics elsewhere)*/
/*9E3D*/extern struct tSurface MapSurface;/*tSurface of the 160x160 map view back buffer (mouse drawing)*/
/*9E4B*/extern unsigned MapScanlines[];/*scanline offsets of MapSurface*/
/*9FAB*/extern unsigned char NPCStatus[];/*[savegame\objlist]*/
/*A0AB*/extern unsigned char ObjStatus[];/*[savegame\objlist]*/
/*ACAB*/extern unsigned char AreaLight[AREA_H][AREA_W];
/*B2EB*/extern unsigned char TalkFlags[];/*[savegame\objlist]*/
/*B3EB*/extern unsigned char far *TerrainType;
/*B3EF*/extern unsigned char far *TileFlag2;/*second tile flag table (TILE_FLAG2_xx)*/
/*B3F3*/extern void far *FarHeap;/*the whole far heap allocated at startup*/
/*B3F7*/extern unsigned char AnimFrameShift[MAX_ANIM];/*animdata: counter shift*/
/*B417*/extern unsigned char far *TypeWeight;
/*B41B*/extern unsigned long FarHeapSize;/*farcoreleft() at startup*/
/*B41F*/extern int NumAnimData;
/*B421*/extern int SchedPointer[];
/*B623*/extern void far *PointerShapes;/*mouse pointer shapes (decompressed u6mcga.ptr/u6curs.*)*/
/*B628*/extern unsigned char HexInputBuf[];/*5-byte keyboard buffer for ReadHexNumber*/
/*B64E*/extern unsigned char PromptCnt;
/*B64F*/extern int MouseHeldCmd;/*command produced by the mouse press; only it repeats while the button is held*/
/*B651*/extern unsigned char LineBufChars[];/*line buffer characters (41)*/
/*B67A*/extern unsigned char LineBufColors[];/*line buffer colors (41)*/
/*B62D*/extern unsigned char StateAnimData[];
/*B6A3,B6A5*/extern int PointerX,PointerY;
/*B6A7*/extern unsigned char IncantScrX[4];/*text column of each typed syllable (for backspace)*/
/*B6AB*/extern unsigned char IncantScrY[4];/*text row of each typed syllable*/
extern struct {
	/*B6AF,B6B1,B6B3*/int x,y,obj;
} Selection;
/*B6B5*/extern struct tClipInfo TextWindows[4];/*text windows: [0] full screen, [1] status panel, [2] unused?, [3] message scroll*/
/*B6D9*/extern int AdvanceDir;
/*B6DB*/extern unsigned char IncantLetters[4];/*letters of the typed syllables*/
/*B6DF*/extern int SelectionIsDark;/*selected map tile is hidden/darkness (nothing to see)*/
/*B6E1*/extern int VarInt[10+26];
/*B729*/extern int LinesBeforeMore;/*lines still printable before the 'more' prompt*/
/*B72B*/extern int MouseButtons;/*mouse buttons of the current click (bit0 left, bit1 right)*/
/*B72D*/extern char *VarStr[10+26];
/*B775*/extern struct tSurface MouseArea;
/*module map.c*/
#define CHUNKS_CACHE_MAX 16

/*B784*/extern int ChunkCacheAge[CHUNKS_CACHE_MAX];/*LRU age of each chunk cache entry (0 = most recent)*/
/*B7A4*/extern unsigned char far *ChunkIndexBuf;/*0x600-byte buffer of packed 12-bit chunk indexes*/
/*B7A8*/extern int ChunkCacheIdx[CHUNKS_CACHE_MAX];/*chunk index stored in each cache entry (-1 = empty)*/
/*B7C8*/extern unsigned char ChunkCache[CHUNKS_CACHE_MAX][8][8];/*16 cached 8x8 chunks of ground tiles*/
/*BBC8,BBCA*/extern int AreaX,AreaY;
/*module visible.c*/
/*BBCC*/extern unsigned char VisQueueX[256];/*flood fill ring-buffer queue, x*/
/*BCCC*/extern unsigned char VisQueueY[256];/*flood fill ring-buffer queue, y*/
/*BDCC*/extern int AreaOffset;
/*module object.c*/
/*BDCE*/extern int SearchRect[4];/*SearchArea rectangle x1,y1,x2,y2*/
/*BDD6*/extern int FreeMonsterHead;/*head of the free temporary-monster slot list (0xE0-0xFF)*/
/*BDD8*/extern int SearchZ;/*SearchArea level*/
/*BDDA*/extern int Link[];
/*D5DA*/extern int FreeObjHead;/*head of the free object slot list*/
/*D5DC*/extern int CellObjTile[];/*tile of each entry of the 11x11 view object lists*/
/*D7DC*/extern char TileStringBuf[];/*[25] buffer for GetTileString/GetObjectString*/
/*D7F5*/extern int Tile_11x11[11][11];
/*D8E7*/extern int MapObjPtr[AREA_H][AREA_W];
/*E567*/extern unsigned char Obj_11x11[11][11];
/*E5E0*/extern unsigned char CellObjNext[];/*next entry in the 11x11 view object lists ([0] = free list)*/
/*E6E0*/extern int FreeObjCount;/*number of free object slots*/
/*E6E2*/extern int FreeMonsterCount;/*number of free temporary-monster slots*/
/*E6E4*/extern int Equipment[];
/*E6F4*/extern int WeightEquip;
/*E6F6*/extern unsigned char InvenSize;
/*E6F7*/extern int WeightInven;
/*E6F9*/extern unsigned char Party_2Handed[16];/*set when a two-handed weapon blocks the left hand*/
/*E709*/extern int InvenOwner;/*character or container whose content the equipment view shows*/
/*E70B*/extern int NPC_2Handed;/*same, for the NPC inventory view*/
/*E70D*/extern int ShowNPCInven;
/*E70F*/extern int InvenSlotObj[];/*object shown in each of the 12 inventory grid squares (0 = empty)*/
/*E728*/extern unsigned char TalkKeyword[];/*current keyword of a KEY list / allowed keys of OP_GET*/
/*E732*/extern unsigned char TalkInput[];/*player's last input ($Z)*/
/*E764*/extern char TalkNpcName[];/*name buffer filled by TALK_GetNpcName*/
/*E796*/extern int TalkNpcs[2];/*[0] npc being talked to (OP_NPC), [1] party member talking*/
/*E79C*/extern unsigned char TalkAnswered;/*a KEY block already matched the current input*/
/*E79D*/extern unsigned long TalkDescPC;/*script offset of the description (replayed by 'look')*/
/*E7A1*/extern int TalkBase;/*base subtracted from script addresses (always 0)*/
/*E7A7*/extern long TalkSavedPC;/*temporary saved Talk_PC*/
/*E7AB*/extern int Talk_PC;
/*E7AD*/extern char TalkStrBuf[10][100];/*storage of conversation string variables $0-$9*/
/*EB96*/extern int Spellbook[2];
/*EB9A*/extern int SpellList[8];
/*EBAA*/extern unsigned char SpellbookPageCount;/*number of spells on the displayed page*/
/*EBAB*/extern int SpellSavedStatus;/*StatusDisplay saved while the spellbook is shown*/
/*EBAD*/extern unsigned char SpellPageNav[];/*spellbook navigation [0]=prev circle,[1]=prev page kind,[2]=next page kind; [3] overflows into SpellNextCircle*/
/*EBB0*/extern unsigned char SpellNextCircle;/*= SpellPageNav[3]: next circle with spells (next circle button), 0xff none*/
/*EBB1*/extern unsigned char SpellbookFirst;/*index (among known spells) of the first spell on the page*/
/*EBB3*/extern unsigned char SelectedSpell;/*spell number selected in the spellbook*/
/*EBB2*/extern char SpellResult;
/*EBB4*/extern unsigned char SeanceFlag;
/*EBB5*/extern unsigned char PickpocketActive;/*set while choosing an object in another NPC's inventory (pickpocket), forces inventory redraw*/
/*EBB6*/extern unsigned char SpellbookPage[];/*spell numbers of the 8 lines of the page*/
/*EBBE*/extern unsigned char SpellPageCounts[];/*castable count of each page line, bit 0x80 = current spell*/
/*EBC6*/extern unsigned char PTH_Cur;
/*EBC7*/extern int PTH_Top;
/*EBC9*/extern unsigned char PTH_Index;
/*EBCA*/extern int PTH_AnX;
/*EBCC*/extern int PTH_Found;
/*EBCE*/extern int PTH_AnY;
/*EBD0*/extern unsigned char PTH_MaxCost;/*it holds the highest cost queued so far (upper bound for PTH_Cur)*/
/*EBD1*/extern int PTH_direct;
/*EBD3*/extern int PTH_object;
/*EBD5*/extern unsigned char FormationObjs[];/*leader + followers already placed (MoveFollowers)*/
/*EBE5*/extern int PartyGravityX;
/*EBE7*/extern int PartyGravityY;
/*EBE9*/extern int PartyFlankPos;/*party front fighter with max cross product (one flank)*/
/*EBEB*/extern int EnemiesGravityX;
/*EBED*/extern int PartyFrontMember;/*party front/command member furthest toward the enemies (max dot product)*/
/*EBEF*/extern int EnemiesGravityY;
/*EBF1*/extern int EnemyFlankPos;/*non-party front actor with max cross product*/
/*EBF3*/extern int LastActiveIdx;/*copy of Active when control changes; never read*/
/*EBF5*/extern int PartyFlankNeg;/*party front fighter with min cross product (other flank)*/
/*EBF7*/extern int EnemyFrontFirst;/*first non-party AI_FRONT actor found*/
/*EBF9*/extern int EnemyFlankNeg;/*non-party front actor with min cross product*/
/*EBFB*/extern int EnemiesNum;
/*EBFE*/extern int MissileEnd[2];/*x,y of the last tile reached by missile #0 (where thrown weapons/oil land)*/
/*EC02,EC08*/extern int TripleBoltX[3],TripleBoltY[3];/*3xcrossbow bolts aim x,y*/
/*EC0E*/extern int EquipWeapons[];/*[8] equipped weapons of the attacking party member*/
/*EC1E*/extern int MissileHitList[];/*-1 terminated list (max 0x3f) of NPCs crossed/hit by the last Missiles/Explosion/MagicWind*/
/*EC9E*/extern int MissileHitObj;/*object hit at the stop tile of missile #0 (via COMBAT_GetTargetAt)*/
/*module music.c*/
/*ECA0*/extern int MUS_MidiDatPos;/*parse position in MIDI.DAT (in TalkBuf)*/
/*ECA2*/extern int MUS_AmbientType[4];/*object types of the 4 nearest ambient sources*/
/*ECAA*/extern int MUS_AmbientCount;/*number of registered ambient sources*/
/*ECAC*/extern int MUS_AmbientDist[4];/*distances of the ambient sources (ascending)*/
/*ECB4*/extern union tHandler MUS_Driver;/*far pointer to the loaded music driver; offset = entry point*/
/*module gfx.c */
/*ECB8*/extern union tHandler GrDriver;/*entry point of the loaded graphic driver (ofs = function number)*/
/*ECBC*/extern int DirtyRect[4];/*map-buffer rectangle to copy to screen during missile/hail animations: [0]=left [1]=right [2]=top [3]=bottom; empty = 168,7,168,7*/
/*ECC4*/extern struct tGr far *GrInfo;/*struct tGr returned by GR_INIT: driver state/callbacks*/
/*module disk.c*/
/*ECC8,ECC9*/extern unsigned char DosVerMajor,DosVerMinor;/*dos version.major/minor*/
/*====---- ----====*/

#endif
