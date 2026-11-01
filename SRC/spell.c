/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	module spell/magic

	Everything about spells: the spell tables (incantation letters
	SpellCode, words of power MagicWords, SpellName, reagents), the spellbook
	display in the status area (SPELL_RefreshList builds the SpellList
	bitmasks from the readied spellbook(s), SPELL_RefreshPage draws one
	page of up to 8 spells with their castable counts), the "Cast" command
	(CMD_CastSpell -> SPELL_CastFromBook: choose spell, check level/magic/reagents)
	and the effect of each spell (dispatcher SPELL_Execute, also used by
	monsters and wands/staves).
	Spell numbers are circle * 0x10 + index (see spells.h). The outcome of
	a spell is reported in SpellResult: 0 = "Success", 1 = "Failed"
	(resisted), 2 = "No effect", -1 = done without message, -2 = done,
	message already printed.
*/
#include "u6.h"

#undef abs
#include <math.h>

/*incantation of each spell: one letter per word of power
  ('A' = "An" ... 'Z' = "Zu", see MagicWords); indexed by spell number*/
/*0AF0*/unsigned char *SpellCode[] = {
	/*1st circle*/
	/*0EF8*/"IMY",
	/*0EFC*/"WO",
	/*0EFF*/"WJ",
	/*0F02*/"AJO",
	/*0F06*/"AF",
	/*0F09*/"AM",
	/*0F0C*/"IM",
	/*0F0F*/"KL",
	/*0F12*/"IF",
	/*0F15*/"IL",
	"","","","","","",
	/*2nd circle*/
	/*0F18*/"QL",
	/*0F1B*/"OJ",
	/*0F1E*/"INP",
	/*0F22*/"IY",
	/*0F25*/"IZ",
	/*0F28*/"OPY",
	/*0F2C*/"IJ",
	/*0F2F*/"EP",
	/*0F32*/"AJ",
	/*0F35*/"AY",
	"","","","","","",
	/*3rd circle*/
	/*0F38*/"AS",
	/*0F3B*/"AG",
	/*0F3E*/"PF",
	/*0F41*/"VL",
	/*0F44*/"AP",
	/*0F47*/"AVZ",
	/*0F47+1*/"VZ",
	/*0F4B*/"VWY",
	/*0F4F*/"IS",
	/*0F52*/"AXC",
	"","","","","","",
	/*4th circle*/
	/*0F56*/"OY",
	/*0F59*/"KX",
	/*0F5C*/"AVM",
	/*0F60*/"IFG",
	/*0F5C+1*/"VM",
	/*0F64*/"IW",
	/*0F67*/"VAJO",
	/*0F6C*/"ING",
	/*0F70*/"IZG",
	/*0F74*/"RH",
	"","","","","","",
	/*5th circle*/
	/*0F77*/"ISG",
	/*0F7B*/"VPF",
	/*0F7F*/"KBX",
	/*0F83*/"SL",
	/*0F86*/"OG",
	/*0F89*/"AXP",
	/*0F28+1*/"PY",
	/*0F8D*/"ASL",
	/*0F91*/"KMC",
	/*0F4B+1*/"WY",
	"","","","","","",
	/*6th circle*/
	/*0F95*/"AXE",
	/*0F99*/"IQX",
	/*0F9D*/"VQ",
	/*0FA0*/"FH",
	/*0FA3*/"KDY",
	/*0FA7*/"VIS",
	/*0FAB*/"AO",
	/*0FAE*/"NH",
	/*0FB1*/"IQY",
	/*0FB5*/"IDP",
	"","","","","","",
	/*7th circle*/
	/*0FB9*/"VOG",
	/*0FBD*/"IOY",
	/*0FC1*/"GH",
	/*0FC4*/"QC",
	/*0FC7*/"VRP",
	/*0FCB*/"IC",
	/*0FCE*/"VAS",
	/*0FD2*/"VSL",
	/*0FD6*/"KOX",
	/*0FDA*/"POW",
	"","","","","","",
	/*8th circle*/
	/*0FDE*/"VCBM",
	/*0FE3*/"CH",
	/*0FE6*/"VAL",
	/*0FEA*/"VAXE",
	/*0FEF*/"VC",
	/*0FF2*/"IMC",
	/*0FF6*/"VRX",
	/*0FFA*/"KXC",
	/*0FFE*/"AT",
	/*1001*/"VPY",
	"","","","","",""
};

/*Words of power, indexed by letter - 'A'*/
/*0BF0*/unsigned char *MagicWords[] = {
	/*1005*/"An",
	/*1008*/"Bet",
	/*100C*/"Corp",
	/*1011*/"Des",
	/*1015*/"Ex",
	/*1018*/"Flam",
	/*101D*/"Grav",
	/*1022*/"Hur",
	/*1026*/"In",
	/*1029*/"Jux",
	/*102D*/"Kal",
	/*1031*/"Lor",
	/*1035*/"Mani",
	/*103A*/"Nox",
	/*103E*/"Ort",
	/*1042*/"Por",
	/*1046*/"Quas",
	/*104B*/"Rel",
	/*104F*/"Sanct",
	/*1055*/"Tym",
	/*1059*/"Uus",
	/*105D*/"Vas",
	/*1061*/"Wis",
	/*1065*/"Xen",
	/*1069*/"Ylem",
	/*106E*/"Zu"
};

/*0C24*/unsigned char *SpellName[] = {
	/*1st circle*/
	/*1071*/"Create Food",
	/*107D*/"Detect Magic",
	/*108A*/"Detect Trap",
	/*1096*/"Dispel Magic",
	/*10A3*/"Douse",
	/*10A9*/"Harm",
	/*10AE*/"Heal",
	/*10B3*/"Help",
	/*10B8*/"Ignite",
	/*10BF*/"Light",
	"","","","","","",
	/*2nd circle*/
	/*10C5*/"Infravision",
	/*10D1*/"Magic Arrow",
	/*10DD*/"Poison",
	/*10E4*/"Reappear",
	/*10ED*/"Sleep",
	/*10F3*/"Telekinesis",
	/*108A+6*/"Trap",
	/*10FF*/"Unlock Magic",
	/*110C*/"Untrap",
	/*1113*/"Vanish",
	"","","","","","",
	/*3rd circle*/
	/*111A*/"Curse",
	/*1120*/"Dispel Field",
	/*112D*/"Fireball",
	/*1136*/"Great Light",
	/*1142*/"Lock",
	/*1147*/"Mass Awaken",
	/*1153*/"Mass Sleep",
	/*115E*/"Peer",
	/*1163*/"Protection",
	/*116E*/"Repel Undead",
	"","","","","","",
	/*4th circle*/
	/*117B*/"Animate",
	/*1183*/"Conjure",
	/*118B*/"Disable",
	/*1193*/"Fire Field",
	/*119E*/"Great Heal",
	/*11A9*/"Locate",
	/*11B0*/"Mass Dispel",
	/*11BC*/"Poison Field",
	/*11C9*/"Sleep Field",
	/*11D5*/"Wind Change",
	"","","","","","",
	/*5th circle*/
	/*11E1*/"Energy Field",
	/*11EE*/"Explosion",
	/*11F8*/"Insect Swarm",
	/*1205*/"Invisibility",
	/*1212*/"Lightning",
	/*121C*/"Paralyze",
	/*1225*/"Pickpocket",
	/*1230*/"Reveal",
	/*1237*/"Seance",
	/*123E*/"X-ray",
	"","","","","","",
	/*6th circle*/
	/*1244*/"Charm",
	/*124A*/"Clone",
	/*1250*/"Confuse",
	/*1258*/"Flame Wind",
	/*1263*/"Hail Storm",
	/*126E*/"Mass Protect",
	/*127B*/"Negate Magic",
	/*1288*/"Poison Wind",
	/*1294*/"Replicate",
	/*129E*/"Web",
	"","","","","","",
	/*7th circle*/
	/*12A2*/"Chain Bolt",
	/*12AD*/"Enchant",
	/*12B5*/"Energy Wind",
	/*12C1*/"Fear",
	/*12C6*/"Gate Travel",
	/*12D2*/"Kill",
	/*12D7*/"Mass Curse",
	/*12E2*/"Mass Invis",
	/*12ED*/"Wing Strike",
	/*12F9*/"Wizard Eye",
	"","","","","","",
	/*8th circle*/
	/*1304*/"Armageddon",
	/*130F*/"Death Wind",
	/*131A*/"Eclipse",
	/*1322*/"Mass Charm",
	/*132D*/"Mass Kill",
	/*1337*/"Resurrect",
	/*1341*/"Slime",
	/*1347*/"Summon",
	/*134E*/"Time Stop",
	/*1358*/"Tremor",
	"","","","","","",
};

/*0D24*/char *Reagents_name[] = {
	/*135F*/"mandrake root",
	/*136D*/"nightshade",
	/*1378*/"black pearl",
	/*1384*/"blood moss",
	/*138F*/"spider silk",
	/*139B*/"garlic",
	/*13A2*/"ginseng",
	/*13AA*/"sulfurous ash"
};

/*object type of each reagent (bit order of REAGENT_xx)*/
/*0D34*/int16 ReagType[] = {OBJ_MANDRAKE,OBJ_NIGHTSHADE,OBJ_BLACK_PEARL,OBJ_BLOOD_MOSS,OBJ_SPIDERSILK,OBJ_GARLIC,OBJ_GINSENG,OBJ_ASH};

/*reagents mask (REAGENT_xx) of each spell, indexed by spell number*/
/*0D44*/unsigned char Reagents_needed[] = {
	/*1st circle*/
	REAGENT_GS|REAGENT_GA|REAGENT_MR,
	REAGENT_SA|REAGENT_NS,
	REAGENT_SA|REAGENT_NS,
	REAGENT_GS|REAGENT_GA,
	REAGENT_GA|REAGENT_BP,
	REAGENT_SS|REAGENT_NS,
	REAGENT_GS|REAGENT_SS,
	0,
	REAGENT_SA|REAGENT_BP,
	REAGENT_SA,
	0,0,0,0,0,0,
	/*2nd circle*/
	REAGENT_SA|REAGENT_NS,
	REAGENT_SA|REAGENT_BP,
	REAGENT_BM|REAGENT_BP|REAGENT_NS,
	REAGENT_SS|REAGENT_BM|REAGENT_BP,
	REAGENT_SS|REAGENT_BP|REAGENT_NS,
	REAGENT_BM|REAGENT_BP|REAGENT_MR,
	REAGENT_SS|REAGENT_NS,
	REAGENT_SA|REAGENT_BM,
	REAGENT_SA|REAGENT_BM,
	REAGENT_GA|REAGENT_BM|REAGENT_BP,
	0,0,0,0,0,0,
	/*3rd circle*/
	REAGENT_SA|REAGENT_GA|REAGENT_NS,
	REAGENT_SA|REAGENT_BP,
	REAGENT_SA|REAGENT_BP,
	REAGENT_SA|REAGENT_MR,
	REAGENT_SA|REAGENT_GA|REAGENT_BM,
	REAGENT_GS|REAGENT_GA,
	REAGENT_GS|REAGENT_SS|REAGENT_NS,
	REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_GS|REAGENT_GA,
	REAGENT_SA|REAGENT_GA,
	0,0,0,0,0,0,
	/*4th circle*/
	REAGENT_SA|REAGENT_BM|REAGENT_MR,
	REAGENT_SS|REAGENT_MR,
	REAGENT_SS|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BP,
	REAGENT_GS|REAGENT_SS|REAGENT_MR,
	REAGENT_NS,
	REAGENT_SS|REAGENT_BP|REAGENT_NS,
	REAGENT_GS|REAGENT_SS|REAGENT_BP,
	REAGENT_GS|REAGENT_GA,
	REAGENT_SA|REAGENT_BM,
	0,0,0,0,0,0,
	/*5th circle*/
	REAGENT_SS|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_BM|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BM,
	REAGENT_BM|REAGENT_NS,
	REAGENT_SA|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BP|REAGENT_NS,
	REAGENT_SS|REAGENT_BM|REAGENT_NS,
	REAGENT_SS|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_MR,
	0,0,0,0,0,0,
	/*6th circle*/
	REAGENT_SS|REAGENT_BP|REAGENT_NS,
	REAGENT_SA|REAGENT_GS|REAGENT_SS|REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_BM|REAGENT_MR,
	REAGENT_BM|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_GS|REAGENT_GA|REAGENT_MR,
	REAGENT_SA|REAGENT_GA|REAGENT_MR,
	REAGENT_SA|REAGENT_BM|REAGENT_NS,
	REAGENT_SA|REAGENT_GS|REAGENT_SS|REAGENT_BM|REAGENT_NS,
	REAGENT_SS,
	0,0,0,0,0,0,
	/*7th circle*/
	REAGENT_SA|REAGENT_BM|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_MR,
	REAGENT_SA|REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_GA|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_BP|REAGENT_MR,
	REAGENT_SA|REAGENT_BP|REAGENT_NS,
	REAGENT_SA|REAGENT_GA|REAGENT_NS|REAGENT_MR,
	REAGENT_BM|REAGENT_BP|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BM|REAGENT_MR,
	REAGENT_SA|REAGENT_SS|REAGENT_BM|REAGENT_BP|REAGENT_NS|REAGENT_MR,
	0,0,0,0,0,0,
	/*8th circle*/
	0,
	REAGENT_SA|REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_GA|REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_SS|REAGENT_BP|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_BP|REAGENT_NS|REAGENT_MR,
	REAGENT_SA|REAGENT_GS|REAGENT_GA|REAGENT_SS|REAGENT_BM|REAGENT_MR,
	REAGENT_BM|REAGENT_NS|REAGENT_MR,
	REAGENT_GA|REAGENT_SS|REAGENT_BM|REAGENT_MR,
	REAGENT_GA|REAGENT_BM|REAGENT_MR,
	REAGENT_SA|REAGENT_BM|REAGENT_MR,
	0,0,0,0,0,0
};

