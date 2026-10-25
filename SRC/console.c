/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Console / user input module.

	- Text output: a small "console" driver writing 8x8 characters into
	  rectangular text windows (struct tClipInfo; CurClip is the current one,
	  TextWindows[] holds the game's windows: [0] full screen, [1] status panel,
	  [3] message scroll area). Characters are collected in a line buffer
	  (LineBufChars chars / LineBufColors colors) and flushed one line at a time, which
	  allows word wrap, centering, scrolling and "more" pagination
	  according to the window's _attr bits:
	    0x01 word wrap        0x02 center lines
	    0x04 '`' capitalizes  0x08 runic font (char | 0x80)
	    0x10 paginate         0x20 scroll at bottom
	  CON_putch interprets the in-text escapes used by conversations and
	  messages (#n $n / \ ^ @ * & < > `), CON_printf is a minimal printf.
	- Input: CON_prompt (animated cursor + one key), CON_ReadCommand (translate
	  keys and mouse clicks into command codes, cmd.h), CON_getch (also
	  handles status panel commands itself), CON_gets (line input).
	- Mouse: pointer shape, direction from pointer position, hit-testing of
	  the map view, the icon bar and the status panel views.
	- Keyboard "slot cursor" for the equipment/portrait views.
	- Game load/save/restore (savegame\objlist).
*/
#include "u6.h"

/*0492*/unsigned char SelectMode = 0;
/*0493*/char AimX = 0;
/*0494*/char AimY = 0;
/*0495*/char SelectRange = 0;
/*0496*/static unsigned char SpellbookSelectFirst = 0;/*spellbook page changed by keyboard: put the selection arrow on its first spell*/
/*0497*/unsigned char FreeAimMode = 0;/*free aiming (TAB pressed with no command pending)*/
/*0498*/static unsigned char CapitalizeNext = 0;/*'`' seen: capitalize next char (if clip _attr & 4)*/
/*0499*/char SlotCursorCol = 3;/*slot cursor column (equipment view: 0~2 equipment, 3~6 inventory; bottom row: button #)*/
/*049A*/char SlotCursorRow = 0;/*slot cursor row (-1 container slot, 0~2 slots, 3 bottom buttons row)*/
/*049B*/static unsigned char InGetString = 0;/*CON_gets() in progress*/
/*049C*/unsigned char AnimateAtPrompt = 1;/*run animations while waiting at the prompt*/
/*049D*/int LastNumberPlural = 1;/*last number printed was != 1 (plural, for '/' and '\\' escapes)*/
/*049F*/int EndPagingAtPrompt = 0;/*clear the paginate attribute (0x10) at next prompt*/
/*04A1*/int HelpOn = 1;/*help flag: highlight '@' keywords*/
/*04A3*/int InHiddenSection = 0;/*inside a '&'...'&' section: hidden unless SpeakGargish (SpeakGarglish)*/
/*04A5*/int TextOn = 1;
/*04A7*/int LastAttackTarget = -1;/*last attack target object (or -1)*/
/*04A9*/int SkipNewlines = 0;/*ignore '\n' in scrolling window until a printable char*/
/*04AB*/static int KbdKeepLastOnly = 1;/*keyboard: keep only the last key in the BIOS buffer*/
/*04AD*/int GetsAborted = 0;/*last CON_gets() was aborted with ESC*/
/*04AF*/int CookedInput = 1;/*0 = return raw keys (used by "more" prompt), 1 = full command processing*/
/*04B1*/int InputToUpper = 1;/*convert input keys to uppercase*/
/*04B3*/int StatusMember = 0;/*party member # shown in portrait/equipment view*/
/*04B5*/unsigned char IncantLen = 0;/*number of syllables in the current incantation*/
/*04B6*/unsigned long IncantMask = 0;/*incantation as a mask of syllable letters (bit n = 'A'+n)*/
/*04BA*/int MousePress = 0;
/*04BC*/int MouseTrigger = 0;
/*04BE*/int MouseMode = 0;
/*04C0*/int StatusDisplay = CMD_STAT_PARTY;
/*04C2*/int PendingCommand = CMD_PASS;/*command currently being executed (waiting for a target)*/
/*04C4*/int AllowMouseMov = 0;
/*04C6*/int MouseMapX = -1;
/*04C8*/int MouseMapY = -1;
/*screen pixel used as origin by MkDirection() (88,88: party leader in the map view)*/
/*04CA*/unsigned char DirOriginX = 88;
/*04CB*/unsigned char DirOriginY = 88;
/*C_0C9C_0286*/void CON_DrawCharCb(int ch, int x, int y);/*"put char" callback*/
/*C_0C9C_02F1*/void CON_ScrollUpCb(int x, int y, int w, int h);/*"scroll up" callback*/
/*04CC*/void (*CON_PutCharFn)(int, int, int) = CON_DrawCharCb;/*"put char" callback pointer*/
/*04D0*/void (*CON_ScrollUpFn)(int, int, int, int) = CON_ScrollUpCb;/*"scroll up" callback pointer*/
/*04D4*/int PromptCh = 5;
/*initial text window: full 40x25 screen, white on black*/
/*04D6*/struct tClipInfo BootClip = {
	0, 0, 40, 25,
	0, 0,
	0xf, 0,
	0
};
/*04DF,04D6*/struct tClipInfo *CurClip = &BootClip;/*current clip*/
/*04E1*/int DefaultClipAttr = 1;/*_attr given to new clips by CON_createClip (word wrap)*/
/*-- 3x5 font, used to print quantities over inventory tiles
  (5 rows of 3 bits, MSB first; the low bit is unused)*/
/*04E3*/unsigned TinyFont[] = {
	0xF6DE,/*'0'*/
	0x4924,/*'1'*/
	0xE7CE,/*'2'*/
	0xE59E,/*'3'*/
	0xB792,/*'4'*/
	0xF39E,/*'5'*/
	0xF3DE,/*'6'*/
	0xE4A4,/*'7'*/
	0xF7DE,/*'8'*/
	0xF79E,/*'9'*/
	0xF7DA,/*'A'*/
	0xD75C,/*'B'*/
	0xF24E,/*'C'*/
	0xD6DC,/*'D'*/
	0xF34E,/*'E'*/
	0xF348,/*'F'*/
	0xF2DE,/*'G'*/
	0xB7DA,/*'H'*/
	0xE92E,/*'I'*/
	0x24DE,/*'J'*/
	0xB75A,/*'K'*/
	0x924E,/*'L'*/
	0xBEDA,/*'M'*/
	0x03DA,/*'N'*/
	0x03DE,/*'O'*/
	0xF7C8 /*'P'*/
};
/*x position (in the 16x16 tile) of each digit, least significant first,
  for a number of 1~4 digits: right aligned*/
/*0517*/unsigned char TinyDigitX[][4] = {
	{ 7,0,0,0},/*1 digit*/
	{ 9,4,0,0},/*2 digit*/
	{11,7,3,0},/*3 digit*/
	{13,9,5,1} /*4 digit*/
};
/*map view tile last pointed at by the mouse (for continuous walking)*/
/*0527*/int LastMouseTileX = -1;
/*0529*/int LastMouseTileY = -1;
/*mouse hit zones {left,top,right,bottom}, see mkMouseCommand()*/
/*052B*/int MouseZones[][4] = {
	{  8,  8,167,167},/*map view?*/
	{  8,176,167,191},/*icons?*/
	{168,  8,311,103},/*party/portrait/equip view?*/
	{  0,  0,319,199} /*all screen*/
};
/*equipment view "slots" screen x [column] & y [row][column]
(columns 0~2: equipment, 3~6: inventory)*/
/*054B*/int SlotScrX[] = {176,200,224,248,264,280,296};
/*0559*/unsigned char SlotScrY[][7] = {
	{24,16,24,32,32,32,32},
	{40,40,40,48,48,48,48},
	{56,64,56,64,64,64,64}
};
/*tiles of the buttons row (x = 176 + 16 * i, y = 88) of the
  portrait/equipment views, used to erase the slot cursor (updated by DrawSlotCursor)*/
/*056E*/int SlotButtonTiles[] = {TIL_BUTTON_NEXT_LEFT,TIL_BUTTON_PARTY,TIL_BUTTON_PORTRAIT,TIL_BUTTON_NEXT_RIGHT,TIL_BUTTON_COMBAT_MODE};
/*0578*/int PendingSelect = 0;/*next CON_getch() returns CMD_TARGET at once (target already selected by right click)*/
/*057A*/unsigned char LineBufLen = 0;/*line buffer: length (= column of next char)*/
/*057B*/unsigned char LineBufDrawn = 0;/*line buffer: chars before this column are already on screen*/
/*057C*/unsigned char LineAcceptSpace = 1;/*0 = skip spaces (at the start of a word-wrapped line)*/
/*057D*/unsigned char PendingEscape = 0;/*pending CON_putch() escape char ('#','$','%','/','@','\\','^') or 0*/

/*MouseButtons: mouse buttons, bit0 left, bit1 right*/
#define LEFT_CLICK (MouseButtons & 1)
#define RIGHT_CLICK (MouseButtons & 2)

/*hide the mouse pointer if it is inside the screen rectangle
  (x0,y0)-(x1,y1), before drawing there.
  returns 1 if it was hidden: pass the result to CON_mouseOn() afterwards.
  (the result is uninitialized when there is no mouse; harmless since
  CON_mouseOn() tests MouseOn too)*/
/*C_0C9C_0007*/CON_mouseOffAt(int x0, int y0, int x1, int y1) {
	int inArea;

	if(MouseOn) {
		MouseArea.left = x0;
		MouseArea.top = y0;
		MouseArea.right = x1;
		MouseArea.bottom = y1;
		inArea = MOUSE_IN_AREA(&MouseArea);
		if(MOUSE_IN_AREA(&MouseArea))
			MOUSE_HIDE();
	}
	return inArea;
}

/*show the mouse pointer again if mustShow (result of CON_mouseOffAt)*/
/*C_0C9C_0050*/CON_mouseOn(int mustShow) {
	if(MouseOn && mustShow)
		MOUSE_SHOW();
}

/*direction (0=N,1=NE,2=E ... 7=NW, -1 = none) pointed at by the screen
  pixel (x,y), relative to the origin (DirOriginX,DirOriginY).
  Near the origin (48x48 box) the 16x16 cell decides (-1 for the center cell);
  farther, the slope decides: mostly horizontal/vertical (ratio >= 2) gives
  E/W/N/S, else a diagonal.*/
/*C_0C9C_0069*/MkDirection(int x, int y) {
	int dir;
	int deltaX, deltaY;

	dir = -1;
	deltaX = abs(x - DirOriginX);
	deltaY = abs(y - DirOriginY);
	if(
		DirOriginX - 24 <= x && DirOriginX + 24 > x &&
		DirOriginY - 24 <= y && DirOriginY + 24 > y
	) {
		if(DirOriginX - 8 > x) {
			if(DirOriginY - 8 > y)
				dir = 7;
			else if(DirOriginY + 8 > y)
				dir = 6;
			else
				dir = 5;
		} else if(DirOriginX + 8 > x) {
			if(DirOriginY - 8 > y)
				dir = 0;
			else if(DirOriginY + 8 > y)
				dir = -1;
			else
				dir = 4;
		} else {
			if(DirOriginY - 8 > y)
				dir = 1;
			else if(DirOriginY + 8 > y)
				dir = 2;
			else
				dir = 3;
		}
	} else {
		if(deltaX > deltaY) {
			if(deltaY == 0 || (deltaX / deltaY) >= 2) {
				dir = (DirOriginX <= x)?2:6;
			}
		} else {
			if(deltaX == 0 || (deltaY / deltaX) >= 2) {
				dir = (DirOriginY <= y)?4:0;
			}
		}
		if(dir == -1) {
			if(DirOriginX <= x) {
				dir = (DirOriginY <= y)?3:1;
			} else {
				dir = (DirOriginY <= y)?5:7;
			}
		}
	}
	return dir;
}

/*mouse callback (Mouse.f_14), also called after mode changes:
update mouse pointer shape according to position.
shapes: 0 arrow (outside the map view), 1 target/crosshair (MouseMode != 0,
or center of the map), 2~9 direction arrows (2 + direction)*/
/*C_0C9C_01FB*/CON_UpdateMousePointer() {
	int dir;

	if(!MouseOn)
		return;
	if(Mouse.posX >= 8 && Mouse.posX < 168 && Mouse.posY >= 8 && Mouse.posY < 168) {
		if(MouseMode == 0) {
			dir = MkDirection(Mouse.posX, Mouse.posY);
			if(dir >= 0) {
				MOUSE_POINTER(PointerShapes, dir + 2);
				return;
			}
		}
	} else {
		if(MouseMode == 0) {
			MOUSE_POINTER(PointerShapes, 0);
			return;
		}
	}
	MOUSE_POINTER(PointerShapes, 1);
}

/*"put char" callback (CON_PutCharFn):
draw character ch at text cell (x,y) with the current clip's colors*/
/*C_0C9C_0286*/void CON_DrawCharCb(int ch, int x, int y) {
	int mouseHidden;

	x <<= 3;
	y <<= 3;
	mouseHidden = CON_mouseOffAt(x, y, x + 7, y + 7);
	GrInfo->textBg = CurClip->bgColor;
	GrInfo->textFg = CurClip->fgColor;
	GR_PUT_CHAR(ch, x, y);
	CON_mouseOn(mouseHidden);
}

/*"scroll up" callback (CON_ScrollUpFn):
scroll the text window at cell (x,y) of w x h cells up by one line (8 pixels)
and clear its last line with the background color*/
/*C_0C9C_02F1*/void CON_ScrollUpCb(int x, int y, int w, int h) {
	int mouseHidden;

	x <<= 3;
	y <<= 3;
	w <<= 3;
	h <<= 3;
	mouseHidden = CON_mouseOffAt(x, y, x + w - 1, y + h - 1);
	GR_SCROLL_RECT_Y(x, y, x + w - 1, y + h - 1, -8);
	GR_SET_COLOR(CurClip->bgColor);
	GR_FILL_RECT(x, y + h - 8, x + w - 1, y + h - 1);
	CON_mouseOn(mouseHidden);
}

/*Restore game: throw away the current state and reload the saved game
(called from the main loop when DiskFullChoice == 'R')*/
/*C_0C9C_0397*/GAME_Restore() {
	int i;

	IsArmageddon = 0;
	MapAnimEnabled = 1;
	SelectMode = 0;
	StatusDisplay = CMD_STAT_PARTY;
	StatusMember = 0;
	StatusDirty = 1;
	DiskFullChoice = ' ';
	OBJ_DeleteTmpBlocks();/*delete "savegame\\objblk??.tmp" files?*/
	for(i = 0; i < 0x45; i++)
		ObjBlkTmpPending[i] = 0;
	OBJ_InitLists();
	MAP_InitChunkCache();
	setDefaultCommand(0xff);
	StatusDirty = 1;
	OSI_close(ChunksFile);
	GAME_Load();
	InitNPCTracker();
	MUS_PlayAreaSong();
	if(InCombat)
		COMBAT_begin();
	else
		COMBAT_breakOff();
	PassTime(0);
	PromptFlag = 1;
}

/*Load game: read "schedule", "basetile" and the saved state in
"savegame\\objlist" (same layout as written by GAME_Save), then rebuild
the map around the party and open the "chunks" file*/
/*C_0C9C_042A*/GAME_Load() {
	int fd, i;

	BuiltAreaX = BuiltAreaY = BuiltAreaZ = -1;

	DISK_confirm(DISK_GAME);
	fd = OSI_open(/*057E*/"schedule");
	OSI_read(fd, 0, (0x100 + 1) * sizeof(int), SchedPointer);
	OSI_read(fd, -1, (long)SchedPointer[0x100] * sizeof(struct tSchedule), Schedule);
	OSI_close(fd);

	DISK_confirm(DISK_GAME);
	fd = OSI_open(/*0587*/"basetile");
	OSI_Read(fd, 0L, 0x800L, BaseTile);
	OSI_close(fd);

	DISK_confirm(DISK_PLAYER);
	fd = OSI_open(/*0590*/"savegame\\objlist");
	OSI_read(fd, 0, 0x100, ObjStatus);
	OSI_read(fd, -1, 0x100 * sizeof(struct coord), ObjPos);
	OSI_read(fd, -1, 0x200, ObjShapeType);
	OSI_read(fd, -1, 0x200, Amount);
	OSI_read(fd, -1, 0x100, NPCStatus);
	OSI_read(fd, -1, 0x100, STREN);
	OSI_read(fd, -1, 0x100, DEXTE);
	OSI_read(fd, -1, 0x100, INTEL);
	OSI_read(fd, -1, 0x200, ExpPoints);
	OSI_read(fd, -1, 0x100, HitPoints);
	OSI_read(fd, -1, 16 * 14, Names);
	OSI_read(fd, -1, 0x10, Party);
	OSI_read(fd, -1, 1, &PartySize);
	OSI_read(fd, -1, 0x100, Level);
	OSI_read(fd, -1, 0x100, SchedIndex);
	OSI_read(fd, -1, 0x100, NPCMode);
	OSI_read(fd, -1, 0x100, NPCComMode);
	OSI_read(fd, -1, 0x100, MAGIC);
	OSI_read(fd, -1, 0x100, MovePts);
	OSI_read(fd, -1, 0x100 * sizeof(int), OrigShapeType);
	OSI_read(fd, -1, 0x100, TalkFlags);
	OSI_read(fd, -1, 0x100, Leader);
	OSI_read(fd, -1, 0x100, NPCFlag);
	OSI_read(fd, -1, 0x100, PathData);
	OSI_read(fd, -1, (long)((char *)&GameStateEnd - (char *)&GameStateStart), &GameStateStart);
	OSI_close(fd);

	OBJ_RelinkNPCs();
	VarInt['Q' - 0x37] = IsOnQuest;
	MapX = GetX(Party[Active]);
	MapY = GetY(Party[Active]);
	MapZ = GetZ(Party[Active]);

	/*chains, cranks and bells only animate when triggered*/
	for(i = 0; i < NumAnimData; i ++) {
		if(
			AnimTile[i] == BaseTile[OBJ_CHAIN] ||
			AnimTile[i] == BaseTile[OBJ_CRANK] ||
			AnimTile[i] == BaseTile[OBJ_BELL]
		)
			StateAnimData[i] = 0;
		else
			StateAnimData[i] = 1;
	}

	setDefaultCommand(DefaultCommand);
	LoadNewRegions(MapX, MapY, MapZ);
	MAP_LoadChunkIndexes(MapX, MapY, MapZ);

	DISK_confirm(DISK_MAP);
	ChunksFile = OSI_open(/*05A1*/"chunks");
	MoveAppliesTerrain = 1;
	MAP_LoadAround();
	/*party members aboard a vehicle are kept off the map*/
	if(IN_VEHICLE) {
		for(i = 0; i < PartySize; i++)
			InsertObj(Party[i], 0, INVEN);
	}
	VIS_UpdateView();
}

/*Save game: write the state to "savegame\\objlist" (party members aboard
a vehicle are put back on the map for the time of the save)*/
/*C_0C9C_089F*/GAME_Save() {
	int fd, i;

	DiskFullChoice = ' ';
	BuiltAreaX = BuiltAreaY = BuiltAreaZ = -1;
	if(IN_VEHICLE) {
		for(i = 0; i < PartySize; i++)
			MoveObj(Party[i], MapX, MapY, MapZ);
	}
	DISK_confirm(DISK_PLAYER);
	OBJ_CommitBlocks();
	IsOnQuest = VarInt['Q' - 0x37];

	DISK_confirm(DISK_PLAYER);
	fd = OSI_create(/*0590*/"savegame\\objlist");
	OSI_write(fd, 0, 0x100, ObjStatus);
	OSI_write(fd, -1, 0x100 * sizeof(struct coord), ObjPos);
	OSI_write(fd, -1, 0x200, ObjShapeType);
	OSI_write(fd, -1, 0x200, Amount);
	OSI_write(fd, -1, 0x100, NPCStatus);
	OSI_write(fd, -1, 0x100, STREN);
	OSI_write(fd, -1, 0x100, DEXTE);
	OSI_write(fd, -1, 0x100, INTEL);
	OSI_write(fd, -1, 0x200, ExpPoints);
	OSI_write(fd, -1, 0x100, HitPoints);
	OSI_write(fd, -1, 16 * 14, Names);
	OSI_write(fd, -1, 0x10, Party);
	OSI_write(fd, -1, 1, &PartySize);
	OSI_write(fd, -1, 0x100, Level);
	OSI_write(fd, -1, 0x100, SchedIndex);
	OSI_write(fd, -1, 0x100, NPCMode);
	OSI_write(fd, -1, 0x100, NPCComMode);
	OSI_write(fd, -1, 0x100, MAGIC);
	OSI_write(fd, -1, 0x100, MovePts);
	OSI_write(fd, -1, 0x100 * sizeof(int), OrigShapeType);
	OSI_write(fd, -1, 0x100, TalkFlags);
	OSI_write(fd, -1, 0x100, Leader);
	OSI_write(fd, -1, 0x100, NPCFlag);
	OSI_write(fd, -1, 0x100, PathData);
	OSI_write(fd, -1, (long)((char *)&GameStateEnd - (char *)&GameStateStart), &GameStateStart);
	OSI_close(fd);

	MAP_LoadAround();
	if(IN_VEHICLE) {
		for(i = 0; i < PartySize; i++)
			InsertObj(Party[i], 0, INVEN);
	}
	StatusDirty ++;
}

/*draw tile ftile over background tile btile at screen (x,y)
(composed in the backbuffer's top-left 16x16 corner, then copied)*/
/*C_0C9C_0BCE*/DrawTileOnBg(int ftile, int btile, int x, int y) {
	GR_PUT_TILE(btile, 0, 0);
	GR_PUT_TILE(ftile, 0, 0);
	/*backbuffer {0,0} 15x15 -> screen {x,y}*/
	GR_COPY_TO_SCREEN(0, 0, 15, 15, x, y);
}

/*draw a 3x5 character in the backbuffer's 16x16 work tile, at
column xOfs, rows 11~15 (bottom of the tile), in text color.
glyph: 0~9 = '0'-'9', 10~25 = 'A'-'P'*/
/*C_0C9C_0C10*/DrawTinyGlyph(int glyph, int xOfs) {
	int px, py;
	unsigned bits;

	GrInfo->toBackBuffer = 1;
	bits = TinyFont[glyph];
	GR_SET_COLOR(ColorText);
	for(py = 11; py < 16; py++) {
		for(px = xOfs; px < xOfs + 3; px ++) {
			if(bits & 0x8000) {
				GR_PUT_PIXEL(px, py);
			}
			bits <<= 1;
		}
	}
	GrInfo->toBackBuffer = 0;
}

/*draw tile ftile on an empty slot at screen (x,y) with the number val
(0~9999, right aligned, 3x5 font) printed over its bottom*/
/*C_0C9C_0C7D*/DrawTileWithNumber(int ftile, int val, int x, int y) {
	int numDigits, digit;

	GR_PUT_TILE(TIL_SLOT_EMPTY, 0, 0);
	GR_PUT_TILE(ftile, 0, 0);
	if(val < 10)
		numDigits = 1;
	else if(val < 100)
		numDigits = 2;
	else if(val < 1000)
		numDigits = 3;
	else if(val < 10000)
		numDigits = 4;
	else
		numDigits = 0;
	for(digit = 0; digit < numDigits; digit ++) {
		DrawTinyGlyph(val % 10, TinyDigitX[numDigits - 1][digit]);
		val /= 10;
	}
	GR_COPY_TO_SCREEN(0, 0, 15, 15, x, y);
}

/*draw an inventory object in its slot at screen (x,y):
keys show their quality as a letter ('A' = key #1), gold (QuanType 4)
its amount, stackable objects (QuanType 2) their quantity*/
/*C_0C9C_0D31*/DrawInvObject(int objNum, int x, int y) {
	int quanType, ftile;

	quanType = QuanType(ObjShapeType[objNum]);
	ftile = TILE_FRAME(objNum);
	if(GetType(objNum) == OBJ_KEY) {
		GR_PUT_TILE(TIL_SLOT_EMPTY, 0, 0);
		GR_PUT_TILE(ftile, 0, 0);
		DrawTinyGlyph(GetQual(objNum) + 9, 7);
		GR_COPY_TO_SCREEN(0, 0, 15, 15, x, y);
	} else if(quanType == 4) {
		GR_PUT_TILE_SCREEN(TIL_SLOT_EMPTY, x, y);
		DrawTileWithNumber(ftile, Amount[objNum], x, y);
	} else if(quanType == 2) {
		GR_PUT_TILE_SCREEN(TIL_SLOT_EMPTY, x, y);
		DrawTileWithNumber(ftile, GetQuan(objNum), x, y);
	} else {
		DrawTileOnBg(ftile, TIL_SLOT_EMPTY, x, y);
	}
}

/*same as DrawTileOnBg (entry point used by the status views)*/
/*C_0C9C_0E57*/DrawSlotTile(int ftile, int btile, int x, int y) {
	DrawTileOnBg(ftile, btile, x, y);
}

/*poll for one key at the text cursor of the current clip.
while no key is available, draw one frame of the animated cursor
(PromptCh + 0~3) and run the idle animations once.
returns the key code (see KBD_GetKey: ASCII, or 0x100 + scan code for
extended keys/keypad) or 0 if none*/
/*C_0C9C_0E6E*/CON_prompt() {
	int promptX, promptY;
	int ch, savedFg;

	/*backup/set new fg*/
	savedFg = CurClip->fgColor;
	CurClip->fgColor = ColorText;

	/*the player is reading: reset the "more" line counter*/
	LinesBeforeMore = CurClip->rows;
	if(EndPagingAtPrompt) {
		CurClip->attrFlags &= ~0x10;/*end of paginated text*/
		EndPagingAtPrompt = 0;
	}
	TextIdle = 0;
	if(CurClip->curX >= CurClip->cols)
		CON_putch0(0);/*line full: new line*/
	promptX = CurClip->curX + CurClip->originX;
	promptY = CurClip->curY + CurClip->originY;
	ch = KBD_GetKey(KbdKeepLastOnly);/*keyboard function*/
	if(ch == 0) {/*animated cursor*/
		(*CON_PutCharFn)(PromptCh + ((PromptCnt ++) & 3), promptX, promptY);
		IdleAnimTick = 1;
		if(AnimateAtPrompt) {
			DisplayTick();
			PaletteAnimation();
		} else {
			OSI_delay(1);
		}
		ServeMouse
	}
	(*CON_PutCharFn)(' ', promptX, promptY);
	TextIdle = 1;

	/*restore fg*/
	CurClip->fgColor = savedFg;

	return ch;
}

/*echo one typed character for CON_gets(), directly at the cursor
(no line buffer); '\b' erases the previous char, also across lines*/
/*C_0C9C_0F52*/CON_EchoChar(int ch) {
	int scrX, scrY;

	scrX = CurClip->curX + CurClip->originX;
	scrY = CurClip->curY + CurClip->originY;
	TextIdle = 0;
	if(ch == '\b') {/*backspace*/
		if(CurClip->curX) {
			CurClip->curX --;
			scrX --;
		} else if(CurClip->curY) {
			CurClip->curY --;
			scrY --;
			CurClip->curX = CurClip->cols - 1;
			scrX = CurClip->curX + CurClip->originX;
		}
		(*CON_PutCharFn)(' ', scrX, scrY);
	} else {
		(*CON_PutCharFn)(ch, scrX, scrY);
		if(++CurClip->curX >= CurClip->cols) {
			CurClip->curX = 0;
			if(CurClip->curY < CurClip->rows - 1) {
				CurClip->curY ++;
			} else if(CurClip->attrFlags & 0x20) {
				TextIdle = 1;
				(*CON_ScrollUpFn)(CurClip->originX, CurClip->originY, CurClip->cols, CurClip->rows);
				TextIdle = 0;
			}
		}
	}
	TextIdle = 1;
}

/*uncalled?
draw string str directly at text cell (x,y), no clipping*/
/*__0C9C_102A*/CON_PutStrAt(unsigned char *str, int x, int y) {
	while(*str) {
		(*CON_PutCharFn)(*str++, x++, y);
	}
}

/*mouse held over the map view without command: return the walking
direction (MkDirection, -1 = center) and, if AllowMouseMov, update the
walk target MouseMapX/Y (map coordinates of the pointed tile; used by
the walking code to slide around obstacles). The target is kept while the
pointer stays on the same tile, unless the party is next to it on the same
row/column or the button was just pressed.*/
static /*C_0C9C_1054*/mkMouseDir() {
	int tileX, tileY;
	int dir;

	tileX = SCR2TIL(PointerX);
	tileY = SCR2TIL(PointerY);
	dir = MkDirection(PointerX, PointerY);

	if(AllowMouseMov) {
		if(
			MouseTrigger || MouseMapX == -1 ||
			(MouseMapX == MapX && abs(MouseMapY - MapY) < 2) ||
			(MouseMapY == MapY && abs(MouseMapX - MapX) < 2) ||
			tileX != LastMouseTileX || tileY != LastMouseTileY
		) {
			MouseMapX = tileX + MapX - 5;
			MouseMapY = tileY + MapY - 5;
			LastMouseTileX = tileX;
			LastMouseTileY = tileY;
		}
	} else {
		MouseMapX =
		MouseMapY = -1;
	}

	return dir;
}

/*fill Selection (x, y, obj) from the pointer position PointerX/Y.
- map view: Selection.x/y = map coordinates; returns CMD_HIT_MAP_OBJ if there is
  an object (the first visible one, COMBAT_GetTargetAt), else Selection.obj = -1
  and SelectionIsDark tells if the tile is hidden/darkness.
- equipment view: returns CMD_HIT_INVEN_SLOT for an inventory slot or the container
  slot (STAT_PickInvenSlot), CMD_HIT_EQUIP_SLOT for an equipment slot (STAT_PickEquipSlot).
returns 0 when nothing selectable is pointed at*/
static /*C_0C9C_1107*/mkMouseSelection() {
	int tile;

	SelectionIsDark = 0;
	if(PointerX < 168) {
		Selection.x = SCR2TIL(PointerX) + MapX - 5;
		Selection.y = SCR2TIL(PointerY) + MapY - 5;
		if(Obj_11x11[SCR2TIL(PointerY)][SCR2TIL(PointerX)] || Tile_11x11[SCR2TIL(PointerY)][SCR2TIL(PointerX)] >= 0x200) {
			Selection.obj = COMBAT_GetTargetAt(Party[Active], Selection.x, Selection.y);
			return CMD_HIT_MAP_OBJ;
		}
		Selection.obj = -1;
		tile = Tile_11x11[SCR2TIL(PointerY)][SCR2TIL(PointerX)];
		SelectionIsDark = (tile == TIL_HIDDEN || tile == TIL_DARKNESS);
	} else if(StatusDisplay == CMD_STAT_EQUIP) {
		Selection.x = -1;
		Selection.y = -1;
		if(STAT_PickInvenSlot())
			return CMD_HIT_INVEN_SLOT;
		if(STAT_PickEquipSlot())
			return CMD_HIT_EQUIP_SLOT;
	} else {
		Selection.x = -1;
		Selection.y = -1;
		Selection.obj = -1;
	}
	return 0;
}

/*Status:Party
mouse click in the party view: returns the command to execute (0 = none).
Each member line is 16 pixels high, from y = 24; PartyListTop = first line shown.
Clicking the portrait column (x > 192) opens his portrait, the
small icon column (176 < x <= 192) his equipment; the arrows scroll the list.
Selection.obj = party index*/
static /*C_0C9C_121F*/MouseInPartyView() {
	int cmd;

	cmd = 0;
	if(PointerY > 24) {
		if(PointerX > 192) {/*to portrait view*/
			cmd = CMD_STAT_PORTRAIT;
			Selection.obj = SCR2TIL(PointerY - 28) + PartyListTop;
			if(PartySize <= Selection.obj)
				cmd = 0;
		} else if(PointerX > 176) {/*to inventory view*/
			cmd = CMD_STAT_EQUIP;
			Selection.obj = SCR2TIL(PointerY - 24) + PartyListTop;
			if(PartySize <= Selection.obj)
				cmd = 0;
		} else if(PointerY >= 24 && PointerY < 40) {/*party up*/
			if(PartyListTop != 0)
				cmd = CMD_PARTY_SCROLL_UP;
		} else if(PointerY >= 88) {/*party down*/
			if(PartyListTop < PartySize - 5)
				cmd = CMD_PARTY_SCROLL_DOWN;
		}
	}

	return cmd;
}

/*Status:Portrait
mouse click in the portrait view: only the buttons row (y >= 88) reacts.
returns the command (0 = none)*/
static /*C_0C9C_12B7*/MouseInPortraitView() {
	int cmd;

	cmd = 0;
	if(PointerY >= 88) {
		if(PointerX >= 192 && PointerX < 208) {/*to party view*/
			cmd = CMD_STAT_PARTY;
		} else if(PointerX >= 208 && PointerX < 224) {/*to inventory view*/
			cmd = CMD_STAT_EQUIP;
			Selection.obj = -1;
		} else if(PointerX >= 224 && PointerX < 240) {/*next char*/
			cmd = CMD_NEXT_MEMBER;
		} else if(PointerX >= 176 && PointerX < 192) {/*previous char*/
			cmd = CMD_PREV_MEMBER;
		}
	}
	return cmd;
}

/*Status:Equipment
mouse click (or slot cursor validation) in the equipment/inventory view.
returns the command (0 = none). Some actions are done here directly:
the combat mode button (240,88; not for the Avatar) cycles the member's
combat mode; clicking a locked chest prints "It's locked."; a trapped
chest springs its trap before being opened*/
static /*C_0C9C_131D*/MouseInEquipView() {
	int cmd, objType;
	int selKind;

	cmd = 0;
	if(IsInConversation)
		return cmd;

	/*combat mode button: cycle AI_COMMAND ... AI_ASSAULT*/
	if(
		StatusMember != 0 &&
		PointerX >= 240 && PointerX < 256 && 
		PointerY >= 88 && PointerY < 104
	) {
		NPCComMode[Party[StatusMember]] ++;
		if(NPCComMode[Party[StatusMember]] == AI_ASSAULT + 1)
			NPCComMode[Party[StatusMember]] = AI_COMMAND;
		if(
			InCombat && 
			Active != StatusMember && 
			!IsCharmed(Party[StatusMember])
		) NPCMode[Party[StatusMember]] = NPCComMode[Party[StatusMember]];
		STAT_refreshComMode();
	} else if(PointerY >= 88) {
		if(PointerX >= 192 && PointerX < 208) {/*to party view*/
			cmd = CMD_STAT_PARTY;
		} else if(PointerX >= 208 && PointerX < 224) {/*to portrait view*/
			cmd = CMD_STAT_PORTRAIT;
			Selection.obj = -1;
		} else if(PointerX >= 224 && PointerX < 240) {/*next char*/
			cmd = CMD_NEXT_MEMBER;
		} else if(PointerX >= 176 && PointerX < 192) {/*previous char*/
			cmd = CMD_PREV_MEMBER;
		}
	} else if(PointerX >= 240 && PointerX < 248) {
		if(PointerY >= 32 && PointerY < 40) {/*inventory up*/
			if(InvenScroll != 0)
				cmd = CMD_INVEN_SCROLL_UP;
		} else if(PointerY >= 72 && PointerY < 80) {/*inventory down*/
			if(InvenSize - InvenScroll > 12)
				cmd = CMD_INVEN_SCROLL_DOWN;
		}
	} else if(
		PointerX >= 272 && PointerX < 288 &&
		PointerY >= 16 && PointerY < 32 &&
		MouseMode == 0
	) {/*container slot (top right): back to the parent container/owner*/
		cmd = CMD_CONTAINER_UP;
	} else if(MouseMode == 0 && PendingCommand != CMD_MOVE) {
		selKind = mkMouseSelection();
		if(Selection.obj != -1) {
			if(selKind == CMD_HIT_INVEN_SLOT) {
				/*inventory object: ready it, or open it if it is a container*/
				objType = GetType(Selection.obj);
				if(!IsContainerType(objType) || objType == OBJ_SPELLBOOK) {
					cmd = CMD_READY;
				} else if(objType == OBJ_OPEN_CHEST && (GetFrame(Selection.obj) & 2)) {/*frame bit 1: locked*/
					CON_printf(/*05A8*/"It's locked.\n");
					CON_printf(/*05B6*/"\n%s:\n%>", Names[Active]);
					CON_putch0(-1);
				} else if(
					objType == OBJ_OPEN_CHEST && GetFrame(Selection.obj) == 1 &&
					FindInvType(Selection.obj, OBJ_EFFECT, SPELL_TRAP) != -1
				) {
					SpringTrap(Selection.obj);
					CON_printf(/*05B6*/"\n%s:\n%>", Names[Active]);
					cmd = CMD_CONTAINER_OPEN;
				} else {
					cmd = CMD_CONTAINER_OPEN;
				}
			} else if(selKind == CMD_HIT_EQUIP_SLOT) {
				/*equipped object: unready it*/
				cmd = CMD_UNREADY;
			}
		}
	}

	return cmd;
}

/*Status:Spellbook
mouse click in the spellbook view. returns the command (0 = none).
The bottom row has 4 navigation buttons, 16 pixels each from x = 176
(see SPELL_RefreshPage): previous circle, previous page, next page,
next circle -> CMD_SPELLBOOK_PREV_CIRCLE + 0~3. SpellPageNav[] (SpellPageNav[3] is SpellNextCircle)
is 0xff for a button not available.
Clicking a spell line returns CMD_TARGET with Selection.obj = line # (0~7)*/
static /*C_0C9C_15EB*/MouseInSpellbook() {
	int cmd, button;

	cmd = 0;
	if(
		PointerY >= 88 && PointerY < 104 &&
		PointerX >= 176 && PointerX < 240
	) {
		/*navigation button*/
		if(SpellPageNav[button = SCR2TIL(PointerX - 176)] != 0xff)
			cmd = CMD_SPELLBOOK_PREV_CIRCLE + button;
		Selection.x = Selection.y = -1;
	} else if(PointerX > 184 && PointerY >= 16 && PointerY < 80) {
		/*spell line (8 pixels high, from y = 16)*/
		cmd = CMD_TARGET;
		Selection.obj = (PointerY - 16) >> 3;
		if(SpellbookPageCount <= Selection.obj)
			cmd = 0;
		Selection.x = Selection.y = -1;
	}
	return cmd;
}

/*set the default command (one of CMD_ATTACK~CMD_COMBAT, or 0xff = none),
executed by a right click; it is shown by a line under its icon
(icons are 16 pixels wide from x = 8, line at y = 192)*/
static /*C_0C9C_166D*/setDefaultCommand(int cmd) {
	int x;

	HideMouse
	if(DefaultCommand != 0xff) {
		GR_SET_COLOR(ColorPaper);
		x = TIL2SCR(DefaultCommand - CMD_ATTACK) + 8;
		GR_FILL_RECT(x, 192, x + 15, 192);
	}
	DefaultCommand = cmd;
	if(DefaultCommand != 0xff) {
		GR_SET_COLOR(ColorBlue);
		x = TIL2SCR(DefaultCommand - CMD_ATTACK) + 8;
		GR_FILL_RECT(x, 192, x + 15, 192);
	}
	ShowMouse
}

/*translate the current mouse button state (MouseButtons, MouseTrigger = just
pressed) at PointerX/Y into a command code (0 = none), by zone (MouseZones):
- map view: walk (CMD_DIRECTION + AdvanceDir; repeated while held unless the last
  step was blocked, AdvanceBlocked), pass (center), target (CMD_TARGET when a command
  waits for one, MouseMode != 0), right click = default command (CMD_DEFAULT_ACTION)
- icon bar: left click = command icon, right click = set default command
- status panel: depends on StatusDisplay
- elsewhere: pass*/
static /*C_0C9C_1714*/mkMouseCommand() {
	int mouseCmd, zone;
	int mouseDir;

	mouseCmd = 0;
	
	/*(zone 3 is the whole screen: always found)*/
	for(zone = 0; ; zone ++) {
		if(
			MouseZones[zone][0] <= PointerX && MouseZones[zone][2] >= PointerX &&
			MouseZones[zone][1] <= PointerY && MouseZones[zone][3] >= PointerY
		) break;
	}
	switch(zone) {
		case 0:/*map view*/
			if(MouseMode == 0) {
				if(DefaultCommand != 0xff && RIGHT_CLICK) {
					if(MouseTrigger) {
						mouseCmd = CMD_DEFAULT_ACTION;
						mkMouseSelection();
					}
				} else {
					mouseDir = mkMouseDir();
					if(mouseDir == -1) {
						if(MouseTrigger)
							mouseCmd = CMD_PASS;
						else
							mouseCmd = 0;
					} else if(MouseTrigger || AdvanceBlocked == 0 || mouseDir != AdvanceDir) {
						mouseCmd = CMD_DIRECTION;
						AdvanceDir = mouseDir;
					}
				}
			} else if(MouseTrigger) {
				mouseCmd = CMD_TARGET;
				mkMouseSelection();
			}
		break;
		case 1:/*icons: 10 icons of 16 pixels from x = 8, CMD_ATTACK~CMD_COMBAT*/
			if(MouseTrigger) {
				if(LEFT_CLICK)
					mouseCmd = CMD_ATTACK + SCR2TIL(PointerX - 8);
				else
					setDefaultCommand(CMD_ATTACK + SCR2TIL(PointerX - 8));
			}
		break;
		case 2:/*party/portait/equip view*/
			if(MouseTrigger) {
				if(DefaultCommand != 0xff && MouseMode == 0 && RIGHT_CLICK) {
					mouseCmd = CMD_DEFAULT_ACTION;
					mkMouseSelection();
				} else {
					switch(StatusDisplay) {
						case CMD_STAT_PARTY: mouseCmd = MouseInPartyView(); break;
						case CMD_STAT_PORTRAIT: mouseCmd = MouseInPortraitView(); break;
						case CMD_STAT_EQUIP:
							mouseCmd = MouseInEquipView();
							if(mouseCmd == 0 && MouseMode == 1 && mkMouseSelection())
								mouseCmd = CMD_TARGET;
						break;
						case CMD_STAT_SPELLBOOK: mouseCmd = MouseInSpellbook(); break;
					}
				}
			}
		break;
		case 3:/*all screen*/
			if(MouseTrigger) {
				mouseCmd = CMD_PASS;
				Selection.x =
				Selection.y = -1;
			}
		break;
	}

	return mouseCmd;
}

/*find the spell whose incantation matches the typed syllables (IncantMask,
compared as letter sets, so the order does not matter) among the spells
in the spellbook (SpellList). prints NotInBookMsg if none.
returns the spell number or -1*/
static /*C_0C9C_18D8*/IncantToSpell() {
	int i;
	unsigned char *syllable;
	int spellNum;
	long spellMask;

	spellNum = -1;
	for(i = 0; i < 8 * 0x10; i ++) {
		if(SpellList[i >> 4] & (1 << (i & 0xf))) {
			spellMask = 0;
			for(syllable = SpellCode[i]; *syllable; syllable ++)
				spellMask |= 1L << (*syllable - 'A');
			if(spellMask == IncantMask) {
				spellNum = i;
				break;
			}
		}
	}
	if(spellNum < 0)
		CON_printf(NotInBookMsg);
	CON_printf(/*05A8+0Ch*/"\n");
	IncantMask = 0;
	SelectMode = 1;

	return spellNum;
}

/*make spellNum the selected spell (SelectedSpell); it is remembered in the
quality of the spellbook(s) readied*/
/*C_0C9C_1991*/static SPELL_SetSelected(int spellNum) {
	SelectedSpell = spellNum;
	if(Spellbook[0])
		SetQual(Spellbook[0], spellNum);
	if(Spellbook[1])
		SetQual(Spellbook[1], spellNum);
}

/*spellbook navigation with the direction keys:
right = next page (CMD_SPELLBOOK_NEXT_PAGE), left = previous page (CMD_SPELLBOOK_PREV_PAGE),
up/down = move the selection arrow (char 0x1a) on the page,
line i of the page being at text row i + 1 of the status clip.
returns the command or 0*/
/*C_0C9C_19D6*/static SpellbookKeyNav() {
	int line, cmd;

	if(AdvanceDir == 2 && SpellPageNav[2] != 0xff) {
		cmd = CMD_SPELLBOOK_NEXT_PAGE;
		SpellbookSelectFirst = 1;
	} else if(AdvanceDir == 6 && SpellPageNav[1] != 0xff) {
		cmd = CMD_SPELLBOOK_PREV_PAGE;
		SpellbookSelectFirst = 1;
	} else {
		if(AdvanceDir == 0) {
			line = SPELL_FindPageLine(SelectedSpell);
			if(line == -1)
				line = 0;
			if(line) {
				CON_setClip(&TextWindows[1]);
				CON_gotoxy(0, line + 1);
				CON_putch0(' ');
				CON_gotoxy(0, line);
				CON_putch0(0x1a);
				CON_setClip(&TextWindows[3]);
				SPELL_SetSelected(SpellbookPage[line-1]);
			}
		} else if(AdvanceDir == 4) {
			line = SPELL_FindPageLine(SelectedSpell);
			if(line == -1)
				line = 0;
			if(SpellbookPageCount - 1 > line && line < 7) {
				CON_setClip(&TextWindows[1]);
				CON_gotoxy(0, line + 1);
				CON_putch0(' ');
				CON_gotoxy(0, line + 2);
				CON_putch0(0x1a);
				CON_setClip(&TextWindows[3]);
				SPELL_SetSelected(SpellbookPage[line+1]);
			}
		}
		cmd = 0;
	}
	return cmd;
}

/*keyboard "slot cursor" of the equipment/portrait views (SelectMode 2),
at column SlotCursorCol / row SlotCursorRow:
 mode 1: draw the cursor,
 mode 0: erase it (redraw the slot's object or button),
 mode 2: move PointerX/Y on the slot (to validate it like a mouse click).
In every mode PointerX/Y are first set inside the slot, so that
mkMouseSelection()/MouseInEquipView() can hit-test it.*/
/*C_0C9C_1AE5*/DrawSlotCursor(int mode) {
	int x, y;
	int selKind, objType, mouseHidden;

	if(SelectMode != 2)
		return;
	if(StatusDisplay != CMD_STAT_EQUIP && StatusDisplay != CMD_STAT_PORTRAIT)
		return;
	if(StatusMember == 0) {
		SlotButtonTiles[0] =
		SlotButtonTiles[4] = TIL_SLOT_EMPTY;
	} else {
		SlotButtonTiles[0] = TIL_BUTTON_NEXT_LEFT;
		SlotButtonTiles[4] = TIL_BUTTON_COMBAT_MODE;
	}
	if(PartySize - 1 == StatusMember)
		SlotButtonTiles[3] = TIL_SLOT_EMPTY;
	else
		SlotButtonTiles[3] = TIL_BUTTON_NEXT_RIGHT;
	if(StatusDisplay == CMD_STAT_PORTRAIT)
		SlotButtonTiles[2] = TIL_BUTTON_BAG;
	else
		SlotButtonTiles[2] = TIL_BUTTON_PORTRAIT;
	if(StatusDisplay == CMD_STAT_PORTRAIT) {
		SlotCursorRow = 3;
		if(SlotCursorCol > 3)
			SlotCursorCol = 3;
	}
	if(SlotCursorRow < 0) {
		/*container/owner slot*/
		x = 272;
		PointerX = 272;
		y = 16;
		PointerY = 16;
	} else if(SlotCursorRow > 2) {
		/*buttons row*/
		if(SlotCursorCol > 4)
			SlotCursorCol = 4;
		x = SlotCursorCol * 16 + 176;
		PointerX = x;
		y = 88;
		PointerY = y;
	} else if(SlotCursorCol < 3) {
		/*equipment slots*/
		PointerX = SlotCursorCol * 24 + 176;
		PointerY = SlotCursorRow * 20 + 24;
	} else {
		/*inventory slots*/
		PointerX = SlotCursorCol * 16 + 200;
		PointerY = SlotCursorRow * 16 + 32;
	}
	if(SlotCursorRow >= 0 && SlotCursorRow < 3) {
		x = SlotScrX[SlotCursorCol];
		y = SlotScrY[SlotCursorRow][SlotCursorCol];
	}
	mouseHidden = CON_mouseOffAt(x, y, x + 15, y + 15);
	if(mode == 1) {
		GR_PUT_TILE_SCREEN(TIL_CURSOR_SELECT, x, y);
	} else if(mode == 0) {
		selKind = mkMouseSelection();
		if(SlotCursorRow > 2) {
			GR_PUT_TILE_SCREEN(SlotButtonTiles[SlotCursorCol], x, y);
		} else if((selKind == CMD_HIT_INVEN_SLOT || selKind == CMD_HIT_EQUIP_SLOT) && Selection.obj >= 0) {
			if(SlotCursorRow < 0) {
				/*container slot: object (or character, facing south)*/
				objType = GetType(Selection.obj);
				DrawTileOnBg(OBJ_MakeDirFrame(objType, 4) + BaseTile[objType], TIL_SLOT_EMPTY, x, y);
			} else {
				DrawInvObject(Selection.obj, x, y);
			}
		} else if(
			SlotCursorRow == 1 && SlotCursorCol == 2 &&
			((!ShowNPCInven && Party_2Handed[StatusMember]) || (ShowNPCInven && NPC_2Handed))
		) {
			/*left hand slot taken by a two-handed weapon*/
			GR_PUT_TILE_SCREEN(TIL_SLOT_TWO_HANDED, x, y);
		} else {
			/*empty slot (so the TIL_SLOT_FRAME name of this tile looks doubtful)*/
			GR_PUT_TILE_SCREEN(TIL_SLOT_FRAME, x, y);
		}
	} else if(mode == 2) {
		PointerX = x;
		PointerY = y;
	}
	CON_mouseOn(mouseHidden);
}

/*wait for the next input and translate it into a key or a command code.
- mouse: mkMouseCommand(); while the button is held, the command produced
  by the press (MouseHeldCmd) is repeated (e.g. walking), others are ignored.
- keyboard: direction keys -> CMD_DIRECTION + AdvanceDir, <ALT+letter> sets the
  default command; then according to SelectMode
  (0 none, 1 aiming cursor AimX/AimY on the map, 2 slot cursor in the
  equipment/portrait view, 3 typing an incantation for "Cast"):
  the command letter again or <ENTER> validates, <SPACE>/<ESC> cancels,
  <TAB> switches between map aiming and slot cursor, '1'~'9' selects a
  party member; plus the status view hotkeys
  ('*' '/' F10 '+' '-' F1~F8 '`').
CookedInput == 0 returns the raw key (used for "more" prompts).
returns a key code, a CMD_xx code, never 0*/
/*C_0C9C_1D59*/static CON_ReadCommand() {
	int buttons, mouseX;
	int ch, mouseY, newX, newY, i;

	if(CookedInput)
		CON_putch0(-1);
	ch = 0;
	LinesBeforeMore = CurClip->rows;
	while(!ch) {
		/* */
		if(MouseOn) {
			MOUSE_SERVE();
			buttons = (Mouse.buttonL?1:0) | (Mouse.buttonR?2:0);
			mouseX = Mouse.posX;
			mouseY = Mouse.posY;
			if(buttons & 3) {
				if(!MousePress) {
					MousePress =
					MouseTrigger = 1;
				} else {
					MouseTrigger = 0;
				}
				PointerX = mouseX;
				PointerY = mouseY;
				MouseButtons = buttons;
				ch = mkMouseCommand();
				if(MouseTrigger)
					MouseHeldCmd = ch;
				else if(MouseHeldCmd != ch)
					ch = 0;
			} else {
				MousePress =
				MouseTrigger = 0;
			}
		}
		if(ch)
			continue;
		/* */
		ch = CON_prompt();
		if(ch) {
			MouseMapX =
			MouseMapY = -1;
		}
		if(InputToUpper && ch < 0x100)
			ch = toupper(ch);
		/*0x1xx = 0x100 + BIOS scan code (arrows/keypad, <ALT+key>)*/
		switch(ch) {
			case 0x148: ch = CMD_DIRECTION; AdvanceDir = 0; break;/*up*/
			case 0x150: ch = CMD_DIRECTION; AdvanceDir = 4; break;/*down*/
			case 0x14b: ch = CMD_DIRECTION; AdvanceDir = 6; break;/*left*/
			case 0x14d: ch = CMD_DIRECTION; AdvanceDir = 2; break;/*right*/
			case 0x147: ch = CMD_DIRECTION; AdvanceDir = 7; break;/*up left*/
			case 0x149: ch = CMD_DIRECTION; AdvanceDir = 1; break;/*up right*/
			case 0x14f: ch = CMD_DIRECTION; AdvanceDir = 5; break;/*down left*/
			case 0x151: ch = CMD_DIRECTION; AdvanceDir = 3; break;/*down right*/
			case 0x11e: setDefaultCommand(CMD_ATTACK); ch = 0; break;/*<ALT A>*/
			case 0x12e: setDefaultCommand(CMD_CAST); ch = 0; break;/*<ALT C>*/
			case 0x114: setDefaultCommand(CMD_TALK); ch = 0; break;/*<ALT T>*/
			case 0x126: setDefaultCommand(CMD_LOOK); ch = 0; break;/*<ALT L>*/
			case 0x122: setDefaultCommand(CMD_GET); ch = 0; break;/*<ALT G>*/
			case 0x120: setDefaultCommand(CMD_DROP); ch = 0; break;/*<ALT D>*/
			case 0x132: setDefaultCommand(CMD_MOVE); ch = 0; break;/*<ALT M>*/
			case 0x116: setDefaultCommand(CMD_USE); ch = 0; break;/*<ALT U>*/
			case 0x113: setDefaultCommand(CMD_REST); ch = 0; break;/*<ALT R>*/
			case 0x130: setDefaultCommand(CMD_COMBAT); ch = 0; break;/*<ALT B>*/
		}
		if(MouseMode == 0 && !FreeAimMode) {
			DrawSlotCursor(0);
			SelectMode = 0;
		}
		/*raw key wanted: the loop ends since ch != 0 (unless no key)*/
		if(CookedInput == 0 || (InGetString && ch == '\r'))
			continue;
		if(SelectMode != 0) {
			/*pressing the pending command's letter again = <ENTER>*/
			if(
				(PendingCommand == CMD_ATTACK && ch == 'A') ||
				(PendingCommand == CMD_CAST && ch == 'C' && SelectMode == 1) ||
				(PendingCommand == CMD_TALK && ch == 'T') ||
				(PendingCommand == CMD_LOOK && ch == 'L') ||
				(PendingCommand == CMD_GET && ch == 'G') ||
				(PendingCommand == CMD_DROP && ch == 'D') ||
				(PendingCommand == CMD_MOVE && ch == 'M') ||
				(PendingCommand == CMD_USE && ch == 'U')
			) ch = '\r';

			if(PendingCommand == CMD_CAST && SelectMode == 3) {
				/*magic incantation: each letter prints its syllable (MagicWords,
				"An", "Bet", ...); up to 4 syllables, their screen positions
				are kept in IncantScrX/IncantScrY for <BACKSPACE>*/
				if(ch >= 'A' && ch <= 'Z') {
					if(IncantLen < 4) {
						IncantLetters[IncantLen] = ch;
						IncantScrX[IncantLen] = CurClip->curX;
						IncantScrY[IncantLen] = CurClip->curY;
						CON_printf(MagicWords[ch - 'A']);
						CON_putch(' ');
						CON_putch0(-1);
						if(CurClip->curX >= CurClip->cols)
							CON_putch0(0);
						/*same row but cursor went left: the window has
						scrolled, move the saved rows up*/
						if(CurClip->curY == IncantScrY[IncantLen] && CurClip->curX < IncantScrX[IncantLen]) {
							for(i = 0; i <= IncantLen; i ++)
								IncantScrY[i] --;
						}
						IncantLen ++;
					}
					ch = 0;
				} else if(ch == 0x10e/*backspace (see KBD_GetKey)*/ || ch == 0x7f/*<CTRL+BACKSPACE>*/) {
					/*erase the last syllable*/
					if(IncantLen) {
						IncantLen --;
						i = IncantLetters[IncantLen] - 'A';
						/*(newX used here as the syllable's length)*/
						for(newX = 0; MagicWords[i][newX]; newX ++);
						if(CurClip->curX > newX)
							CON_gotoxy(CurClip->curX - newX - 1, CurClip->curY);
						else
							CON_gotoxy(IncantScrX[IncantLen], IncantScrY[IncantLen]);
						CON_PutRepeat(newX, ' ');
						CON_gotoxy(IncantScrX[IncantLen], IncantScrY[IncantLen]);
					}
					ch = 0;
				} else {
					if(ch == ' ' || ch == 27) {
						IncantMask = 0;
						IncantLen = 0;
						ch = CMD_PASS;
						Selection.x = Selection.y = -1;
					} else if(ch == '\r') {
						/*make spell mask*/
						IncantMask = 0;
						for(i = 0; i < IncantLen; i ++)
							IncantMask |= 1L << (IncantLetters[i] - 'A');
						if(IncantMask) {
							i = IncantToSpell();
						} else {
							/*nothing typed: the spell selected in the book*/
							SelectMode = 1;
							if(Spellbook[0])
								i = GetQual(Spellbook[0]);
							else
								i = GetQual(Spellbook[1]);
						}
						if(i >= 0) {
							SelectedSpell = i;
							if(Spellbook[0])
								SetQual(Spellbook[0], i);
							if(Spellbook[1])
								SetQual(Spellbook[1], i);
							SPELL_RefreshList(Spellbook[0], Spellbook[1]/*bug?*/);
							SPELL_RefreshPage();
							Selection.obj = SPELL_FindPageLine(i);
							Selection.x =
							Selection.y = -1;
							ch = CMD_TARGET;
						} else {
							ch = CMD_PASS;
							Selection.x =
							Selection.y = -1;
						}
					} else if(ch == CMD_DIRECTION) {
						ch = SpellbookKeyNav();
					} else {
						ch = 0;
					}
				}
			} else {
				if(ch == 9) {
					/*<TAB>: map aiming <-> slot cursor in the equipment view*/
					if(SelectMode == 1) {
						SelectMode = 2;
						if(StatusDisplay  != CMD_STAT_EQUIP) {
							if(IN_VEHICLE)
								StatusMember = 0;
							else
								StatusMember = Active;
							StatusDisplay = CMD_STAT_EQUIP;
							StatusDirty = 1;
							RefreshStatus();
						}
						DrawSlotCursor(1);
					} else if(StatusDisplay != CMD_STAT_TALK && !IsInConversation) {
						DrawSlotCursor(0);
						SelectMode = 1;
					}
					DrawMapWindow(1);
					ch = 0;
				} else {
					if(ch == ' ' || ch == 27) {
						DrawSlotCursor(0);
						SelectMode = 0;
						ch = 0;
					} else if(ch == '\r') {
						if(SelectMode == 1) {
							/*target = aimed map tile; free aiming
							(no command) runs the default command*/
							ch = CMD_TARGET;
							if(MouseMode == 0) {
								if(FreeAimMode && DefaultCommand != 0xff)
									ch = CMD_DEFAULT_ACTION;
								FreeAimMode = 0;
							}
							SelectMode = 0;
							PointerX = TIL2SCR(AimX);
							PointerY = TIL2SCR(AimY);
							mkMouseSelection();
						} else if(SelectMode == 2) {
							if(StatusDisplay == CMD_STAT_EQUIP || StatusDisplay == CMD_STAT_PORTRAIT) {
								DrawSlotCursor(2);
								if(StatusDisplay == CMD_STAT_EQUIP)
									ch = MouseInEquipView();
								else
									ch = MouseInPortraitView();
								if(ch == 0 && MouseMode == 1 && mkMouseSelection())
									ch = CMD_TARGET;
							}
						}
					} else if(ch == CMD_DIRECTION) {
						if(SelectMode == 1) {
							/*move the aiming cursor inside the 11x11 view, within
							SelectRange of the center; SelectRange -1 means
							"adjacent tile": the direction selects at once*/
							ch = 0;
							newX = AimX + DirIncrX[AdvanceDir];
							newY = AimY + DirIncrY[AdvanceDir];
							if(
								newX >= 0 && newX < 11 &&
								newY >= 0 && newY < 11 &&
								COMBAT_range(newX, newY, 5, 5) <= SelectRange
							) {
								AimX = newX;
								AimY = newY;
								DrawMapWindow(1);
							} else if(SelectRange == -1) {
								ch = CMD_TARGET;
								PointerX = TIL2SCR(newX);
								PointerY = TIL2SCR(newY);
								mkMouseSelection();
							}
						} else if(SelectMode == 2 && StatusDisplay == CMD_STAT_EQUIP) {
							/*move the slot cursor; past the edges it scrolls
							the inventory, or goes to the container slot (up)
							or to the buttons row (down)*/
							ch = 0;
							newX = SlotCursorCol + DirIncrX[AdvanceDir];
							newY = SlotCursorRow + DirIncrY[AdvanceDir];
							if(
								newX >= 0 && newX < 7 &&
								newY >= 0 && newY < 3
							) {
								DrawSlotCursor(0);
								if(newX == 1 && newY == 1) {/*skip the paper doll's body*/
									newX += DirIncrX[AdvanceDir];
									newY += DirIncrY[AdvanceDir];
								}
								SlotCursorCol = newX;
								SlotCursorRow = newY;
								DrawSlotCursor(1);
							} else if((newX > 2 && newX < 7) || newY > 2) {
								if(newY > 2 && InvenSize - InvenScroll > 12 && AdvanceDir == 4) {
									SlotCursorCol = newX;
									ch = CMD_INVEN_SCROLL_DOWN;
								} else if(newY < 0 && InvenScroll) {
									SlotCursorCol = newX;
									ch = CMD_INVEN_SCROLL_UP;
								} else if(newY < 0 && PickpocketActive == 0) {
									DrawSlotCursor(0);
									SlotCursorRow = 0xff;
									DrawSlotCursor(1);
								} else if(newY > 2 && PickpocketActive == 0) {
									DrawSlotCursor(0);
									SlotCursorRow = 3;
									SlotCursorCol = newX;
									if(SlotCursorCol > 4)
										SlotCursorCol = 4;
									else if(SlotCursorCol < 0)
										SlotCursorCol = 0;
									DrawSlotCursor(1);
								}
							}
						} else if(SelectMode == 2 && StatusDisplay == CMD_STAT_PORTRAIT) {
							ch = 0;
							newX = SlotCursorCol + DirIncrX[AdvanceDir];
							if(newX < 0)
								newX = 0;
							else if(newX > 3)
								newX = 3;
							DrawSlotCursor(0);
							SlotCursorCol = newX;
							SlotCursorRow = 3;
							DrawSlotCursor(1);
						}
					} else if(ch >= '1' && ch - '1' < PartySize && SelectMode == 1) {
						/*'1'~: select a party member as target*/
						SelectionIsDark = 0;
						SelectMode = 0;
						Selection.obj = Party[ch - '1'];
						if(IN_VEHICLE) {
							Selection.x = MapX;
							Selection.y = MapY;
						} else {
							Selection.x = GetX(Selection.obj);
							Selection.y = GetY(Selection.obj);
						}
						ch = CMD_TARGET;
					}
				}
			}
		} else if(ch == 9 && StatusDisplay != CMD_STAT_TALK && !IsInConversation) {
			/*<TAB> with no command pending: free aiming (FreeAimMode)*/
			if(StatusDisplay == CMD_STAT_EQUIP) {
				SelectMode = 2;
				DrawSlotCursor(1);
			} else if(StatusDisplay != CMD_STAT_SPELLBOOK) {
				SelectMode = 1;
			}
			if(SelectMode != 0) {
				AimX =
				AimY = 5;
				SelectRange = 7;
				FreeAimMode = 1;
				DrawMapWindow(1);
				ch = 0;
			}
		}
		if(StatusDisplay == CMD_STAT_EQUIP && !IsInConversation) {
			/*character equipment view*/
			if(ch == '*') {
				ch = CMD_STAT_PORTRAIT;
				Selection.obj = -1;
			} else if(ch == '/' || ch == 0x144/*F10*/) {
				ch = CMD_STAT_PARTY;
			} else if(ch == '+') {
				ch = CMD_NEXT_MEMBER;
			} else if(ch == '-') {
				ch = CMD_PREV_MEMBER;
			} else if(ch == '`' || ch == '~') {
				/*"click" on the combat mode button*/
				PointerX = 248;
				PointerY = 96;
				MouseInEquipView();
				ch = 0;
			} else if(ch >= 0x13b/*F1*/ && PartySize + 0x13b > ch) {
				Selection.obj = ch - 0x13b;
				ch = CMD_STAT_EQUIP;
			}
		} else if(StatusDisplay == CMD_STAT_PORTRAIT && !IsInConversation) {
			/*character stat view*/
			if(ch == '*') {
				ch = CMD_STAT_EQUIP;
				Selection.obj = -1;
			} else if(ch == '/' || ch == 0x144/*F10*/) {
				ch = CMD_STAT_PARTY;
			} else if(ch == '+') {
				ch = CMD_NEXT_MEMBER;
			} else if(ch == '-') {
				ch = CMD_PREV_MEMBER;
			} else if(ch >= 0x13b && PartySize + 0x13b > ch) {
				Selection.obj = ch - 0x13b;
				ch = CMD_STAT_PORTRAIT;
			}
		} else if(StatusDisplay == CMD_STAT_PARTY && !IsInConversation) {
			if(ch == '+' && PartyListTop < PartySize - 5) {
				ch = CMD_PARTY_SCROLL_DOWN;
			} else if(ch == '-' && PartyListTop != 0) {
				ch = CMD_PARTY_SCROLL_UP;
			} else if(ch >= 0x13b/*F1*/ && PartySize + 0x13b > ch) {
				Selection.obj = ch - 0x13b;
				ch = CMD_STAT_EQUIP;
			}
		} else if(StatusDisplay == CMD_STAT_SPELLBOOK && !IsInConversation) {
			/*look at the spellbook*/
			if(ch == ' ' || ch == 27) {
				ch = CMD_PASS;
				Selection.x = Selection.y = -1;
			} else if(ch == '\r') {
				SelectMode = 1;
				if(Spellbook[0])
					i = GetQual(Spellbook[0]);
				else
					i = GetQual(Spellbook[1]);
				if(i < 0) {
					ch = 0;
				} else {
					SelectedSpell = i;
					if(Spellbook[0])
						SetQual(Spellbook[0], i);
					if(Spellbook[1])
						SetQual(Spellbook[1], i);
					SPELL_RefreshList(Spellbook[0], Spellbook[1]/*bug?*/);
					SPELL_RefreshPage();
					Selection.obj = SPELL_FindPageLine(i);
					Selection.x =
					Selection.y = -1;
					ch = CMD_TARGET;
				}
			} else if(ch == CMD_DIRECTION) {
				ch = SpellbookKeyNav();
			}
		}
	}

	return ch;
}

/*redraw the spellbook page after a page change; if it was requested by
the keyboard (SpellbookSelectFirst), select the page's first spell*/
/*C_0C9C_2A16*/static SpellbookPageChanged() {
	SPELL_RefreshPage();
	if(SpellbookSelectFirst) {
		SpellbookSelectFirst = 0;
		SPELL_SetSelected(SpellbookPage[0]);
		CON_setClip(&TextWindows[1]);
		CON_gotoxy(0, 1);
		CON_putch0(0x1a);
		CON_setClip(&TextWindows[3]);
	}
}

/*get char:
wait for a key or a command (CON_ReadCommand). The status panel commands
(view changes, party/inventory scrolling, containers, ready/unready,
spellbook pages) are executed here and the wait goes on; any other
key/command is returned.
CMD_DEFAULT_ACTION (default command on a right-clicked target) returns DefaultCommand
and, except for cast/rest/save which ask for more input, makes the next
call return CMD_TARGET at once since Selection is already filled*/
/*C_0C9C_2A59*/CON_getch() {
	int circle, ch;
	int endLoop, spellIdx, oldFirst;

	if(PendingSelect) {
		PendingSelect = 0;
		return CMD_TARGET;
	}
	do {
		endLoop = 0;
		ch = CON_ReadCommand();
		switch(ch) {
			case CMD_DEFAULT_ACTION:
				ch = DefaultCommand;
				if(ch != CMD_CAST && ch != CMD_REST && ch != CMD_SAVE)
					PendingSelect = 1;
				endLoop = 1;
			break;
			case CMD_STAT_PARTY:
				SelectMode = 0;
				StatusDisplay = CMD_STAT_PARTY;
				StatusMember = 0;
				StatusDirty = 1;
			break;
			case CMD_STAT_PORTRAIT:
				StatusDisplay = CMD_STAT_PORTRAIT;
				if(Selection.obj >= 0)
					StatusMember = Selection.obj;
				StatusDirty = 1;
			break;
			case CMD_STAT_EQUIP:
				StatusDisplay = CMD_STAT_EQUIP;
				if(Selection.obj >= 0)
					StatusMember = Selection.obj;
				StatusDirty = 1;
			break;
			case CMD_PREV_MEMBER:/*portrait/equipment select --*/
				if(StatusMember > 0) {
					StatusMember --;
					StatusDirty = 1;
					Selection.obj = -1;
				}
			break;
			case CMD_NEXT_MEMBER:/*portrait/equipment select ++*/
				if(PartySize - 1 > StatusMember) {
					StatusMember ++;
					StatusDirty = 1;
					Selection.obj = -1;
				}
			break;
			case CMD_PARTY_SCROLL_UP:/*party list scroll up*/
				PartyListTop --;
				StatusDirty = 1;
			break;
			case CMD_PARTY_SCROLL_DOWN:/*party list scroll down*/
				PartyListTop ++;
				StatusDirty = 1;
			break;
			case CMD_INVEN_SCROLL_UP:/*inventory scroll up (4 slots = 1 row)*/
				STAT_InvenScrollUp(StatusMember);
				DrawSlotCursor(1);
			break;
			case CMD_INVEN_SCROLL_DOWN:/*inventory scroll down*/
				STAT_InvenScrollDown(StatusMember);
				DrawSlotCursor(1);
			break;
			case CMD_CONTAINER_UP:/*leave the container shown: back to its owner/parent container, or from a character to the party view*/
				if(InvenOwner < 0x100) {
					StatusDisplay = CMD_STAT_PARTY;
					StatusMember = 0;
					StatusDirty = 1;
				} else {
					InvenOwner = GetAssoc(InvenOwner);
					InvenScroll = 0;
					STAT_RefreshInventory(InvenOwner);
					DrawSlotCursor(1);
				}
			break;
			case CMD_CONTAINER_OPEN:/*show the content of container Selection.obj*/
				InvenOwner = Selection.obj;
				InvenScroll = 0;
				STAT_RefreshInventory(InvenOwner);
				DrawSlotCursor(1);
			break;
			case CMD_READY:/*Ready ...*/
				STAT_Ready();
				DrawSlotCursor(1);
			break;
			case CMD_UNREADY:/*Unready ...*/
				STAT_Unready();
				DrawSlotCursor(1);
			break;
			case CMD_SPELLBOOK_PREV_CIRCLE:/*previous circle: first page of circle SpellPageNav[0]*/
				/*SpellbookFirst = index of the first spell displayed = number of known spells before that circle*/
				SpellbookFirst = 0;
				for(circle = 0; circle <= SpellPageNav[0]; circle ++) {
					for(spellIdx = 0; spellIdx < 16; spellIdx ++) {
						if(HAS_SPELL(circle, spellIdx)) {
							if(SpellPageNav[0] == circle)
								break;
							SpellbookFirst ++;
						}
					}
				}
				SPELL_RefreshPage();
			break;
			case CMD_SPELLBOOK_NEXT_CIRCLE:/*next circle: first page of circle SpellNextCircle (= SpellPageNav[3])*/
				SpellbookFirst = 0;
				for(circle = 0; circle <= SpellNextCircle; circle ++) {
					for(spellIdx = 0; spellIdx < 16; spellIdx ++) {
						if(HAS_SPELL(circle, spellIdx)) {
							if(SpellNextCircle == circle)
								break;
							SpellbookFirst ++;
						}
					}
				}
				SPELL_RefreshPage();
			break;
			case CMD_SPELLBOOK_PREV_PAGE:/*previous page (8 spells per page); SpellPageNav[1] == 2: same circle*/
				if(SpellPageNav[1] == 2) {
					SpellbookFirst -= 8;
				} else {
					oldFirst = SpellbookFirst;
					SpellbookFirst = 0;
					for(circle = 0; circle <= SpellPageNav[0]; circle ++) {
						for(spellIdx = 0; spellIdx < 16; spellIdx ++) {
							if(HAS_SPELL(circle, spellIdx)) {
								if(SpellPageNav[0] == circle)
									break;
								SpellbookFirst ++;
							}
						}
					}
					/*previous circle has a 2nd page: go to it*/
					if(oldFirst - SpellbookFirst > 8)
						SpellbookFirst += 8;
				}
				SpellbookPageChanged();
			break;
			case CMD_SPELLBOOK_NEXT_PAGE:/*next page; SpellPageNav[2] == 2: same circle, else first page of the next circle*/
				if(SpellPageNav[2] == 2) {
					SpellbookFirst += 8;
				} else {
					SpellbookFirst = 0;
					for(circle = 0; circle <= SpellNextCircle; circle ++) {
						for(spellIdx = 0; spellIdx < 16; spellIdx ++) {
							if(HAS_SPELL(circle, spellIdx)) {
								if(SpellNextCircle == circle)
									break;
								SpellbookFirst ++;
							}
						}
					}
				}
				SpellbookPageChanged();
			break;
			default:
				endLoop = 1;
			case CMD_REFRESH_STATUS: ;/*(no producer found) only refreshes the status panel*/
		}
		if(!endLoop)
			RefreshStatus();
	} while(!endLoop);

	return ch;
}

/*read a line of at most maxLen printable chars into buf, echoing it.
<ENTER> validates, <ESC> erases it and aborts (GetsAborted = 1, buf = "").
returns the length*/
/*C_0C9C_2D3A*/CON_gets(char *buf, int maxLen) {
	int len, done;
	int ch;

	InGetString = 1;
	InputToUpper = 0;
	CON_putch0(-1);
	len =
	done = 0;
	KbdKeepLastOnly = 0;
	while(!done) {
		while(!(ch = CON_prompt()));
		switch(ch) {
			case '\r':
				buf[len] = 0;
				GetsAborted = 0;
				done ++;
			break;
			case 27:
				buf[0] = 0;
				while(len --)
					CON_EchoChar(8);
				GetsAborted = 1;
				done ++;
			break;
			case 0x10e:/*backspace*/
				if(len) {
					len --;
					CON_EchoChar(8);
				}
			break;
			default:
				if(len < maxLen && ch >= ' ' && ch <= '~') {
					buf[len] = ch;
					len ++;
					CON_EchoChar(ch);
				}
		}
	}
	KbdKeepLastOnly = 1;
	LinesBeforeMore = CurClip->rows;
	InputToUpper = 1;
	InGetString = 0;
	return len;
}

/*initialize text window clip on the text cells (x0,y0)-(x1,y1)
(inclusive) with default colors/attributes, and make it current*/
/*C_0C9C_2E09*/CON_createClip(struct tClipInfo *clip, int x0, int y0, int x1, int y1) {
	CurClip = clip;
	clip->originX = x0;
	clip->originY = y0;
	clip->cols = x1 - x0 + 1;
	clip->rows = y1 - y0 + 1;
	clip->curX = clip->curY = 0;
	clip->bgColor = ColorPaper;
	clip->fgColor = ColorText;
	clip->attrFlags = DefaultClipAttr;
}

/*make clip the current text window (pending text of the previous one
is flushed first)*/
/*C_0C9C_2E54*/CON_setClip(struct tClipInfo *clip) {
	CON_putch0(-1);
	CurClip = clip;
	GrInfo->textBg = CurClip->bgColor;
	GrInfo->textFg = CurClip->fgColor;
	CON_putch0(-1);
}

/*move the text cursor of the current clip to (x,y), clamped to it*/
/*C_0C9C_2E8F*/CON_gotoxy(int x, int y)
{
	if(x < 0)
		x = 0;
	if(y < 0)
		y = 0;
	if(x > CurClip->cols)
		x = CurClip->cols;
	if(y > CurClip->rows)
		y = CurClip->rows;
	CON_putch0(-1);
	CurClip->curX = x;
	CurClip->curY = y;
	CON_putch0(-1);
}

/*low level character output to the current clip (no escapes).
ch >= 0: the char is appended to the line buffer (LineBufChars/LineBufColors);
  0 or '\n' ends the line. When the line is complete (newline, or buffer
  full = window width) it is drawn: with word wrap (_attr & 1) the line is
  cut after the last ' ' or '-' and the rest is carried to the next line,
  with _attr & 2 it is centered. Then the cursor goes to the next line,
  scrolling (_attr & 0x20) at the bottom. With _attr & 0x10 a "more"
  prompt waits for a key when the window is full of new text (LinesBeforeMore).
ch < 0 (-1): flush: draw the pending part of the line without ending it
  (done before moving the cursor, changing the clip, or reading input)*/
/*C_0C9C_2EFA*/CON_putch0(int ch) {
	int i, key;
	int breakPos, unusedMidX, row;

	if(!TextOn)
		return;
	/*skip empty lines after an attack message (see SkipNewlines)*/
	if(SkipNewlines && (CurClip->attrFlags & 0x20)) {
		if(ch == '\n')
			return;
		if(ch >= 0)
			SkipNewlines = 0;
	}
	TextIdle = 0;
	if((CurClip->attrFlags & 4) && CapitalizeNext)
		ch = toupper(ch);
	CapitalizeNext = 0;
	if(ch == 0)
		ch = '\n';
	/*runic font: upper half of the character set*/
	if((CurClip->attrFlags & 8) && ch > ' ')
		ch |= 0x80;
	if(CurClip->cols == LineBufLen || ch == '\n' || ch < 0) {
		/*-- line complete (or flush): draw it --*/
		if(LinesBeforeMore && ch >= 0 && (CurClip->attrFlags & 0x10))
			LinesBeforeMore --;
		if((CurClip->attrFlags & 0x10) && LinesBeforeMore == 0 && ch >= 0) {
			/*"more": other cursor shape (PromptCh 1), raw keys*/
			unusedMidX = (CurClip->cols >> 1) - 2;
			row = (CurClip->rows + CurClip->originY) - 1;
			CookedInput = 0;
			PromptCh = 1;
			do
				key = CON_getch();
			while(key != ' ' && key != '\r' && key != 27 && key != CMD_PASS);
			PromptCh = 5;
			CookedInput = 1;
			TextIdle = 0;
			/*erase the bottom line (where the "more" cursor was)*/
			for(i = 0; i < CurClip->cols; i ++)
				(*CON_PutCharFn)(' ', CurClip->originX + i, row);
			LinesBeforeMore = CurClip->rows - 1;
		}
		/*word wrap: find where to cut the full line (not before the part
		already drawn; no break found: cut at the window width)*/
		breakPos = LineBufLen;
		if((CurClip->attrFlags & 1) && CurClip->cols == LineBufLen && ch >= 0) {
			LineBufChars[LineBufLen] = (ch == '\n')?' ':ch;
			LineBufColors[LineBufLen] = CurClip->fgColor;
			while(LineBufChars[breakPos] != ' ' && LineBufChars[breakPos - 1] != '-' && LineBufDrawn < breakPos)
				breakPos --;
			if(breakPos == 0)
				breakPos = LineBufLen;
		}
		row = CurClip->originY + CurClip->curY;
		if(CurClip->attrFlags & 2) {
			for(i = LineBufDrawn; i < ((CurClip->cols - breakPos) >> 1) + LineBufDrawn; i++)
				(*CON_PutCharFn)(' ', CurClip->originX + i, row);
		}
		for(i = LineBufDrawn; i < breakPos; i ++) {
			CurClip->fgColor = LineBufColors[i];
			(*CON_PutCharFn)(LineBufChars[i], CurClip->originX + i, row);
		}
		CurClip->fgColor = ColorText;
		if(LineBufDrawn < breakPos)
			CurClip->curX = breakPos;
		if((CurClip->attrFlags & 1) && CurClip->cols == LineBufLen && ch >= 0) {
			/*carry the cut word to the start of the buffer (dropping the space)*/
			if(LineBufChars[breakPos] == ' ')
				breakPos ++;
			for(i = breakPos; i <= LineBufLen; i ++) {
				LineBufChars[i - breakPos] = LineBufChars[i];
				LineBufColors[i - breakPos] = LineBufColors[i];
			}
			LineBufDrawn = 0;
			LineBufLen = i - breakPos;
			if(ch == ' ')
				LineAcceptSpace = 0;
		} else if(ch >= 0) {
			/*new empty line, then the char that did not fit (if any)*/
			LineBufLen = LineBufDrawn = 0;
			if(ch >= 0 && ch != '\n') {
				LineBufColors[LineBufLen] = CurClip->fgColor;
				LineBufChars[LineBufLen] = ch;
				LineBufLen ++;
			}
		}
		if(ch >= 0) {
			CurClip->curX = 0;
			if(CurClip->curY < CurClip->rows - 1) {
				CurClip->curY ++;
			} else if(CurClip->attrFlags & 0x20) {
				TextIdle = 1;
				(*CON_ScrollUpFn)(CurClip->originX, CurClip->originY, CurClip->cols, CurClip->rows);
				TextIdle = 0;
			}
		} else {
			/*flush: the line stays open at the cursor*/
			LineBufDrawn = LineBufLen = CurClip->curX;
			InHiddenSection = 0;
		}
	} else if((CurClip->attrFlags & 1) == 0 || ch != ' ' || LineAcceptSpace) {
		/*buffer the char (leading spaces of a wrapped line are dropped)*/
		LineBufColors[LineBufLen] = CurClip->fgColor;
		LineBufChars[LineBufLen] = ch;
		LineBufLen ++;
	}
	if(ch != ' ')
		LineAcceptSpace = 1;
	TextIdle = 1;
}

/*output ch count times (no escapes)*/
/*C_0C9C_32DE*/CON_PutRepeat(int count, int ch) {
	int i;

	for(i = 0; i < count; i ++)
		CON_putch0(ch);
}

/*output string str (no escapes)*/
/*C_0C9C_32F8*/CON_PutsRaw(unsigned char far *str) {
	while(*str)
		CON_putch0(*str++);
}

/*misnamed: this is an itoa.
convert number to a string in str, in base |base| (2~36); a negative
base means signed (number taken as a signed int, '-' counts in maxLen).
at most maxLen digits are kept (the most significant ones are lost).
returns the length*/
/*C_0C9C_331A*/CON_itoa(unsigned number, char far *str, int maxLen, int base) {
	int len;
	char digits[8];
	int i, isNegative, digit;

	isNegative = 0;
	len = 0;
	if(base < 0) {
		base = -base;
		if(number > 0x7fff) {
			number = -number;
			isNegative = 1;
			maxLen --;
		}
	}
	/*digits, least significant first*/
	if(base) {
		while(number && len < maxLen) {
			digit = number % base;
			digits[len++] = (digit < 10)?(digit + '0'):(digit - 10 + 'A');
			number /= base;
		}
	}
	if(len == 0)
		digits[len++] = '0';
	if(isNegative)
		digits[len++] = '-';
	/*reverse*/
	for(i = 0; i < len; i ++)
		str[i] = digits[len - i - 1];
	str[i] = 0;
	return len;
}

/*character output with the in-text escapes (used by CON_puts/CON_printf,
so by all game messages, books, signs and conversations):
  #c   print VarInt[c] (c = '0'~'9' or 'A'~'Z')
  $c   print VarStr[c]
  /    following letters printed only if the last number was 1 (singular)
  \    following letters printed only if the last number was not 1 (plural)
       (both end at the next non letter)
  ^c   print control char c & 0x1f (special glyphs)
  @    following word is a keyword: highlighted (ColorRed) if help is on
  %    cancels a pending '/' or '\' (no effect by itself)
  *    flush and wait for a key
  &    toggle a section hidden unless SpeakGargish (SpeakGarglish)
  < >  begin/end runic font
  `    capitalize the next char*/
/*C_0C9C_33D3*/CON_putch(int ch) {
	int varIdx;
	char numBuf[8];

	if(!TextOn)
		return;
	/*hidden '&' section*/
	if(ch != '&' && InHiddenSection && !SpeakGargish)
		return;
	switch(ch) {
		case '#': case '$': case '%':
		case '/': case '@': case '\\':
		case '^': PendingEscape = ch; return;
		case '*':
			CON_putch0(-1);
			PromptCh = 1;
			CON_getch();
			PromptCh = 5;
		return;
		case '&': InHiddenSection = !InHiddenSection; return;
		case '<': CurClip->attrFlags |= 8; return;
		case '>': CurClip->attrFlags &= ~8; return;
		case '`': CapitalizeNext = 1; return;
	}
	switch(PendingEscape) {
		case '#':
			ch = toupper(ch);
			if(ch >= '0' && ch <= '9')
				varIdx = ch - '0';
			else if(ch >= 'A' && ch <= 'Z')
				varIdx = (ch - 'A') + 10;
			else
				varIdx = 0;
			LastNumberPlural = (VarInt[varIdx] != 1);
			CON_itoa(VarInt[varIdx], numBuf, 6, -10);
			CON_PutsRaw(numBuf);
			PendingEscape = 0;
		break;
		case '$':
			ch = toupper(ch);
			if(ch >= '0' && ch <= '9')
				varIdx = ch - '0';
			else if(ch >= 'A' && ch <= 'Z')
				varIdx = (ch - 'A') + 10;
			else
				varIdx = 0;
			CON_PutsRaw(VarStr[varIdx]);
			PendingEscape = 0;
		break;
		case '/':
			if(!isalpha(ch) || !LastNumberPlural) {
				CON_putch0(ch);
				if(!isalpha(ch))
					PendingEscape = 0;
			}
		break;
		case '\\':
			if(!isalpha(ch) || LastNumberPlural) {
				CON_putch0(ch);
				if(!isalpha(ch))
					PendingEscape = 0;
			}
		break;
		case '^':
			if(CurClip->attrFlags & 8)
				CON_putch0((ch & 0x1f) | 0x80);
			else
				CON_putch0(ch & 0x1f);
			PendingEscape = 0;
		break;
		case '@':
			/*keyword ends with punctuation/space*/
			if(strchr(" ,.:;!?'-\"\n", ch)) {
				PendingEscape = 0;
				CurClip->fgColor = ColorText;
			} else if(HelpOn) {
				CurClip->fgColor = ColorRed;
			}
		/*no break: print it*/
		default:
			CON_putch0(ch);
	}
}

/*output string str with escapes (CON_putch)*/
/*C_0C9C_3621*/CON_puts(unsigned char far *str) {
	while(*str)
		CON_putch(*str++);
}

/*minimal printf: %[-][0][width] followed by d (signed), o, x, u, c, s;
an unknown conversion prints its char raw ("%>" prints the '>' prompt);
the numbers set LastNumberPlural (plural flag); literal chars go through
CON_putch() so escapes apply, but the converted values (%s too) also do*/
cdecl /*C_0C9C_3643*/CON_printf(unsigned char far *format, ...) {
	void **argp;
	unsigned num;
	char numBuf[8];
	int c, fmtCh;
	int fieldWidth, len, padCh, leftAlign;
	char *str;

	if(!TextOn)
		return;
	argp = ...;/*Turbo C: address of the variable arguments*/
	while(*format) {
		c = *format++;
		fieldWidth = len = leftAlign = 0;
		padCh = ' ';
		str = numBuf;
		switch(c) {
		case '%':
			fmtCh = *format++;
			while(fmtCh == '0' || fmtCh == '-') {
				switch(fmtCh) {
					case '0': padCh = '0'; break;
					case '-': leftAlign = 1; break;
				}
				fmtCh = *format++;
			}
			while(fmtCh >= '0' && fmtCh <= '9') {
				fieldWidth *= 10;
				fieldWidth += fmtCh - '0';
				fmtCh = *format++;
			}
			switch(tolower(fmtCh)) {
				case 'd':
					num = *(int *)argp++;
					len = CON_itoa(num, numBuf, 6, -10);
					LastNumberPlural = (num != 1);
				break;
				case 'o':
					num = *(int *)argp++;
					len = CON_itoa(num, numBuf, 6, 8);
					LastNumberPlural = (num != 1);
				break;
				case 'x':
					num = *(int *)argp++;
					len = CON_itoa(num, numBuf, 4, 16);
					LastNumberPlural = (num != 1);
				break;
				case 'u':
					num = *(int *)argp++;
					len = CON_itoa(num, numBuf, 5, 10);
					LastNumberPlural = (num != 1);
				break;
				case 'c':
					numBuf[0] = *(char *)argp++;
					numBuf[1] = 0;
				break;
				case 's':
					str = (char *)*argp++;
					len = strlen(str);
				break;
				default:
					numBuf[0] = 0;
					CON_putch0(fmtCh);
			}
			if(leftAlign)
				CON_puts(str);
			CON_PutRepeat(fieldWidth - len, padCh);
			if(leftAlign == 0)
				CON_puts(str);
		break;
		default:
			CON_putch(c);
		}
	}
}
