/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	creature generation "EGGs" module

	An egg (OBJ_EGG) is an invisible map object whose inventory holds the
	"embryos": one object per creature type to spawn, quantity = maximum
	number of creatures, quality = their combat mode (NPCMode), "mutant"
	status bit = two-headed (giant scorpion & co). The egg's own quantity
	is the percentage chance to hatch and its quality encodes
	when (quality / 10: 0 always, 1 day, 2 night) and the alignment of the
	creatures (quality % 10: 0 class default, else (q - 1) << 5, i.e.
	1 neutral, 2 evil, 3 good, 4 chaotic).
	Eggs hatch when their area is loaded (EGG_hatchArea), once until the
	HATCHED status bit is cleared. EGG_generate creates one monster with random
	attributes from its class tables (monsdata.c), its extra map parts
	(multi-tile monsters) and its random possessions.
	Also holds GetMonsterClass(), the type -> monster class lookup.
*/
#include "u6.h"

#undef abs
#include <math.h>

/*28A0*/int ForceHatching = 0;
/*next segment index of the silver serpent being built (stored in the
  quality of every segment)*/
/*28A2*/static unsigned char SerpentSegIndex = 0;

/*uncalled?*/
/*same as COMBAT_range(), but returns 8 when too far*/
/*__2E2D_0003*/EGG_CombatRange(int x0, int y0, int x1, int y1) {
	int delta_x, delta_y;

	delta_x = abs(x1 - x0);
	delta_y = abs(y1 - y0);
	if(delta_x > 7 || delta_y > 7)
		return 8;
	return RangeTable[delta_x][delta_y];
}

/*monster class (index in the monsdata.c tables) of an object type;
  -1 if the type is not a creature*/
/*C_2E2D_004B*/GetMonsterClass(int objType) {
	int i, cls;

	cls = -1;
	for(i = 0; MonsterTypes[i]; i ++) {
		if(MonsterTypes[i] == objType) {
			cls = i;
			break;
		}
	}
	return cls;
}

/*random value in [val - val/2, val + val/2] (triangular distribution)*/
static /*C_2E2D_0088*/mkRandom(int val) {
	int half = val >> 1;
	if(half)
		val = OSI_rand(0, half) + (val - half) + OSI_rand(0, half);
	return val;
}

/*food found in monster loot*/
/*28A3*/static unsigned LootFoodTypes[] = {OBJ_PORTION_OF_MEAT,OBJ_HAM,OBJ_LOAF_OF_BREAD};

/*generate monster posessions: the class list (MonsterInvLists at offset
  MonsterInvIndex[class]) is 4 zero-terminated byte lists:
  spells (given as invisible equipped charges), weapons, armours and
  treasures; the n-th entry of the last 3 lists is given with a
  1 in 2^n chance. OBJ_SPELL in the treasure list means "random loot"
  (gold, food, potion)*/
