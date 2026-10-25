/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	far message strings (data segment 356A)

	Messages printed with CON_printf by the combat, movement, command
	and system modules (declared in msgs.h). A '`' makes the console
	capitalize the next character (e.g. the start of an inserted name).
	Comments like D_356A_0010 give a string's offset in the segment.
*/
#include "msgs.h"

/*
names found here in the original source but not used anywhere:
GuardianMsg
ParalyzedMsg
CantMoveMsg
UndergroundMsg
HelpGuardMsg
EnlightenMsg
AllDeadMsg
EnterDNGMsg*/

/*D_356A_0000*/char SlimeDividesMsg[] = "Slime divides!\n";
/*D_356A_0010*/char CriticalMsg[] = "critical!\n";
/*D_356A_001B*/char HeavilyMsg[] = "heavily";
/*D_356A_0023*/char LightlyMsg[] = "lightly";
/*D_356A_002B*/char BarelyMsg[] = "barely";
/*D_356A_0032*/char WoundedMsg[] = " wounded.\n";
/*D_356A_003D*/char GrazedMsg[] = "`%s grazed.\n";
/*D_356A_004A*/char KilledMsg[] = "`%s killed!\n";
/*D_356A_0057*/char ShipCannonMsg[] = "with ship cannons-";
/*D_356A_006A*/char BareHandsMsg[] = "with bare hands-";
/*D_356A_007B*/char WithMsg[] = "with %s-";
/*D_356A_0084*/char OutOfArrowsMsg[] = "Out of arrows!\n";
/*D_356A_0094*/char OutOfBoltsMsg[] = "Out of bolts!\n";
/*PassMsg[2]*/char AttackSelfPassMsg[] = "pass.\n";/*attacking oneself*/
/*D_356A_00AA*/char NothingMsg[] = "nothing!\n";
/*D_356A_00B4*/char AttackWithMsg[] = "\nAttack with %s-";
/*D_356A_00C5*/char AttackMsg[] = "Attack";
/*D_356A_00CC*/char WhatMsg[] = "what?\n";
/*D_356A_00D3*/char PartyModeMsg[] = "Party mode\n";
/*solo mode refused (member dead/asleep/paralyzed, dragged under or charmed)*//**/char FailsToRespondMsg[] = "`%s fails to respond.\n";
/*D_356A_00F6*/char SoloModeMsg[] = "Solo mode\n";
/*D_356A_0101*/char ExitToDOSMsg[] = "Exit to DOS?";
/*not referenced*//*__356A_010E*/char RestMsg[] = "Rest!\n";
/*D_356A_0115*/char SaveMsg[] = "Save game? ";
/*PassMsg[2]*/char PassMsg[] = "Pass!\n";/*"pass" command*/
/*ShaminoMsg[1]*/char ShaminoHearsMsg[] = "Shamino says, \"I hear something to the %s!\"\n";
/*D_356A_0155*/char GlassSwordMsg[] = "Thy sword hath shattered!\n";
/*D_356A_0170*/char InterferenceMsg[] = "`%s interferes!\n";
/*npc dragged under by a corpser*//**/char ArghMsg[] = "\n%s:\n%>ARGH!\n";
/*D_356A_018F*/char RegurgitatedMsg[] = " regurgitated!\n";
/*D_356A_019F*/char DraggedUnderMsg[] = "`%s dragged under!\n";
/*D_356A_01B3*/char PoisonedMsg[] = "`%s poisoned!\n";
/*D_356A_01C2*/char DissolvedMsg[] = "A slug dissolves %s's %s!\n";
/*D_356A_01DD*/char StolenMsg[] = "`%s has been robbed!\n";
/*D_356A_01F3*/char BurnOutMsg[] = "A torch burned out.\n";
/*D_356A_0208*/char VineKilledMsg[] = "Tangle vine killed!\n";
/*D_356A_021D*/char TeleportMsg[] = "Wisp teleports!\n";
/*D_356A_022E*/char CombatMsg[] = "Begin combat!\n";
/*D_356A_023D*/char BreakOffMsg[] = "Break off combat!\n";
/*D_356A_0250*/char OutOfRangeMsg[] = "Out of range!\n";
/*D_356A_025F*/char ArrestMsg[] = "\n\"Thou art under arrest!\"\n\n\"Wilt thou come quietly?\"\n\n:";
/*D_356A_0297*/char GotoJailMsg[] = "Yes\n\nThe guard strikes thee unconscious!\n\nThou dost awaken to...\n";
/*refusing the arrest*//**/char DefendThyselfMsg[] = "No\n\n\"Then defend thyself, rogue!\"\n";
/*D_356A_02FC*/char StealMsg[] = "The %s stole some gold!\n";
/*D_356A_0315*/char NotOnShipMsg[] = "Not while aboard ship!\n";
/*death of the avatar...*//**/char DarknessEngulfsMsg[] = "\nAn unending darkness engulfs thee...\n";
/*...resurrection by Lord British*//**/char KalLorMsg[] = "\nA voice in the darkness intones, \"KAL LOR!\"\n";
/*invisible barrier (OBJ_NOTHING) without the Sacred Quest*//**/char NoSacredQuestMsg[] = "\n\"Thou art not upon a Sacred Quest!\nPassage denied!\"\n";
/*D_356A_03B8*/char NoWindMsg[] = "Thou canst not move without wind!\n";
/*D_356A_03DB*/char NotInBookMsg[] = "\nThat spell is not in thy spellbook!\n";
/*D_356A_0401*/char BrokenMsg[] = "`%s broken!\n";
/*D_356A_040E*/char RingVanishedMsg[] = "A %s has vanished!\n";
/*D_356A_0422*/char NotInSoloMsg[] = "Not in solo mode!\n";
/*D_356A_0435*/char NotInCombatMsg[] = "Not in combat mode!\n";
/*a neutral human was killed*//**/char YellsForHelpMsg[] = "`%s yells for help!\n";
/*Shamino names the dungeon*//*ShaminoMsg[1]*/char ShaminoThisIsMsg[] = "Shamino says, \"This is the ";
/*web tile*//**/char StuckInWebMsg[] = "`%s is stuck in a web!\n";
/*moongate, red gate or dungeon entered in solo mode*//**/char MustBePartyModeMsg[] = "You must be in party mode to enter.\n";
/*D_356A_04B8*/char NotAllHereMsg[] = "Not everyone is here.\n";
/*D_356A_04CF*/char DateMsg[] = "%2d-%02d-%04d Wind:%2s\n";
/*D_356A_04E7*/char SoundOnMsg[] = "Sound on\n";
/*D_356A_04F1*/char SoundOffMsg[] = "Sound off\n";
/*D_356A_04FC*/char InsertDiskMsg[] = "\n\nPlease insert the %s disk.";
/*D_356A_0519*/char VersionMsg[] = "Version 4.5\n";

/*padding:*/
/*__356A_0526	db 0Ah dup(0)*/
