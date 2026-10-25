/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	module combat

	Everything about fighting:
	- combat mode on/off for the party (COMBAT_begin/COMBAT_breakOff),
	- derived attributes (GetStr/GetDex/GetInt with curse and time stop
	  penalties, MaxHP, MaxMagic, armour and weapon tables),
	- the "Attack" command of the player (COMBAT_attack, cycling through
	  every equipped weapon),
	- resolving one attack (__TryHit -> __AttackTest -> __DoHit -> LooseHP)
	  including ranged weapons, ammunition, thrown weapons, wands, the
	  triple crossbow and monster special attacks (acid, theft, poison,
	  dragging under),
	- death handling (SetTypeUnconscious: corpses, blood, insects, slime),
	- the reaction of the townsfolk when a neutral NPC is attacked,
	- the combat AI modes (front, rear, flank, berserk, retreat, assault,
	  like, unfriendly, immobile, tangle vine).
	Objects 0..0xff are the "actors" (NPCs and monsters; they own the
	NPCStatus/STREN/... entries); objects >= 0x100 are plain items.
	The front/rear/flank AIs use the party and enemy centres of gravity
	(PartyGravityX/Y, EnemiesGravityX/Y) and the flank/front members
	(PartyFrontMember, PartyFlankPos, PartyFlankNeg, EnemyFrontFirst, EnemyFlankPos, EnemyFlankNeg) computed each turn
	by npctrack.c. With axis = enemy centre - party centre, "depth" is the
	dot product of a position with the axis and "side" is the cross
	product (positive = left of the party when facing the enemies).
*/
#include "u6.h"

#undef abs
#include <math.h>

/*when 0, the next COMBAT_showHit() call skips the hit flash*/
/*1968*/int ShowHitFlash = 1;
/*object firing the current missile (-1: none); lets Missiles() find
  the object hit at the impact tile (MissileHitObj)*/
/*196A*/int MissileShooter = -1;

/*range table: RangeTable[|dx|][|dy|] is the "combat distance" between two
  tiles (roughly max + min/2); everything 8 or more tiles away in one
  axis is treated as 9 by COMBAT_range()*/
/*196C*/unsigned char RangeTable[][8] = {
	0,1,2,3,4,5,6,7,
	1,1,2,3,4,5,6,7,
	2,2,2,3,4,5,6,7,
	3,3,3,4,5,6,7,7,
	4,4,4,5,6,7,7,8,
	5,5,5,6,7,7,8,8,
	6,6,6,7,7,8,8,8,
	7,7,7,7,8,8,8,8
};

/*for triple-crossbow & magic wind:
  SpreadDirTable[dy + 5][dx + 5] gives, for a target at offset (dx,dy), the two
  directions (high nibble, low nibble) in which the side missiles are
  spread around the aimed tile (roughly perpendicular to the shot)*/
/*19AC*/unsigned char SpreadDirTable[][11] = {
/*	0x42,0x52,0x52,0x52,0x52,0x62,0x63,0x63,0x63,0x63,0x64,
	0x41,0x42,0x52,0x52,0x52,0x62,0x63,0x63,0x63,0x64,0x74,
	0x41,0x41,0x42,0x52,0x52,0x62,0x63,0x63,0x64,0x74,0x74,
	0x41,0x41,0x41,0x42,0x52,0x62,0x63,0x64,0x74,0x74,0x74,
	0x41,0x41,0x41,0x41,0x42,0x62,0x64,0x74,0x74,0x74,0x74,
	0x40,0x40,0x40,0x40,0x40,0x00,0x04,0x04,0x04,0x04,0x04,
	0x30,0x30,0x30,0x30,0x20,0x26,0x06,0x05,0x05,0x05,0x05,
	0x30,0x30,0x30,0x20,0x27,0x26,0x16,0x06,0x05,0x05,0x05,
	0x30,0x30,0x20,0x27,0x27,0x26,0x16,0x16,0x06,0x05,0x05,
	0x30,0x20,0x27,0x27,0x27,0x26,0x16,0x16,0x16,0x06,0x05,
	0x20,0x27,0x27,0x27,0x27,0x26,0x16,0x16,0x16,0x16,0x06*/
#define MD(dir_0,dir_1) (((dir_0)<<4)|(dir_1))
	MD(4,2),MD(5,2),MD(5,2),MD(5,2),MD(5,2),MD(6,2),MD(6,3),MD(6,3),MD(6,3),MD(6,3),MD(6,4),
	MD(4,1),MD(4,2),MD(5,2),MD(5,2),MD(5,2),MD(6,2),MD(6,3),MD(6,3),MD(6,3),MD(6,4),MD(7,4),
	MD(4,1),MD(4,1),MD(4,2),MD(5,2),MD(5,2),MD(6,2),MD(6,3),MD(6,3),MD(6,4),MD(7,4),MD(7,4),
	MD(4,1),MD(4,1),MD(4,1),MD(4,2),MD(5,2),MD(6,2),MD(6,3),MD(6,4),MD(7,4),MD(7,4),MD(7,4),
	MD(4,1),MD(4,1),MD(4,1),MD(4,1),MD(4,2),MD(6,2),MD(6,4),MD(7,4),MD(7,4),MD(7,4),MD(7,4),
	MD(4,0),MD(4,0),MD(4,0),MD(4,0),MD(4,0),MD(0,0),MD(0,4),MD(0,4),MD(0,4),MD(0,4),MD(0,4),
	MD(3,0),MD(3,0),MD(3,0),MD(3,0),MD(2,0),MD(2,6),MD(0,6),MD(0,5),MD(0,5),MD(0,5),MD(0,5),
	MD(3,0),MD(3,0),MD(3,0),MD(2,0),MD(2,7),MD(2,6),MD(1,6),MD(0,6),MD(0,5),MD(0,5),MD(0,5),
	MD(3,0),MD(3,0),MD(2,0),MD(2,7),MD(2,7),MD(2,6),MD(1,6),MD(1,6),MD(0,6),MD(0,5),MD(0,5),
	MD(3,0),MD(2,0),MD(2,7),MD(2,7),MD(2,7),MD(2,6),MD(1,6),MD(1,6),MD(1,6),MD(0,6),MD(0,5),
	MD(2,0),MD(2,7),MD(2,7),MD(2,7),MD(2,7),MD(2,6),MD(1,6),MD(1,6),MD(1,6),MD(1,6),MD(0,6)
#undef MD
};

/*1A25*/static int NumEquipWeapons = 0;/*number of equiped weapons in EquipWeapons*/
/*1A27*/static int CurEquipWeapon = 0;/*index in EquipWeapons of the weapon currently attacking*/

/*distance in tiles (max(|dx|,|dy|)) between an object and a location*/
/*C_2337_000B*/COMBAT_TileDistance(int objNum, int x, int y) {
	int delta_x, delta_y;

	delta_x = abs(GetX(objNum) - x);
	delta_y = abs(GetY(objNum) - y);
	if(delta_x > delta_y)
		return delta_x;
	else
		return delta_y;
}

/*end the "charm" effect: restore the original alignment (kept in
  NPCFlag) and give the NPC back its normal mode*/
/*C_2337_005E*/COMBAT_stopCharm(int objNum) {
	ClrCharmed(objNum);
	SetAlignment(objNum, GetBKAlignment(objNum));
	if(IsPlrControl(objNum))
		SetAlignment(objNum, GOOD);
	if(InCombat)
		NPCMode[objNum] = NPCComMode[objNum];
	else if(SoloMember < 0)/*party mode*/
		NPCMode[objNum] = AI_FOLLOW;
	else/*solo mode*/
		NPCMode[objNum] = AI_MOTIONLESS;
}

/*enter combat mode: every party member takes its combat mode
  (NPCComMode), the controlled member (first able one in party mode, the
  solo member otherwise) gets AI_COMMAND, the combat icon is drawn and
  the combat music starts*/
/*C_2337_00C8*/COMBAT_begin() {
	int partyIdx, mouseSave;

	InCombat = 1;
	for(partyIdx = 0; partyIdx < PartySize; partyIdx ++) {
		if(!IsCharmed(Party[partyIdx]))
			NPCMode[Party[partyIdx]] = NPCComMode[Party[partyIdx]];
		/*in combat Leader[] holds the melee opponent (see __DoHit);
		  pointing to itself means "none"*/
		Leader[Party[partyIdx]] = Party[partyIdx];
	}
	if(SoloMember < 0) {
		for(
			partyIdx = 0;
			(IsIncapacitated(Party[partyIdx]) || IsDraggedUnder(Party[partyIdx])) && partyIdx < PartySize;
			partyIdx ++
		);
	} else {
		partyIdx = SoloMember;
	}
	SavedNPCMode = NPCMode[Party[partyIdx]];
	NPCMode[Party[partyIdx]] = AI_COMMAND;
	mouseSave = CON_mouseOffAt(152, 176, 167, 191);
	GR_PUT_TILE_SCREEN(TIL_BUTTON_COMBAT_ON, 152, 176);/*"combat on" icon*/
	CON_mouseOn(mouseSave);
	MUS_PlaySong(7);/*combat song*/
}

/*leave combat mode: party members follow again (or stay put in solo
  mode), the combat icon is cleared and the combat music stopped*/
/*C_2337_01BC*/COMBAT_breakOff() {
	int mouseSave;
	int partyIdx, newMode;

	InCombat =
	EnemyWarningGiven = 0;
	if(SoloMember < 0)
		newMode = AI_FOLLOW;
	else
		newMode = AI_MOTIONLESS;
	for(partyIdx = 0; partyIdx < PartySize; partyIdx ++) {
		if(IsDead(Party[partyIdx])) {
			NPCMode[Party[partyIdx]] = AI_MOTIONLESS;
		} else if(!IsCharmed(Party[partyIdx])) {
			NPCMode[Party[partyIdx]] = newMode;
			if(newMode == AI_FOLLOW)
				Leader[Party[partyIdx]] = partyIdx;
		}
	}
	if(SoloMember < 0)
		partyIdx = 0;
	else
		partyIdx = SoloMember;
	SavedNPCMode = NPCMode[Party[partyIdx]];
	NPCMode[Party[partyIdx]] = AI_COMMAND;
	mouseSave = CON_mouseOffAt(152, 176, 167, 191);
	GR_PUT_TILE_SCREEN(TIL_BUTTON_COMBAT_OFF, 152, 176);/*"combat off" icon*/
	CON_mouseOn(mouseSave);
	if(MUS_MIndex == 7)
		MUS_PlayAreaSong();
}

/*move an object to a random free spot at most 3 tiles away (8 tries);
  with onScreenNotOk the spot must be outside the visible 11x11 area.
  returns 1 if the object was moved*/
/*C_2337_02DC*/COMBAT_TryTeleport(int objNum, int onScreenNotOk) {
	int tries, x, y, z, new_x, new_y;

	if(objNum < 0)
		return 0;
	x = GetX(objNum);
	y = GetY(objNum);
	z = GetZ(objNum);
	for(tries = 0; tries < 8; tries ++) {
		new_x = OSI_rand(1, 4) + OSI_rand(1, 4) + x - 5;
		new_y = OSI_rand(1, 4) + OSI_rand(1, 4) + y - 5;
		if(
			onScreenNotOk &&
			MapX - 5 <= new_x && new_x <= MapX + 5 &&
			MapY - 5 <= new_y && new_y <= MapY + 5
		) continue;
		if(CanMoveTo(objNum, new_x, new_y)) {
			MoveObj(objNum, new_x, new_y, z);
			return 1;
		}
	}
	return 0;
}

/*effective strength (-3 when cursed, minimum 1)*/
/*C_2337_03E7*/GetStr(int objNum) {
	int value = STREN[objNum];
	if(IsCursed(objNum)) {
		if((value -= 3) < 1)
			value = 1;
	}
	return value;
}

/*effective dexterity (-3 when cursed, minimum 1; 1 when asleep, or for
  everyone but the party during "time stop")*/
/*C_2337_0411*/GetDex(int objNum) {
	int value = DEXTE[objNum];
	if(IsCursed(objNum)) {
		if((value -= 3) < 1)
			value = 1;
	}
	if(SpellFx[14]/*time stop*/ && !IsPlrControl(objNum))
		value = 1;
	if(IsAsleep(objNum))
		value = 1;
	return value;
}

/*effective intelligence (-3 when cursed, minimum 1)*/
/*C_2337_0458*/GetInt(int objNum) {
	int value = INTEL[objNum];
	if(IsCursed(objNum)) {
		if((value -= 3) < 1)
			value = 1;
	}
	return value;
}

/*intelligence contest for spells: returns 1 when the target resists,
  i.e. when INT(target)/2 + 15 - INT(caster) > 1d30*/
