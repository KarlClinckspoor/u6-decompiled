/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __SEG_3522_H__
#define __SEG_3522_H__

/*3522:0000*/extern int far MonsterTypes[];/*object type of each monster class, 0-terminated*/

/*3522:0082*/extern unsigned char far MonsterStr[];/*base strength per class*/
/*3522:00C2*/extern unsigned char far MonsterDex[];/*base dexterity per class*/
/*3522:0102*/extern unsigned char far MonsterInt[];/*base intelligence per class*/
/*3522:0142*/extern unsigned char far MonsterArmor[];/*armor per class (only used in the XP formula)*/
/*3522:0182*/extern unsigned char far MonsterDamage[];/*natural attack damage per class*/
/*3522:01C2*/extern unsigned char far MonsterHP[];/*base hit points per class*/

/*3522:0202*/extern unsigned char far MonsterAlignment[];/*initial NPCStatus (alignment) per class*/

/*3522:0242*/extern int far MonsterFlags[];/*MONSTER_xxx ability flags per class*/

/*0x0001 immune to sleep (spell, sleep field)*/
#define MONSTER_IMMUNE_SLEEP 0x0001
/*0x0002 immune to death (kill) spells and fear*/
#define MONSTER_IMMUNE_DEATH 0x0002
/*0x0004 immune to poison*/
#define MONSTER_IMMUNE_POISON 0x0004
/*0x0008 immune to fire*/
#define MONSTER_IMMUNE_FIRE 0x0008
/*0x0010 double damage from fire*/
#define MONSTER_DOUBLE_FIRE_DMG 0x0010
/*0x0020 natural attack to-hit uses STR instead of DEX (__AttackTest)*/
#define MONSTER_STR_ATTACK 0x0020
/*0x0040 hits poison 1 time in 4*/
#define MONSTER_POISONOUS 0x0040
/*0x0080 undead: repel undead, half damage from normal weapons*/
#define MONSTER_UNDEAD 0x0080
/*0x0100 flies: ignores terrain effects and tremor*/
#define MONSTER_FLIES 0x0100
/*0x0200 moves on water*/
#define MONSTER_SWIMS 0x0200
/*0x0400 passes through walls*/
#define MONSTER_ETHEREAL 0x0400
/*0x0800 object deleted when killed (if no dead shape)*/
#define MONSTER_NO_CORPSE 0x0800
/*0x1000 never leaves a dead shape; deleted with a screen fade when near*/
#define MONSTER_VANISHES 0x1000
/*0x2000 leaves blood when killed*/
#define MONSTER_BLEEDS 0x2000
/*0x4000 human: opens doors/passthroughs, 'yells for help' when killed by party*/
#define MONSTER_HUMAN 0x4000
/*0x8000 never used*/
#define MONSTER_UNUSED_8000 0x8000

/*[IS]imune to sleep/zu*/
#define IsMonsterImmuneSleep(cls) (MonsterFlags[cls] & MONSTER_IMMUNE_SLEEP)
/*[ID]imune to death spells/corp*/
#define IsMonsterDeathImmune(cls) (MonsterFlags[cls] & MONSTER_IMMUNE_DEATH)
/*[IP]imune to poison/nox*/
#define IsMonsterImmunePoison(cls) (MonsterFlags[cls] & MONSTER_IMMUNE_POISON)
/*[IF]imune to fire/flam*/
#define IsMonsterFireImmune(cls) (MonsterFlags[cls] & MONSTER_IMMUNE_FIRE)
/*[DF]take double damage from fire*/
#define IsMonsterFireVulnerable(cls) (MonsterFlags[cls] & MONSTER_DOUBLE_FIRE_DMG)
/*flag test MONSTER_STR_ATTACK*/
#define IsMonsterStrAttack(cls) (MonsterFlags[cls] & MONSTER_STR_ATTACK)
/*flag test MONSTER_POISONOUS*/
#define IsMonsterPoisonous(cls) (MonsterFlags[cls] & MONSTER_POISONOUS)
/*[UN]undead nature*/
#define IsMonsterUndead(cls) (MonsterFlags[cls] & MONSTER_UNDEAD)
/*[FL]flies; also immune to tremor spells*/
#define IsMonsterFlying(cls) (MonsterFlags[cls] & MONSTER_FLIES)
/*[SW]swims*/
#define IsMonsterSwimming(cls) (MonsterFlags[cls] & MONSTER_SWIMS)
/*[ET]ethereal (walk through walls)*/
#define IsMonsterEthereal(cls) (MonsterFlags[cls] & MONSTER_ETHEREAL)
/*flag test MONSTER_NO_CORPSE*/
#define IsMonsterNoCorpse(cls) (MonsterFlags[cls] & MONSTER_NO_CORPSE)
/*flag test MONSTER_VANISHES*/
#define IsMonsterVanishing(cls) (MonsterFlags[cls] & MONSTER_VANISHES)
/*flag test MONSTER_BLEEDS*/
#define IsMonsterBleeding(cls) (MonsterFlags[cls] & MONSTER_BLEEDS)
/*flag test MONSTER_HUMAN*/
#define IsMonsterHuman(cls) (MonsterFlags[cls] & MONSTER_HUMAN)

/*[AC]acid (dissolves inventory item){Slug}*/
#define IsMonsterAcidSlug(cls) (cls == 0x00)
/*[AM]amphibious{Alligator}*/
#define IsMonsterAlligator(cls) (cls == 0x01)
/*[DR]drags victims underground{Corpser}*/
#define IsMonsterCorpser(cls) (cls == 0x08)
/*[IM]immortal{Lord British}*/
#define IsMonsterLordBritish(cls) (cls == 0x1f)
/*[IN]changes to insects when dead*/
#define IsMonsterGazer(cls) (cls == 0x15)
/*[SP]splits in two{Slime}*/
#define IsMonsterSlime(cls) (cls == 0x30)
/*[SF]steals food{Gremlin}*/
#define IsMonsterGremlin(cls) (cls == 0x17)
/*class 0x13; such creatures get no gold from OBJ_SPELL treasure*/
#define IsMonsterWingedGargoyle(cls) (cls == 0x13)


/*3522:02C2*/extern int far MonsterInvIndex[];/*offset in MonsterInvLists of the class's starting inventory*/

/*3522:0342*/extern unsigned char far MonsterInvLists[];/*packed 0-terminated spell/weapon/armor/treasure lists*/


#endif