/*circle names (spellbook page footer "xTH level")*/
/*0DC4*/char *CircleNames[] = {
	/*13B8*/"1ST",
	/*13BC*/"2ND",
	/*13C0*/"3RD",
	/*13C4*/"4TH",
	/*13C8*/"5TH",
	/*13CC*/"6TH",
	/*13D0*/"7TH",
	/*13D4*/"8TH"
};

/*alignment names, indexed by alignment >> 5 (NEUTRAL/EVIL/GOOD/CHAOTIC)
  (unused here)*/
char *AlignmentNames[] = {
	/*13D8*/"neutral",
	/*13E0*/"evil",
	/*13E5*/"good",
	/*13EA*/"chaotic"
};

/*spell messages (also used by status.c and objcmd.c):
  0 success, 1 failed, 2 no effect, 3 out of range, 4 blocked, 5 nothing,
  6 not possible, 7 done, 8 "On what" prompt, 9 "On whom" prompt,
  10 "Location" prompt, 11 not usable, 12 amulet message*/
/*0DDC*/char *SpellMessages[] = {
	/*13F2*/"Success",
	/*13FA*/"Failed",
	/*1401*/"No effect",
	/*140B*/"Out of range",
	/*1418*/"Blocked",
	/*1420*/"nothing",
	/*1428*/"Not possible",
	/*1435*/"Done",
	/*143A*/"On what: ",
	/*1444*/"On whom: ",
	/*144E*/"Location: ",
	/*1459*/"Not usable",
	/*1464*/"Magical energy prevents you from removing the amulet."
};

/*tiles of the magical objects (for "detect magic", see IsMagical)*/
/*0DF6*/int16 MagicalTiles[] = {
	TIL_MAGIC_HELM,TIL_MAGIC_SHIELD,TIL_MAGIC_ARMOR,TIL_GLASS_SWORD,TIL_FORCE_FIELD,TIL_MAGIC_BOW,TIL_SPELLBOOK,TIL_SPELL,TIL_VORTEX_CUBE,TIL_MOONSTONE_0,
	TIL_MOONSTONE_1,TIL_MOONSTONE_2,TIL_MOONSTONE_3,TIL_MOONSTONE_4,TIL_MOONSTONE_5,TIL_MOONSTONE_6,TIL_MOONSTONE_7,TIL_SNAKE_AMULET,TIL_AMULET_OF_SUBMISSION,TIL_GEM,
	TIL_LIGHTNING_WAND,TIL_FIRE_WAND,TIL_STORM_CLOAK,TIL_RED_GATE_LEFT,TIL_RED_GATE_RIGHT,TIL_MOONGATE_LEFT,TIL_MOONGATE_RIGHT,TIL_ORB_OF_THE_MOONS,TIL_CHEST_MAGIC_LOCKED,TIL_CRYSTAL_BALL,
	TIL_BRAZIER_2,TIL_BRAZIER_3,TIL_FAN,TIL_RUNE_OF_HONESTY,TIL_RUNE_OF_COMPASSION,TIL_RUNE_OF_VALOR,TIL_RUNE_OF_JUSTICE,TIL_RUNE_OF_SACRIFICE,TIL_RUNE_OF_HONOR,TIL_RUNE_OF_SPIRITUALITY,
	TIL_RUNE_OF_HUMILITY,TIL_PROTECTION_RING,TIL_REGENERATION_RING,TIL_INVISIBILITY_RING,TIL_POTION_0,TIL_POTION_1,TIL_POTION_2,TIL_POTION_3,TIL_POTION_4,TIL_POTION_5,
	TIL_POTION_6,TIL_POTION_7,TIL_LIGHTSOURCE,TIL_HEATSOURCE,TIL_OAKEN_DOOR_MAGIC_S_L,
	TIL_OAKEN_DOOR_MAGIC_S_R,TIL_OAKEN_DOOR_MAGIC_E_U,TIL_OAKEN_DOOR_MAGIC_E_D,
	TIL_WINDOWED_DOOR_MAGIC_S_L,TIL_WINDOWED_DOOR_MAGIC_S_R,
	TIL_WINDOWED_DOOR_MAGIC_E_U,TIL_WINDOWED_DOOR_MAGIC_E_D,TIL_CEDAR_DOOR_MAGIC_S_L,
	TIL_CEDAR_DOOR_MAGIC_S_R,TIL_CEDAR_DOOR_MAGIC_E_U,TIL_CEDAR_DOOR_MAGIC_E_D,TIL_STEEL_DOOR_MAGIC_S_L,
	TIL_STEEL_DOOR_MAGIC_S_R,TIL_STEEL_DOOR_MAGIC_E_U,TIL_STEEL_DOOR_MAGIC_E_D,
	TIL_SILVER_HORN,TIL_SILVER_HORN_1,TIL_PROTECTION_FIELD,TIL_SLEEP_FIELD,TIL_BROKEN_LENS_0,TIL_LENS_0,TIL_BROKEN_LENS_1,TIL_LENS_1
};

/*returns 1 if the tile of object objNum is in the MagicalTiles list of magical
  tiles (78 entries), 0 otherwise ("detect magic")*/
/*C_1944_0006*/static IsMagical(int16 objNum) {
	int16 i, tileFrame;

	tileFrame = TILE_FRAME(objNum);
	for(i = 0; i < 78; i ++) {
		if(MagicalTiles[i] == tileFrame)
			return 1;
	}
	return 0;
}

/*returns 0 for creature types that can not be made invisible (giant
  scorpion and above, dragon, hydra, silver serpent, slime), 1 otherwise*/
/*C_1944_0054*/static SPELL_CanBecomeInvisible(int16 objTyp) {
	if(objTyp >= OBJ_GIANT_SCORPION || objTyp == OBJ_DRAGON || objTyp == OBJ_HYDRA || objTyp == OBJ_SILVER_SERPENT || objTyp == OBJ_SLIME)
		return 0;
	return 1;
}

/*parameters of the casting sound for circles 0..8 (see SPELL_CastEffect):
  OSI_playWavedNote(CastSoundPitch pitch, 1, length, CastSoundStartUp start, +CastSoundStep step)
  then (CastSoundPitch, 1, length, CastSoundStartDown start, -CastSoundStep step)*/
/*0E92*/int16 CastSoundPitch[] = {0x226A,0x1E96,0x1B94,0x1996,0x173E,0x15C2,0x143C,0x12D4,0x1180};
/*0EA4*/int16 CastSoundStartUp[] = { 2700, 3000, 1000,  100, 5000, 4000, 2500, 1000,    1};
/*0EB6*/int16 CastSoundStartDown[] = {32700,31000,37000,45000,31000,34000,36500,39000,42000};
/*0EC8*/int16 CastSoundStep[] = {    3,    2,    2,    2,    1,    1,    1,    1,    1};

/*spell casting effect: for circle 0..8 flash the map area and play a
  rising then falling sweep whose pitch depends on the circle
  (CastSoundPitch/CastSoundStartUp/CastSoundStartDown/CastSoundStep); circle >= 9 just plays a short sound*/
/*C_1944_0085*/SPELL_CastEffect(int16 circle) {
	int16 mouseState;

	mouseState = CON_mouseOffAt(0, 0, 311, 191);
	if(circle < 9) {
		OSI_playNoise(800, circle * 1600 + 8000, 700);
		GR_SET_COLOR(15);
		GrInfo->toBackBuffer = 1;
		GrInfo->xorMode = 1;
		DrawMapWindow(0);
		GR_FILL_RECT(8, 8, 168, 168);
		RedrawMapBorder(8, 8, 167, 167);
		GR_COPY_TO_SCREEN(8, 8, 167, 167, 8, 8);
		OSI_playWavedNote(CastSoundPitch[circle], 1, circle * 4000 + 10000, CastSoundStartUp[circle], CastSoundStep[circle]);
		OSI_playWavedNote(CastSoundPitch[circle], 1, circle * 4000 + 10000, CastSoundStartDown[circle], -CastSoundStep[circle]);
		GR_FILL_RECT(8, 8, 168, 168);
		RedrawMapBorder(8, 8, 167, 167);
		GR_COPY_TO_SCREEN(8, 8, 167, 167, 8, 8);
		GrInfo->xorMode = 0;
		GrInfo->toBackBuffer = 0;
	} else {
		OSI_playWavedNote(0x1180, 1, (int16)65000, 300, 1);
	}
	CON_mouseOn(mouseState);
}

/*resistance roll, strength vs. strength: returns non-zero if target
  resists caster ((Str(target)/2 + 30 - Str(caster))/2 > 1d30)
  (used by objcmd.c when moving/pushing an NPC)*/
/*C_1944_01EC*/SPELL_ResistStrVsStr(int16 caster, int16 target) {
	return ((GetStr(target) / 2) + 30 - GetStr(caster)) / 2 > OSI_rand(1, 30);
}

/*resistance roll, target strength vs. caster intelligence:
  returns non-zero if target resists ("poison")*/
/*C_1944_0232*/static SPELL_ResistStrVsInt(int16 caster, int16 target) {
	return ((GetStr(target) / 2) + 30 - GetInt(caster)) / 2 > OSI_rand(1, 30);
}

/*resistance roll, target dexterity vs. caster intelligence:
  returns non-zero if target dodges ("magic arrow")*/
/*C_1944_0278*/static SPELL_ResistDexVsInt(int16 caster, int16 target) {
	return ((GetDex(target) / 2) + 30 - GetInt(caster)) / 2 > OSI_rand(1, 30);
}

/*total quantity of objects of type objTyp and quality objQual carried
  by objNum (walks the Link chain after objNum until an object lying on
  the map is reached, so nested containers are counted too)*/
/*C_1944_02BE*/static SPELL_CountCarried(int16 objNum, int16 objTyp, int16 objQual) {
	int16 item, quantity;

	quantity = 0;
	for(item = Link[objNum]; item >= 0 && GetCoordUse(item) != LOCXYZ; item = Link[item]) {
		if(GetType(item) == objTyp && GetQual(item) == objQual)
			quantity += GetQuan(item);
	}
	return quantity;
}

/*rebuild SpellList[] (one 10-bit mask per circle) from the spell objects
  (OBJ_SPELL, quality = spell number) inside the readied spellbook(s).
  A spell of quality 0xff means "all spells". Also sets SelectedSpell (current
  spell = quality of the spellbook) and SpellbookFirst (index of the first spell
  of the page that shows the current spell).
  (callers in console.c pass 2 arguments which are ignored)*/
/*C_1944_0321*/SPELL_RefreshList(/*int bp08, bp06*/) {
	int16 spellQual, i;
	int16 item, firstSpell, circle,  spellIdx, spellCount, curCircle, hasAllSpells;

	/*if(Spellbook[0]) */{
		firstSpell = 0;
		hasAllSpells = 0;
		for(i = 0; i < 8; i ++)
			SpellList[i] = 0;
		for(item = FindInv(Spellbook[0]); item >= 0; item = NextInv()) {
			spellQual = GetQual(item);
			if(spellQual == 0xff) {/*"all spells" scroll*/
				hasAllSpells = 1;
				break;
			}
			if(!HAS_SPELL(spellQual >> 4, spellQual & 0xf)) {
				if(firstSpell == 0)
					firstSpell = spellQual;
				SET_SPELL(spellQual >> 4, spellQual & 0xf);
			}
		}
		/*the book's quality is its selected spell: if it says "create food"
		  (0) but the book does not hold that spell, select the first one*/
		if(GetQual(Spellbook[0]) == 0 && FindInvType(Spellbook[0], OBJ_SPELL, 0) == -1)
			SetQual(Spellbook[0], firstSpell);
	}
	if(Spellbook[1]) {
		firstSpell = 0;

		for(item = FindInv(Spellbook[1]); item >= 0; item = NextInv()) {
			spellQual = GetQual(item);
			if(spellQual == 0xff) {/*"all spells" scroll*/
				hasAllSpells = 1;
				break;
			}
			if(!HAS_SPELL(spellQual >> 4, spellQual & 0xf)) {
				if(firstSpell == 0)
					firstSpell = spellQual;
				SET_SPELL(spellQual >> 4, spellQual & 0xf);
			}
		}
		if(GetQual(Spellbook[1]) == 0 && FindInvType(Spellbook[1], OBJ_SPELL, 0) == -1)
			SetQual(Spellbook[1], firstSpell);
	}
	if(hasAllSpells) {
		for(i = 0; i < 8; i ++)
			SpellList[i] = -1;
	}
	for(i = 0; i < 8; i ++)
		SpellList[i] &= 0x3ff;/*10 spells per circle*/
	if(Spellbook[0])
		SelectedSpell = GetQual(Spellbook[0]);
	else
		SelectedSpell = GetQual(Spellbook[1]);
	/*first page to show: the page of the current spell's circle that
	  holds the current spell (pages of 8 lines)*/
	SpellbookFirst = -1;
	spellCount = 0;
	curCircle = SelectedSpell / 0x10;
	for(circle = 0; circle <= curCircle; circle ++) {
		for(spellIdx = 0; spellIdx < 0x10; spellIdx ++) {
			if(HAS_SPELL(circle, spellIdx)) {
				if(circle == curCircle) {
					if((char)SpellbookFirst == -1)
						SpellbookFirst = spellCount;
					if((circle << 4) + spellIdx == SelectedSpell) {
						if(SpellbookFirst + 8 <= spellCount)
							SpellbookFirst += 8;
						break;
					}
				}
				spellCount ++;
			}
		}
	}
}

/*draw the empty spellbook parchment in the status area (176..311,6..103)
  (picture #1 of BitmapFiles, loaded from disk 6)*/
/*C_1944_05B1*/SPELL_DrawBG() {
	int16 mouseState;

	mouseState = CON_mouseOffAt(176, 6, 311, 103);
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(168, 6, 311, 103);
	DISK_confirm(DISK_POPULACE_A);
	IsZFile = 1;
	LoadFile(BitmapFiles[1], ScratchBuf);
	IsZFile = 0;
	GR_DRAW_PIXMAP(ScratchBuf, 168, 6);
	CON_mouseOn(mouseState);
}

/*computes how many times spellNum can be cast with the reagents carried
  by objNum (99 max; "help" and "armageddon" count as 1), stores it in
  SpellPageCounts[index] and prints it. Prints nothing for spells needing no
  reagents (other than help/armageddon)*/