/*C_2337_0482*/COMBAT_ResistsSpell(int casterObj, int targetObj) {
	if(GetInt(targetObj) / 2 + 15 - GetInt(casterObj) > OSI_rand(1, 30))
		return 1;
	return 0;
}

/*uncalled*/
/*level from experience: 1 + number of halvings to get 100xp or less*/
/*__2337_04BE*/ComputeLevel(int objNum) {
	unsigned lvl, xp;

	xp = ExpPoints[objNum];
	lvl = 1;
	while(xp > 99) {
		lvl ++;
		xp >>= 1;
	}
	return lvl;
}

/*C_2337_04E1*/GetLevel(int objNum) {
	return Level[objNum];
}

/*add experience points (capped at 9999)*/
/*C_2337_04F4*/AddXP(int objNum, int xp) {
	if(ExpPoints[objNum] < 9999 - xp)
		ExpPoints[objNum] += xp;
	else
		ExpPoints[objNum] = 9999;
}

/*maximum hit points: 30 per level, between 1 and 255*/
/*C_2337_0529*/MaxHP(int objNum) {
	int hp;
	
	hp = Level[objNum] * 30;
	if(hp == 0)
		hp = 1;
	if(hp > 255)
		return 255;
	return hp;
}

/*maximum magic points: 2*INT for the avatar, INT for a mage,
  INT/2 for a swashbuckler or long-caped musician, 0 otherwise*/
/*C_2337_0559*/MaxMagic(int objNum) {
	int objType, mana;

	mana = 0;
	objType = GetType(objNum);
	if(objType == OBJ_AVATAR)
		mana = INTEL[objNum] << 1;
	if(objType == OBJ_MAGE)
		mana = INTEL[objNum];
	else if(objType == OBJ_SWASHBUCKLER || objType == OBJ_MUSICIAN_LONG_CAPE)
		mana = INTEL[objNum] >> 1;
	return mana;
}

/*object type==>corpse type*/
/*1A29*/int DeadShape[] = {
	OBJ_HORSE,TypeFrame(OBJ_HORSES_CARCASS,1),
	OBJ_DAEMON,TypeFrame(OBJ_DEAD_BODY,0),
	OBJ_GARGOYLE,TypeFrame(OBJ_DEAD_BODY,0),
	OBJ_TROLL,TypeFrame(OBJ_DEAD_BODY,0),
	OBJ_HEADLESS,TypeFrame(OBJ_DEAD_BODY,1),
	OBJ_CYCLOPS,TypeFrame(OBJ_DEAD_CYCLOPS,3),
	OBJ_MERCHANT,TypeFrame(OBJ_DEAD_BODY,2),
	OBJ_VILLAGER,TypeFrame(OBJ_DEAD_BODY,2),
	OBJ_WOMAN,TypeFrame(OBJ_DEAD_BODY,3),
	OBJ_MAGE,TypeFrame(OBJ_DEAD_BODY,3),
	OBJ_GUARD,TypeFrame(OBJ_DEAD_BODY,4),
	OBJ_PEASANT,TypeFrame(OBJ_DEAD_BODY,5),
	OBJ_SWASHBUCKLER,TypeFrame(OBJ_DEAD_BODY,5),
	OBJ_FIGHTER,TypeFrame(OBJ_DEAD_BODY,6),
	OBJ_AVATAR,TypeFrame(OBJ_DEAD_BODY,7),
	OBJ_MUSICIAN_LONG_CAPE,TypeFrame(OBJ_DEAD_BODY,7),
	OBJ_MUSICIAN_SHORT_CAPE,TypeFrame(OBJ_DEAD_BODY,7),
	OBJ_CHILD,TypeFrame(OBJ_DEAD_BODY,8),
	OBJ_FARMER,TypeFrame(OBJ_DEAD_BODY,8),
	OBJ_FARMER_2,TypeFrame(OBJ_DEAD_BODY,8),
	OBJ_JESTER,TypeFrame(OBJ_DEAD_BODY,8),
	OBJ_SKELETON,TypeFrame(OBJ_DEAD_BODY,9),
	OBJ_WINGED_GARGOYLE,TypeFrame(OBJ_DEAD_GARGOYLE,3),
	-1
};

/*set to "unconscious" type: called when an actor falls asleep or dies.
  Humanoids/horses take their corpse shape from DeadShape (multi-tile
  monsters keep their real shape in OrigShapeType). When dead: monsters
  without corpse vanish (with a flash for the MONSTER_VANISHES ones), a gazer
  turns into insects, slimes are re-joined, dragons removed, and the
  bleeding ones leave a pool of blood*/
/*C_2337_05B4*/SetTypeUnconscious(int objNum) {
	int objType, objCls;
	int *pTypUnc;
	int newObj, isDead;

	objType = GetType(objNum);
	isDead = IsDead(objNum);
	if(objType == OBJ_MOUNTED_HORSE) {
		DismountHorse(objNum);
		objType = GetType(objNum);
	} else if(objType == OBJ_DRAGON || objType >= OBJ_GIANT_SCORPION || objType == OBJ_SILVER_SERPENT || objType == OBJ_SHIP) {
		OrigShapeType[objNum] = ObjShapeType[objNum];
	}
	objCls = GetMonsterClass(objType);
	/*pairs {type, corpse}; pTypUnc ends pointing to the corpse
	  (or to the -1 terminator)*/
	for(pTypUnc = DeadShape; *pTypUnc >= 0; pTypUnc ++) {
		if(*pTypUnc ++ == objType)
			break;
	}
	if(*pTypUnc >= 0 && !IsMonsterVanishing(objCls)) {
		if(objType == OBJ_HORSE) {
			if(isDead) {
				DeleteObj(OBJ_getHead(objNum));
				ObjShapeType[objNum] = TypeFrame(*pTypUnc, 0);
			}
		} else {
			ObjShapeType[objNum] = TypeFrame(*pTypUnc, 0);
		}
	} else if(isDead) {
		if(objCls >= 0) {
			if(IsMonsterGazer(objCls))
				EGG_generate(OBJ_INSECTS, GetX(objNum), GetY(objNum), MapZ);

			if(IsMonsterNoCorpse(objCls) || IsMonsterVanishing(objCls)) {
				DeleteObj(objNum);
				if(IsMonsterVanishing(objCls) && CLOSE_ENOUGH(5, GetX(objNum), GetY(objNum), MapX, MapY)) {
					DrawMapWindow(1);
					ShowObjects();
					ScreenFade = 1;
					DrawMapWindow(1);
				}
			}
		}
		if(objType == OBJ_SLIME)
			OBJ_UpdateSlimeFrames();
		if(objType == OBJ_DRAGON)
			DeleteObj(objNum);
	}
	if(isDead) {
		if(
			(objCls >= 0 && IsMonsterBleeding(objCls)) ||
			objType == OBJ_FARMER_2 || objType == OBJ_MUSICIAN_SHORT_CAPE
		) {
			newObj = AddMapObj(OBJ_BLOOD, LOCAL, 0, GetX(objNum), GetY(objNum), MapZ);
			if(newObj >= 0)
				SetFrame(newObj, OSI_rand(0, 2));
		}
	}
	VisibilityDirty = 1;
}

/*can viewerObj see (and so target) targetObj? not if it is invisible
  (unless both are party members), dragged under, a buried corpser, an
  "ignore" tile or on another level*/
/*C_2337_0837*/COMBAT_canSee(int viewerObj, int targetObj) {
	int isVisible;

	isVisible = !IsInvisible(targetObj);

	if(
		IsPlrControl(viewerObj) &&
		!isVisible && targetObj < 0x100 && IsPlrControl(targetObj)
	) isVisible = 1;

	if(targetObj < 0x100 && IsDraggedUnder(targetObj))
		isVisible = 0;

	if(ObjShapeType[targetObj] == TypeFrame(OBJ_CORPSER, 0))
		isVisible = 0;

	if(IsTileIg(TILE_FRAME(targetObj)))
		isVisible = 0;

	if(GetZ(viewerObj) != GetZ(targetObj))
		isVisible = 0;

	return isVisible;
}

/*pick the object targeted at (x,y) by viewerObj: the first visible
  living actor, else the first visible object, else the first hidden
  object; -1 if the tile is empty*/
/*C_2337_08F1*/COMBAT_GetTargetAt(int viewerObj, int x, int y) {
	int result;
	int liveNpc, firstHidden;

	firstHidden = -1;
	/*first "canSee" object*/
	for(result = FindLoc(x, y, MapZ); result >= 0; result = NextLoc()) {
		if(COMBAT_canSee(viewerObj, result))
			break;
		if(firstHidden < 0)
			firstHidden = result;
	}
	/*first "canSee" notDead NPC*/
	for(liveNpc = result; liveNpc >= 0; liveNpc = NextLoc()) {
		if(COMBAT_canSee(viewerObj, liveNpc)) {
			if(liveNpc < 0x100 && !IsDead(liveNpc))
				break;
			continue;
		}
		if(firstHidden < 0)
			firstHidden = liveNpc;
	}
	/* */
	if(liveNpc >= 0)
		result = liveNpc;
	else if(firstHidden >= 0 && result < 0)
		result = firstHidden;

	return result;
}

/*armours ... and their protection (ArmorValues)*/
/*1A87*/int ArmorTypes[] = {
	OBJ_LEATHER_HELM,OBJ_CHAIN_COIF,OBJ_IRON_HELM,OBJ_SPIKED_HELM,OBJ_WINGED_HELM,OBJ_BRASS_HELM,OBJ_GARGOYLE_HELM,OBJ_MAGIC_HELM,
	OBJ_WOODEN_SHIELD,OBJ_CURVED_HEATER,OBJ_WINGED_SHIELD,OBJ_KITE_SHIELD,OBJ_SPIKED_SHIELD,OBJ_BLACK_SHIELD,OBJ_DOOR_SHIELD,OBJ_MAGIC_SHIELD,
	OBJ_CLOTH_ARMOR,OBJ_LEATHER_ARMOR,OBJ_RING_MAIL,OBJ_SCALE_MAIL,OBJ_CHAIN_MAIL,OBJ_PLATE_MAIL,OBJ_MAGIC_ARMOR,OBJ_MAIN_GAUCHE,
	OBJ_SPIKED_COLLAR,OBJ_PROTECTION_RING
};
/*1ABB*/unsigned char ArmorValues[] = {
	1,2,3,3,2,2,3,5,
	2,3,3,3,2,2,4,5,
	1,2,3,4,5,7,10,1,
	2,5
};

/*protection given by an armour object (0 if not an armour)*/
/*C_2337_0983*/GetArmorPoints(int objNum) {
	int i, points;

	points = 0;
	for(i = 0; i < 26; i ++) {
		if(ArmorTypes[i] == GetType(objNum)) {
			points = ArmorValues[i];
			break;
		}
	}
	return points;
}

/*total protection of an actor: equipped armours, -3 cursed,
  +3 "protected"*/
static /*C_2337_09BF*/__GetProtection(int objNum) {
	int protection, invObj;

	protection = 0;
	for(invObj = FindInv(objNum); invObj >= 0; invObj = NextInv()) {
		if(GetCoordUse(invObj) == EQUIP)
			protection += GetArmorPoints(invObj);
	}
	if(IsCursed(objNum))
		protection -= 3;
	if(IsProtected(objNum))
		protection += 3;

	return protection;
}

/*weapons with a range > 1 ... and their range (WeaponRanges)*/
/*1AD5*/int RangedWeaponTypes[] = {
	OBJ_MORNING_STAR,OBJ_HALBERD,OBJ_DAGGER,OBJ_FLASK_OF_OIL,OBJ_SLING,OBJ_THROWING_AXE,OBJ_SPEAR,OBJ_BOW,
	OBJ_CROSSBOW,OBJ_SPELLBOOK,OBJ_CHARGE,OBJ_BOOMERANG,OBJ_TRIPLE_CROSSBOW,OBJ_SHIP,OBJ_ZU_YLEM,OBJ_MAGIC_BOW,
	OBJ_LIGHTNING_WAND,OBJ_FIRE_WAND
};
/*1AF9*/unsigned char WeaponRanges[] = {
	2,2,3,5,4,3,4,5,
	7,7,5,5,7,5,4,7,
	7,7
};

/*range of a weapon (or of an actor's natural attack): 1 by default*/
static /*C_2337_0A14*/__GetWeaponRange(int objNum) {
	int i, range;

	range = 1;
	for(i = 0; i < 18; i ++) {
		if(GetType(objNum) == RangedWeaponTypes[i]) {
			range = WeaponRanges[i];
			break;
		}
	}
	return range;
}

/*is objNum engaged in melee (so that it can't use a ranged weapon)?
  in combat modes Leader[] holds the last adjacent attacker (see
  __DoHit). returns 1 if that one is still able, at most 1 tile away
  and not frozen by "time stop"*/
