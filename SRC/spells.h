/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __SPELLS_H__
#define __SPELLS_H__

#define REAGENT_MR 0x01
#define REAGENT_NS 0x02
#define REAGENT_BP 0x04
#define REAGENT_BM 0x08
#define REAGENT_SS 0x10
#define REAGENT_GA 0x20
#define REAGENT_GS 0x40
#define REAGENT_SA 0x80

/*1st circle*/
#define SPELL_CREATE_FOOD 0x00
#define SPELL_DETECT_MAGIC 0x01
#define SPELL_DETECT_TRAP 0x02
#define SPELL_DISPEL_MAGIC 0x03
#define SPELL_DOUSE 0x04
#define SPELL_HARM 0x05
#define SPELL_HEAL 0x06
#define SPELL_HELP 0x07
#define SPELL_IGNITE 0x08
#define SPELL_LIGHT 0x09
/*2nd circle*/
#define SPELL_INFRAVISION 0x10
#define SPELL_MAGIC_ARROW 0x11
#define SPELL_POISON 0x12
#define SPELL_REAPPEAR 0x13
#define SPELL_SLEEP 0x14
#define SPELL_TELEKINESIS 0x15
#define SPELL_TRAP 0x16
#define SPELL_UNLOCK_MAGIC 0x17
#define SPELL_UNTRAP 0x18
#define SPELL_VANISH 0x19
/*3rd circle*/
#define SPELL_CURSE 0x20
#define SPELL_DISPEL_FIELD 0x21
#define SPELL_FIREBALL 0x22
#define SPELL_GREAT_LIGHT 0x23
#define SPELL_LOCK 0x24
#define SPELL_MASS_AWAKEN 0x25
#define SPELL_MASS_SLEEP 0x26
#define SPELL_PEER 0x27
#define SPELL_PROTECTION 0x28
#define SPELL_REPEL_UNDEAD 0x29
/*4th circle*/
#define SPELL_ANIMATE 0x30
#define SPELL_CONJURE 0x31
#define SPELL_DISABLE 0x32
#define SPELL_FIRE_FIELD 0x33
#define SPELL_GREAT_HEAL 0x34
#define SPELL_LOCATE 0x35
#define SPELL_MASS_DISPEL 0x36
#define SPELL_POISON_FIELD 0x37
#define SPELL_SLEEP_FIELD 0x38
#define SPELL_WIND_CHANGE 0x39
/*5th circle*/
#define SPELL_ENERGY_FIELD 0x40
#define SPELL_EXPLOSION 0x41
#define SPELL_INSECT_SWARM 0x42
#define SPELL_INVISIBILITY 0x43
#define SPELL_LIGHTNING 0x44
#define SPELL_PARALYZE 0x45
#define SPELL_PICKPOCKET 0x46
#define SPELL_REVEAL 0x47
#define SPELL_SEANCE 0x48
#define SPELL_X_RAY 0x49
/*6th circle*/
#define SPELL_CHARM 0x50
#define SPELL_CLONE 0x51
#define SPELL_CONFUSE 0x52
#define SPELL_FLAME_WIND 0x53
#define SPELL_HAIL_STORM 0x54
#define SPELL_MASS_PROTECT 0x55
#define SPELL_NEGATE_MAGIC 0x56
#define SPELL_POISON_WIND 0x57
#define SPELL_REPLICATE 0x58
#define SPELL_WEB 0x59
/*7th circle*/
#define SPELL_CHAIN_BOLT 0x60
#define SPELL_ENCHANT 0x61
#define SPELL_ENERGY_WIND 0x62
#define SPELL_FEAR 0x63
#define SPELL_GATE_TRAVEL 0x64
#define SPELL_KILL 0x65
#define SPELL_MASS_CURSE 0x66
#define SPELL_MASS_INVIS 0x67
#define SPELL_WING_STRIKE 0x68
#define SPELL_WIZARD_EYE 0x69
/*8th circle*/
#define SPELL_ARMAGEDDON 0x70
#define SPELL_DEATH_WIND 0x71
#define SPELL_ECLIPSE 0x72
#define SPELL_MASS_CHARM 0x73
#define SPELL_MASS_KILL 0x74
#define SPELL_RESURRECT 0x75
#define SPELL_SLIME 0x76
#define SPELL_SUMMON 0x77
#define SPELL_TIME_STOP 0x78
#define SPELL_TREMOR 0x79

/*kind of invisibility?*/
#define SPELL_SELF_INVISIBILITY 0x80
/*teleport (for wisps)*/
#define SPELL_WISP_TELEPORT 0x81

#define MK_CIRCLE(spell) (spell / 0x10 + 1)

#define HAS_SPELL(circle,spell) ((SpellList[circle] >> (spell)) & 1)
#define SET_SPELL(circle,spell) (SpellList[circle] |= 1 << (spell))

#endif