/*C_1944_0639*/static PrintSpellQuantity(int16 objNum, int16 spellNum, int16 index) {
	int16 quantity;
	unsigned char reagentsMask;
	int16 reagent,numMax;

	numMax = 99;
	reagentsMask = Reagents_needed[spellNum];
	if(reagentsMask == 0 && spellNum != SPELL_HELP && spellNum != SPELL_ARMAGEDDON)
		return;
	for(reagent = 0; reagent < 8; reagent ++) {
		if((reagentsMask >> reagent) & 1) {
			quantity = SPELL_CountCarried(objNum, ReagType[reagent], 0);
			if(quantity < numMax)
				numMax = quantity;
		}
	}
	if(spellNum == SPELL_HELP || spellNum == SPELL_ARMAGEDDON)
		numMax = 1;
	SpellPageCounts[index] = numMax;
	CON_printf(/*149A*/"%d", numMax);
}

/*returns the line (0..7) of spellNum on the displayed spellbook page
  (SpellbookPage), or -1 if it is not on the page*/
/*C_1944_06CA*/SPELL_FindPageLine(int16 spellNum) {
	int16 i;

	for(i = 0; i < 8; i ++) {
		if(SpellbookPage[i] == spellNum)
			return i;
	}
	return -1;
}

/*remove one of each reagent needed by spellNum from objNum's inventory,
  and decrement the castable count SpellPageCounts[index] of that page line*/
static /*C_1944_06EF*/TakeReagents(int16 objNum, int16 spellNum, int16 index) {
	int16 item, itemType;
	unsigned char reagentsMask;
	int16 reagent;

	reagentsMask = Reagents_needed[spellNum];
	for(reagent = 0; reagent < 8; reagent ++) {
		if((reagentsMask >> reagent) & 1) {
			for(item = Link[objNum]; item >= 0; item = Link[item]) {
				if(ReagType[reagent] == (itemType = GetType(item))) {
					TakeObj(objNum, itemType, 1);
					break;
				}
			}
		}
	}
	if(spellNum != SPELL_HELP)
		SpellPageCounts[index] --;
}

/*redraw the spellbook page: skips the first SpellbookFirst known spells, then
  lists (up to 8) spells of the next circle with their castable counts.
  Fills SpellbookPage[] (spell of each line), SpellPageCounts[] (count | 0x80 for the
  current spell), SpellbookPageCount (number of lines) and the navigation state
  SpellPageNav[0..3] (SpellPageNav[3] is SpellNextCircle):
    [0] circle to go back to (previous circle button)
    [1] previous page button: 1 = previous circle, 2 = earlier page of
        the same circle
    [2] next page button: 1 = next circle, 2 = next page of same circle
    [3] first circle after the displayed one (next circle button)
  0xff = button disabled*/
/*C_1944_0773*/SPELL_RefreshPage() {
	int16 i, prevCircle;
	int16 line, col, row, caster;
	int16 spellNum, circle, spellIdx;
	int16 numSkipped, prevSpellIdx, pageCircle, firstSkippedCircle;

	prevCircle = -1;
	prevSpellIdx = -1;
	pageCircle = -1;
	firstSkippedCircle = -1;
	HideMouse
	GR_SET_COLOR(ColorPaper);
	GR_FILL_RECT(176, 14, 303, 79);
	CON_setClip(&TextWindows[1]);
	caster = Party[Active];
	for(i = 0; i < 4; i++)
		SpellPageNav[i] = -1;
	col = 1;
	row = 1;
	line = 0;
	numSkipped = 0;
	for(circle = 0; circle < 8; circle ++) {
		for(spellIdx = 0; spellIdx < 16; spellIdx ++) {
			if(!HAS_SPELL(circle, spellIdx))
				continue;
			if(SpellbookFirst > numSkipped) {
				/*before the displayed page*/
				numSkipped ++;
				if(firstSkippedCircle == -1)
					firstSkippedCircle = circle;
				prevCircle = circle;
				prevSpellIdx = spellIdx;
				continue;
			}
			if(pageCircle == -1)
				pageCircle = circle;
			if(line < 8 && circle == pageCircle) {
				/*on the displayed page*/
				if(circle == prevCircle && spellIdx > prevSpellIdx && line == 0)
					SpellPageNav[1] = 2;
				/*(quirk: gives prevCircle - 2 when [1] is 2)*/
				if(circle > firstSkippedCircle && line == 0)
					SpellPageNav[0] = prevCircle - ((SpellPageNav[1] == 0xff)?0:SpellPageNav[1]);
				if(SpellPageNav[0] != 0xff && SpellPageNav[1] == 0xff)
					SpellPageNav[1] = 1;
				spellNum = (circle << 4) + spellIdx;
				SpellbookPage[line] = spellNum;
				CON_gotoxy(col,      row); CON_printf(/*149D*/"%s", SpellName[spellNum]);
				CON_gotoxy(col + 13, row); PrintSpellQuantity(caster, spellNum, line);
				/*selection arrow (0x1a) in front of the current spell*/
				if(SelectedSpell == spellNum) {
					CON_gotoxy(col - 1, row);
					CON_putch0(0x1a);
					SpellPageCounts[line] |= 0x80;
				}
				prevCircle = circle;
				prevSpellIdx = spellIdx;
				line ++;
				row ++;
				continue;
			}
			/*after the displayed page*/
			if(SpellPageNav[2] == 0xff && circle == prevCircle && spellIdx > prevSpellIdx)
				SpellPageNav[2] = 2;
			if(SpellPageNav[2] == 0xff)
				SpellPageNav[2] = 1;
			if(SpellNextCircle == 0xff && circle > prevCircle)
				SpellNextCircle = circle;
		}
	}
	SpellbookPageCount = line;
	/*draw the previous/next page buttons at x = 192/208, y = 88*/
	for(i = 1; i < 3; i ++) {
		if(SpellPageNav[i] != 0xff)
			GR_PUT_TILE_SCREEN(TIL_SLOT_EMPTY + i, ((i << 1) + 22) << 3, 88);
		else
			GR_PUT_TILE_SCREEN(TIL_SLOT_EMPTY, ((i << 1) + 22) << 3, 88);
	}
	CON_gotoxy(11, 10); CON_printf(/*149D*/"%s", CircleNames[pageCircle]);
	CON_gotoxy(10, 11); CON_printf(/*14A0*/"level");
	CON_setClip(&TextWindows[3]);
	ShowMouse
}

/*returns the first object of type objType (and quality objQual, 0 = any)
  inside objNum (inventory or container), -1 if none*/
/*C_1944_0A43*/OBJ_FindInContainer(int16 objNum, int16 objType, int16 objQual) {
	int16 item;
	for(
		item = Link[objNum];
		item >= 0 && GetCoordUse(item) != LOCXYZ && OBJ_IsInside(item, objNum);
		item = Link[item]
	) {
		if(GetType(item) == objType) {
			if(objQual == 0 || GetQual(item) == objQual)
				return item;
		}
	}
	return -1;
}

/*door object types (see IsDoorType)*/
/*0EDA*/int16 DoorTypes[] = {OBJ_OAKEN_DOOR,OBJ_WINDOWED_DOOR,OBJ_CEDAR_DOOR,OBJ_STEEL_DOOR};

/*returns 1 if objType is one of the 4 door types (DoorTypes)*/
/*C_1944_0AA9*/IsDoorType(int16 objType) {
	int16 i;

	for(i = 0; i < 4; i ++) {
		if(DoorTypes[i] == objType)
			return 1;
	}
	return 0;
}

/*"sleep" on one target: returns 1 if resisted (intelligence test),
  2 if the creature class is immune to sleep, else puts target to sleep
  and returns -2 (SpellResult codes, see SPELL_CastMissile). Also used by
  combat.c for sleep-inducing attacks*/
/*C_1944_0AD0*/SPELL_SleepTarget(int16 caster, int16 target) {
	int16 objCls;

	objCls = GetMonsterClass(GetType(target));
	if(objCls < 0 || !IsMonsterImmuneSleep(objCls))  {
		if(COMBAT_ResistsSpell(caster, target))
			return 1;
		COMBAT_showHit(target);
		SetAsleep(target);
		SetTypeUnconscious(target);
		return (unsigned char)-2;
	}
	return 2;
}

/*"poison" on one target: returns 1 if resisted, 2 if immune, -2 when
  the target got poisoned*/
/*C_1944_0B32*/static SPELL_PoisonTarget(int16 caster, int16 target) {
	if(!IsMonsterImmunePoison(GetMonsterClass(GetType(target)))) {
		if(SPELL_ResistStrVsInt(caster, target))
			return 1;
		SetPoisoned(target);
		COMBAT_showHit(target);
		return (unsigned char)-2;
	}
	return 2;
}

/*"magic arrow": 1d10 damage to Selection.obj unless it dodges*/
/*C_1944_0B85*/static SPELL_MagicArrow(int16 caster) {
	int16 damage;
	int16 xp;

	if(SPELL_ResistDexVsInt(caster, Selection.obj)) {
		SpellResult = 1;
		return;
	}
	damage = OSI_rand(1, 10);
	xp = LoseHP(Selection.obj, damage);
	AddXP(caster, xp);
	COMBAT_ProvokeNeutral(Selection.obj, caster, damage);
	CON_printf(/*14A6*/"\n");
	COMBAT_displaySeverity(Selection.obj);
	SpellResult = -1;
}

/*direct damage on Selection.obj: "harm" 1d10, "energy wind" 1d30,
  "disable" leaves the target with 1 hit point*/
/*C_1944_0BF4*/static SPELL_DirectDamage(int16 spellNum) {
	int16 damage, caster;
	int16 xp;

	caster = Party[Active];
	if(COMBAT_ResistsSpell(caster, Selection.obj)) {
		SpellResult = 1;
		return;
	}
	if(spellNum == SPELL_HARM)
		damage = OSI_rand(1, 10);
	else if(spellNum == SPELL_ENERGY_WIND)
		damage = OSI_rand(1, 30);
	else if(spellNum == SPELL_DISABLE)
		damage = HitPoints[Selection.obj] - 1;
	CON_printf(/*14A6*/"\n");
	xp = LoseHP(Selection.obj, damage);
	AddXP(caster, xp);
	COMBAT_ProvokeNeutral(Selection.obj, caster, damage);
	COMBAT_displaySeverity(Selection.obj);
	SpellResult = -1;
}

/*"charm" on one target: makes it fight for the caster (the original
  alignment is saved with SetBKAlignment); casting it on an already
  charmed creature ends the charm instead. SpellResult = 1 if resisted*/
/*C_1944_0CA3*/static SPELL_CharmTarget(int16 caster, int16 target) {
	if(!COMBAT_ResistsSpell(caster, target)) {
		if(!IsCharmed(target)) {
			SetBKAlignment(target, GetAlignment(target));
			SetCharmed(target);
			SetAlignment(target, GetAlignment(caster));
			NPCMode[target] = AI_ASSAULT;
			COMBAT_showHit(target);
			CON_printf(/*14A6*/"\n");
			CON_printf((char *)GetObjectString(target));
			CON_printf(/*14A8*/" is charmed.\n");
			SpellResult = -2;
		} else if(IsCharmed(target)) {
			COMBAT_stopCharm(target);
		}
	} else {
		SpellResult = 1;
	}
}

/*"kill" on one target: returns 2 if dead already or immune to death
  spells, 1 if resisted, -2 when killed*/
/*C_1944_0D46*/static SPELL_KillTarget(int16 caster, int16 target) {
	int16 objCls;
	int16 xp;

	objCls = GetMonsterClass(GetType(target));
	if(!IsDead(target) && (objCls < 0 || !IsMonsterDeathImmune(objCls))) {
		if(!COMBAT_ResistsSpell(caster, target)) {
			xp = LoseHP(target, HitPoints[target]);
			AddXP(caster, xp);
			COMBAT_ProvokeNeutral(target, caster, 1);
			COMBAT_displaySeverity(target);
			return (unsigned char)-2;
		}
		return 1;
	}
	return 2;
}

/*"douse"/"ignite" objNum (torch, fireplace, candle, campfire, brazier,
  candelabra, powder keg). Bit 0 of the frame is the "lit" state.
  Igniting a powder keg starts its fuse (KegFuse/KegObject)*/
/*C_1944_0DCF*/static SPELL_DouseIgnite(int16 objNum, int16 spellNum) {
	int16 objType;
	int16 frame;

	objType = GetType(objNum);
	frame = GetFrame(objNum);
	if(
		(objType != OBJ_TORCH && objType != OBJ_FIREPLACE && objType != OBJ_CANDLE && objType != OBJ_CAMPFIRE && objType != OBJ_BRAZIER && objType != OBJ_CANDELABRA && objType != OBJ_POWDER_KEG) ||
		(objType == OBJ_BRAZIER && frame > 1) ||
		(spellNum == SPELL_DOUSE && (frame & 1) == 0) ||
		(spellNum == SPELL_IGNITE && (frame & 1))
	) {
		SpellResult = 2;
	} else if(objType == OBJ_POWDER_KEG && frame == 0) {
		SetFrame(objNum, 1);
		KegFuse = 3;/*fuse*/
		KegObject = objNum;
	} else if(frame & 1) {
		if(objType == OBJ_TORCH) {
			DeleteObj(objNum);
			CON_printf(/*14A6*/"\n");
			CON_printf(BurnOutMsg);
		} else {
			SetFrame(objNum, frame & 2);
			ClrLit(objNum);
		}
	} else {
		SetFrame(objNum, frame | 1);
		SetLit(objNum);
	}
	VIS_UpdateView();
}

/*"chain bolt": returns the nearest living NPC visible from fromNpc and
  within 5 tiles of the screen center (ties broken at random),
  -1 if none*/