/*C_2337_0A4E*/static COMBAT_IsEngagedInMelee(int objNum) {
	int engagedObj;

	if(NPCMode[objNum] > AI_FOLLOW && NPCMode[objNum] < AI_PATROL_NS && Leader[objNum] != objNum) {
		engagedObj = Leader[objNum];
		if(
			ObjShapeType[engagedObj] &&
			!IsDead(engagedObj) &&
			!IsAsleep(engagedObj) &&
			!IsParalyzed(engagedObj) &&
			!IsDraggedUnder(engagedObj) &&
			CLOSE_ENOUGH_S(2, GetX(objNum), GetY(objNum), GetX(engagedObj), GetY(engagedObj))
		) {
			if(SpellFx[14] == 0)
				return 1;
			if(IsPlrControl(engagedObj))
				return 1;
		}
	}
	return 0;
}

/*fire one missile (tile numTile, 0: invisible) from objNum toward
  *pX_dst,*pY_dst; on return they hold where it stopped and MissileHitObj the
  object found there. returns non-zero if it reached its destination*/
/*C_2337_0B3F*/COMBAT_Missile(int objNum, int *pX_dst, int *pY_dst, int max_range, int numTile) {
	int ret;

	MissileShooter = objNum;
	ret = Missiles(GetX(objNum), GetY(objNum), 1, pX_dst, pY_dst, max_range, numTile, 0);
	MissileShooter = -1;

	return ret;
}

/*range weapon management: show the missile(s) of a ranged attack and
  handle its consequences: ammunition used, thrown weapon dropped at the
  impact tile (or lost in water/void, or destroyed if it hits an actor),
  burning oil, zu ylem (sleep), wands that may vanish (1 in 100).
  *pX_dest,*pY_dest are updated to the impact tile. returns 1*/
/*C_2337_0B99*/static COMBAT_FireRangedWeapon(int att_objNum, int *pX_dest, int *pY_dest, int weap_objNum) {
	int typ;
	int foundSpare, itemObj, invChanged, range, dist_x;
	int dist_y, missileTile, numBolts, spreadDirs, landTile;

	typ = GetType(weap_objNum);
	range = __GetWeaponRange(weap_objNum);

	if(typ == OBJ_BOW || typ == OBJ_MAGIC_BOW)
		missileTile = BaseTile[OBJ_ARROW];
	else if(typ == OBJ_CROSSBOW || typ == OBJ_TRIPLE_CROSSBOW)
		missileTile = BaseTile[OBJ_BOLT];
	else if(typ == OBJ_LIGHTNING_WAND)
		missileTile = TIL_MISSILE_0;
	else if(typ == OBJ_FIRE_WAND)
		missileTile = TIL_MISSILE_1;
	else if(typ == OBJ_SHIP)
		missileTile = TIL_MISSILE_7;
	else if(typ == OBJ_SLING || (range > 1 && att_objNum == weap_objNum))
		missileTile = TIL_MISSILE_6;
	else if(att_objNum != weap_objNum)
		missileTile = TILE_FRAME(weap_objNum);
	else
		missileTile = 0;

	if(typ == OBJ_TRIPLE_CROSSBOW) {/*triple crossbow: 3 bolts, the side ones 1 tile apart*/
		spreadDirs = SpreadDirTable[*pY_dest - GetY(att_objNum) + 5][*pX_dest - GetX(att_objNum) + 5];
		TripleBoltX[0] = *pX_dest;
		TripleBoltX[1] = *pX_dest + DirIncrX[spreadDirs >> 4];
		TripleBoltX[2] = *pX_dest + DirIncrX[spreadDirs & 0xf];
		TripleBoltY[0] = *pY_dest;
		TripleBoltY[1] = *pY_dest + DirIncrY[spreadDirs >> 4];
		TripleBoltY[2] = *pY_dest + DirIncrY[spreadDirs & 0xf];
		numBolts = TestObj(att_objNum, OBJ_BOLT);
		if(numBolts > 3)
			numBolts = 3;

		MissileShooter = att_objNum;
		Missiles(GetX(att_objNum), GetY(att_objNum), numBolts, TripleBoltX, TripleBoltY, range, missileTile, 0);
		MissileShooter = -1;

		*pX_dest = TripleBoltX[0];
		*pY_dest = TripleBoltY[0];
	} else {
		COMBAT_Missile(att_objNum, pX_dest, pY_dest, range, missileTile);
	}
	invChanged = 0;
	switch(typ) {
		case OBJ_BOW:
		case OBJ_CROSSBOW:
		case OBJ_TRIPLE_CROSSBOW:
		case OBJ_MAGIC_BOW:
			if(typ == OBJ_BOW || typ == OBJ_MAGIC_BOW)
				itemObj = FindInvType(att_objNum, OBJ_ARROW, -1);
			else
				itemObj = FindInvType(att_objNum, OBJ_BOLT, -1);
			if(typ == OBJ_TRIPLE_CROSSBOW)
				numBolts = 3;
			else
				numBolts = 1;
			if(itemObj >= 0) {
				if(GetQuan(itemObj) > numBolts)
					SubQuan(itemObj, numBolts);
				else
					DeleteObj(itemObj);
				invChanged = 1;
			}
		break;
		case OBJ_SPEAR:
		case OBJ_THROWING_AXE:
		case OBJ_DAGGER:
			dist_x = *pX_dest;
			dist_y = *pY_dest;
			dist_x = abs(dist_x - GetX(att_objNum));
			dist_y = abs(dist_y - GetY(att_objNum));
			if(dist_x > 1 || dist_y > 1) {/*thrown*/
				/*throw a spare one from the inventory if any,
				  else the readied one*/
				foundSpare = 0;
				for(itemObj = FindInv(att_objNum); itemObj >= 0; itemObj = NextInv()) {
					if(GetCoordUse(itemObj) == INVEN && GetType(itemObj) == typ) {
						foundSpare = 1;
						break;
					}
				}
				if(foundSpare == 0)
					itemObj = weap_objNum;
				invChanged = 1;
				if(MissileHitObj >= 0 && MissileHitObj < 0x100) {/*hit an actor: lost*/
					DeleteObj(itemObj);
				} else {
					landTile = GetTileAtXYZ(MissileEnd[0], MissileEnd[1], MapZ);
					/*typ now holds terrain flags*/
					typ = TerrainType[landTile];
					/*lost in deep water or in the void, else dropped*/
					if(
						(
						((typ & TERRAIN_WET) && (typ & TERRAIN_IMPASSABLE)) || (landTile >= TIL_0FC && landTile <= TIL_HIDDEN)
						) && FindLoc(MissileEnd[0], MissileEnd[1], MapZ) < 0
					) {
						DeleteObj(itemObj);
					} else {
						MoveObj(itemObj, MissileEnd[0], MissileEnd[1], MapZ);
						SetOwned(itemObj);
					}
					ShowObjects();
				}
			}
		break;
		case OBJ_FLASK_OF_OIL:
			dist_x = *pX_dest;
			dist_y = *pY_dest;
			dist_x = abs(dist_x - GetX(att_objNum));
			dist_y = abs(dist_y - GetY(att_objNum));
			invChanged = 1;
			TakeObj(att_objNum, OBJ_FLASK_OF_OIL, 1);
			typ = TerrainType[GetTileAtXYZ(MissileEnd[0], MissileEnd[1], MapZ)];
			if(!(typ & TERRAIN_WET)) {/*not wet: fire field*/
				AddMapObj(OBJ_FIRE_FIELD, LOCAL, 0, MissileEnd[0], MissileEnd[1], MapZ);
				ShowObjects();
			}
		break;
		case OBJ_ZU_YLEM:
			TakeObj(att_objNum, OBJ_ZU_YLEM, 1);
			if(MissileHitObj >= 0 && MissileHitObj < 0x100)
				SPELL_SleepTarget(att_objNum, MissileHitObj);/*"sleep"*/
		break;
		case OBJ_LIGHTNING_WAND:
		case OBJ_FIRE_WAND:
			if(OSI_rand(1, 100) == 37) {
				DeleteObj(weap_objNum);
				CON_printf(RingVanishedMsg, /*1BC8*/"wand");
				PromptFlag = 1;
			}
		break;
	}
	if(invChanged && StatusDisplay == CMD_STAT_EQUIP && Party[StatusMember] == att_objNum)
		StatusDirty = 1;
	return 1;
}

/*weapons ... and their damage (WeaponDamages)*/
/*1B0B*/int WeaponTypes[] = {
	OBJ_SPIKED_HELM,OBJ_SLING,OBJ_CLUB,OBJ_MAIN_GAUCHE,OBJ_SPEAR,OBJ_THROWING_AXE,OBJ_DAGGER,OBJ_MACE,
	OBJ_MORNING_STAR,OBJ_BOW,OBJ_CROSSBOW,OBJ_SWORD,OBJ_TWO_HANDED_HAMMER,OBJ_TWO_HANDED_AXE,OBJ_TWO_HANDED_SWORD,OBJ_HALBERD,
	OBJ_BOOMERANG,OBJ_TRIPLE_CROSSBOW,OBJ_GLASS_SWORD,OBJ_SPIKED_SHIELD,OBJ_CANNON,OBJ_FLASK_OF_OIL,OBJ_MAGIC_BOW,OBJ_ROLLING_PIN,
	OBJ_SCYTHE,OBJ_PITCHFORK,OBJ_PICK,OBJ_SHOVEL,OBJ_HOE,OBJ_CLEAVER,OBJ_KNIFE,OBJ_DECORATIVE_SWORD,
	OBJ_SHIP,OBJ_ZU_YLEM,OBJ_STAFF,OBJ_LIGHTNING_WAND,OBJ_FIRE_WAND
};
/*1B55*/unsigned char WeaponDamages[] = {
	 4, 6,  8, 8,10,10, 6,15,
	15,10, 12,15,20,20,20,30,
	 8,12,255, 4,90, 4,20, 2,
	 6, 4,  4, 4, 4, 4, 4, 4,
	30, 0,  4,30,20
};

/*maximum damage of a weapon, or of an actor's natural attack (from its
  monster class); -1 if the object is not a weapon*/
/*C_2337_10E4*/GetDamagePoints(int objNum) {
	int i, damage;
	int objCls;

	damage = -1;
	if(objNum < 0x100)
		objCls = GetMonsterClass(GetType(objNum));
	else
		objCls = -1;
	if(objCls >= 0) {
		damage = MonsterDamage[objCls];
	} else {
		for(i = 0; i < 37; i ++)
			if(GetType(objNum) == WeaponTypes[i]) {
				damage = WeaponDamages[i];
				break;
			}
	}
	return damage;
}

/*copy the state of an actor into another one (slime division)*/
/*C_2337_115F*/CloneObject(int src_objNum, int dst_objNum) {
	ObjStatus[dst_objNum] = ObjStatus[src_objNum];
	if(GetType(src_objNum) != OBJ_SLIME)
		ObjShapeType[dst_objNum] = ObjShapeType[src_objNum];
	Amount[dst_objNum] = Amount[src_objNum];
	NPCStatus[dst_objNum] = NPCStatus[src_objNum];
	STREN[dst_objNum] = STREN[src_objNum];
	DEXTE[dst_objNum] = DEXTE[src_objNum];
	INTEL[dst_objNum] = INTEL[src_objNum];
	MAGIC[dst_objNum] = MAGIC[src_objNum];
	MovePts[dst_objNum] = 0;
	ExpPoints[dst_objNum] = ExpPoints[src_objNum];
	HitPoints[dst_objNum] = HitPoints[src_objNum];
	NPCMode[dst_objNum] = NPCMode[src_objNum];
	NPCComMode[dst_objNum] = NPCComMode[src_objNum];
	SchedIndex[dst_objNum] = SchedIndex[src_objNum];
	Leader[dst_objNum] = Leader[src_objNum];
	NPCFlag[dst_objNum] = NPCFlag[src_objNum];
}

/*flash the "hit" tile over an object (if on screen and ShowHitFlash set) and
  wake it up if it was asleep*/
/*C_2337_121F*/COMBAT_showHit(int objNum) {
	if(ShowHitFlash) {
		int x = GetX(objNum) - MapX + 5;
		int y = GetY(objNum) - MapY + 5;
		if(x >= 0 && x < 11 && y >= 0 && y < 11) {
			/*display hit?*/
			ShowObjects();
			ShowObject(TIL_HIT, x, y, 1);
			DrawMapWindow(1);
			MUS_PlaySfx(4, 0);
			ShowObjects();
			VisibilityDirty = 1;
		}
		if(objNum < 0x100)
			SetHurtThisTurn(objNum);
	}
	if(IsAsleep(objNum)) {
		ClrAsleep(objNum);
		ObjShapeType[objNum] = OrigShapeType[objNum];
		VisibilityDirty = 1;
	}
	ShowHitFlash = 1;
}

