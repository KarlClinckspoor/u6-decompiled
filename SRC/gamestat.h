/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __D_2C4A_H__
#define __D_2C4A_H__

#include "portable.h"

/*__2C4A*/extern unsigned char IsOnQuest;
/*__2C4B*/extern unsigned char NextSleep;
/*__2C4C*/extern unsigned char Time_M;
/*__2C4D*/extern unsigned char Time_H;
/*__2C4E*/extern unsigned char Date_D;
/*__2C4F*/extern unsigned char Date_M;
/*__2C50*/extern uint16 Date_Y;
/*__2C52*/extern unsigned char KARMA;
/*__2C53*/extern char WindDir;
/*__2C54*/extern unsigned char Active;
/*__2C55*/extern char AmbientLight;/*sun light level 0..7 (0 night/underground/eclipse)*/
/*__2C56*/extern int16 MapX;
/*__2C58*/extern int16 MapY;
/*__2C5A*/extern int16 MapZ;
/*__2C5C*/extern unsigned char SpellFx[16];
/*__2C6C*/extern int16 VanishedShape;/*ObjShapeType of the last Vanish'ed object, used by Reappear*/
/*__2C6E*/extern unsigned char HourlyUnused;/*cleared every game hour, never read*/
/*__2C6F*/extern unsigned char CaughtFish;
/*__2C70*/extern unsigned char DrunkCounter;
/*__2C71*/extern unsigned char MustRingBell;
/*__2C72*/extern int16 SoundFlag;
/*__2C74*/extern int16 MoonstonePos[8][3];/*[8][x,y,z] buried moonstone / moongate location per phase, 0 if none*/
/*__2CA4*/extern int16 KegFuse;/*minutes before the lit powder keg explodes*/
/*__2CA6*/extern int16 KegObject;/*lit powder keg object number*/
/*__2CA8*/extern int16 SpeakGargish;/*avatar understands Gargish (conversation var 'L')*/
/*__2CAA*/extern unsigned char PathObject[8];/*related to npctrack.c/NPCTracker*/
/*__2CB2*/extern unsigned char PathCounter[8];/*related to npctrack.c/NPCTracker*/
/*__2CBA*/extern unsigned char PathTries[8];/*related to npctrack.c/NPCTracker*/
/*__2CC2*/extern char InCombat;
/*__2CC3*/extern char SoloMember;/*party index in solo mode, -1 = party mode*/
/*__2CC4*/extern char SavedNPCMode;/*NPCMode the commanded member had before AI_COMMAND, restored when control changes*/
/*__2CC5*/extern unsigned char DefaultCommand;
/*__2CC6*/extern unsigned char TrammelPhase;/*moon 1 phase 0..7 (from MoonPhaseByDay by day); destination of moongate travel*/
/*__2CC7*/extern unsigned char TrammelPos;/*moon 1 sky position 0..23, visible when <= 14*/
/*__2CC8*/extern unsigned char FeluccaPhase;/*moon 2 phase 0..7*/
/*__2CC9*/extern unsigned char FeluccaPos;/*moon 2 sky position 0..23*/
/*__2CCA*/extern unsigned char AvatarFemale;/*!= 0: avatar is female (milady)*/
/*__2CCB*/extern unsigned char AvatarPortrait;/*avatar portrait + 1; 0 = no character created (back to ULTIMA6.EXE)*/
/*__2CCC*/extern int16 GameStateEnd;/*end marker of the saved state block*/

/*alias of IsOnQuest used only as the start address of the saved block*/
#define GameStateStart IsOnQuest

#endif