/*C_1944_0F02*/static SPELL_FindChainTarget(int16 fromNpc) {
	int16 npc, best;
	int16 bestDist, fromX, fromY, x, y, dist;

	best = -1;
	bestDist = 0x7fff;
	fromX = GetX(fromNpc);
	fromY = GetY(fromNpc);
	for(npc = 0; npc < 0x100; npc ++) {
		if(ObjShapeType[npc] && !IsDead(npc) && npc != fromNpc && COMBAT_canSee(fromNpc, npc)) {
			x = GetX(npc);
			y = GetY(npc);
			if(x > MapX - 5 && x < MapX + 5 && y > MapY - 5 && y < MapY + 5) {
				dist = (x - fromX) * (x - fromX) + (y - fromY) * (y - fromY);
				if(dist > 0 && dist <= bestDist) {
					if(dist == bestDist) {
						best = OSI_Rand(0, 1)?best:npc;
					} else {
						bestDist = dist;
						best = npc;
					}
				}
			}
		}
	}
	return best;
}

/*"chain bolt": 1d30 damage to target, then jumps up to 6 times to the
  nearest other creature (1d30 damage each)*/
/*C_1944_1038*/static SPELL_ChainBolt(int16 target) {
	int16 nextTarget;
	int16 hop, reached, x, y, xp;

	xp = LoseHP(target, OSI_rand(1, 30));
	CON_printf(/*14A6*/"\n");
	AddXP(Party[Active], xp);
	COMBAT_ProvokeNeutral(target, Party[Active], 1);
	COMBAT_displaySeverity(target);
	for(hop = 0; hop < 6; hop ++) {
		nextTarget = SPELL_FindChainTarget(target);
		if(nextTarget == -1)
			break;
		x = GetX(nextTarget);
		y = GetY(nextTarget);
		reached = COMBAT_Missile(target, &x, &y, 8, TIL_MISSILE_0);
		if(reached == 0)
			break;
		target = nextTarget;
		xp = LoseHP(target, OSI_rand(1, 30));
		AddXP(Party[Active], xp);
		CON_printf(/*14A6*/"\n");
		COMBAT_displaySeverity(target);
	}
}

/*"heal" (+1d30 hit points, capped at max) / "great heal" (full hit
  points) on Selection.obj*/
/*C_1944_114D*/SPELL_DoHeal(int16 spellNum) {
	int16 healAmount;

	if(IsDead(Selection.obj)) {
		SpellResult = 2;
		return;
	}
	if(spellNum == SPELL_HEAL) {
		healAmount = OSI_rand(1, 30);
		if(MaxHP(Selection.obj) > HitPoints[Selection.obj] + healAmount) {
			HitPoints[Selection.obj] += healAmount;
			return;
		}
	} else if(spellNum != SPELL_GREAT_HEAL) {
		return;
	}
	HitPoints[Selection.obj] = MaxHP(Selection.obj);
}

/*"dispel magic" on one creature: removes sleep, poison, paralysis and
  charm. Returns 0 if something was dispelled, 1 otherwise*/
/*C_1944_11BB*/static SPELL_DispelTarget(int16 target) {
	int16 dispelled;

	dispelled = 0;
	MagicalHalo(target, 1);
	if(IsAsleep(target)) {
		ClrAsleep(target);
		ObjShapeType[target] = OrigShapeType[target];
		dispelled = 1;
	}
	if(IsPoisoned(target)) {
		ClrPoisoned(target);
		dispelled = 1;
	}
	if(IsParalyzed(target)) {
		ClrParalyzed(target);
		dispelled = 1;
	}
	if(IsCharmed(target)) {
		COMBAT_stopCharm(target);
		dispelled = 1;
	}
	if(dispelled)
		return 0;
	else
		return 1;
}

/*fire damage ("fireball", "explosion", "flame wind"): 1d20, doubled for
  creatures vulnerable to fire. Returns 0 if target is immune to fire*/
/*C_1944_1232*/static SPELL_FireDamage(int16 target) {
	int16 objCls;
	int16 xp;

	objCls = GetMonsterClass(GetType(target));
	if(objCls < 0 || !IsMonsterFireImmune(objCls)) {
		if(IsMonsterFireVulnerable(objCls))
			xp = LoseHP(target, OSI_rand(1, 20) << 1);
		else
			xp = LoseHP(target, OSI_rand(1, 20));
		AddXP(Party[Active], xp);
		CON_printf(/*14A6*/"\n");
		COMBAT_ProvokeNeutral(target, Party[Active], 1);
		COMBAT_displaySeverity(target);
		return 1;
	}
	return 0;
}

/*cast a spell that needs a target and travels as a missile:
    projTile  = missile tile, range = max distance
    objOrNpc  = 1: target must be an NPC ("On whom: "),
               -1: target must be an object ("On what: ")
    targetPreset = target already in Selection (do not prompt)
  SpellResult: 0 = "Success", 1 = "Failed", 2 = "No effect",
  -1 = done/no message, -2 = done, message already printed*/