/*inflict damage to an object. Actors: Lord British is immortal, a
  skiff/raft passes the damage to a random party member, a ship becomes
  a raft, others die (karma, party removal, inventory dropped) or lose
  hit points (slimes may divide). Tangle vine parts kill their pod.
  Breakable items lose "quantity" points and are broken at 0.
  returns the experience gained by killing a hostile monster*/
/*C_2337_12F5*/LoseHP(int objNum, int damage) {
	int i;
	int new_x, new_y, otherObj, objTyp;
	int mouseSave, objCls, xpGained, savedTextOn;

	if(damage < 0)
		return 0;
	xpGained = 0;
	objTyp = GetType(objNum);
	if(objTyp == OBJ_BALLOON_INFLATED)
		return 0;

	savedTextOn = TextOn; TextOn = 1;

	if(objNum < 0x100) {
		COMBAT_showHit(objNum);
		if(IsMonsterLordBritish(GetMonsterClass(objTyp))) {
			HitPoints[objNum] = 255;
		} else if(objTyp == OBJ_SKIFF || objTyp == OBJ_RAFT) {/*Skiff/Raft*/
			otherObj = Party[0];
			if(PartySize > 1)
				otherObj = Party[OSI_rand(0, PartySize - 1)];
			LoseHP(otherObj, damage);
			COMBAT_displaySeverity(otherObj);
		} else if(HitPoints[objNum] <= damage) {/*too many hits?*/
			if(LastAttackTarget == objNum)
				LastAttackTarget = -1;
			if(objTyp == OBJ_SHIP) {
				OBJ_MoveCreatureParts(objNum, GetX(objNum), GetY(objNum), 1);
				ObjShapeType[objNum] = TypeFrame(OBJ_RAFT, 0);
				HitPoints[objNum] = 10;
				VisibilityDirty = 1;
			} else {
				if(HitPoints[objNum] != 0) {
					if(GetAlignment(objNum) == NEUTRAL)/*killing an innocent*/
						SubKarma(5);
					HitPoints[objNum] = 0;
					SetDead(objNum);
					if(Is_ATKPLR(objNum)) {
						objCls = GetMonsterClass(GetType(objNum));
						if(objCls >= 0) {
							/*experience = sum of the class stats / 4*/
							xpGained = (
								/*STR*/MonsterStr[objCls] +
								/*DEX*/MonsterDex[objCls] +
								/*INT*/MonsterInt[objCls] +
								/*ARM*/MonsterArmor[objCls] +
								/*DMG*/MonsterDamage[objCls] +
								/*HIT*/MonsterHP[objCls]
							) >> 2;
						}
					}
					ClrInvisible(objNum);
					SetTypeUnconscious(objNum);
					NPCMode[objNum] = AI_MOTIONLESS;
					/*a dead companion leaves the party*/
					if(IsPlrControl(objNum) && Party[0] != objNum) {
						for(i = 1; i <= PartySize; i ++) {
							if(Party[i] == objNum) {
								if(SoloMember >= i)
									SoloMember --;
								if(Active == i)
									SetPartyMode();
								if(Active > i)
									Active --;
								for( ; i < PartySize; i ++) {
									Party[i] = Party[i + 1];
									strcpy(Names[i], Names[i + 1]);
								}
								PartySize --;
								StatusDirty = 1;
								break;
							}
						}
					}
					OBJ_MakeDeadBody(objNum);
					VisibilityDirty = 1;
				}
				if(Party[Active] == objNum)
					CheckActiveMember();
			}
		} else {
			HitPoints[objNum] -= damage;
			if(NPCMode[objNum] == AI_SHY)
				NPCMode[objNum] = AI_ASSAULT;
			if(IsMonsterSlime(GetMonsterClass(objTyp))) {/*slime splits*/
				for(i = 0;i < 8; i ++) {
					new_x = GetX(objNum) + OSI_rand(0, 2) - 1;
					new_y = GetY(objNum) + OSI_rand(0, 2) - 1;
					if(CanMoveTo(objNum, new_x, new_y)) {
						otherObj = AddMonster(GetType(objNum), new_x, new_y, MapZ);
						if(otherObj >= 0) {
							CloneObject(objNum, otherObj);
							CON_printf(SlimeDividesMsg);
							PromptFlag = 1;
							VisibilityDirty = 1;
						}
						break;
					}
				}
			}
		}
		/*refresh the hit points on the status display*/
		if(IsPlrControl(objNum)) {
			if(StatusDisplay == CMD_STAT_PORTRAIT && Party[StatusMember] == objNum) {
				StatusDirty = 1;
			} else if(StatusDisplay == CMD_STAT_PARTY) {
				for(i = PartyListTop; i < PartySize && i < PartyListTop + 5; i ++) {
					if(Party[i] == objNum) {
						mouseSave = CON_mouseOffAt(176, 6, 311, 103);
						CON_setClip(&(TextWindows[1]));
						CON_gotoxy(14, ((i - PartyListTop) << 1) + 2);
						if(IsPoisoned(objNum))
							CurClip->fgColor = ColorGreen;
						else if(HitPoints[objNum] < 10)
							CurClip->fgColor = ColorRed;
						CON_printf(/*1BCD*/"%3d\n", HitPoints[objNum]);
						CurClip->fgColor = ColorText;
						CON_setClip(&(TextWindows[3]));
						CON_mouseOn(mouseSave);
					}
				}
			}
		}
	} else if(objTyp == OBJ_TANGLE_VINE_VINE) {
		/*vine segment: its quality is the pod (actor) number*/
		HitPoints[GetQual(objNum)] = 0;
		SetDead(GetQual(objNum));
		ClrInvisible(GetQual(objNum));
		NPCMode[GetQual(objNum)] = AI_MOTIONLESS;
		CON_printf(VineKilledMsg);
		PromptFlag = 1;
	} else if(!QuanType(ObjShapeType[objNum]) && GetQuan(objNum) != 0) {
		/*breakable object: quantity is its "hit points"*/
		COMBAT_showHit(objNum);
		/*not for broken/destroyed doors (frame >= 12) or broken chests*/
		if(
			!(objTyp >= OBJ_OAKEN_DOOR && objTyp <= OBJ_STEEL_DOOR && GetFrame(objNum) >= 12) &&
			!(objTyp == OBJ_OPEN_CHEST && GetFrame(objNum) == 3)
		) {
			if(GetQuan(objNum) > damage) {
				SubQuan(objNum, damage);
			} else {
				CON_printf(BrokenMsg, GetObjectString(objNum));
				PromptFlag = 1;
				if(FindInvType(objNum, OBJ_EFFECT, SPELL_TRAP) != -1)
					SpringTrap(objNum);
				if(objTyp == OBJ_MIRROR) {
					MUS_PlaySfx(15, 0);
					SetFrame(objNum, 2);
					VisibilityDirty = 1;
					SubKarma(10);
				} else {
					DeleteObj(objNum);
					VisibilityDirty = 1;
				}
			}
		}
	}

	TextOn = savedTextOn;

	return xpGained;
}

/*does the attack hit? items are always hit, the glass sword always
  hits. Bludgeons (and MONSTER_STR_ATTACK monsters' natural attacks) use STR,
  other weapons DEX: hit if 1d30 >= (DEX(defender) + 30 - STAT) / 2*/
static /*C_2337_18CF*/__AttackTest(int objNum_att, int objNum_def, int objNum_weapon) {
	int objTyp, attackStat;
	int defDex, objCls, strOverDex;

	if(objNum_def >= 0x100)
		return 1;
	objTyp = GetType(objNum_weapon);
	if(objTyp == OBJ_GLASS_SWORD)
		return 1;
	if(objNum_weapon == objNum_att) {
		objCls = GetMonsterClass(objTyp);
		strOverDex = IsMonsterStrAttack(objCls);
	} else {
		strOverDex = (objTyp == OBJ_SPIKED_HELM || objTyp == OBJ_CLUB || objTyp == OBJ_MACE || objTyp == OBJ_TWO_HANDED_HAMMER);
	}
	if(strOverDex)
		attackStat = GetStr(objNum_att);
	else
		attackStat = GetDex(objNum_att);
	defDex = GetDex(objNum_def);

	return OSI_rand(1, 30) >= (defDex + 30 - attackStat) / 2;
}

/*print how badly an actor is wounded (in quarters of its max HP);
  a critically wounded intelligent NPC in a mobile combat mode flees*/
/*C_2337_198F*/COMBAT_displaySeverity(int objNum) {
	int severity;

	if(GetType(objNum) == OBJ_BALLOON_INFLATED || HitPoints[objNum] == 0)
		return;
	severity = (HitPoints[objNum] << 2) / MaxHP(objNum);
	if(severity < 4) {
		CON_printf(/*1BD2*/"`%s ", GetObjectString(objNum));
		PromptFlag = 1;
	}
	if(severity == 0) {/*critical*/
		CON_printf(CriticalMsg);
		if(
			GetInt(objNum) >= 5 &&
			NPCMode[objNum] != AI_BERSEK &&
			NPCMode[objNum] != AI_IMMOBILE &&
			NPCMode[objNum] != AI_COMMAND &&
			NPCMode[objNum] != AI_FRONT &&
			NPCMode[objNum] != AI_FOLLOW &&
			NPCMode[objNum] != AI_MOTIONLESS
		) NPCMode[objNum] = AI_RETREAT;
	} else if(severity == 1) {/*heavily*/
		CON_printf(HeavilyMsg);
	} else if(severity == 2) {/*lightly*/
		CON_printf(LightlyMsg);
	} else if(severity == 3) {/*barely*/
		CON_printf(BarelyMsg);
	}
	if(severity != 0 && severity < 4)
		CON_printf(WoundedMsg);
}

/*dissolvable objects*/
/*1B7A*/int DissolvableTypes[] = {
	OBJ_CHAIN_COIF,OBJ_IRON_HELM,OBJ_SPIKED_HELM,OBJ_WINGED_HELM,OBJ_GARGOYLE_HELM,OBJ_CURVED_HEATER,OBJ_WINGED_SHIELD,OBJ_KITE_SHIELD,
	OBJ_SPIKED_SHIELD,OBJ_DOOR_SHIELD,OBJ_RING_MAIL,OBJ_SCALE_MAIL,OBJ_CHAIN_MAIL,OBJ_PLATE_MAIL,OBJ_MAIN_GAUCHE,OBJ_DAGGER,
	OBJ_SWORD,OBJ_CLEAVER,OBJ_KNIFE,
	0
};
/*robbable objects (food stolen by gremlins)*/
/*1BA2*/int StealableFoodTypes[] = {
	OBJ_LOAF_OF_BREAD,OBJ_PORTION_OF_MEAT,OBJ_ROLLS,OBJ_CAKE,OBJ_CHEESE,OBJ_HAM,OBJ_HORSE_CHOPS,OBJ_MEAT_RIB,
	OBJ_RIBS,OBJ_DEAD_ANIMAL,
	0
};

/*pick a random object in ownerObj's inventory that can be dissolved by
  acid (wantFood == 0) or stolen by a gremlin (wantFood != 0); -1 if none*/
/*C_2337_1A99*/static COMBAT_PickVulnerableItem(int ownerObj, int wantFood/*disolve/rob flag*/) {
	int invObj, picked;
	int *pList;

	picked = -1;
	for(invObj = FindInv(ownerObj); invObj >= 0; invObj = NextInv()) {
		pList = wantFood?StealableFoodTypes:DissolvableTypes;
		while(*pList) {
			if(GetType(invObj) == *(pList++)) {
				if(picked < 0 || OSI_rand(0, 1))
					picked = invObj;
				break;
			}
		}
	}
	return picked;
}

/*The avatar did a bad action and "everyone"
run after him: -5 karma, guards within 32 tiles arrest him, other
neutral NPCs within 6 tiles become vigilantes (scheduled NPCs) or flee*/
/*C_2337_1B0E*/COMBAT_RaiseAlarm() {
	int objNum;

	SubKarma(5);
	for(objNum = 0; objNum < 0x100; objNum ++) {
		if(!ObjShapeType[objNum])
			continue;
		if(GetZ(objNum) != MapZ)
			continue;
		if(IsDead(objNum) || IsPlrControl(objNum) || GetAlignment(objNum) != NEUTRAL)
			continue;
		/*guard*/
		if(GetType(objNum) == OBJ_GUARD) {
			if(COMBAT_TileDistance(objNum, MapX, MapY) < 32)
				NPCMode[objNum] = AI_ARREST;
			continue;
		}

		if(NPCMode[objNum] == AI_IMMOBILE || NPCMode[objNum] == AI_MOTIONLESS)
			continue;
		if(IsIncapacitated(objNum) || IsDraggedUnder(objNum))
			continue;
		if(COMBAT_TileDistance(objNum, MapX, MapY) >= 6)
			continue;
		if(NPCMode[objNum] >= AI_SCHEDULE) {
			if(NPCMode[objNum] <= AI_PATH_FAILED)/*busy walking its schedule path*/
				continue;
			if(GetType(objNum) == OBJ_MUSICIAN_SHORT_CAPE)
				ObjShapeType[objNum] = OrigShapeType[objNum];
			NPCMode[objNum] = AI_VIGILANTE;
		} else {
			NPCMode[objNum] = AI_FEAR;
		}
	}
}