/*C_2E2D_00BE*/static EGG_GivePossessions(int objNum, int objClass) {
	int oneIn, j;
	int newObj, numWeapons;
	int chestObj;
	unsigned char itemType;
	unsigned char far *pList;

	pList = &(MonsterInvLists[MonsterInvIndex[objClass]]);
	/*spells*/
	while(itemType = *pList++) {
		newObj = AddInvObj(OBJ_CHARGE, LOCAL | EQUIP, QualQuan(itemType, 1), objNum);
		SetInvisible(newObj);
	}
	/*weapons*/
	for(oneIn = 2; itemType = *pList++; oneIn <<= 1) {
		if(OSI_rand(1, oneIn) == 1) {
			/*throwing weapons come by 6 to 12*/
			if(itemType  == OBJ_SPEAR || itemType == OBJ_THROWING_AXE || itemType == OBJ_DAGGER)
				numWeapons = OSI_rand(6, 12);
			else
				numWeapons = 1;
			for(j = 0; j < numWeapons; j ++) {
				newObj = AddInvObj(itemType, LOCAL | EQUIP | OWNED, 0, objNum);
				if(newObj >= 0) {
					if(itemType == OBJ_BOW || itemType == OBJ_MAGIC_BOW)
						AddInvObj(OBJ_ARROW, LOCAL | INVEN | OWNED, OSI_rand(12, 24), objNum);
					if(itemType == OBJ_CROSSBOW || itemType == OBJ_TRIPLE_CROSSBOW)
						AddInvObj(OBJ_BOLT, LOCAL | INVEN | OWNED, OSI_rand(12, 24), objNum);
				}
			}
		}
	}
	/*armors*/
	for(oneIn = 2; itemType = *pList++; oneIn <<= 1) {
		if(OSI_rand(1, oneIn) == 1)
			AddInvObj(itemType, LOCAL | EQUIP | OWNED, 1, objNum);
	}
	/*treasures*/
	for(oneIn = 2; itemType = *pList++; oneIn <<= 1) {
		if(OSI_rand(1, oneIn) == 1) {
			if(itemType == OBJ_OPEN_CHEST) {/*a closed chest, maybe trapped*/
				chestObj = AddInvObj(itemType, LOCAL | CONTAINED | OWNED, OSI_rand(1, 100), objNum);
				if(chestObj >= 0) {
					SetFrame(chestObj, 1);
					if(OSI_rand(0, 1) != 0 && !IsMonsterAcidSlug(objClass))
						AddInvObj(OBJ_EFFECT, LOCAL | CONTAINED | OWNED, QualQuan(SPELL_TRAP, 0), chestObj);
					if(OSI_rand(0, 3) != 0)
						AddInvObj(OBJ_GOLD, LOCAL | CONTAINED | OWNED, OSI_rand(1, 100), chestObj);
					if(OSI_rand(0, 3) != 0)
						AddInvObj(LootFoodTypes[OSI_rand(0, 2)], LOCAL | CONTAINED | OWNED, OSI_rand(1, 10), chestObj);
					if(OSI_rand(0, 3) == 0)
						AddInvObj(OBJ_TORCH, LOCAL | CONTAINED | OWNED, OSI_rand(1, 6), chestObj);
					if(OSI_rand(0, 10) == 0)
						AddInvObj(OBJ_GEM, LOCAL | CONTAINED | OWNED, OSI_rand(1, 4), chestObj);
				}
			} else if(itemType == OBJ_SPELL) {
				if(OSI_rand(0, 3) != 0 && !IsMonsterWingedGargoyle(objClass))
					AddInvObj(OBJ_GOLD, LOCAL | INVEN | OWNED, OSI_rand(1, 100), objNum);
				if(OSI_rand(0, 3) == 0)
					AddInvObj(LootFoodTypes[OSI_rand(0, 2)], LOCAL | INVEN | OWNED, OSI_rand(1, 10), objNum);
				if(OSI_rand(0, 3) == 0)
					AddInvObj(TypeFrame(OBJ_POTION, OSI_rand(0, 7)), LOCAL | INVEN | OWNED, 1, objNum);
			} else if(itemType == OBJ_GOLD) {
				AddInvObj(itemType, LOCAL | INVEN | OWNED, OSI_rand(1, 30) + OSI_rand(1, 30), objNum);
			} else {
				AddInvObj(itemType, LOCAL | INVEN | OWNED, 1, objNum);
			}
		}
	}
}

/*create a creature of type objType (bit 15: mutant/two-headed) at x,y,z:
  attributes from its class (mkRandom'ized), level from HP, alignment
  from the class, AI_ASSAULT, extra parts of multi-tile monsters and its
  possessions. returns the new actor number (< 0 if none was free)*/
