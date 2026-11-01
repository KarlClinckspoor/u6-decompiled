/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Game state block saved in the savegame (OBJLIST): everything from
	IsOnQuest (GameStateStart) to GameStateEnd (excluded) is read/written as one
	block (read by GAME_Load, written by GAME_Save), so the order and the
	sizes of these variables must not change.
*/
#include "gamestat.h"

/*2C4A*/unsigned char IsOnQuest = 0;
/*2C4B*/unsigned char NextSleep = 0;
/*2C4C*/unsigned char Time_M = 0;/*minutes*/
/*2C4D*/unsigned char Time_H = 8;/*hour of day*/
/*2C4E*/unsigned char Date_D = 4;/*day of month*/
/*2C4F*/unsigned char Date_M = 7;/*month*/
/*2C50*/uint16 Date_Y = 161;/*year*/
/*2C52*/unsigned char KARMA = 75;
/*2C53*/char WindDir = 0;
/*2C54*/unsigned char Active = 0;
/*2C55*/char AmbientLight = 7;/*ambient (sun) light level 0..7, see PassTime*/
/*2C56*/int16 MapX = 0x133;
/*2C58*/int16 MapY = 0x160;
/*2C5A*/int16 MapZ = 0;
/*2C5C*/unsigned char SpellFx[16] = {0};
/*2C6C*/int16 VanishedShape = 0;/*shape/type of the last "Vanish"ed object, for "Reappear"*/
/*2C6E*/unsigned char HourlyUnused = 0;/*reset every hour, never read*/
/*2C6F*/unsigned char CaughtFish = 0;
/*2C70*/unsigned char DrunkCounter = 0;
/*2C71*/unsigned char MustRingBell = 0;
/*2C72*/int16 SoundFlag = 1;
/*2C74*/int16 MoonstonePos[8][3] = {/*moonstones x, y, z (moongate of each phase; 0,0,0 when not buried)*/
	0x3A7,0x106,0,
	0x1F7,0x166,0,
	0x09F,0x3AE,0,
	0x127,0x026,0,
	0x33F,0x0A6,0,
	0x147,0x336,0,
	0x017,0x016,1,
	0x397,0x3A6,0
};
/*2CA4*/int16 KegFuse = 0;/*lit powder keg: minutes before explosion*/
/*2CA6*/int16 KegObject = 0;/*lit powder keg object*/
/*2CA8*/int16 SpeakGargish = 0;/*the Avatar speaks Gargish (conversation variable 'L')*/
/*2CAA*/unsigned char PathObject[8] = {0};
/*2CB2*/unsigned char PathCounter[8] = {0};
/*2CBA*/unsigned char PathTries[8] = {0};
/*2CC2*/char InCombat = 1;
/*2CC3*/char SoloMember = -1;/*solo mode: party index of the solo member, -1 = party mode*/
/*2CC4*/char SavedNPCMode = 0;/*NPCMode of the commanded member before it got AI_COMMAND*/
/*2CC5*/unsigned char DefaultCommand = 0xff;
/*2CC6*/unsigned char TrammelPhase = 0;/*moon 1 (Trammel?) phase 0..7*/
/*2CC7*/unsigned char TrammelPos = 0;/*moon 1 position in the sky 0..23 (visible if <= 14)*/
/*2CC8*/unsigned char FeluccaPhase = 0;/*moon 2 (Felucca?) phase 0..7*/
/*2CC9*/unsigned char FeluccaPos = 0;/*moon 2 position in the sky 0..23 (visible if <= 14)*/
/*2CCA*/unsigned char AvatarFemale = 0;/*avatar's sex (!= 0: female, "milady")*/
/*2CCB*/unsigned char AvatarPortrait = 0;/*avatar's portrait + 1 (0: no character created yet)*/
/*2CCC*/int16 GameStateEnd = 0;/*end marker*/