/*attack morality/alignement check: called when "attacker" hurts the
  neutral NPC "victim". If the player hurt a human, he yells for help
  (COMBAT_RaiseAlarm). A scheduled NPC becomes a vigilante; otherwise the
  victim turns hostile toward the attacker's side (temporary monsters,
  0xe0 and above, also switch to assault)*/
/*C_2337_1C1E*/COMBAT_ProvokeNeutral(int victim, int attacker, int damage) {
	int objCls;

	if(victim >= 0x100)
		return;
	if(GetAlignment(victim) != NEUTRAL)
		return;
	if(IsPlrControl(victim))
		return;
	objCls = GetMonsterClass(GetType(victim));
	if(NPCMode[attacker] == AI_COMMAND && objCls >= 0 && IsMonsterHuman(objCls) && damage > 0) {
		/*killed a human ? ...yells for help...*/
		CON_printf(YellsForHelpMsg, GetObjectString(victim));
		COMBAT_RaiseAlarm();
		PromptFlag = 1;
	}
	if(NPCMode[victim] >= AI_SCHEDULE) {
		if(NPCMode[attacker] == AI_COMMAND) {
			if(GetType(victim) == OBJ_MUSICIAN_SHORT_CAPE)
				ObjShapeType[victim] = OrigShapeType[victim];
			NPCMode[victim] = AI_VIGILANTE;
		}
		return;
	}
	if(GetAlignment(attacker) == GOOD)
		SetAlignment(victim, CHAOTIC);
	else
		SetAlignment(victim, GOOD);
	if(victim >= 0xe0)
		NPCMode[victim] = AI_ASSAULT;
}

/*a hit landed: roll the damage (1..hitPts; 255 is a sure kill), subtract
  1..armour, apply it, print the messages and the monster special
  attacks (drag under, acid, theft, poison)*/
static /*C_2337_1D20*/__DoHit(int attacker, int defender, int hitPts) {
	char *pObjStr;
	char defName[26];
	int armorPts, objCls, lostObj, defTyp;

#if 0
	CON_printf("%03x hits %03x (%03d)\n", attacker, defender, hitPts);
#endif
	if(defender < 0)
		return;
	pObjStr = GetObjectString(defender);
	if(ObjShapeType[attacker] == TypeFrame(OBJ_CORPSER, 0)) {/*corpser surfaces*/
		SetFrame(attacker, 1);
		VisibilityDirty = 1;
	}

	if(hitPts == -1)
		hitPts = 1;
	else if(hitPts > 1 && hitPts < 255)
		hitPts = OSI_rand(1, hitPts);

	if(defender < 0x100)
		armorPts = __GetProtection(defender);
	else
		armorPts = 0;
	if(armorPts > 0 && hitPts < 255)
		hitPts -= OSI_rand(1, armorPts);

	if(hitPts <= 0) {
		COMBAT_showHit(defender);
		CON_printf(GrazedMsg, pObjStr);
		PromptFlag = 1;
	} else {
		/*an adjacent attacker engages the defender in melee*/
		if(
			defender < 0x100 &&
			NPCMode[defender] > AI_FOLLOW && NPCMode[defender] < AI_PATROL_NS &&
			CLOSE_ENOUGH_S(2, GetX(defender), GetY(defender), GetX(attacker), GetY(attacker))
		) Leader[defender] = attacker;
		AddXP(attacker, LoseHP(defender, hitPts));
	}
	if(defender >= 0x100)
		return;

	if(defender == Selection.obj)
		COMBAT_ProvokeNeutral(defender, attacker, hitPts);
	defTyp = GetType(defender);
	if(
		HitPoints[defender] == 0 &&
		defTyp != OBJ_BALLOON_INFLATED &&
		defTyp != OBJ_SKIFF &&
		defTyp != OBJ_RAFT &&
		defTyp != OBJ_SHIP
	) {
		CON_printf(KilledMsg, pObjStr);
		PromptFlag = 1;
		if(StatusDisplay == CMD_STAT_PORTRAIT && Party[StatusMember] == attacker)
			StatusDirty = 1;
		return;
	}
	objCls = GetMonsterClass(GetType(attacker));
	if(IsMonsterCorpser(objCls)) {
		MUS_PlaySfx(9, 0);
		CON_printf(DraggedUnderMsg, pObjStr);
		SetDraggedUnder(defender);
		PromptFlag = 1;
		VisibilityDirty = 1;
	}
	if(IsMonsterAcidSlug(objCls)) {
		lostObj = COMBAT_PickVulnerableItem(defender, 0);
		if(lostObj >= 0) {
			MUS_PlaySfx(11, 0);
			strcpy(defName, GetObjectString(defender));
			CON_printf(DissolvedMsg, defName, GetObjectString(lostObj));
			PromptFlag = 1;
			DeleteObj(lostObj);
		}
	}
	if(IsMonsterGremlin(objCls)) {
		lostObj = COMBAT_PickVulnerableItem(defender, 1);
		if(lostObj >= 0) {
			MUS_PlaySfx(12, 0);
			CON_printf(StolenMsg, GetObjectString(defender));
			PromptFlag = 1;
			DeleteObj(lostObj);
		}
	}
	if(IsMonsterPoisonous(objCls) && OSI_rand(0, 3) == 0) {
		CON_printf(PoisonedMsg, pObjStr);
		PromptFlag = 1;
		SetPoisoned(defender);
		if(
			StatusDisplay == CMD_STAT_PARTY ||
			(StatusDisplay == CMD_STAT_PORTRAIT && Party[StatusMember] == defender)
		) StatusDirty = 1;
	}
	if(hitPts > 0)
		COMBAT_displaySeverity(defender);
}

/*not very proud of that name ...*/
/*return the actor owning a part of a multi-tile monster (dragon,
  scorpion, ship, serpent, hydra body); the object itself otherwise*/
/*C_2337_205C*/COMBAT_getHead(int objNum) {
	int obj;
	int objType, neck_x, neck_y;

	if(objNum < 0)
		return -1;
	objType = GetType(objNum);
	if(objType >= OBJ_GIANT_SCORPION || objType == OBJ_DRAGON || objType == OBJ_SHIP || objType == OBJ_SILVER_SERPENT) {
		if(objNum >= 0x100)
			objNum = OBJ_getHead(objNum);
	} else if(objType == OBJ_HYDRA_BODY) {
		/*frame >> 2 is the direction of this part from the hydra*/
		neck_x = GetX(objNum) - DirIncrX[GetFrame(objNum) >> 2];
		neck_y = GetY(objNum) - DirIncrY[GetFrame(objNum) >> 2];
		for(obj = FindLoc(neck_x, neck_y, MapZ); obj >= 0; obj = NextLoc()) {
			if(obj < 0x100 && GetType(obj) == OBJ_HYDRA) {
				objNum = obj;
				break;
			}
		}
	}
	return objNum;
}

/*att_objNum attacks location (x,y) with weap_objNum (itself for a
  natural attack). Turns the attacker, handles spellbooks/charges (casts
  the spell), missiles and misses of ranged weapons (the missile may
  then hit a neighbouring tile), the triple crossbow, undead resistance
  and the glass sword / boomerang.
  returns 0: done, 1: engaged in melee (can't shoot), 2: out of range*/
static /*C_2337_214D*/__TryHit(int att_objNum, int x, int y, int weap_objNum) {
	int weap_typ, opp_objNum;
	int dir, tmp, damage, didHit;
	int delta_x, delta_y, isRanged, numBolts, savedActive, manaCost, objTyp;

#if 0
	CON_printf("%04x att. %03x,%03x w.%04x\n", att_objNum, x, y, weap_objNum);
#endif
	if(COMBAT_range(x, y, GetX(att_objNum), GetY(att_objNum)) > __GetWeaponRange(weap_objNum))
		return 2;

	objTyp = GetType(weap_objNum);
	if((objTyp == OBJ_BOW || objTyp == OBJ_MAGIC_BOW) && FindInvType(att_objNum, OBJ_ARROW, -1) < 0)
		return 0;
	if((objTyp == OBJ_CROSSBOW || objTyp == OBJ_TRIPLE_CROSSBOW) && FindInvType(att_objNum, OBJ_BOLT, -1) < 0)
		return 0;

	/*face the target (dir = 0,2,4,6 = N,E,S,W)*/
	delta_x = x - GetX(att_objNum);
	delta_y = y - GetY(att_objNum);
	if(abs(delta_x) > abs(delta_y))
		dir = (delta_x < 0)?6:2;
	else
		dir = (delta_y < 0)?0:4;
	objTyp = GetType(att_objNum);
	if(objTyp == OBJ_SHIP) {
		/*a ship fires its broadside: perpendicular to its heading*/
		if(((GetDirection(att_objNum) & 6) ^ dir) & 2)
			dir = GetDirection(att_objNum);
		else
			dir ^= 2;
	}
	SetDirection(att_objNum, dir);
	UpdateMoveFrame(att_objNum, dir);
	if(objTyp >= OBJ_SKELETON && objTyp <= OBJ_MONGBAT) {
		/*attack frame (tmp is reused below as the bolt index)*/
		tmp = (GetFrame(att_objNum) & 0xc) + 2;
		SetFrame(att_objNum, tmp);
	}

	FlushViewIfDirty();
	MissileHitObj = Selection.obj;
	weap_typ = GetType(weap_objNum);
	isRanged = 0;
	/*morning star and halberd reach 2 tiles but are melee weapons*/
	if(weap_typ != OBJ_MORNING_STAR && weap_typ != OBJ_HALBERD) {
		if(delta_x > 1 || delta_x < -1 || delta_y > 1 || delta_y < -1)
			isRanged = 1;
		if(
			weap_typ == OBJ_SLING || weap_typ == OBJ_THROWING_AXE || weap_typ == OBJ_BOW ||
			weap_typ == OBJ_CROSSBOW || weap_typ == OBJ_BOOMERANG || weap_typ == OBJ_FLASK_OF_OIL ||
			weap_typ == OBJ_TRIPLE_CROSSBOW || weap_typ == OBJ_ZU_YLEM || weap_typ == OBJ_LIGHTNING_WAND ||
			weap_typ == OBJ_FIRE_WAND
		) isRanged = 1;
	}
	if(isRanged && COMBAT_IsEngagedInMelee(att_objNum))
		return 1;
	if(weap_typ == OBJ_TRIPLE_CROSSBOW) {
		numBolts = TestObj(att_objNum, OBJ_BOLT);
		if(numBolts > 3)
			numBolts = 3;
	}

	damage = GetDamagePoints(weap_objNum);
	if(damage < 0)
		damage = 1;

	didHit = -1;
	opp_objNum = COMBAT_getHead(MissileHitObj);
	/*spell*/
	if(weap_typ == OBJ_CHARGE || weap_typ == OBJ_SPELLBOOK) {
		/*quality of the charge/spellbook = spell number*/
		if(GetQual(weap_objNum) == SPELL_WISP_TELEPORT && objTyp == OBJ_WISP) {
			x = GetX(att_objNum);
			y = GetY(att_objNum);
			MoveObj(att_objNum, MapX, MapY, MapZ);
			if(COMBAT_TryTeleport(att_objNum, 0)) {
				CON_printf(TeleportMsg);
				PromptFlag = 1;
			} else {
				MoveObj(att_objNum, x, y, MapZ);
			}
			return 0;
		}
		if(weap_typ == OBJ_SPELLBOOK) {
			/*cost = circle (spell / 16 + 1); needs that much magic and level*/
			manaCost = GetQual(weap_objNum) / 0x10 + 1;
			if(MAGIC[att_objNum] < manaCost || Level[att_objNum] < manaCost)
				return 0;
			MAGIC[att_objNum] -= manaCost;
		}
		/*cast as if the caster was party member #0x10*/
		savedActive = Active;
		Active = 0x10;
		Party[0x10] = att_objNum;
		Selection.obj = opp_objNum;
		Selection.x = GetX(opp_objNum);
		Selection.y = GetY(opp_objNum);
		TextOn = 0;
		SPELL_Execute(GetQual(weap_objNum), 1);/*SPELL_cast*/
		TextOn = 1;
		if(GetQual(weap_objNum) == SPELL_CHARM && SpellResult == -2)
			CON_printf(/*1BD7*/"%s is charmed.\n", GetObjectString(Selection.obj));
		else if(GetQual(weap_objNum) == SPELL_PARALYZE && SpellResult == 0)
			CON_printf(/*1BE7*/"%s is paralyzed.\n", GetObjectString(Selection.obj));
		Active = savedActive;
		return 0;
	}
	/* */
	if(MissileHitObj >= 0 && opp_objNum < 0x100) {
		didHit = __AttackTest(att_objNum, opp_objNum, weap_objNum);
		if(!didHit && isRanged) {
			/*missed: the missile goes to a random neighbouring tile*/
			didHit = -1;
			x += OSI_rand(0, 2) - 1;
			y += OSI_rand(0, 2) - 1;
			MissileHitObj = COMBAT_GetTargetAt(att_objNum, x, y);
			opp_objNum = COMBAT_getHead(MissileHitObj);
		}
	}
	if(isRanged && !COMBAT_FireRangedWeapon(att_objNum, &x, &y, weap_objNum))
		didHit = -1;
	opp_objNum = COMBAT_getHead(MissileHitObj);
	if(!isRanged && COMBAT_TileDistance(att_objNum, MapX, MapY) < 6)
		MUS_PlaySfx(17, 0);/*melee sound*/
	if(didHit == -1) {
		didHit = 1;
		if(MissileHitObj >= 0 && opp_objNum < 0x100)
			didHit = __AttackTest(att_objNum, opp_objNum, weap_objNum);
	}
	/*triple crossbow*/
	if(weap_typ == OBJ_TRIPLE_CROSSBOW) {
		for(tmp = 0; tmp < numBolts; tmp ++) {
			if(tmp > 0) {
				MissileHitObj = COMBAT_GetTargetAt(att_objNum, TripleBoltX[tmp], TripleBoltY[tmp]);
				opp_objNum = COMBAT_getHead(MissileHitObj);
				didHit = __AttackTest(att_objNum, opp_objNum, weap_objNum);
			}
			if(didHit && MissileHitObj >= 0 && opp_objNum != att_objNum)
				__DoHit(att_objNum, opp_objNum, damage);
		}
		return 0;
	}

	if(didHit && MissileHitObj >= 0 && opp_objNum != att_objNum) {
		/*undead take half damage, except from magic weapons*/
		if(
			IsMonsterUndead(GetMonsterClass(GetType(opp_objNum))) &&
			weap_typ != OBJ_GLASS_SWORD && weap_typ != OBJ_TRIPLE_CROSSBOW && weap_typ != OBJ_MAGIC_BOW
		) damage = (damage + 1) >> 1;
		if(weap_typ != OBJ_ZU_YLEM)
			__DoHit(att_objNum, opp_objNum, damage);
		if(weap_typ == OBJ_GLASS_SWORD) {
			CON_printf(GlassSwordMsg);
			DeleteObj(weap_objNum);
			PromptFlag = 1;
		}
	}
	/*boomerang returns*/
	if(weap_typ == OBJ_BOOMERANG)
		Missiles(GetX(att_objNum), GetY(att_objNum), 1, &x, &y, 100, BaseTile[OBJ_BOOMERANG], 1);

	return 0;
}

