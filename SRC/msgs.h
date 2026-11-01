/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __SEG_356A_H__
#define __SEG_356A_H__

#include "portable.h"

extern char far SlimeDividesMsg[];
extern char far CriticalMsg[];
extern char far HeavilyMsg[];
extern char far LightlyMsg[];
extern char far BarelyMsg[];
extern char far WoundedMsg[];
extern char far GrazedMsg[];
extern char far KilledMsg[];/*typo*/
extern char far ShipCannonMsg[];
extern char far BareHandsMsg[];
extern char far WithMsg[];
extern char far OutOfArrowsMsg[];
extern char far OutOfBoltsMsg[];
/*356A:00A3*/extern char far AttackSelfPassMsg[];/*"pass." when the attack target is oneself*/
extern char far NothingMsg[];
extern char far AttackWithMsg[];
extern char far AttackMsg[];
extern char far WhatMsg[];
extern char far PartyModeMsg[];
/*356A:00DF*/extern char far FailsToRespondMsg[];/*"`%s fails to respond." (solo mode refused)*/
extern char far SoloModeMsg[];
extern char far ExitToDOSMsg[];
extern char far SaveMsg[];
/*356A:0121*/extern char far PassMsg[];/*"Pass!" command*/
/*356A:0128*/extern char far ShaminoHearsMsg[];/*"Shamino says, I hear something to the %s!"*/
extern char far GlassSwordMsg[];
extern char far InterferenceMsg[];
/*356A:0181*/extern char far ArghMsg[];/*"%s: ARGH!" npc dragged under*/
extern char far RegurgitatedMsg[];
extern char far DraggedUnderMsg[];
extern char far PoisonedMsg[];
extern char far DissolvedMsg[];
extern char far StolenMsg[];
extern char far BurnOutMsg[];
extern char far VineKilledMsg[];
extern char far TeleportMsg[];
extern char far CombatMsg[];
extern char far BreakOffMsg[];
extern char far OutOfRangeMsg[];
extern char far ArrestMsg[];
extern char far GotoJailMsg[];
/*356A:02D9*/extern char far DefendThyselfMsg[];/*"No ... Then defend thyself, rogue!" refusing arrest*/
extern char far StealMsg[];
extern char far NotOnShipMsg[];
/*356A:032D*/extern char far DarknessEngulfsMsg[];/*"An unending darkness engulfs thee..." avatar death*/
/*356A:0354*/extern char far KalLorMsg[];/*"A voice in the darkness intones, KAL LOR!"*/
/*356A:0382*/extern char far NoSacredQuestMsg[];/*"Thou art not upon a Sacred Quest! Passage denied!"*/
extern char far NoWindMsg[];
extern char far NotInBookMsg[];
extern char far BrokenMsg[];
extern char far RingVanishedMsg[];
extern char far NotInCombatMsg[];
/*356A:044A*/extern char far YellsForHelpMsg[];/*"`%s yells for help!"*/
/*356A:045F*/extern char far ShaminoThisIsMsg[];/*"Shamino says, This is the " (dungeon name follows)*/
/*356A:0493*/extern char far MustBePartyModeMsg[];/*"You must be in party mode to enter."*/
extern char far NotAllHereMsg[];
extern char far NotInSoloMsg[];
/*356A:047B*/extern char far StuckInWebMsg[];/*"`%s is stuck in a web!"*/
extern char far DateMsg[];
extern char far SoundOnMsg[];
extern char far SoundOffMsg[];
extern char far InsertDiskMsg[];
extern char far VersionMsg[];

#endif