/*C_2E2D_0499*/EGG_generate(int objType, int x, int y, int z) {
	int objNum, partObj;
	int objClass, dir, isMutant;

	isMutant = 0;
	if(objType & 0x8000) {
		objType &= 0x7fff;
		isMutant = 1;
	}
	objNum = AddMonster(objType, x, y, z);
	if(objNum < 0)
		return objNum;
	objClass = GetMonsterClass(objType);
	if(objClass >= 0) {
		STREN[objNum] = mkRandom(MonsterStr[objClass]);
		DEXTE[objNum] = mkRandom(MonsterDex[objClass]);
		INTEL[objNum] = mkRandom(MonsterInt[objClass]);
		HitPoints[objNum] = mkRandom(MonsterHP[objClass]);
		Level[objNum] = (HitPoints[objNum] + 29) / 30;
		NPCStatus[objNum] = MonsterAlignment[objClass];
	} else {
		STREN[objNum] =
		DEXTE[objNum] =
		INTEL[objNum] = 10;
		HitPoints[objNum] = 30;
		Level[objNum] = 1;
		NPCStatus[objNum] = EVIL;
	}
	MAGIC[objNum] = MaxMagic(objNum);
	MovePts[objNum] = DEXTE[objNum];
	ExpPoints[objNum] = 100;
	NPCMode[objNum] =
	NPCComMode[objNum] = AI_ASSAULT;

	if(objType == OBJ_WINGED_GARGOYLE || objType == OBJ_CYCLOPS) {
		SetFrame(objNum, 0x13);
	} else if(objType >= OBJ_GIANT_SCORPION) {
		/*2-tile monsters: second part at x+1*/
		SetFrame(objNum, 6);
		if(isMutant)
			partObj = AddMapObj(TypeFrame(objType, 2), 1, 0, x + 1, y, z);
		else
			partObj = AddMapObj(TypeFrame(objType, 0xe), 1, 0, x + 1, y, z);
		if(partObj >= 0) 
			ClrLocal(partObj);
	} else if(objType == OBJ_DRAGON) {
		/*body*/
		SetFrame(objNum, 0);
		/*head*/
		partObj = AddMapObj(TypeFrame(OBJ_DRAGON, 8), 1, 0, x, y - 1, z);
		if(partObj >= 0)
			ClrLocal(partObj);
		/*tail*/
		partObj = AddMapObj(TypeFrame(OBJ_DRAGON, 0x10), 1, 0, x, y + 1, z);
		if(partObj >= 0) 
			ClrLocal(partObj);
		/*left wing*/
		partObj = AddMapObj(TypeFrame(OBJ_DRAGON, 0x18), 1, 0, x - 1, y, z);
		if(partObj >= 0) 
			ClrLocal(partObj);
		/*right wing*/
		partObj = AddMapObj(TypeFrame(OBJ_DRAGON, 0x20), 1, 0, x + 1, y, z);
		if(partObj >= 0) 
			ClrLocal(partObj);
	} else if(objType == OBJ_HYDRA) {
		/*8 bodies around the head; frame = direction << 2 (see COMBAT_getHead)*/
		for(dir = 0; dir < 8; dir ++) {
			partObj = AddMapObj(OBJ_HYDRA_BODY + (dir << 12), 1, 0, x + DirIncrX[dir], y + DirIncrY[dir], z);
			if(partObj >= 0) 
				ClrLocal(partObj);
		}
	}

	EGG_GivePossessions(objNum, objClass);

	return objNum;
}

/*silver serpent related: the body is coiled in a 2x2 square, segment n
  is put at (x + SerpentSegDX[n & 3], y + SerpentSegDY[n & 3]) with frame SerpentBodyFrames[n & 3],
  the last one gets the tail frame SerpentTailFrames[n & 3]*/
/*28A9*/static char SerpentBodyFrames[] = {0xD,0xA,0xB,0xC};/*curves*/
/*28AD*/static char SerpentTailFrames[] = {0x1,0x3,0x5,0x7};/*tails*/
/*28B1*/static char SerpentSegDX[] = {0, -1, -1, 0};/*x offset*/
/*28B5*/static char SerpentSegDY[] = {1,  1,  0, 0};/*y offset*/
/*__28B9	db 0*/

/*try to hatch an egg (see the module header). The first creature
  appears on the egg, the others are scattered around it (not on
  screen unless the egg is "local" or ForceHatching is set). Gargoyles
  only graze when a gargoyle is in the party or the avatar wears the
  amulet of submission. When hostile creatures appear off-screen,
  Shamino may say where he hears something. The egg is then marked
  hatched (and invisible) whatever the result*/