/*reset equiped weapon list: collect the (up to 8) equipped weapons of
  the active party member into EquipWeapons and print the first one*/
/*C_2337_27C5*/static COMBAT_InitWeaponList() {
	int invObj, attacker;

	attacker = Party[Active];
	NumEquipWeapons =
	CurEquipWeapon = 0;
	for(invObj = FindInv(attacker); invObj >= 0 && NumEquipWeapons < 8; invObj = NextInv()) {
		if(GetCoordUse(invObj) == EQUIP && GetDamagePoints(invObj) >= 0) {
			EquipWeapons[NumEquipWeapons] = invObj;
			NumEquipWeapons ++;
		}
	}
	if(NumEquipWeapons == 0) {
		if(GetType(attacker) == OBJ_SHIP)
			CON_printf(ShipCannonMsg);
		else
			CON_printf(BareHandsMsg);
	} else
		CON_printf(WithMsg, GetObjectString(EquipWeapons[0]));
}

/*next equiped weapon list: attack the selected target with the current
  weapon (EquipWeapons[CurEquipWeapon], bare hands if none), then prepare the
  targeting of the next equipped weapon, or end the command*/
/*C_2337_2875*/static COMBAT_AttackWithNextWeapon() {
	int attacker, weapTyp;
	int weapObj, hitResult;

	attacker = Party[Active];
	if(NumEquipWeapons == 0)
		weapObj = attacker;
	else
		weapObj = EquipWeapons[CurEquipWeapon];
	weapTyp = GetType(weapObj);
	if((weapTyp == OBJ_BOW || weapTyp == OBJ_MAGIC_BOW) && FindInvType(attacker, OBJ_ARROW, -1) < 0) {
		CON_printf(OutOfArrowsMsg);
	} else if((weapTyp == OBJ_CROSSBOW || weapTyp == OBJ_TRIPLE_CROSSBOW) && FindInvType(attacker, OBJ_BOLT, -1) < 0) {
		CON_printf(OutOfBoltsMsg);
	} else {
		hitResult = 0;
		if(Selection.obj >= 0) {
			if(Selection.obj == attacker)
				CON_printf(AttackSelfPassMsg);
			else
				CON_printf(/*1BF9*/"%s.\n", GetObjectString(Selection.obj));
		} else {
			if(SelectionIsDark)
				CON_printf(NothingMsg);
			else
				CON_printf(/*1BF9*/"%s.\n", GetTileString(GetTileAtXYZ(Selection.x, Selection.y, MapZ)));
		}
		if(Selection.obj != attacker) {
			CON_printf(/*1BCD+3*/"\n");
			SkipNewlines = 1;
			if(Selection.obj < 0)
				Selection.obj = COMBAT_GetTargetAt(attacker, Selection.x, Selection.y);
			hitResult = __TryHit(attacker, Selection.x, Selection.y, weapObj);
			if(hitResult == 1) {
				MUS_PlaySfx(12, 0);
				CON_printf(InterferenceMsg, GetObjectString(Leader[attacker]));
			} else if(hitResult == 2) {
				CON_printf(OutOfRangeMsg);
				LastAttackTarget = -1;
				MUS_PlaySfx(12, 0);
			}
		}
	}
	CurEquipWeapon ++;
	if(CurEquipWeapon < NumEquipWeapons) {
		SelectMode = 1;
		SelectRange = __GetWeaponRange(EquipWeapons[CurEquipWeapon]);
		if(COMBAT_range(AimX, AimY, 5, 5) > SelectRange) {
			AimX =
			AimY = 5;
		}
		CON_printf(AttackWithMsg, GetObjectString(EquipWeapons[CurEquipWeapon]));
	} else {
		MouseMode = 0;
		SelectMode = 0;
		CON_UpdateMousePointer();
	}
}

/*"attack"*/
/*the player's "Attack" command: select a target for each equipped
  weapon in turn*/
/*C_2337_2AA9*/COMBAT_attack() {
	int ch, weapon_objNum;

	CON_printf(AttackMsg);
	if(IN_VEHICLE && GetType(Party[Active]) != OBJ_SHIP) {
		CON_printf(/*1BFE*/"-");
		CON_printf(NotOnShipMsg);
		return;
	}
	CON_printf(/*1BD2+3*/" ");
	MouseMode = 1;
	PendingCommand = CMD_ATTACK;
	CON_UpdateMousePointer();
	COMBAT_InitWeaponList();
	if(NumEquipWeapons == 0)
		weapon_objNum = Party[Active];
	else
		weapon_objNum = EquipWeapons[CurEquipWeapon];
	SelectRange = __GetWeaponRange(weapon_objNum);
	if(COMBAT_range(AimX, AimY, 5, 5) > SelectRange) {
		AimX =
		AimY = 5;
	}
	ch = CON_getch();
	LastAttackTarget = Selection.obj;
	if(ch == CMD_TARGET && (Selection.obj < 0 || GetCoordUse(Selection.obj) == LOCXYZ)) {
		do {
			COMBAT_AttackWithNextWeapon();
			if(MouseMode == 1) {
				RefreshStatus();
				ch = CON_getch();
				if(ch != CMD_TARGET) {
					CON_printf(WhatMsg);
					break;
				}
			}
		} while(MouseMode == 1);
	} else {
		CON_printf(WhatMsg);
	}
	SubMov(Party[Active], 10);
	MouseMode = 0;
	SelectMode = 0;
	CON_UpdateMousePointer();
}

/*combat distance between two locations (RangeTable; 9 if too far)*/
/*C_2337_2BEE*/COMBAT_range(int x0, int y0, int x1, int y1) {
	int delta_x, delta_y;
	int range;

	delta_x = abs(x0 - x1);
	delta_y = abs(y0 - y1);
	if(delta_x < 8 && delta_y < 8)
		range = RangeTable[delta_x][delta_y];
	else
		range = 9;
	return range;
}

/*choose what an NPC attacks with: sometimes (1 in 4, not during negate
  magic, more likely for low spells) an equipped spellbook/charge, else
  the equipped weapon with the best damage that reaches the opponent,
  else its natural attack (att_objNum itself)*/
/*C_2337_2C43*/static __PickWeapon(int att_objNum, int opp_objNum) {
	int ret, i;
	int opp_x, opp_y, att_x, att_y;
	int range, dmg, max_dmg, itemTyp;

	ret = att_objNum;
	max_dmg = 0;
	opp_x = GetX(opp_objNum);
	opp_y = GetY(opp_objNum);
	att_x = GetX(ret);
	att_y = GetY(ret);
	range = COMBAT_range(opp_x, opp_y, att_x, att_y);
	for(i = FindInv(att_objNum); i >= 0; i = NextInv()) {
		itemTyp = GetType(i);
		if(GetCoordUse(i) == EQUIP) {
			if(itemTyp == OBJ_SPELLBOOK || itemTyp == OBJ_CHARGE) {
				if(
					OSI_rand(0, 3) == 0 &&
					SpellFx[13] == 0 &&/*negate magic*/
					OSI_rand(0, GetQual(i)) < 16
				) {
					ret = i;
					break;
				}
			}
		}
		if(GetCoordUse(i) == EQUIP) {
			dmg = GetDamagePoints(i);
			if(dmg > max_dmg && __GetWeaponRange(i) >= range) {
				max_dmg = dmg;
				ret = i;
			}
		}
	}
	return ret;
}

/*closest (euclidian) visible enemy within 8 tiles, according to the
  alignments (GOOD attacks the ones hostile to the party, EVIL the ones
  hostile to monsters, CHAOTIC everything but chaotic, NEUTRAL nothing
  unless vigilante). Fleeing modes take any actor (the one to flee).
  Retreating actors off-screen are ignored. returns -1 if none*/
/*C_2337_2D6E*/COMBAT_pickOpponent(int objNum) {
	int i;
	int x, y, opp_x, opp_y, objAln;
	int dst2, ret, min_dst2;

	ret = -1;
	objAln = GetAlignment(objNum);
	x = GetX(objNum);
	y = GetY(objNum);
	min_dst2 = 0x7fff;
	for(i = 0; i < 0x100; i++) {
		if(ObjShapeType[i] != 0 && !IsDead(i) && i != objNum && COMBAT_canSee(objNum, i)) {
			if(
				NPCMode[objNum] != AI_SHY &&
				NPCMode[objNum] != AI_SCHED_SHY &&
				NPCMode[objNum] != AI_FEAR &&
				NPCMode[objNum] != AI_RETREAT &&
				NPCMode[objNum] != AI_BRAWL
			) {
				if(objAln == NEUTRAL && (NPCMode[objNum] != AI_VIGILANTE || GetAlignment(i) != GOOD))
					continue;
				if(objAln == CHAOTIC && GetAlignment(i) == CHAOTIC)
					continue;
				if(GetAlignment(i) == NEUTRAL)
					continue;
				if(objAln == GOOD && !Is_ATKPLR(i))
					continue;
				if(objAln == EVIL && !Is_ATKMON(i))
					continue;
			}
			opp_x = GetX(i);
			opp_y = GetY(i);
			if(COMBAT_TileDistance(objNum, opp_x, opp_y) <= 8) {
				if(NPCMode[i] != AI_RETREAT || CLOSE_ENOUGH(5, opp_x, opp_y, MapX, MapY)) {
						dst2 = (opp_x - x) * (opp_x - x) + (opp_y - y) * (opp_y - y);
						if(dst2 < min_dst2 || (dst2 == min_dst2 && OSI_rand(0, 1))) {
							min_dst2 = dst2;
							ret = i;
						}
				}
			}
		}
	}
	return ret;
}