/*C_1944_12FC*/SPELL_CastMissile(int16 spellNum, int16 projTile, int16 range, int16 objOrNpc, int16 targetPreset) {
	int16 newObj, caster;
	int16 targetType, hitOrNpc, tries, x, y, xp;
	struct coord xyz;

	caster = Party[Active];
	if(IsPlrControl(Party[Active])) {
		if(objOrNpc == 1)
			CON_printf(/*149D*/"%s", SpellMessages[9]);
		else
			CON_printf(/*149D*/"%s", SpellMessages[8]);
	}
	if(!targetPreset) {
		MouseMode = 1;
		if(SelectMode != 0)
			SelectRange = range;
		CON_getch();
	}
	if(Selection.obj == -1 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	MagicalHalo(Party[Active], 0);
	if(Selection.x != -1 && Selection.y != -1)
		hitOrNpc = COMBAT_Missile(caster, &Selection.x, &Selection.y, range, projTile);
	if(Selection.obj == -1 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(hitOrNpc) {
		if(
			(objOrNpc == 1 && Selection.obj >= 0x100) ||
			(objOrNpc == -1 && Selection.obj < 0x100 && spellNum != SPELL_DISPEL_FIELD)
		) {
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
			MUS_PlaySfx(13, 0);
			return;
		}
		Selection.obj = COMBAT_getHead(Selection.obj);
		targetType = GetType(Selection.obj);
		SpellResult = 0;
		switch(spellNum) {
			case SPELL_DISPEL_MAGIC:/*"dispel magic"*/
				if(SPELL_DispelTarget(Selection.obj))
					SpellResult = 2;
			break;
			case SPELL_HARM:/*"harm"*/
				SPELL_DirectDamage(spellNum);
			break;
			case SPELL_HEAL:/*"heal"*/
			case SPELL_GREAT_HEAL:/*"great heal"*/
				MagicalHalo(Selection.obj, 1);
				SPELL_DoHeal(spellNum);
			break;
			case SPELL_DOUSE:/*"douse"*/
			case SPELL_IGNITE:/*"ignite"*/
				SPELL_DouseIgnite(Selection.obj, spellNum);
			break;
			case SPELL_VANISH:/*"vanish"*/
				if(IsTileGe(TILE_FRAME(Selection.obj))) {/*gettable objects only*/
					VanishedShape = ObjShapeType[Selection.obj];/*remembered for "reappear"*/
					DeleteObj(Selection.obj);
					ScreenFade = 1;
					VIS_UpdateView();
					DisplayTick();
				} else {
					SpellResult = 2;
				}
			break;
			case SPELL_SLEEP:/*"sleep"*/
				SpellResult = SPELL_SleepTarget(caster, Selection.obj);
			break;
			case SPELL_MAGIC_ARROW:/*"magic arrow"*/
				SPELL_MagicArrow(caster);
			break;
			case SPELL_LIGHTNING:/*"lightning"*/
				xp = LoseHP(Selection.obj, OSI_rand(1, 30));
				AddXP(Party[Active], xp);
				CON_printf(/*14A6*/"\n");
				COMBAT_displaySeverity(Selection.obj);
				COMBAT_ProvokeNeutral(Selection.obj, Party[Active], 1);
				SpellResult = -1;
			break;
			case SPELL_FIREBALL:/*"fireball"*/
				if(SPELL_FireDamage(Selection.obj))
					SpellResult = -1;
				else
					SpellResult = 2;
			break;
			case SPELL_CHAIN_BOLT:/*"chain bolt"*/
				SPELL_ChainBolt(Selection.obj);
				SpellResult = -1;
			break;
			case SPELL_POISON:/*"poison"*/
				SpellResult = SPELL_PoisonTarget(caster, Selection.obj);
			break;
			case SPELL_CHARM:/*"charm"*/
				SPELL_CharmTarget(caster, Selection.obj);
			break;
			case SPELL_PARALYZE:/*"paralyze"*/
				SetParalyzed(Selection.obj);
				COMBAT_showHit(Selection.obj);
				CON_printf(/*14A6*/"\n");
				CON_printf((char *)GetObjectString(Selection.obj));
				CON_printf(/*14BF*/" is paralyzed.\n");
			break;
			case SPELL_CURSE:/*"curse"*/
				SetCursed(Selection.obj);
			break;
			case SPELL_PROTECTION:/*"protection"*/
				SetProtected(Selection.obj);
			break;
			case SPELL_INVISIBILITY:/*"invisibility"*/
				if(!IsInvisible(Selection.obj) && SPELL_CanBecomeInvisible(targetType)) {
					SetInvisible(Selection.obj);
					MagicalHalo(Selection.obj, 1);
				} else {
					SpellResult = 2;
				}
			break;
			case SPELL_KILL:/*"kill"*/
				SpellResult = SPELL_KillTarget(caster, Selection.obj);
			break;
			case SPELL_ANIMATE:/*"animate"*/
				if(STAT_GetEquipSlot(Selection.obj) != -1) {
					hitOrNpc = EGG_generate(targetType, GetX(Selection.obj), GetY(Selection.obj), MapZ);
					SetAlignment(hitOrNpc, GetAlignment(caster));
					NPCMode[hitOrNpc] = AI_ASSAULT;
					DeleteObj(Selection.obj);
				} else {
					SpellResult = 2;
				}
			break;
			case SPELL_REPLICATE:/*"replicate"*/
				if(IsTileGe(TILE_FRAME(Selection.obj))) {
					for(tries = 0; tries < 8; tries ++) {
						x = OSI_rand(0, 10) + MapX - 5;
						y = OSI_rand(0, 10) + MapY - 5;
						SetCoordXYZ(xyz, x, y, MapZ);
						newObj = AddObj(0, xyz, GetType(Selection.obj), GetFrame(Selection.obj), 1);
						SetOkToGet(newObj);
						if(CanMoveTo(newObj, x, y)) {
							ScreenFade = 1;
							VIS_UpdateView();
							DisplayTick();
							break;
						}
						DeleteObj(newObj);
					}
					if(tries == 8)
						SpellResult = 1;
				} else {
					SpellResult = 1;
				}
			break;
			case SPELL_CLONE:/*"clone"*/
				for(tries = 0; tries < 8; tries ++) {
					x = OSI_rand(0, 10) + MapX - 5;
					y = OSI_rand(0, 10) + MapY - 5;
					if(CanMoveTo(Selection.obj, x, y))
						break;
				}
				if(tries == 8) {
					SpellResult = 2;
					break;
				}
				hitOrNpc = EGG_generate(GetType(Selection.obj), x, y, MapZ);
				SetAlignment(hitOrNpc, GetAlignment(Selection.obj));
				NPCMode[hitOrNpc] = NPCMode[Selection.obj];
				ScreenFade = 1;
				VIS_UpdateView();
				DisplayTick();
			break;
			case SPELL_DISPEL_FIELD:/*"dispel field"*/
				for(newObj = FindLoc(Selection.x, Selection.y, MapZ); newObj >= 0; newObj = NextLoc()) {
					targetType = GetType(newObj);
					if(targetType == OBJ_FIRE_FIELD || targetType == OBJ_POISON_FIELD || targetType == OBJ_PROTECTION_FIELD || targetType == OBJ_SLEEP_FIELD) {
						DeleteObj(newObj);
						COMBAT_showHit(newObj);
						VIS_UpdateView();
						break;
					}
				}
				if(newObj == -1)
					SpellResult = 2;
			break;
		}
		if(SpellResult != 0 && SpellResult != -1 && SpellResult != -2) {
			if(SpellResult == 1) {
				CON_printf(/*14BA*/"\n%s\n", SpellMessages[1]);
				MUS_PlaySfx(13, 0);
			} else if(SpellResult == 2) {
/*C_1984:*/
				CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
				MUS_PlaySfx(13, 0);
			} else {
				MUS_PlaySfx(13, 0);
			}
		} else if(SpellResult == 0) {
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
			MUS_PlaySfx(14, 0);
		}
		return;
	}
/*19c2*/
	if(!CLOSE_ENOUGH(range, Selection.x, Selection.y, GetX(caster), GetY(caster)))
		CON_printf(/*14CF*/"\n%s!\n", SpellMessages[3]);
	else
		CON_printf(/*14CF*/"\n%s!\n", SpellMessages[4]);
	MUS_PlaySfx(13, 0);
}

/*bring the NPC whose dead body is corpse back to life with 1 hit point
  and full magic; experience is reduced to KARMA% (if KARMA < 98).
  The NPC is placed on the first free tile in growing squares around
  the body. Party members rejoin the party.
  (also used by map.c and talkdr.c)*/
/*C_1944_1A42*/SPELL_ReviveNPC(int16 corpse) {
	int16 npc, radius;
	int16 placed, step, new_x, new_y;

	placed = 0;
	npc = OBJ_ReviveFromBody(corpse);
	ClrDead(npc);
	HitPoints[npc] = 1;
	ObjShapeType[npc] = OrigShapeType[npc];
	MAGIC[npc] = MaxMagic(npc);
	ClrAilmentsProtected(npc);
	if(KARMA < 98)/*lose experience according to karma*/
		ExpPoints[npc] = ((int32)ExpPoints[npc] * KARMA) / 100L;
	for(radius = 1; radius < 5; radius ++) {
		for(step = 0; step < (radius << 1) + 1; step ++) {
			new_x = GetX(npc) - radius + step;
			new_y = GetY(npc) - radius;
			if(CanMoveTo(npc, new_x, new_y)) {
				MoveObj(npc, new_x, new_y, MapZ);
				placed = 1;
				break;
			}
			new_x = GetX(npc) - radius;
			new_y = GetY(npc) - radius + step;
			if(CanMoveTo(npc, new_x, new_y)) {
				MoveObj(npc, new_x, new_y, MapZ);
				placed = 1;
				break;
			}
			new_x = GetX(npc) + radius - step;
			new_y = GetY(npc) + radius;
			if(CanMoveTo(npc, new_x, new_y)) {
				MoveObj(npc, new_x, new_y, MapZ);
				placed = 1;
				break;
			}
			new_x = GetX(npc) + radius;
			new_y = GetY(npc) + radius - step;
			if(CanMoveTo(npc, new_x, new_y)) {
				MoveObj(npc, new_x, new_y, MapZ);
				placed = 1;
				break;
			}
		}
		if(placed)
			break;
	}
	if(!placed)
		MoveObj(npc, GetX(npc), GetY(npc), MapZ);
	if(IsPlrControl(npc))
		JoinParty(npc);
}

/*"resurrect": target must be a dead body (quality = NPC number, 0 means
  the corpse is too old, >= 0xe0 = temporary NPC: no effect)*/
/*C_1944_1C51*/static SPELL_DoResurrect(int16 targetPreset) {
	int16 corpseType, savedStatus;

	CON_printf(/*149D*/"%s", SpellMessages[9]);
	if(!targetPreset) {
		savedStatus = StatusDisplay;
		StatusDisplay = CMD_STAT_EQUIP;
		STAT_ShowInventory(Party[Active]);
		MouseMode = 1;
		CON_getch();
		StatusDisplay = savedStatus;
	}
	if(Selection.obj == -1) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	MagicalHalo(Party[Active], 0);
	if(Selection.obj == -1 || IsArmageddon) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	MagicalHalo(Selection.obj, 1);
	corpseType = GetType(Selection.obj);
	if(
		(corpseType != OBJ_DEAD_BODY && corpseType != OBJ_MOUSE && corpseType != OBJ_DEAD_GARGOYLE && corpseType != OBJ_DEAD_CYCLOPS) ||
		GetQual(Selection.obj) >= 0xe0
	) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(GetQual(Selection.obj) == 0) {
		CON_printf(/*14D5*/"\nThe corpse is too old!\n");
		return;
	}
	SPELL_ReviveNPC(Selection.obj);
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
	MUS_PlaySfx(14, 0);
	SpellResult = 0;
}

/*uncalled: returns 0 for special creature types (ghost, wisp, big
  monsters, dragon, hydra, tangle vine, silver serpent, Lord British)*/
/*__1944_1DAB*/static SPELL_IsAffectableType(int16 objTyp) {
	if(
		objTyp == OBJ_GHOST ||
		objTyp == OBJ_WISP ||
		objTyp >= OBJ_GIANT_SCORPION ||
		objTyp == OBJ_DRAGON ||
		objTyp == OBJ_HYDRA_BODY ||
		objTyp == OBJ_HYDRA ||
		objTyp == OBJ_TANGLE_VINE_VINE ||
		objTyp == OBJ_SILVER_SERPENT ||
		objTyp == OBJ_LORD_BRITISH
	)
		return 0;
	else
		return 1;
}

/*potion descriptions for "detect magic", indexed by potion frame*/
/*0EE2*/char *PotionNames[] = {
	/*14EE*/"an awaken",
	/*14F8*/"a cure",
	/*14FF*/"a heal",
	/*1506*/"a poison",
	/*150F*/"a sleep",
	/*1517*/"a protection",
	/*1524*/"an invisibility",
	/*1534*/"an xray vision"
};

/*"wing strike": horizontal direction of the dragon, and the frames of
  its 2 objects, indexed by direction (0: from the left, 1: from the right)*/
/*0EF2*/char WingStrikeDir[] = {1, -1};
/*0EF4*/char WingStrikeFrame0[] = {0x1a, 0x26};/*left wing*/
/*0EF6*/char WingStrikeFrame1[] = {0x22, 0x1e};/*right wing*/

/*cast a spell on a target without missile ("disable", "lock",
  "unlock magic", "trap", "untrap", "detect trap", "detect magic").
  Same parameters and SpellResult codes as SPELL_CastMissile; the range is
  not checked for the lock/trap/detect spells*/
/*C_1944_1DF4*/static SPELL_CastOnObject(int16 spellNum, int16 range, int16 objOrNpc, int16 targetPreset) {
	int16 targetType;
	int16 caster, item, found;

	found = 0;
	caster = Party[Active];
	if(IsPlrControl(Party[Active])) {
		if(objOrNpc == 1)
			CON_printf(/*149D*/"%s", SpellMessages[9]);
		else
			CON_printf(/*149D*/"%s", SpellMessages[8]);
	}
	if(!targetPreset) {
		MouseMode = 1;
		if(SelectMode != 0)
			SelectRange = range;
		CON_getch();
	}
	if(Selection.obj == -1 || (Selection.x == -1 && spellNum != SPELL_DETECT_MAGIC && spellNum != SPELL_DETECT_TRAP)) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else{
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	MagicalHalo(Party[Active], 0);
	if(Selection.obj == -1) {
		goto C_22A9;/*TODO*/
	}
	if(Selection.x == -1 && spellNum != SPELL_DETECT_MAGIC && spellNum != SPELL_DETECT_TRAP) {
		goto C_22A9;/*TODO*/
	}
	if(!CLOSE_ENOUGH(range, Selection.x, Selection.y, GetX(caster), GetY(caster))) {
		if(
			spellNum != SPELL_DETECT_MAGIC &&
			spellNum != SPELL_DETECT_TRAP &&
			spellNum != SPELL_TRAP &&
			spellNum != SPELL_UNTRAP &&
			spellNum != SPELL_LOCK &&
			spellNum != SPELL_UNLOCK_MAGIC
		) {
			CON_printf(/*14CF*/"\n%s!\n", SpellMessages[3]);
			MUS_PlaySfx(13, 0);
			return;
		}
	}
	if(objOrNpc == 1 && Selection.obj >= 0x100) {
		goto C_22A9;/*TODO*/
	}
	if(objOrNpc == -1 && Selection.obj < 0x100) {
		goto C_22A9;/*TODO*/
	}
	Selection.obj = COMBAT_getHead(Selection.obj);
	targetType = GetType(Selection.obj);
	SpellResult = 0;
	switch(spellNum) {
		case SPELL_DISABLE:/*"disable"*/ SPELL_DirectDamage(spellNum); break;
		case SPELL_LOCK:/*"lock"*/
			if(IsDoorType(targetType)) {
				UseDoor(Selection.obj, 0, 1, 0);
			} else if(targetType == OBJ_OPEN_CHEST) {
				UseChest(Selection.obj, 0, 1, 0);
				SpellResult = -1;
			} else {
				SpellResult = 2;
			}
		break;
		case SPELL_UNLOCK_MAGIC:/*"unlock magic"*/
			if(IsDoorType(targetType) && (GetFrame(Selection.obj) & 0xc) == 0xc) {
				UseDoor(Selection.obj, 0, 0, 1);
				SpellResult = -1;
			} else if(targetType == OBJ_OPEN_CHEST && GetFrame(Selection.obj) == 3) {
				UseChest(Selection.obj, 0, 0, 1);
				SpellResult = -1;
			} else {
				SpellResult = 2;
			}
		break;
		case SPELL_UNTRAP:/*"untrap"*/
			MagicalHalo(Selection.obj, 1);
			if(
				(targetType == OBJ_OPEN_CHEST || IsDoorType(targetType)) &&
				FindInvType(Selection.obj, OBJ_EFFECT, SPELL_TRAP) != -1
			) {
				DeleteObj(OBJ_FindInContainer(Selection.obj, OBJ_EFFECT, SPELL_TRAP));
			} else {
				SpellResult = 2;
			}
		break;
		case SPELL_TRAP:/*"trap"*/
			MagicalHalo(Selection.obj, 1);
			if(
				(targetType == OBJ_OPEN_CHEST || IsDoorType(targetType)) &&
				FindInvType(Selection.obj, OBJ_EFFECT, SPELL_TRAP) == -1
			) {
				GiveObj(Selection.obj, OBJ_EFFECT, QualQuan(SPELL_TRAP, 1));
			} else {
				SpellResult = 2;
			}
		break;
		case SPELL_DETECT_TRAP:
			MagicalHalo(Selection.obj, 1);
			if(OSI_rand(1, 30) < GetInt(caster)) {
				for(item = FindInv(Selection.obj); item >= 0; item = NextInv()) {
					if(GetType(item) == OBJ_EFFECT) {
						CON_printf(/*1543*/"\nIt's trapped.\n");
						found = 1;
					}
				}
				if(found) {
					SpellResult = -1;
					break;
				}
			}
			CON_printf(/*1553*/"\nNo trap\n");
			SpellResult = -1;
		break;
		case SPELL_DETECT_MAGIC:
			MagicalHalo(Selection.obj, 1);
			if(IsMagical(Selection.obj)) {
				found = 1;
			} else {
				for(item = FindInv(Selection.obj); item >= 0; item = NextInv()) {
					if(GetType(item) == OBJ_CHARGE) {
						CON_printf(/*155D*/"\nIt shows a charge of %s.\n", SpellName[GetQual(item)]);
						found = 2;
						break;
					}
				}
			}
			if(found == 0) {
				CON_printf(/*1578*/"\nIt's not magical.\n");
			} else if(found == 1) {
				if(GetType(Selection.obj) == OBJ_POTION)
					CON_printf(/*158C*/"\nIt's %s potion.\n", PotionNames[GetFrame(Selection.obj)]);
				else
					CON_printf(/*159E*/"\nIt's magical.\n");
			}
			SpellResult = -1;
		break;
	}
	if(SpellResult != 0 && SpellResult != -1) {
		if(SpellResult == 1)
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[1]);
		else if(SpellResult == 2)
C_22A9:
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(SpellResult == 0) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
		MUS_PlaySfx(14, 0);
	}
}

/*"slime": every evil NPC within 5 tiles that fails an intelligence test
  becomes a slime (swimmers are spared, except amphibians);
  temporary NPCs (>= 0xe0) also lose their inventory*/
/*C_1944_22F4*/static SPELL_DoSlime() {
	int16 npc, x;
	int16 caster, y, item, objCls;

	caster = Party[Active];
	MagicalHalo(caster, 0);
	for(x = MapX - 5; x <= MapX + 5; x ++) {
		for(y = MapY - 5; y <= MapY + 5; y ++) {
			npc = FindLoc(x, y, MapZ);
			if(
				npc != -1 && npc < 0x100 &&
				GetAlignment(npc) == EVIL &&
				!COMBAT_ResistsSpell(caster, npc)
			) {
				objCls = GetMonsterClass(GetType(npc));
				if(objCls < 0 || !IsMonsterSwimming(objCls) || IsMonsterAlligator(objCls)) {
					ObjShapeType[npc] = TypeFrame(OBJ_SLIME, 0);
					if(npc >= 0xe0) {
						while((item = FindInv(npc)) >= 0)
							DeleteObj(item);
					}
				}
			}
		}
	}
	ScreenFade = 1;
	VIS_UpdateView();
	DisplayTick();
}

/*"reappear": recreate the object last removed by "vanish" (VanishedShape,
  type | frame << 10) at the selected location*/
/*C_1944_23F2*/static SPELL_DoReappear(int16 targetPreset) {
	int16 newObj, caster;
	struct coord xyz;

	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	if(Selection.x == -1 || Selection.y == -1 || VanishedShape == 0) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(COMBAT_Missile(caster, &Selection.x, &Selection.y, 8, TIL_17F)) {
		SetCoordXYZ(xyz, Selection.x, Selection.y, MapZ);
		newObj = AddObj(0, xyz, VanishedShape & 0x3ff, VanishedShape >> 10, 1);
		SetOkToGet(newObj);
		if(CanMoveTo(newObj, Selection.x, Selection.y)) {
			ScreenFade = 1;
			VIS_UpdateView();
			DisplayTick();
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
			MUS_PlaySfx(14, 0);
			VanishedShape = 0;
			return;
		}
		DeleteObj(newObj);
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*area spells ("mass ..." and "explosion"): a missile (missileTile)
  flies to the selected location, then an explosion of blastTile
  affects every NPC in it*/
/*C_1944_256A*/static SPELL_CastMass(int16 spellNum, int16 missileTile, int16 blastTile, int16 targetPreset) {
	int16 caster;
	int16 far *victimPtr;
	
	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	if(Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(COMBAT_Missile(caster, &Selection.x, &Selection.y, 8, missileTile)) {
		for(victimPtr = Explosion(Selection.x, Selection.y, blastTile); *victimPtr >= 0; victimPtr ++) {
			if(*victimPtr < 0x100) {
				switch(spellNum) {
					case SPELL_MASS_AWAKEN:
						ClrAsleep(*victimPtr);
						ObjShapeType[*victimPtr] = OrigShapeType[*victimPtr];
					break;
					case SPELL_MASS_PROTECT: SetProtected(*victimPtr); break;
					case SPELL_MASS_CURSE: SetCursed(*victimPtr); break;
					case SPELL_MASS_SLEEP: SPELL_SleepTarget(caster, *victimPtr); break;
					case SPELL_MASS_DISPEL: SPELL_DispelTarget(*victimPtr); break;
					case SPELL_EXPLOSION: SPELL_FireDamage(*victimPtr); SpellResult = -1; break;
					case SPELL_MASS_CHARM: SPELL_CharmTarget(caster, *victimPtr); break;
					case SPELL_MASS_KILL: SPELL_KillTarget(caster, *victimPtr); break;
					case SPELL_MASS_INVIS:
						if(!IsInvisible(*victimPtr) && SPELL_CanBecomeInvisible(GetType(*victimPtr))) {
							SetInvisible(*victimPtr);
							MagicalHalo(*victimPtr, 1);
						}
					break;
				}
			}
		}
		if(spellNum == SPELL_EXPLOSION || spellNum == SPELL_MASS_KILL)
			AfterExplosion(Selection.x, Selection.y);
		if(SpellResult != -1)
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*"fire|poison|sleep|energy field": creates a field object at the selected
  location ("energy field" makes an OBJ_PROTECTION_FIELD)*/
/*C_1944_27AF*/static SPELL_CastField(int16 spellNum, int16 missileTile, int16 targetPreset) {
	int16 field, caster;
	struct coord xyz;

	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	if(Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(COMBAT_Missile(caster, &Selection.x, &Selection.y, 8, missileTile)) {
		SetCoordXYZ(xyz, Selection.x, Selection.y, MapZ);
		switch(spellNum) {
			case SPELL_FIRE_FIELD: field = AddObj(0, xyz, OBJ_FIRE_FIELD, 0, 1); break;
			case SPELL_POISON_FIELD: field = AddObj(0, xyz, OBJ_POISON_FIELD, 0, 1); break;
			case SPELL_SLEEP_FIELD: field = AddObj(0, xyz, OBJ_SLEEP_FIELD, 0, 1); break;
			case SPELL_ENERGY_FIELD: field = AddObj(0, xyz, OBJ_PROTECTION_FIELD, 0, 1); break;
		}
		if(CanMoveTo(field, Selection.x, Selection.y)) {
			ScreenFade = 1;
			VIS_UpdateView();
			DisplayTick();
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
			return;
		}
		DeleteObj(field);
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[6]);
		MUS_PlaySfx(13, 0);
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*"fear" on one creature: non-good NPCs failing an intelligence test and
  not immune to death spells flee*/
/*C_1944_29AA*/static SPELL_FearTarget(int16 objNum) {
	int16 objCls;

	objCls = GetMonsterClass(GetType(objNum));
	if(objNum >= 0x100)
		return;
	if(GetAlignment(objNum) == GOOD)
		return;
	if(COMBAT_ResistsSpell(Party[Active], objNum))
		return;
	if(objCls < 0 || !IsMonsterDeathImmune(objCls))
		NPCMode[objNum] = AI_FEAR;
}

/*spells affecting the first object of every tile within 7 tiles of the
  party: "repel undead", "reveal", "fear", "confuse"*/
/*C_1944_2A15*/static SPELL_CastAroundParty(int16 spellNum) {
	register int16 objNum, x;
	int16 y, caster, ring;

	MagicalHalo(Party[Active], 0);
	caster = Party[Active];
	for(x = MapX - 7; x <= MapX + 7; x ++) {
		for(y = MapY - 7; y <= MapY + 7; y ++) {
			if((objNum = FindLoc(x, y, MapZ)) >= 0) {
				switch(spellNum) {
					case SPELL_REPEL_UNDEAD:/*"repel undead"*/
						if(IsMonsterUndead(GetMonsterClass(GetType(objNum))) && !COMBAT_ResistsSpell(caster, objNum))
							NPCMode[objNum] = AI_FEAR;
					break;
					case SPELL_REVEAL:/*"reveal"*/
						if(IsInvisible(objNum))
							ClrInvisible(objNum);
						ring = FindInvType(objNum, OBJ_INVISIBILITY_RING, 0);
						if(ring != -1 && GetCoordUse(ring) == EQUIP)
							SetCoordUse(ring, INVEN);
					break;
					case SPELL_FEAR:/*"fear"*/
						SPELL_FearTarget(objNum);
					break;
					case SPELL_CONFUSE:/*"confuse"*/
						if(objNum < 0x100 && GetAlignment(objNum) == EVIL && !COMBAT_ResistsSpell(caster, objNum)) {
							NPCMode[objNum] = AI_ASSAULT;
							SetAlignment(objNum, NEUTRAL);
						}
					break;
				}
			}
		}
	}
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
	MUS_PlaySfx(14, 0);
}

/*"create food": gives 1d10 portions of meat to the caster*/
/*C_1944_2B9A*/static SPELL_CreateFood(int16 spellNum) {
	int16 portions;

	SPELL_CastEffect(MK_CIRCLE(spellNum));
	MagicalHalo(Party[Active], 0);
	portions = OSI_rand(1, 10);
	GiveObj(Party[Active], OBJ_PORTION_OF_MEAT, portions);
	CON_printf(/*15AE*/"\nCreate %d food.\n", portions);
}

/*"conjure" (random troll/giant rat/giant spider/giant bat/snake) or
  "summon" (daemon) a creature near the party; it fights for the caster.
  A summoned daemon that passes an intelligence test is hostile*/
/*C_1944_2BFD*/static SPELL_DoSummon(int16 spellNum, int16 caster) {
	int16 npc, i;
	int16 monsterType, dx, dy, alignment;

	i = OSI_rand(0, 15);
	if(i <= 3)
		monsterType = OBJ_TROLL;
	else if(i <= 6)
		monsterType = OBJ_GIANT_RAT;
	else if(i <= 9)
		monsterType = OBJ_GIANT_SPIDER;
	else if(i <= 12)
		monsterType = OBJ_GIANT_BAT;
	else
		monsterType = OBJ_SNAKE;
	SPELL_CastEffect(MK_CIRCLE(spellNum));
	MagicalHalo(caster, 0);
	if(spellNum == SPELL_CONJURE)
		npc = EGG_generate(monsterType, MapX, MapY, MapZ);
	else if(spellNum == SPELL_SUMMON)
		npc = EGG_generate(OBJ_DAEMON, MapX, MapY, MapZ);
	for(i = 0; i < 8; i ++) {
		dx = OSI_rand(0, 10);
		dy = OSI_rand(0, 10);
		if(CanMoveTo(npc, MapX + dx - 5, MapY + dy - 5)) {
			MoveObj(npc, MapX + dx - 5, MapY + dy - 5, MapZ);
			if(spellNum == SPELL_SUMMON && COMBAT_ResistsSpell(caster, npc)) {
				alignment = CHAOTIC;
				CON_printf(/*15C0*/"\nHostile daemon summoned.\n");
				MUS_PlaySfx(13, 0);
			} else {
				alignment = GetAlignment(caster);
				CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
				MUS_PlaySfx(14, 0);
			}
			SetAlignment(npc, alignment);
			NPCMode[npc] = AI_ASSAULT;
			ScreenFade = 1;
			VIS_UpdateView();
			DisplayTick();
			return;
		}
	}
	DeleteObj(npc);
	MUS_PlaySfx(13, 0);
}

/*"telekinesis": operate a crank/lever or move an object one tile in a
  chosen direction*/
/*C_1944_2DA7*/static SPELL_DoTelekinesis(int16 targetPreset) {
	int16 objType, x;
	int16 caster, newX, newY, y, reached;

	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[8]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	if(Selection.obj == -1 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	MagicalHalo(Party[Active], 0);
	if(Selection.x != -1 && Selection.y != -1)
		reached = COMBAT_Missile(caster, &Selection.x, &Selection.y, 8, TIL_17F);
	if(Selection.obj == -1 || Selection.obj < 0x100 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(reached) {
		objType = GetType(Selection.obj);
		if(objType == OBJ_CRANK) {
			UseCrank(Selection.obj);
			return;
		}
		if(objType == OBJ_LEVER) {
			UseLever(Selection.obj);
			return;
		}
		if(TypeWeight[objType] == 0 && Selection.obj >= 0x100) {
			CON_printf(/*15DB*/"\nNot Possible\n");
			return;
		}
		MagicalHalo(Selection.obj, 1);
		x = GetX(Selection.obj);
		y = GetY(Selection.obj);
		MouseMode = 0;
		DirOriginX = ((x - MapX) << 4) + 88;
		DirOriginY = ((y - MapY) << 4) + 88;
		CON_printf(/*15EA*/"Direction-");
		if(CON_getch() != CMD_DIRECTION) {
			CON_printf(/*15F5*/"nowhere\n");
		} else {
			CON_printf(/*14B6*/"%s\n", DirNames[AdvanceDir]);
			newX = DirIncrX[AdvanceDir] + x;
			newY = DirIncrY[AdvanceDir] + y;
			if(IsPushBlocked(Selection.obj, x, y, AdvanceDir)) {
				CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
			} else {
				MoveObj(Selection.obj, newX, newY, MapZ);
				if(objType == OBJ_CANNON)
					SetFrame(Selection.obj, (AdvanceDir >> 1) & 3);
				VIS_UpdateView();
			}
		}
		DirOriginX = 88;
		DirOriginY = 88;
		CON_UpdateMousePointer();
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*"gate travel": asks for a moon phase (1..8) and travels to that
  moonstone's location (MoonstonePos) if it has been placed*/
/*C_1944_305A*/static SPELL_GateTravel() {
	int16 phase;
	char input[5];

	CON_printf(/*15FE*/"To phase ");
	CON_gets(input, 4);
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	phase = ParseNumberInput(input, 0);
	if(
		phase == -1 ||
		phase == -2 ||
		phase == 0 ||
		phase > 8 ||
		(MoonstonePos[phase - 1][0] == 0 && MoonstonePos[phase - 1][1] == 0 && MoonstonePos[phase - 1][2] == 0)
	) {
		MUS_PlaySfx(13, 0);
		return;
	}
	GateTravel(phase - 1);
}

/*"x-ray": see through walls (XRayView) during 100 animation steps
  (also used by objcmd.c)*/
/*C_1944_310C*/SPELL_XRay(int16 spellNum) {
	int16 i;

	SPELL_CastEffect(MK_CIRCLE(spellNum));
	MagicalHalo(Party[Active], 0);
	XRayView = 1;
	VIS_UpdateView();
	for(i = 0; i < 100; i ++)
		DisplayTick();
	XRayView = 0;
	VIS_UpdateView();
	DisplayTick();
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[7]);
}

/*TODO
extern unsigned char __NPC_1Hand;
extern unsigned char __ShowNPCInven;*/
#define __NPC_1Hand *(unsigned char *)(&NPC_2Handed)
#define __ShowNPCInven *(unsigned char *)(&ShowNPCInven)

/*"pickpocket": shows the inventory of the target NPC and lets the caster
  take one item from it (costs 5 karma). PickpocketActive is set while the
  victim's inventory is displayed*/
/*C_1944_3175*/static SPELL_DoPickpocket(int16 targetPreset) {
	int16 caster, victim;
	int16 reached, casterX, casterY, key, savedSelectMode, cantTake, objType;

	savedSelectMode = 0;
	cantTake = 0;
	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[9]);
	if(!targetPreset) {
		MouseMode = 1;
		savedSelectMode = (char)SelectMode;
		CON_getch();
		SelectMode = savedSelectMode;
	}
	if(Selection.obj == -1 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	MagicalHalo(Party[Active], 0);
	if(Selection.x != -1 && Selection.y != -1)
		reached = COMBAT_Missile(caster, &Selection.x, &Selection.y, 8, TIL_MISSILE_3);
	if(Selection.obj == -1 || Selection.obj >= 0x100 || Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(reached) {
		PickpocketActive = 1;
		casterX = GetX(caster);
		casterY = GetY(caster);
		victim = Selection.obj;
		__ShowNPCInven = Selection.obj;
		STAT_ShowInventory(victim);
		StatusDisplay = CMD_STAT_EQUIP;
		CON_printf(/*1608*/"Which object: ");
		if(SelectMode != 0) {
			SelectMode = 2;
			DrawSlotCursor(1);
		}
		key = CON_getch();
		PickpocketActive = 0;
		if(key != CMD_TARGET || Selection.obj == -1) {
			CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
		} else {
			CON_printf((char *)GetObjectString(Selection.obj));
			CON_printf(/*14A6*/"\n");
		}
		if(Selection.obj == -1 || Selection.obj < 0x100 || GetCoordUse(Selection.obj) == LOCXYZ) {
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
			MUS_PlaySfx(13, 0);
		} else {
			objType = GetType(Selection.obj);
			cantTake = STAT_CheckUnready(Selection.obj, victim);
			if(cantTake == 0) {
				STAT_ComputeWeights(caster);
				if(GetWeight(Selection.obj) + WeightInven + WeightEquip > STREN[caster] * 20) {
					CON_printf(/*1617*/"\nCan't carry!\n");
				} else {
					COMBAT_Missile(victim, &casterX, &casterY, 8, TILE_FRAME(Selection.obj));
					STAT_StopEquipEffect(Selection.obj, victim);
					if(QuanType(GetType(Selection.obj))) {
						GiveObj(caster, objType, Amount[Selection.obj]);
						TakeObj(victim, objType, Amount[Selection.obj]);
					} else {
						InsertObj(Selection.obj, caster, INVEN);
					}
					STAT_RefreshInventory(caster);
					CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
					KARMA -= 5;
				}
				StatusMember = Active;
				InvenScroll = 0;
				StatusDirty ++;
			}
		}
		__ShowNPCInven = 0;
		__NPC_1Hand = 0;
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*"insect swarm": generate 8 insect swarms around the party*/
/*C_1944_34CF*/static SPELL_InsectSwarm(int16 spellNum) {
	int16 npc, tries;
	int16 swarm, dx, dy;

	SPELL_CastEffect(MK_CIRCLE(spellNum));
	MagicalHalo(Party[Active], 0);
	for(swarm = 0; swarm < 8; swarm ++) {
		npc = EGG_generate(OBJ_INSECTS, MapX, MapY, MapZ);
		for(tries = 0; tries < 8; tries ++) {
			dx = OSI_rand(0, 10);
			dy = OSI_rand(0, 10);
			if(CanMoveTo(npc, MapX + dx - 5, MapY + dy - 5)) {
				MoveObj(npc, MapX + dx - 5, MapY + dy - 5, MapZ);
				SetAlignment(npc, GetAlignment(Party[Active]));
				NPCMode[npc] = AI_ASSAULT;
				break;
			}
		}
		if(tries == 8)
			DeleteObj(npc);
	}
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
	MUS_PlaySfx(14, 0);
}

/*"web": put webs on the 3x3 tiles around the selected location*/
/*C_1944_35EC*/static SPELL_DoWeb(int16 targetPreset) {
	int16 x, y;
	int16 caster, web;
	struct coord xyz;

	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	caster = Party[Active];
	MagicalHalo(caster, 0);
	if(Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	if(AnimWebs(caster, Selection.x, Selection.y)) {
		for(x = Selection.x - 1; x <= Selection.x + 1; x ++) {
			for(y = Selection.y - 1; y <= Selection.y + 1; y ++) {
				SetCoordXYZ(xyz, 0, 0, MapZ);
				web = AddObj(0, xyz, OBJ_WEB, 0, 1);
				if(CanMoveTo(web, x, y))
					MoveObj(web, x, y, MapZ);
				else
					DeleteObj(web);
			}
		}
		VIS_UpdateView();
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
		MUS_PlaySfx(14, 0);
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[4]);
		MUS_PlaySfx(13, 0);
	}
}

/*"enchant": put a charge of a spell (chosen from the spellbook) into a
  staff (10 charges max)*/
/*C_1944_3750*/static SPELL_DoEnchant(int16 targetPreset) {
	int16 item, i;
	unsigned char ch;
	int16 foundBook, book0, book1, staff, spellNum;

	foundBook = 0;
	book0 = 0;
	book1 = 0;
	StatusDisplay = CMD_STAT_EQUIP;
	STAT_ShowInventory(Active);
	CON_printf(/*149D*/"%s", SpellMessages[8]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	if(Selection.obj == -1) {
		CON_printf(/*14B6*/"%s\n", SpellMessages[5]);
	} else {
		CON_printf((char *)GetObjectString(Selection.obj));
		CON_printf(/*14A6*/"\n");
	}
	/*(the first test can never be true: probably meant ||)*/
	if(
		(Selection.obj == -1 && Selection.obj >= 0x100 && GetCoordUse(Selection.obj) != LOCXYZ) ||
		GetType(Selection.obj) != OBJ_STAFF
	) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	staff = Selection.obj;
	for(i = 0, item = FindInv(staff); item >= 0; i ++, item = NextInv());
	if(i == 10) {
		CON_printf(/*1626*/"\nIt's full\n");
		MUS_PlaySfx(13, 0);
		return;
	}
	for(item = FindInv(Party[Active]); item >= 0; item = NextInv()) {
		if(GetCoordUse(item) == EQUIP && GetType(item) == OBJ_SPELLBOOK) {
			if(!foundBook) {
				foundBook = OBJ_SPELLBOOK;
				book0 = item;
			} else {
				book1 = item;
			}
		}
	}
	Spellbook[0] = book0;
	Spellbook[1] = book1;
	SPELL_RefreshList();
	SPELL_DrawBG();
	SPELL_RefreshPage();
	StatusDisplay = CMD_STAT_SPELLBOOK;
	CON_printf(/*1632*/"With: ");
	ch = CON_getch();
	MagicalHalo(Party[Active], 0);
	if(ch != CMD_TARGET || Selection.obj == -1) {
		CON_printf(/*1639*/"none\n");
	} else {
		spellNum = SpellbookPage[Selection.obj];
		i = 0;
		ch = SpellCode[spellNum][i++];
		CON_printf(/*149D*/"%s", MagicWords[ch - 'A']);
		while(ch = SpellCode[spellNum][i++])
			CON_printf(/*149D*/" %s", MagicWords[ch - 'A']);
		GiveObj(staff, OBJ_CHARGE, QualQuan(spellNum, 1));
	}
	CON_printf(/*14A6*/"\n");
}

/*"wizard eye": move a wizard's eye from (x,y) for up to 40 steps
  (until Esc or a blocking tile). isSpell = play the casting effect.
  (also used by objcmd.c with a computed location)*/
/*C_1944_39A1*/SPELL_WizardEye(int16 x, int16 y, int16 isSpell) {
	int16 numTile, key;
	int16 movesLeft, sav_x, sav_y, eye;
	struct coord xyz;

	movesLeft = 40;
	numTile = 0;
	key = 0;
	if(isSpell) {
		SPELL_CastEffect(6);
		MagicalHalo(Party[Active], 0);
	}
	AdvanceBlocked = 1;
	sav_x = MapX;
	sav_y = MapY;
	MapX = x;
	MapY = y;
	SetCoordXYZ(xyz, MapX, MapY, MapZ);
	eye = AddObj(0, xyz, OBJ_WIZARDS_EYE, 0, 1);
	XRayView = 1;
	ScreenFade = 1;
	ShowObjects();
	MAP_LoadAround();
	DisplayTick();
	numTile = GetTileAtXYZ(MapX, MapY, MapZ);
	/*until out of moves, Esc, or the eye reaches a blocking tile*/
	while(movesLeft >= 0 && key != 27 && numTile < TIL_0FC) {
		MouseMode = 0;
		AdvanceBlocked = 0;
		key = CON_getch();
		if(key == CMD_DIRECTION) {
			MapX += DirIncrX[AdvanceDir];
			MapY += DirIncrY[AdvanceDir];
			MoveObj(eye, MapX, MapY, MapZ);
			ShowObjects();
			MAP_LoadAround();
			DisplayTick();
			movesLeft --;
			numTile = GetTileAtXYZ(MapX, MapY, MapZ);
		}
	}
	DeleteObj(eye);
	XRayView = 0;
	MapX = sav_x;
	MapY = sav_y;
	ShowObjects();
	MAP_LoadAround();
	ScreenFade = 1;
	DisplayTick();
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[7]);
	if(StatusDisplay == CMD_STAT_EQUIP)
		STAT_RefreshInventory(Party[StatusMember]);
}

/*"tremor": 1d30 damage to every evil, non-flying NPC within 5 tiles*/
/*C_1944_3B2D*/static SPELL_DoTremor() {
	int16 npc, x;
	int16 y, xp;

	MagicalHalo(Party[Active], 0);
	ShakeScreen(20, 3);
	for(x = MapX - 5; x <= MapX + 5; x ++) {
		for(y = MapY - 5; y <= MapY + 5; y ++) {
			npc = FindLoc(x, y, MapZ);
			if(
				npc >= 0 && npc < 0x100 && GetAlignment(npc) == EVIL &&
				!IsMonsterFlying(GetMonsterClass(GetType(npc)))
			) {
				xp = LoseHP(npc, OSI_rand(1, 30));
				AddXP(Party[Active], xp);
				CON_printf(/*14A6*/"\n");
				COMBAT_displaySeverity(npc);
			}
		}
	}
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
}

/*"seance": talk to the dead NPC whose body is selected*/
/*C_1944_3C30*/static SPELL_DoSeance(int16 targetPreset) {
	int16 objType;

	CON_printf(/*149D*/"%s", SpellMessages[9]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	objType = GetType(Selection.obj);
	if(Selection.obj == -1 || (objType != OBJ_DEAD_BODY && objType != OBJ_MOUSE && objType != OBJ_DEAD_GARGOYLE && objType != OBJ_DEAD_CYCLOPS)) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	CON_printf((char *)GetObjectString(Selection.obj));
	CON_printf(/*14A6*/"\n");
	SeanceFlag = 1;
	TALK_initTalk(Active, GetQual(Selection.obj));
	SeanceFlag = 0;
}

/*"hail storm" at the selected location*/
/*C_1944_3D0F*/static SPELL_HailStorm(int16 targetPreset) {
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	if(Selection.x == -1 || Selection.y == -1) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	AnimHailStorm(Selection.x, Selection.y);
	CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
}

/*"flame|poison|energy|death wind": a wind of windTile from the caster
  toward the selected location affects every NPC on its path*/
/*C_1944_3DB8*/static SPELL_CastWind(int16 spellNum, int16 windTile, int16 targetPreset) {
	int16 caster, xp;
	int16 far *victimPtr;

	caster = Party[Active];
	if(IsPlrControl(Party[Active]))
		CON_printf(/*149D*/"%s", SpellMessages[10]);
	if(!targetPreset) {
		MouseMode = 1;
		CON_getch();
	}
	CON_printf(/*14A6*/"\n");
	MagicalHalo(Party[Active], 0);
	if(
		Selection.x == -1 || Selection.y == -1 ||
		(GetX(caster) == Selection.x && GetY(caster) == Selection.y)
	) {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
		MUS_PlaySfx(13, 0);
		return;
	}
	for(victimPtr = MagicWind(caster, Selection.x, Selection.y, windTile); *victimPtr >= 0; victimPtr ++) {
		if(*victimPtr < 0x100) {
			switch(spellNum) {
				case SPELL_FLAME_WIND: SPELL_FireDamage(*victimPtr); break;
				case SPELL_POISON_WIND: SPELL_PoisonTarget(caster, *victimPtr); break;
				case SPELL_ENERGY_WIND:/*"energy wind"*/
					xp = LoseHP(*victimPtr, OSI_rand(1, 30));
					AddXP(Party[Active], xp);
					COMBAT_displaySeverity(*victimPtr);
					COMBAT_ProvokeNeutral(*victimPtr, Party[Active], 1);
				break;
				case SPELL_DEATH_WIND: SPELL_KillTarget(caster, *victimPtr); break;
			}
		}
	}
}

/*"wing strike": for every visible evil/chaotic creature within 5 tiles,
  a dragon (2 objects, one per wing) flies across the screen row and
  deals 1d20 damage when passing over it*/
/*C_1944_3F6C*/SPELL_WingStrike() {
	int16 objNum, hDir;
	int16 x, y;
	int16 distLeft, distRight;
	int16 i;
	int16 dragon0, dragon1;
	int16 hitPts;
	struct coord xyz;
	int16 hasHit;

	hasHit = 0;
	MagicalHalo(Party[Active], 0);
	for(objNum = 0; objNum < 0x100; objNum ++) {
		if(
			ObjShapeType[objNum] && !IsDead(objNum) && !IsInvisible(objNum) &&
			(GetAlignment(objNum) == EVIL || GetAlignment(objNum) == CHAOTIC)
		) {
			x = GetX(objNum);
			y = GetY(objNum);
			if(
				MapX - 5 <= x && MapX + 5 >= x &&
				MapY - 5 <= y && MapY + 5 >= y
			) {
				distLeft = x - (MapX - 5);
				distRight = (MapX + 5) - x;
				if(distLeft > distRight)
					hDir = 0;
				else
					hDir = 1;
				SetCoordXYZ(xyz, MapX - WingStrikeDir[hDir] * 5, y - 1, MapZ);
				dragon0 = AddObj(0, xyz, OBJ_DRAGON, WingStrikeFrame0[hDir], 1);
				SetCoordXYZ(xyz, MapX - WingStrikeDir[hDir] * 5, y, MapZ);
				dragon1 = AddObj(0, xyz, OBJ_DRAGON, WingStrikeFrame1[hDir], 1);
				VIS_UpdateView();
				DisplayTick();
				for(i = 1; i < 11; i ++) {
					SetFrame(dragon0, GetFrame(dragon0) ^ 1);
					SetFrame(dragon1, GetFrame(dragon1) ^ 1);
					MoveObj(dragon0, MapX - (5 - i) * WingStrikeDir[hDir], y - 1, MapZ);
					MoveObj(dragon1, MapX - (5 - i) * WingStrikeDir[hDir], y, MapZ);
					VIS_UpdateView();
					DisplayTick();
					if(MapX - (5 - i) * WingStrikeDir[hDir] == x) {
						hitPts = LoseHP(objNum, OSI_rand(1, 20));
						AddXP(Party[Active], hitPts);
						COMBAT_displaySeverity(objNum);
					}
				}
				hasHit = 1;
				DeleteObj(dragon0);
				DeleteObj(dragon1);
			}
		}
	}
	if(!hasHit) {
		CON_printf(/*1643*/"\nNo foes.\n");
		MUS_PlaySfx(13, 0);
	}
}

/*SPELL_SELF_INVISIBILITY: makes the caster invisible (spell used by monsters, see
  monsdata.c)*/
/*C_1944_4260*/static SPELL_SelfInvisibility() {
	int16 caster;

	caster = Party[Active];
	if(!IsInvisible(caster)) {
		SetInvisible(caster);
		MagicalHalo(caster, 1);
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[0]);
	} else {
		CON_printf(/*14BA*/"\n%s\n", SpellMessages[2]);
	}
}

/*"locate": print the party position as latitude/longitude ("{" is the
  degree glyph); 8 tiles per degree, origin at (0x130,0x168);
  underground coordinates are scaled by 4. (also used by the sextant)*/
/*C_1944_42AC*/SPELL_PrintLocation() {
	int16 x, y;
	int16 lat, lon;

	x = MapX;
	y = MapY;
	if(MapZ) {
		x <<= 2;
		y <<= 2;
	}
	lat = (y - 0x168) >> 3;
	lon = (x - 0x130) >> 3;
	CON_printf(/*164E*/"\n%d{", abs(lat));
	if(lat > 0)
		CON_printf(/*0F38 + 1*/"S");
	else
		CON_printf(/*1653*/"N");
	CON_printf(/*1655*/", %d{", abs(lon));
	if(lon > 0)
		CON_printf(/*165B*/"E\n");
	else
		CON_printf(/*165E*/"W\n");
}

/*"wind change": set WindDir to a chosen direction*/
/*C_1944_435B*/SPELL_WindChange() {
	CON_printf(/*15EA*/"Direction-");
	MouseMode = 0;
	if(CON_getch() != CMD_DIRECTION)
		CON_printf(/*1661*/"nowhere.\n");
	else
		CON_printf(/*155D + 16h*/"%s.\n", DirNames[AdvanceDir]);
	WindDir = AdvanceDir;
}

/*"armageddon": delete every NPC except the Avatar (1) and Lord British (5)*/
/*C_1944_43A3*/SPELL_DoArmageddon() {
	int16 objNum;

	for(objNum = 0; objNum < 0xff; objNum ++) {
		if(objNum != 1 && objNum != 5)/*Avatar, Lord British*/
			DeleteObj(objNum);
	}
	VIS_UpdateView();
	DisplayTick();
	IsArmageddon = 1;
	PartySize = 1;
}

/*cast spellNum (spell effect only: reagents/magic points are handled by
  the caller). targetPreset = target already in Selection, do not prompt.
  Also called for monsters/wands (combat.c, objcmd.c)*/
/*C_1944_43D5*/SPELL_Execute(int16 spellNum, int16 targetPreset) {
	CON_printf(/*14A6*/"\n");
	switch(spellNum) {
		case SPELL_DOUSE:/*"douse"*/ SPELL_CastMissile(spellNum, TIL_MISSILE_3, 8, -1, targetPreset); break;
		case SPELL_VANISH:/*"vanish"*/
		case SPELL_DISPEL_FIELD:/*"dispel field"*/
		case SPELL_ANIMATE:/*"animate"*/
		case SPELL_REPLICATE:/*"replicate"*/ SPELL_CastMissile(spellNum, TIL_17F, 8, -1, targetPreset); break;
		case SPELL_IGNITE:/*"ignite"*/ SPELL_CastMissile(spellNum, TIL_MISSILE_5, 8, -1, targetPreset); break;
		case SPELL_DISABLE:/*"disable"*/ SPELL_CastOnObject(spellNum, 8, 1, targetPreset); break;
		case SPELL_RESURRECT:/*"resurrect"*/ SPELL_DoResurrect(targetPreset); break;
		case SPELL_LIGHT:/*"light"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[0] = 100;/*light level*/
			PassTime(0);
		break;
		case SPELL_HELP:/*"help"?*/
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			PartyResurrect();
		break;
		case SPELL_DETECT_MAGIC:/*"detect magic"*/
		case SPELL_DETECT_TRAP:/*"detech trap"*/
		case SPELL_TRAP:/*"trap"*/
		case SPELL_UNLOCK_MAGIC:/*"unlock magic"*/
		case SPELL_UNTRAP:/*"untrap"*/
		case SPELL_LOCK:/*"lock"*/ SPELL_CastOnObject(spellNum, 8, -1, targetPreset); break;
		case SPELL_SLIME:/*"slime"*/ SPELL_DoSlime(); break;
		case SPELL_CREATE_FOOD:/*"create food"*/ SPELL_CreateFood(spellNum); break;
		case SPELL_REAPPEAR:/*"reappear"*/ SPELL_DoReappear(targetPreset); break;
		case SPELL_SLEEP:/*"sleep"*/ SPELL_CastMissile(spellNum, TIL_17D, 8, 1, targetPreset); break;
		case SPELL_MAGIC_ARROW:/*"magic arrow"*/ SPELL_CastMissile(spellNum, TIL_ARROW, 8, 1, targetPreset); break;
		case SPELL_FIREBALL:/*"fireball"*/ SPELL_CastMissile(spellNum, TIL_FIRE_MISSILE, 8, 1, targetPreset); break;
		case SPELL_LIGHTNING:/*"lightning"*/
		case SPELL_CHAIN_BOLT:/*"chain bolt"*/ SPELL_CastMissile(spellNum, TIL_MISSILE_0, 8, 1, targetPreset); break;
		case SPELL_TELEKINESIS:/*"telekinesis"*/ SPELL_DoTelekinesis(targetPreset); break;
		case SPELL_INFRAVISION:/*"infravision"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[1] = 20;/*infravision duration*/
		break;
		case SPELL_MASS_AWAKEN:/*"mass awaken"*/
		case SPELL_MASS_DISPEL:/*"mass dispel"*/
		case SPELL_MASS_PROTECT:/*"mass protect"*/
		case SPELL_MASS_CURSE:/*"mass curse"*/ SPELL_CastMass(spellNum, TIL_MISSILE_3, TIL_17F, targetPreset); break;
		case SPELL_MASS_INVIS:/*"mass invis"*/
		case SPELL_MASS_CHARM:/*"mass charm"*/ SPELL_CastMass(spellNum, TIL_MISSILE_0, TIL_MISSILE_2, targetPreset); break;
		case SPELL_MASS_KILL:/*"mass kill"*/ SPELL_CastMass(spellNum, TIL_MISSILE_0, TIL_MISSILE_4, targetPreset); break;
		case SPELL_MASS_SLEEP:/*"mass sleep"*/ SPELL_CastMass(spellNum, TIL_MISSILE_3, TIL_17D, targetPreset); break;
		case SPELL_EXPLOSION:/*"explosion"*/ SPELL_CastMass(spellNum, TIL_MISSILE_1, TIL_FIRE_MISSILE, targetPreset); break;
		case SPELL_REPEL_UNDEAD:/*"repel undead"*/
		case SPELL_REVEAL:/*"reveal"*/
		case SPELL_CONFUSE:/*"confuse"*/
		case SPELL_FEAR:/*"fear"*/ SPELL_CastAroundParty(spellNum); break;
		case SPELL_POISON:/*"poison"*/ SPELL_CastMissile(spellNum, TIL_POISON_MISSILE, 8, 1, targetPreset); break;
		case SPELL_GREAT_LIGHT:/*"great light"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[0] = 255;/*light level*/
			PassTime(0);
		break;
		case SPELL_PEER:/*"peer"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			UseGem(0);
			CON_printf(/*14BA*/"\n%s\n", SpellMessages[7]);
		break;
		case SPELL_FIRE_FIELD:/*"fire field"*/ SPELL_CastField(spellNum, TIL_FIRE_MISSILE, targetPreset); break;
		case SPELL_POISON_FIELD:/*"poison field"*/ SPELL_CastField(spellNum, TIL_POISON_MISSILE, targetPreset); break;
		case SPELL_LOCATE:/*"locate"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SPELL_PrintLocation();
		break;
		case SPELL_SLEEP_FIELD:/*"sleep field"*/ SPELL_CastField(spellNum, TIL_17D, targetPreset); break;
		case SPELL_CONJURE:/*"conjure"*/
		case SPELL_SUMMON:/*"summon"*/ SPELL_DoSummon(spellNum, Party[Active]); break;
		case SPELL_PICKPOCKET:/*"pickpocket"*/ SPELL_DoPickpocket(targetPreset); break;
		case SPELL_WIND_CHANGE:/*"wind change"*/ SPELL_WindChange(); break;
		case SPELL_ENERGY_FIELD:/*"energy field"*/ SPELL_CastField(spellNum, TIL_17F, targetPreset); break;
		case SPELL_INSECT_SWARM:/*"insect swarm"*/ SPELL_InsectSwarm(spellNum); break;
		case SPELL_X_RAY:/*"x-ray"*/ SPELL_XRay(spellNum); break;
		case SPELL_INVISIBILITY:/*"invisibility"*/ SPELL_CastMissile(spellNum, TIL_MISSILE_3, 8, 1, targetPreset); break;
		case SPELL_DISPEL_MAGIC:/*"dispel magic"*/
		case SPELL_HARM:/*"harm"*/
		case SPELL_HEAL:/*"heal"*/
		case SPELL_CURSE:/*"curse"*/
		case SPELL_PROTECTION:/*"protection"*/
		case SPELL_GREAT_HEAL:/*"great heal"*/
		case SPELL_PARALYZE:/*"paralyze"*/
		case SPELL_CHARM:/*"charm"*/
		case SPELL_CLONE:/*"clone"*/ SPELL_CastMissile(spellNum, TIL_17F, 8, 1, targetPreset); break;
		case SPELL_KILL:/*"kill"*/ SPELL_CastMissile(spellNum, TIL_MISSILE_4, 8, 1, targetPreset); break;
		case SPELL_FLAME_WIND:/*"flame wind"*/ SPELL_CastWind(spellNum, TIL_MISSILE_5, targetPreset); break;
		case SPELL_POISON_WIND:/*"poison wind"*/ SPELL_CastWind(spellNum, TIL_POISON_MISSILE, targetPreset); break;
		case SPELL_ENERGY_WIND:/*"energy wind"*/ SPELL_CastWind(spellNum, TIL_MISSILE_0, targetPreset); break;
		case SPELL_DEATH_WIND:/*"death wind"*/ SPELL_CastWind(spellNum, TIL_MISSILE_4, targetPreset); break;
		case SPELL_WEB:/*"web"*/ SPELL_DoWeb(targetPreset); break;
		case SPELL_NEGATE_MAGIC:/*"negate magic"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[13] = 20;/*no magic duration*/
		break;
		case SPELL_ENCHANT:/*"enchant"*/ SPELL_DoEnchant(targetPreset); break;
		case SPELL_WIZARD_EYE:/*"wizard eye"*/ SPELL_WizardEye(MapX, MapY, 1); break;
		case SPELL_GATE_TRAVEL:/*"gate travel"*/ SPELL_GateTravel(); break;
		case SPELL_TIME_STOP:/*"time stop"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[14] = 10;/*time stop duration*/
		break;
		case SPELL_ECLIPSE:/*"eclipse"*/
			MagicalHalo(Party[Active], 0);
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SpellFx[15] = 20;/*eclipse duration*/
		break;
		case SPELL_TREMOR:/*"tremor"*/ SPELL_DoTremor(); break;
		case SPELL_HAIL_STORM:/*"hail storm"*/ SPELL_HailStorm(targetPreset); break;
		case SPELL_WING_STRIKE:/*"wing strike"*/ SPELL_WingStrike(); break;
		case SPELL_SELF_INVISIBILITY:/*caster becomes invisible (monsters)*/ SPELL_SelfInvisibility(); break;
		case SPELL_SEANCE:/*"seance"*/ SPELL_DoSeance(targetPreset); break;
		case SPELL_ARMAGEDDON:/*"armageddon"*/
			SPELL_CastEffect(MK_CIRCLE(spellNum));
			SPELL_DoArmageddon();
		break;
	}
	VIS_UpdateView();
	DisplayTick();
}

/*cast a spell from the readied spellbook(s) book0/book1: show the
  spellbook, let the player choose a spell, print its incantation, check
  level, magic points and reagents, then take them and cast it*/
/*C_1944_4926*/static SPELL_CastFromBook(int16 book0, int16 book1) {
	int16 i, caster;
	unsigned char ch;
	int16 spellNum, circle, targetPreset, casted;

	targetPreset = 0;
	casted = 0;
	Spellbook[0] = book0;
	Spellbook[1] = book1;
	if(SpellFx[13]) {/*"negate magic" in effect*/
		CON_printf(/*166B*/"No magic at this time!\n");
		return;
	}
	caster = Party[Active];
	SPELL_RefreshList();
	for(i = 0; i < 8; i ++) {
		if(SpellList[i])
			break;
	}
	if(i == 8) {
		CON_printf(/*1683*/"No spells in the spellbook!\n");
		return;
	}
	SPELL_DrawBG();
	SPELL_RefreshPage();
	SpellSavedStatus = StatusDisplay;
	StatusDisplay = CMD_STAT_SPELLBOOK;
	ch = CON_getch();
	if((ch != CMD_TARGET || Selection.obj == -1) && Selection.x == -1) {
		CON_printf(/*16A0*/"nothing\n");
		StatusDisplay = SpellSavedStatus;
		StatusDirty ++;
		return;
	}
	if(Selection.x != -1 && Selection.y != -1) {
		/*clicked on the map: cast the current spell on that target*/
		spellNum = SelectedSpell;
		targetPreset = 1;
	} else {
		spellNum = SpellbookPage[Selection.obj];
	}
	if(SelectMode != 0 && IncantLen) {
		/*incantation typed by the player: just print the spell name*/
		CON_printf(/*16A9*/"(%s)", SpellName[spellNum]);
		IncantLen = 0;
	} else {
		/*print the name and the words of power*/
		CON_printf(/*16AE*/"%s\n\"", SpellName[spellNum]);
		i = 0;
		ch = SpellCode[spellNum][i++];
		CON_printf(/*149D*/"%s", MagicWords[ch - 'A']);
		while(ch = SpellCode[spellNum][i++])
			CON_printf(/*163F*/" %s", MagicWords[ch - 'A']);
		CON_printf(/*16AE + 3*/"\"");
	}
	circle = MK_CIRCLE(spellNum);
	if(!targetPreset) {
		/*move the selection arrow to the chosen line and make the
		  chosen spell the book's current spell*/
		CON_setClip(&TextWindows[1]);
		for(i = 0; i < 8; i ++) {
			if(SpellPageCounts[i] & 0x80) {
				CON_gotoxy(0, i + 1);
				CON_putch0(' ');
			}
		}
		CON_gotoxy(0, Selection.obj + 1);
		CON_putch0(0x1a);
		CON_setClip(&TextWindows[3]);
		if(Spellbook[0])
			SetQual(Spellbook[0], spellNum);
		if(Spellbook[1])
			SetQual(Spellbook[1], spellNum);
	}
	StatusDisplay = SpellSavedStatus;
	StatusDirty ++;
	RefreshStatus();
	if(Level[caster] < circle) {
		MUS_PlaySfx(13, 0);
		CON_printf(/*16B3*/"\n\nYour level is not high enough.\n");
	} else if(MAGIC[caster] < circle) {
		MUS_PlaySfx(13, 0);
		CON_printf(/*16D5*/"\n\nNot enough magic points.\n");
	} else if((SpellPageCounts[SPELL_FindPageLine(spellNum)] & 0x7f) == 0) {/*castable count*/
		MUS_PlaySfx(13, 0);
		CON_printf(/*16F1*/"\n\nNo Reagents.\n");
	} else {
		/*(when targetPreset, Selection.obj is the target, not a page line)*/
		TakeReagents(caster, spellNum, Selection.obj);
		MAGIC[caster] -= circle;
		SubMov(caster, circle * 3 + 10);/*casting takes movement points*/
		SPELL_Execute(spellNum, targetPreset);
		casted = 1;
	}
	if(casted) {
		StatusDisplay = SpellSavedStatus;
		StatusDirty ++;
		RefreshStatus();
	}
}

/*"Cast" command: find the spellbook(s) readied by the active character*/
/*C_1944_4C2F*/CMD_CastSpell() {
	int16 item, type0, type1, book0, book1;

	type0 = 0;
	type1 = 0;
	book0 = 0;
	book1 = 0;
	for(item = FindInv(Party[Active]); item >= 0; item = NextInv()) {
		if(GetCoordUse(item) == EQUIP && GetType(item) == OBJ_SPELLBOOK) {
			if(type0 == 0) {
				type0 = OBJ_SPELLBOOK;
				book0 = item;
			} else {
				type1 = OBJ_SPELLBOOK;
				book1 = item;
			}
		}
	}
	if(type0 == OBJ_SPELLBOOK || type1 == OBJ_SPELLBOOK)
		SPELL_CastFromBook(book0, book1);
	else
		CON_printf(/*1701*/"No spellbook is readied.\n");
}