/*C_2E2D_0760*/EGG_hatches(int egg_objNum) {
	int monster;
	int i, seg, quan, objTyp, typParam, numMonsters;
	int partObj, prevPart, x, y, embryo_objNum, alignment;
	int dir, hostileOffscreen, isFirstBorn, npcMod, forceGarglGraze, onScreenNotOk;
	
	prevPart = 0;
	npcMod = AI_MOTIONLESS;
	forceGarglGraze = 0;

	if(IsArmageddon)
		return;
	/*Check DAY egg*/
	if(
		(GetQual(egg_objNum) / 10 == 1/*DAY*/) &&
		!(Time_H >= 6 && Time_H <= 18)
	) return;
	/*Check NIGHT egg*/
	if(
		(GetQual(egg_objNum) / 10 == 2/*NIGHT*/) &&
		(Time_H > 5 && Time_H < 19)
	) return;
	/*creatures of a non-local egg must appear off-screen*/
	if(IsLocal(egg_objNum))
		onScreenNotOk = 0;
	else
		onScreenNotOk = !ForceHatching;
	/*Gargoyles/amulet of submission*/
	for(i = 0; i < PartySize; i ++) {
		prevPart = GetType(Party[i]);/*here: just a temporary*/
		if(prevPart == OBJ_GARGOYLE || prevPart == OBJ_WINGED_GARGOYLE) {
			forceGarglGraze = 1;
			break;
		}
		/*NB: always tests the avatar (object 1), once per party member*/
		if(FindInvType(1, OBJ_AMULET_OF_SUBMISSION, -1) >= 0)
			forceGarglGraze = 1;
	}
	/* */
	if(!IsHatched(egg_objNum)) {
		/*Quality of egg: alignment of monsters*/
		alignment = ((GetQual(egg_objNum) % 10) - 1) << 5;
		/*Quantity of egg: percent chance of hatching*/
		if(OSI_rand(1, 100) <= GetQuan(egg_objNum)) {
			hostileOffscreen = 0;
			isFirstBorn = 1;
			/*Contents of egg: "embryos"*/
			for(embryo_objNum = FindInv(egg_objNum); embryo_objNum >= 0; embryo_objNum = NextInv()) {
				objTyp =
				typParam = GetType(embryo_objNum);
				/*two-headed?*/
				if(IsMutant(embryo_objNum))
					typParam |= 0x8000;
				/*Quantity of embryo: number of monsters in party*/
				numMonsters = GetQuan(embryo_objNum);
				if(GetQuan(egg_objNum) != 100)
					numMonsters = OSI_rand(1, numMonsters);
				if(objTyp == OBJ_SILVER_SERPENT)
					numMonsters = 1;
				/* */
				if((objTyp == OBJ_GARGOYLE || objTyp == OBJ_WINGED_GARGOYLE) && forceGarglGraze)
					npcMod = AI_GRAZE;
				/* */
				for(i = 0; i < numMonsters; i ++) {
					if(objTyp < OBJ_GIANT_RAT && objTyp != OBJ_OPEN_CHEST) {
						/*not a creature: plain object*/
						if(QuanType(objTyp))
							quan = 1;
						else
							quan = 0;
						monster = AddMapObj(objTyp, 1, quan, GetX(egg_objNum), GetY(egg_objNum), MapZ);
					} else {
						monster = EGG_generate(typParam, GetX(egg_objNum), GetY(egg_objNum), MapZ);
						if(monster >= 0) {
							if(GetQual(egg_objNum) % 10)
								SetAlignment(monster, alignment);
							if(Is_ATKPLR(monster) && !IsLocal(egg_objNum))
								hostileOffscreen = 1;
						}
					}
					if(monster < 0)
						continue;
					/**/
					if(!isFirstBorn) {
						if(!COMBAT_TryTeleport(monster, onScreenNotOk))
							DeleteObj(monster);
					} else {
						isFirstBorn = 0;
					}
					/* */
					if(objTyp == OBJ_SILVER_SERPENT) {/*Silver serpent*/
						/*embryo quantity = number of body segments;
						  segment quantity = head, quality = index*/
						SerpentSegIndex = 0;
						SetFrame(monster, 0);/*head*/
						SetQual(monster, SerpentSegIndex ++);
						x = GetX(monster);
						y = GetY(monster);
						for(seg = 0; seg <= GetQuan(embryo_objNum); seg ++) {
							partObj = AddMapObj(
								TypeFrame(OBJ_SILVER_SERPENT, SerpentBodyFrames[seg & 3]),
								1,
								QualQuan(SerpentSegIndex ++, monster),
								SerpentSegDX[seg & 3] + x, SerpentSegDY[seg & 3] + y, MapZ
							);
							if(partObj < 0) {
								if(seg)
									SetFrame(prevPart, SerpentTailFrames[(seg - 1) & 3]);
								else
									DeleteObj(monster);
								break;
							}
							ClrLocal(partObj);
							prevPart = partObj;
							if(GetQuan(embryo_objNum) == seg)
								SetFrame(partObj, SerpentTailFrames[seg & 3]);
						}
					} else if(objTyp == OBJ_TANGLE_VINE_POD) {/*Tangle vine(vine)*/
						/*4 vine actors around the pod*/
						SetFrame(monster, 0);
						x = GetX(monster);
						y = GetY(monster);
						for(seg = 0; seg < 8; seg += 2) {
							partObj = EGG_generate(OBJ_TANGLE_VINE_VINE, DirIncrX[seg] + x, DirIncrY[seg] + y, MapZ);
							if(partObj < 0)
								continue;
							SetDirection(partObj, seg);
							SetFrame(partObj, (seg & 2)?0:1);
							SetAlignment(partObj, GetAlignment(monster));
							NPCComMode[partObj] = 
							NPCMode[partObj] = AI_TANGLE;
						}
					}
					if(monster >= 0x100)
						continue;
					/*Quality of embryo: combat strategy for monsters*/
					if(npcMod == AI_MOTIONLESS)
						npcMod = GetQual(embryo_objNum);
					NPCComMode[monster] = 
					NPCMode[monster] = npcMod;

					if(npcMod == AI_PATROL_EW)
						SetDirection(monster, 2);/*east*/
					else if(npcMod == AI_PATROL_NS)
						SetDirection(monster, 0);/*north*/

					if((objTyp == OBJ_GARGOYLE || objTyp == OBJ_WINGED_GARGOYLE) && NPCMode[monster] == AI_BERSEK) {
						STREN[monster] <<= 1;
						DEXTE[monster] <<= 1;
						INTEL[monster] <<= 1;
					}
				}
			}
			/*"Shamino hears something to the <dir>"*/
			if(!EnemyWarningGiven && hostileOffscreen) {
				x = GetX(egg_objNum) - MapX;
				y = GetY(egg_objNum) - MapY;
				dir = MkDirection((x << 4) + DirOriginX, (y << 4) + DirOriginY);
				if(OSI_rand(0, 3) && SHAMINO_COMMENT) {
					CON_printf(ShaminoHearsMsg, DirNames[dir]);
					PromptFlag = 1;
				}
				EnemyWarningGiven = 1;
			}
		}
	}
	SetHatched(egg_objNum);
	SetInvisible(egg_objNum);
}

/*hatch the eggs of the current level in the loaded 40x40 area: the
  ones out of sight (more than 8 tiles away), local ones, or all of them
  with ForceHatching*/
/*C_2E2D_0DFE*/EGG_hatchArea() {

	int objNum, x, y, objTyp;

	if(IsArmageddon)
		return;
	SerpentSegIndex = 0;
	x = AreaX;
	y = AreaY;
	for(objNum = SearchArea(x, y, x + 39, y + 39); objNum >= 0; objNum = NextArea()) {
		x = GetX(objNum);
		y = GetY(objNum);
		objTyp = GetType(objNum);
		if(objTyp != OBJ_EGG || GetZ(objNum) != MapZ)
			continue;
		if(ForceHatching || !CLOSE_ENOUGH(8, x, y, MapX, MapY) || IsLocal(objNum))
			EGG_hatches(objNum);
	}
}