/*AI_ASSAULT (also the fallback of the other modes): attack the closest
  enemy if in range and in line of fire, else sidestep (90 degrees) or
  move toward it*/
/*C_2337_2F3E*/COMBAT_AI_Assault(int objNum) {
	int weapObj, opp_x, opp_y, x, y, range;

	Selection.obj = COMBAT_pickOpponent(objNum);
	weapObj = __PickWeapon(objNum, Selection.obj);
	if(Selection.obj >= 0) {
		opp_x = GetX(Selection.obj);
		opp_y = GetY(Selection.obj);
		x = GetX(objNum);
		y = GetY(objNum);
		range = __GetWeaponRange(weapObj);
		if(
			CLOSE_ENOUGH_S(8, opp_x, opp_y, x, y) &&
			RangeTable[abs(opp_x - x)][abs(opp_y - y)] <= range
		) {
			if(COMBAT_Missile(objNum, &opp_x, &opp_y, range, 0)) {
				__TryHit(objNum, opp_x, opp_y, weapObj);
				SubMov(objNum, 10);
				return;
			}
			/*no line of fire: step perpendicular to the target*/
			if(OSI_rand(0, 1)) {
				opp_x = x - (GetY(Selection.obj) - y);
				opp_y = GetX(Selection.obj) - x + y;
			} else {
				opp_x = (GetY(Selection.obj) - y) + x;
				opp_y = y - (GetX(Selection.obj) - x);
			}
/*C_30D2:*/
			TryMoveTo(objNum, opp_x, opp_y);
			/*return;*/
		} else
/*C_30E0*/
		if(NPCMode[objNum] == AI_FRONT) {
/*C_30EA:*/
			SubMov(objNum, 5);
		} else {
			/*goto C_30D2;*/
			TryMoveTo(objNum, opp_x, opp_y);
		}
	} else {
/*C_30F6*/
		if(NPCMode[objNum] == AI_FRONT)
			/*goto C_30EA;*/
			SubMov(objNum, 5);
		else
/*C_3100*/
			WanderRandomly(objNum);
	}
}

/*front line helper: move objNum onto the front line, i.e. the point of
  the axis between the two sides at the depth of the reference member
  (Party[0] plus one step for the party, EnemyFrontFirst for the enemies),
  sliding sideways along the line while that point is occupied.
  returns 1 if nothing was done (caller decides), 0 if handled*/
/*C_2337_310E*/static COMBAT_MoveToFrontLine(int objNum) {
	int axisX;
	int onLeft, refObj, refDepth;
	int cross, axisLen2, axisY, destX, destY, stepX, stepY;
	int depthStepX, depthStepY, x, y, occupied, originX, originY, savedMovePts;

	if((EnemiesNum == 0 || EnemyFrontFirst == 0) && !IsPlrControl(objNum)) {
		SubMov(objNum, 5);
		return 1;
	}
	x = GetX(objNum);
	y = GetY(objNum);
	if(IsPlrControl(objNum)) {
		originX = GetX(Party[0]);
		originY = GetY(Party[0]);
		axisX = EnemiesGravityX - originX;
		axisY = EnemiesGravityY - originY;
	} else {
		originX = PartyGravityX;
		originY = PartyGravityY;
		axisX = GetX(EnemyFrontFirst) - originX;
		axisY = GetY(EnemyFrontFirst) - originY;
	}
	/*which side of the axis are we on? slide away from the axis*/
	cross = (x - originX) * axisY - (y - originY) * axisX;
	onLeft = (cross > 0);
	if(cross == 0)
		onLeft = OSI_rand(0, 1);
	stepX = (axisY > 0)?1:-1;
	stepY = (axisX > 0)?-1:1;
	if(!onLeft) {
		stepX = -stepX;
		stepY = -stepY;
	}
	if(!IsPlrControl(objNum))
		refObj = EnemyFrontFirst;
	else
		refObj = Party[0];
	refDepth = (GetX(refObj) - originX) * axisX + (GetY(refObj) - originY) * axisY;
	if(Party[0] == refObj)
		refDepth += abs(axisX) + abs(axisY);
	axisLen2 = axisX * axisX + axisY * axisY;
	if(axisLen2 == 0)
		axisLen2 = 1;
	/*projection of the reference on the axis*/
	destX = (refDepth * axisX) / axisLen2 + originX;
	destY = (refDepth * axisY) / axisLen2 + originY;
	PassThroughParty = 0;
	do {
		if(
			destX < AreaX || AreaX + 39 < destX ||
			destY < AreaY || AreaY + 39 < destY ||
			(x == destX && y == destY)
		) {
			PassThroughParty = 1;
			return 1;
		}
		occupied = (OBJ_FindNPCAt(0, destX, destY) >= 0);
		if(occupied) {
			depthStepX = (destX + stepX - originX) * axisX + (destY - originY) * axisY;
			depthStepY = (destX - originX) * axisX + (destY + stepY - originY) * axisY;
			if(abs(depthStepX - refDepth) < abs(depthStepY - refDepth))
				destX += stepX;
			else
				destY += stepY;
		}
	} while(occupied);
	PassThroughParty = 1;
	savedMovePts = MovePts[objNum];
	if(!TryMoveTo(objNum, destX, destY)) {
		MovePts[objNum] = savedMovePts;
	} else {
		if(!IsPlrControl(objNum))
			return 0;
		if(GetX(objNum) == destX && GetY(objNum) == destY)
			return 0;
		if(EnemiesNum == 0)
			MovePts[objNum] = savedMovePts;
		if(TryMoveTo(objNum, destX, destY))
			return 0;
	}
	COMBAT_AI_Assault(objNum);
	return 0;
}

/*AI_FRONT: take position on the front line (COMBAT_MoveToFrontLine); when that is
  not possible, enemies 8+ tiles from the party wait, the enemy anchor
  (EnemyFrontFirst) charges 1 time in 8, and the others assault*/
/*C_2337_3464*/COMBAT_AI_Front(int objNum) {
	if(!COMBAT_MoveToFrontLine(objNum))
		return;
	if(!IsPlrControl(objNum)) {
		if(COMBAT_TileDistance(objNum, PartyGravityX, PartyGravityY) < 8) {
			if(objNum == EnemyFrontFirst && OSI_rand(0, 7) == 0) {
				TryMoveTo(objNum, PartyGravityX, PartyGravityY);
				return;
			}
		} else {
			SubMov(objNum, 5);
			return;
		}
	}
	COMBAT_AI_Assault(objNum);
}

/*AI_REAR: stay behind the own front line (depth smaller than every
  AI_FRONT member of the same alignment) and between the two flank
  members; party members also stay within 3 tiles of the leader.
  attack (assault) when no move is needed or possible*/
/*C_2337_34C9*/COMBAT_AI_Rear(int objNum) {
	int axisX;
	int i, axisY, pos, minFrontDepth, objAln;
	int stuck, savedMovePts, originX, originY, leftFlank, rightFlank;

	stuck = 0;
	if(IsPlrControl(objNum)) {
		if(EnemiesNum == 0) {
			TryMoveTo(objNum, MapX, MapY);
			return;
		}
		leftFlank = PartyFlankPos;
		rightFlank = PartyFlankNeg;
		originX = PartyGravityX;
		originY = PartyGravityY;
		axisX = EnemiesGravityX - PartyGravityX;
		axisY = EnemiesGravityY - PartyGravityY;
	} else {
		if(EnemiesNum == 0) {
			SubMov(objNum, 5);
			return;
		}
		leftFlank = EnemyFlankNeg;
		rightFlank = EnemyFlankPos;
		if(leftFlank < 0)
			leftFlank = objNum;
		if(rightFlank < 0)
			rightFlank = objNum;
		originX = EnemiesGravityX;
		originY = EnemiesGravityY;
		axisX = PartyGravityX - EnemiesGravityX;
		axisY = PartyGravityY - EnemiesGravityY;
	}
	minFrontDepth = 0x7fff;
	objAln = GetAlignment(objNum);
	for(i = 0; i < 0x100; i ++) {
		if(
			!IsDead(i) &&
			NPCMode[i] == AI_FRONT &&
			GetAlignment(i) == objAln
		) {
			pos = (GetX(i) - originX) * axisX + (GetY(i) - originY) * axisY;
			if(pos < minFrontDepth)
				minFrontDepth = pos;
		}
	}
	pos = (GetX(objNum) - originX) * axisX + (GetY(objNum) - originY) * axisY;
	savedMovePts = MovePts[objNum];
	if(IsPlrControl(objNum) && COMBAT_TileDistance(objNum, MapX, MapY) > 3) {
		stuck = !TryMoveTo(objNum, MapX, MapY);
	} else if(pos >= minFrontDepth) {
		stuck = !TryMoveTo(objNum, GetX(objNum) - axisX, GetY(objNum) - axisY);
	} else {
		pos = (GetX(objNum) - originX) * axisY - (GetY(objNum) - originY) * axisX;
		if((GetX(leftFlank) - originX) * axisY - (GetY(leftFlank) - originY) * axisX < pos) {
			stuck = !TryMoveTo(objNum, GetX(objNum) - axisY, GetY(objNum) + axisX);
		} else if((GetX(rightFlank) - originX) * axisY - (GetY(rightFlank) - originY) * axisX > pos) {
			stuck = !TryMoveTo(objNum, GetX(objNum) + axisY, GetY(objNum) - axisX);
		} else {
			stuck = 1;
		}
	}
	if(stuck) {
		MovePts[objNum] = savedMovePts;
		COMBAT_AI_Assault(objNum);
	}
}

/*AI_FLANK: go round the enemy line on our side; target the visible
  enemy furthest out on that side and approach it moving outward, attack
  when in range*/
/*C_2337_37F2*/COMBAT_AI_Flank(int objNum) {
	int i;
	int vx, vy;
	int selfX, selfY, cross, onLeft, oppSide;
	int bestSide, bestOpp, oppX, oppY;
	int objAln, outX, outY, mustMove, weapObj;
	int range, delta_x, delta_y, savedMovePts;

	if(
		EnemiesNum == 0 ||
		(!IsPlrControl(objNum) && COMBAT_TileDistance(objNum, MapX, MapY) > 7)
	) {
		savedMovePts = MovePts[objNum];
		if(IsPlrControl(objNum) && COMBAT_TileDistance(objNum, MapX, MapY) > 2)
			TryMoveTo(objNum, MapX, MapY);
		MovePts[objNum] = savedMovePts;
		SubMov(objNum, 5);
		return;
	}
	objAln = GetAlignment(objNum);
	selfX = GetX(objNum);
	selfY = GetY(objNum);
	/*(vx,vy): first the axis, later the target position*/
	vx = EnemiesGravityX - PartyGravityX;
	vy = EnemiesGravityY - PartyGravityY;
	cross = (selfX - PartyGravityX) * vy - (selfY - PartyGravityY) * vx;
	onLeft = (cross > 0);
	if(cross == 0)
		onLeft = OSI_rand(0, 1);
	if(onLeft) {
		outX = vy;
		outY = - vx;
	} else {
		outX = -vy;
		outY = vx;
		cross = -cross;
	}
	bestSide = 0x8000;
	bestOpp = -1;
	for(i = 0; i < 0x100; i ++) {
		if(
			ObjShapeType[i] &&
			!IsDead(i) &&
			GetAlignment(i) != objAln &&
			COMBAT_canSee(objNum, i)
		) {
			if(objAln == GOOD && !Is_ATKPLR(i))
				continue;
			if(objAln == EVIL && !Is_ATKMON(i))
				continue;
			oppX = GetX(i);
			oppY = GetY(i);
			if(NPCMode[i] == AI_RETREAT || NPCMode[i] == AI_SHY || NPCMode[i] == AI_SCHED_SHY) {
				if(
					(MapX - 5) > oppX || (MapX + 5) < oppX ||
					(MapY - 5) > oppY || (MapY + 5) < oppY
				) continue;
			}
			if(
				CLOSE_ENOUGH(7, oppX, oppY, MapX, MapY) ||
				CLOSE_ENOUGH(5, oppX, oppY, selfX, selfY)
			) {
				oppSide = (oppX - PartyGravityX) * vy - (oppY - PartyGravityY) * vx;
				if(onLeft == 0)
					oppSide = -oppSide;
				if(oppSide > bestSide) {
					bestSide = oppSide;
					bestOpp = i;
				}
			}
		}
	}
	if(bestOpp >= 0) {
		Selection.obj = bestOpp;
		vx = GetX(bestOpp);
		vy = GetY(bestOpp);
		mustMove = 0;
		weapObj = __PickWeapon(objNum, bestOpp);
		delta_x = abs(vx - selfX);
		delta_y = abs(vy - selfY);
		range = __GetWeaponRange(weapObj);
		if(delta_x < 8 && delta_y < 8 && RangeTable[delta_x][delta_y] <= range) {
			if(COMBAT_Missile(objNum, &vx, &vy, range, 0)) {
				__TryHit(objNum, vx, vy, weapObj);
				SubMov(objNum, 10);
			} else {
				if(OSI_rand(0, 1)) {
					vx = selfX - (GetY(Selection.obj) - selfY);
					vy = selfY + (GetX(Selection.obj) - selfX);
					mustMove = 1;
				} else {
					vx = selfX + (GetY(Selection.obj) - selfY);
					vy = selfY - (GetX(Selection.obj) - selfX);
					mustMove = 1;
				}
			}
		} else {
			if(outX > 0)
				vx ++;
			else if(outX < 0)
				vx --;
			if(outY > 0)
				vy ++;
			else if(outY < 0)
				vy --;
			mustMove = 1;
		}
		if(mustMove) {
			savedMovePts = MovePts[objNum];
			if(!TryMoveTo(objNum, vx, vy)) {
				MovePts[objNum] = savedMovePts;
				COMBAT_AI_Assault(objNum);
			}
		}
	} else {
		WanderRandomly(objNum);
	}
}

/*AI_BERSEK: attack the strongest (STR+DEX+INT) visible enemy within 8
  tiles, whatever the distance from the party*/
/*C_2337_3C3A*/COMBAT_AI_Berserk(int objNum) {
	int i;
	int weapObj, bestOpp, objAln, x, y, oppStats, maxStats;
	int oppX, oppY, range, delta_x, delta_y, mustMove, savedMovePts;

	bestOpp = -1;
	mustMove = 0;
	objAln = GetAlignment(objNum);
	x = GetX(objNum);
	y = GetY(objNum);
	maxStats = 0;
	for(i = 0; i < 0x100; i ++) {
		if(
			ObjShapeType[i] &&
			!IsDead(i) &&
			i != objNum &&
			COMBAT_TileDistance(i, x, y) <= 8 &&
			COMBAT_canSee(objNum, i) &&
			(objAln != GOOD || GetAlignment(i) == EVIL) &&
			(objAln != EVIL || GetAlignment(i) == GOOD) &&
			(objAln != CHAOTIC || (GetAlignment(i) != CHAOTIC && GetAlignment(i) != NEUTRAL))
		) {
			oppStats = STREN[i] + DEXTE[i] + INTEL[i];
			if(oppStats > maxStats) {
				maxStats = oppStats;
				bestOpp = i;
			}
		}
	}
	if(bestOpp >= 0) {
		oppX = GetX(bestOpp);
		oppY = GetY(bestOpp);
		delta_x = abs(oppX - x);
		delta_y = abs(oppY - y);
		weapObj = __PickWeapon(objNum, bestOpp);
		range = __GetWeaponRange(weapObj);
		if(delta_x < 8 && delta_y < 8 && RangeTable[delta_x][delta_y] <= range) {
			Selection.obj = bestOpp;
			if(COMBAT_Missile(objNum, &oppX, &oppY, range, 0)) {
				__TryHit(objNum, oppX, oppY, weapObj);
				SubMov(objNum, 10);
			} else {
				if(OSI_rand(0, 1)) {
					oppX = x - (GetY(Selection.obj) - y);
					oppY = y + (GetX(Selection.obj) - x);
					mustMove = 1;
				} else {
					oppX = x + (GetY(Selection.obj) - y);
					oppY = y - (GetX(Selection.obj) - x);
					mustMove = 1;
				}
			}
		} else {
			mustMove = 1;
		}
		if(mustMove) {
			savedMovePts = MovePts[objNum];
			if(!TryMoveTo(objNum, oppX, oppY)) {
				MovePts[objNum] = savedMovePts;
				COMBAT_AI_Assault(objNum);
			}
		}
	} else {
		COMBAT_AI_Assault(objNum);
	}
	SubMov(objNum, 5);
}

/*AI_RETREAT (also AI_SHY, AI_FEAR and AI_SCHED_SHY): flee from the closest
  opponent (when within 8 tiles; 3 for the other modes) or fight back
  if cornered; good ones go back to the party. A retreating NPC regains
  1 HP 1 time in 4 and goes back to its combat mode when no longer
  critical*/
/*C_2337_3ED2*/COMBAT_AI_Retreat(int objNum) {
	int oppObjNum;
	int delta_x, delta_y, fleeDist, savedMovePts;

	if(COMBAT_TileDistance(objNum, MapX, MapY) > 7) {
		MovePts[objNum] = 0;
		return;
	}
	oppObjNum = COMBAT_pickOpponent(objNum);
	if(oppObjNum < 0) {
		WanderRandomly(objNum);
		return;
	}
	delta_x = GetX(oppObjNum) - GetX(objNum);
	delta_y = GetY(oppObjNum) - GetY(objNum);
	fleeDist = (NPCMode[objNum] == AI_RETREAT)?8:3;
	if(CLOSE_ENOUGH_S(fleeDist, delta_x, delta_y, 0, 0)) {
		savedMovePts = MovePts[objNum];
		if(!TryMoveTo(objNum, GetX(objNum) - delta_x, GetY(objNum) - delta_y)) {
			MovePts[objNum] = savedMovePts;
			if(NPCMode[objNum] != AI_SCHED_SHY && NPCMode[objNum] != AI_SHY)
				COMBAT_AI_Assault(objNum);
		}
	} else if(GetAlignment(objNum) == GOOD && NPCMode[objNum] != AI_SHY && NPCMode[objNum] != AI_SCHED_SHY) {
		TryMoveTo(objNum, PartyGravityX, PartyGravityY);
	} else {
		WanderRandomly(objNum);
	}
	if(NPCMode[objNum] == AI_RETREAT && OSI_rand(0, 3) == 0) {
		if(MaxHP(objNum) > HitPoints[objNum])
			HitPoints[objNum] ++;
		if(HitPoints[objNum] * 4 / MaxHP(objNum) > 0)
			NPCMode[objNum] = NPCComMode[objNum];
	}
}

/*AI_LIKE: approach the party now and then when close, else wander*/
/*C_2337_407F*/COMBAT_AI_Like(int objNum) {
	int i;
	int x, y, nearParty;

	nearParty = 0;
	x = GetX(objNum);
	y = GetY(objNum);
	for(i = 0; i < PartySize; i ++) {
		if(CLOSE_ENOUGH_S(3, GetX(Party[i]), GetY(Party[i]), x, y)) {
			nearParty = 1;
			break;
		}
	}
	if(nearParty && OSI_rand(0, 1) == 0) {
		TryMoveTo(objNum, PartyGravityX, PartyGravityY);

		return;
	}
	if(OSI_rand(0, 1)) {
		if(OSI_rand(0, 1))
			x += OSI_rand(0, 1)?10:-10;
		else
			y += OSI_rand(0, 1)?10:-10;
		TryMoveTo(objNum, x, y);
		return;
	}
	SubMov(objNum, 5);
}

/*AI_UNFRIENDLY: wander; when close to the party, 1 in 8 to turn into
  AI_ASSAULT*/
/*C_2337_41B7*/COMBAT_AI_Unfriendly(int objNum) {
	int i;
	int x, y, nearParty;

	nearParty = 0;
	x = GetX(objNum);
	y = GetY(objNum);
	for(i = 0; i < PartySize; i ++) {
		if(CLOSE_ENOUGH_S(3, GetX(Party[i]), GetY(Party[i]), x, y)) {
			nearParty = 1;
			break;
		}
	}
	if(nearParty && OSI_rand(0, 7) == 0) {
		NPCMode[objNum] = AI_ASSAULT;
		COMBAT_AI_Assault(objNum);
		return;
	}
	if(OSI_rand(0, 1)) {
		if(OSI_rand(0, 1))
			x += OSI_rand(0, 1)?10:-10;
		else
			y += OSI_rand(0, 1)?10:-10;
		TryMoveTo(objNum, x, y);
		return;
	}
	SubMov(objNum, 5);
}

/*AI_IMMOBILE: never moves; tries 16 random tiles within 4 for a
  visible enemy and attacks it*/
/*C_2337_42EE*/COMBAT_AI_Immobile(int objNum) {
	int i;
	int objAln, opp_x, opp_y;

	objAln = GetAlignment(objNum);
	if(objAln != NEUTRAL) {
		for(i = 0; i < 16; i ++) {
			opp_x = GetX(objNum) + OSI_rand(0, 8) - 4;
			opp_y = GetY(objNum) + OSI_rand(0, 8) - 4;
			Selection.obj = COMBAT_GetTargetAt(objNum, opp_x, opp_y);
			if(
				Selection.obj >= 0 && Selection.obj < 0x100 && COMBAT_canSee(objNum, Selection.obj) &&
				GetAlignment(Selection.obj) != objAln && GetAlignment(Selection.obj) != NEUTRAL
			) {
				__TryHit(objNum, opp_x, opp_y, __PickWeapon(objNum, Selection.obj));
				SubMov(objNum, 10);
				return;
			}
		}
	}
	SubMov(objNum, 5);
}

/*tangle vine frames: VineFrames[prev_dir][dir >> 1] is the frame of the vine
  segment left behind (corner/straight), VineFrames[dir][rand 0..3] the frame
  of the growing tip. NB: indexed past the 2 columns, so it is really
  used as a flat 16 bytes table (index = 2*row + column)*/
/*1BB8*/unsigned char VineFrames[][2] = {
	{0x01,0x03},
	{0x01,0x04},
	{0x05,0x00},
	{0x04,0x00},
	{0x01,0x02},
	{0x01,0x05},
	{0x02,0x00},
	{0x03,0x00}
};

/*the tangle vine pod moves one tile in direction dir (0,2,4,6), leaving a
  vine segment (quality = pod number) behind; no going backward.
  returns 1 on success*/
static /*C_2337_43D2*/__TryGrowVine(int objNum, int dir) {
	int moved, x, y, prev_dir, vineObj;

	dir &= 6;
	x = GetX(objNum);
	y = GetY(objNum);
	prev_dir = GetDirection(objNum);
	if((dir ^ prev_dir) == 4)
		return 0;
	moved = TryStraightMove(objNum, dir, 1);
	if(moved == 0)
		return 0;
	vineObj = AddMapObj(OBJ_TANGLE_VINE_VINE, LOCAL, QualQuan(objNum, 0), x, y, MapZ);
	if(vineObj >= 0) {
		SetFrame(vineObj, VineFrames[prev_dir][dir >> 1]);
		SetFrame(objNum, VineFrames[dir][OSI_rand(0, 3)]);
		return 1;
	}
	MoveObj(objNum, x, y, MapZ);
	SetDirection(objNum, prev_dir);

	return 0;
}

/*AI_TANGLE (tangle vine pod): hit an adjacent opponent, or grow toward
  one within 4 tiles, or grow at random (1 in 4)*/
/*C_2337_44F6*/COMBAT_AI_Tangle(int objNum) {

	int dir;
	int opp_x, opp_y, tangle_x, tangle_y;

	Selection.obj = COMBAT_pickOpponent(objNum);
	if(Selection.obj >= 0) {
		opp_x = GetX(Selection.obj);
		opp_y = GetY(Selection.obj);
		tangle_x = GetX(objNum);
		tangle_y = GetY(objNum);
		if(CLOSE_ENOUGH_S(2, opp_x, opp_y, tangle_x, tangle_y) && OSI_rand(0, 1)) {
			__TryHit(objNum, opp_x, opp_y, objNum);
			SubMov(objNum, 10);
			return;
		}
		if(CLOSE_ENOUGH_S(5, opp_x, opp_y, tangle_x, tangle_y) && OSI_rand(0, 3) == 0) {
			opp_x -= tangle_x;
			opp_y -= tangle_y;
			if(abs(opp_x) > abs(opp_y)) {
				dir = (opp_x > 0)?2:6;
				if(!__TryGrowVine(objNum, dir)) {
					dir = (opp_y > 0)?4:0;
					__TryGrowVine(objNum, dir);
					return;
				}
			} else {
				dir = (opp_y > 0)?4:0;
				if(!__TryGrowVine(objNum, dir)) {
					dir = (opp_x > 0)?2:6;
					__TryGrowVine(objNum, dir);
					return;
				}
			}
		}
	}
	if(OSI_rand(0, 3) == 0) {
		dir = OSI_rand(0, 3) << 1;
		if(dir == GetDirection(objNum))
			dir ^= 4;
		__TryGrowVine(objNum, dir);
	} else {
		MovePts[objNum] = 0;
	}
}
