/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __TILE_H__
#define __TILE_H__

/*====---- start of grounds ----====*/

#define TIL_001	0x001
#define TIL_SWAMP_0	0x002
#define TIL_003	0x003
#define TIL_004	0x004
#define TIL_005	0x005

#define TIL_007	0x007
#define TIL_008	0x008

#define TIL_010	0x010

#define TIL_01C	0x01c

#define TIL_020	0x020

#define TIL_02C	0x02c

#define TIL_06C	0x06c

#define TIL_06F	0x06f

#define TIL_WALL_FIRST	0x090

#define TIL_0D6	0x0d6

#define TIL_LAVA_0	0x0dc
#define TIL_0DD	0x0dd
#define TIL_0DE	0x0de
#define TIL_0DF	0x0df

#define TIL_0FC	0x0fc
/*Hidden*/
#define TIL_HIDDEN	0x0ff

#define TIL_WALL_CORNERS	0x100
#define TIL_HIT	0x101


#define TIL_SUNDIAL_NIGHT	0x13c

/*Sundial #0*/
#define TIL_SUNDIAL	0x148

/*Outside #0*/
#define TIL_OUTSIDE	0x160

/*Sun #0*/
#define TIL_SUN_0	0x169
/*Sun #1*/
#define TIL_SUN_1	0x16a
/*Sun #2*/
#define TIL_SUN_2	0x16b
/*Cursor direction*/
#define TIL_CURSOR_DIRECTION	0x16c
/*Cursor select*/
#define TIL_CURSOR_SELECT	0x16d

/*Equip up left*/
#define TIL_EQUIP_UP_LEFT	0x170
/*Equip up right*/
#define TIL_EQUIP_UP_RIGHT	0x171
/*Equip down left*/
#define TIL_EQUIP_DOWN_LEFT	0x172
/*Equip down right*/
#define TIL_EQUIP_DOWN_RIGHT	0x173
/*Underground left*/
#define TIL_UNDERGROUND_LEFT	0x174
/*Underground middle*/
#define TIL_UNDERGROUND_MIDDLE	0x175
/*Underground right*/
#define TIL_UNDERGROUND_RIGHT	0x176

#define TIL_POISON_MISSILE	0x17C
#define TIL_17D	0x17D
#define TIL_FIRE_MISSILE	0x17E
#define TIL_17F	0x17F
/*Button party*/
#define TIL_BUTTON_PARTY	0x180
/*Button portrait*/
#define TIL_BUTTON_PORTRAIT	0x181
/*Button bag*/
#define TIL_BUTTON_BAG	0x182
/*Button next left*/
#define TIL_BUTTON_NEXT_LEFT	0x183
/*Button next right*/
#define TIL_BUTTON_NEXT_RIGHT	0x184
/*Slot 0*/
#define TIL_SLOT_TWO_HANDED	0x185

#define TIL_BUTTON_COMBAT_MODE	0x187
/*Missile #0*/
#define TIL_MISSILE_0	0x188
/*Missile #1*/
#define TIL_MISSILE_1	0x189
/*Missile #2*/
#define TIL_MISSILE_2	0x18a
/*Missile #3*/
#define TIL_MISSILE_3	0x18b
/*Missile #4*/
#define TIL_MISSILE_4	0x18c
/*Missile #5*/
#define TIL_MISSILE_5	0x18d
/*Missile #6*/
#define TIL_MISSILE_6	0x18e
/*Missile #7*/
#define TIL_MISSILE_7	0x18f
/*Button attack*/
#define TIL_BUTTON_ATTACK	0x190

/*Button floppy*/
#define TIL_SLOT_FRAME	0x19a
/*Slot empty*/
#define TIL_SLOT_EMPTY	0x19b

#define TIL_BUTTON_COMBAT_OFF	0x19e
#define TIL_BUTTON_COMBAT_ON	0x19f

/*Frame #0*/
#define TIL_FRAME_0	0x1b0
/*Frame #1*/
#define TIL_FRAME_1	0x1b1
/*Frame #2*/
#define TIL_FRAME_2	0x1b2
/*Frame #3*/
#define TIL_FRAME_3	0x1b3
/*Frame #4*/
#define TIL_FRAME_4	0x1b4
/*Frame #5*/
#define TIL_FRAME_5	0x1b5
/*Frame #6*/
#define TIL_FRAME_6	0x1b6
/*Frame #7*/
#define TIL_FRAME_7	0x1b7
/*lens related?*/
#define TIL_1B8	0x1b8
/*lens related?*/
#define TIL_1B9	0x1b9
/*lens related?*/
#define TIL_1BA	0x1ba
/*lens related?*/
#define TIL_1BB	0x1bb
/*Darkness?*/
#define TIL_DARKNESS	0x1bc

/*====---- start of items ----====*/
/*Leather Helm*/
#define TIL_LEATHER_HELM	0x200

/*Magic Helm*/
#define TIL_MAGIC_HELM	0x207
/*Wooden Shield*/
#define TIL_WOODEN_SHIELD	0x208

/*Magic Shield*/
#define TIL_MAGIC_SHIELD	0x20f
/*Cloth Armor*/
#define TIL_CLOTH_ARMOR	0x210

/*Magic Armor*/
#define TIL_MAGIC_ARMOR	0x216
/*Spiked Collar*/
#define TIL_SPIKED_COLLAR	0x217
/*Guild Belt*/
#define TIL_GUILD_BELT	0x218
/*Gargoyle Belt*/
#define TIL_GARGOYLE_BELT	0x219
/*Leather Boots*/
#define TIL_LEATHER_BOOTS	0x21a
/*Swamp Boots*/
#define TIL_SWAMP_BOOTS	0x21b

/*Sling*/
#define TIL_SLING	0x220
/*Club*/
#define TIL_CLUB	0x221
/*Main Gauche*/
#define TIL_MAIN_GAUCHE	0x222
/*Spear*/
#define TIL_SPEAR	0x223
/*Throwing Axe*/
#define TIL_THROWING_AXE	0x224
/*Dagger*/
#define TIL_DAGGER	0x225
/*Mace*/
#define TIL_MACE	0x226
/*Morning Star*/
#define TIL_MORNING_STAR	0x227
/*Bow*/
#define TIL_BOW	0x228
/*Crossbow*/
#define TIL_CROSSBOW	0x229
/*Sword*/
#define TIL_SWORD	0x22a
/*2-handed Hammer*/
#define TIL_TWO_HANDED_HAMMER	0x22b

/*Halberd*/
#define TIL_HALBERD	0x22e
/*Glass Sword*/
#define TIL_GLASS_SWORD	0x22f
/*Boomerang*/
#define TIL_BOOMERANG	0x230
/*Triple Crossbow*/
#define TIL_TRIPLE_CROSSBOW	0x231
/*Force Field*/
#define TIL_FORCE_FIELD	0x232

/*Web*/
#define TIL_WEB	0x234
/*Magic Bow*/
#define TIL_MAGIC_BOW	0x235
/*Arrow*/
#define TIL_ARROW	0x236
/*Bolt*/
#define TIL_BOLT	0x237
/*Spellbook*/
#define TIL_SPELLBOOK	0x238
/*Spell*1*/
#define TIL_SPELL	0x239

/*Vortex Cube*/
#define TIL_VORTEX_CUBE	0x23d

/*moonstone #0*/
#define TIL_MOONSTONE_0	0x248
/*moonstone #1*/
#define TIL_MOONSTONE_1	0x249
/*moonstone #2*/
#define TIL_MOONSTONE_2	0x24a
/*moonstone #3*/
#define TIL_MOONSTONE_3	0x24b
/*moonstone #4*/
#define TIL_MOONSTONE_4	0x24c
/*moonstone #5*/
#define TIL_MOONSTONE_5	0x24d
/*moonstone #6*/
#define TIL_MOONSTONE_6	0x24e
/*moonstone #7*/
#define TIL_MOONSTONE_7	0x24f
/*Ankh Amulet*/
#define TIL_ANKH_AMULET	0x250
/*Snake Amulet*/
#define TIL_SNAKE_AMULET	0x251
/*Amulet of Submission*/
#define TIL_AMULET_OF_SUBMISSION	0x252
/*Gem*/
#define TIL_GEM	0x253
/*Staff*/
#define TIL_STAFF	0x254
/*Lightning Wand*/
#define TIL_LIGHTNING_WAND	0x255
/*Fire Wand*/
#define TIL_FIRE_WAND	0x256
/*Storm Cloak*/
#define TIL_STORM_CLOAK	0x257
/*Ring*/
#define TIL_RING	0x258
/*Flask of Oil*/
#define TIL_FLASK_OF_OIL	0x259
/*red gate left*/
#define TIL_RED_GATE_LEFT	0x25a
/*red gate right*/
#define TIL_RED_GATE_RIGHT	0x25b
/*moongate left*/
#define TIL_MOONGATE_LEFT	0x25c
/*moongate right*/
#define TIL_MOONGATE_RIGHT	0x25d

/*Orb of the Moons*/
#define TIL_ORB_OF_THE_MOONS	0x25f

/*Torch*/
#define TIL_TORCH	0x262
/*Torch(lit)*/
#define TIL_TORCH_LIT	0x263
#define TIL_ZU_YLEM	0x264

/*Checst(magical lock)*/
#define TIL_CHEST_MAGIC_LOCKED	0x26e

/*Scythe*/
#define TIL_SCYTHE	0x270
/*Pitchfork*/
#define TIL_PITCHFORK	0x271
/*Rake*/
#define TIL_RAKE	0x272
/*Pick*/
#define TIL_PICK	0x273
/*Shovel*/
#define TIL_SHOVEL	0x274
/*Hoe*/
#define TIL_HOE	0x275

/*Rolling Pin*/
#define TIL_ROLLING_PIN	0x279

#define TIL_CLEAVER	0x27d
#define TIL_KNIFE	0x27e
#define TIL_KNIFE_1	0x27f
/*Wine*/
#define TIL_WINE	0x280
#define TIL_MEAD	0x281

/*Tunic*/
#define TIL_TUNIC	0x28c

/*Dress*/
#define TIL_DRESS	0x28e

/*Pants*/
#define TIL_PANTS	0x29d

/*Decorative Sword #0*/
#define TIL_DECORATIVE_SWORD_0	0x2a2
/*Decorative Sword #1*/
#define TIL_DECORATIVE_SWORD_1	0x2a3

/*Crystal Ball*/
#define TIL_CRYSTAL_BALL	0x2b6

/*Lute*/
#define TIL_LUTE	0x2b9

/*Spikes*/
#define TIL_SPIKES	0x2db
/*Trap*/
#define TIL_TRAP	0x2dc

/*Brazier #2*/
#define TIL_BRAZIER_2 0x31e
/*Brazier #3*/
#define TIL_BRAZIER_3 0x31f

/*Fan*/
#define TIL_FAN 0x326

/*Rune of Honesty*/
#define TIL_RUNE_OF_HONESTY 0x368
/*Rune of Compassion*/
#define TIL_RUNE_OF_COMPASSION 0x369
/*Rune of Valor*/
#define TIL_RUNE_OF_VALOR 0x36a
/*Rune of Justice*/
#define TIL_RUNE_OF_JUSTICE 0x36b
/*Rune of Sacrifice*/
#define TIL_RUNE_OF_SACRIFICE 0x36c
/*Rune of Honor*/
#define TIL_RUNE_OF_HONOR 0x36d
/*Rune of Spirituality*/
#define TIL_RUNE_OF_SPIRITUALITY 0x36e
/*Rune of Humility*/
#define TIL_RUNE_OF_HUMILITY 0x36f

/*Campfire*/
#define TIL_CAMPFIRE 0x37a

/*Protection Ring*/
#define TIL_PROTECTION_RING 0x37d
/*Regeneration Ring*/
#define TIL_REGENERATION_RING 0x37e
/*Invisibility Ring*/
#define TIL_INVISIBILITY_RING 0x37f

/*Potion #0*/
#define TIL_POTION_0 0x3a8
/*Potion #1*/
#define TIL_POTION_1 0x3a9
/*Potion #2*/
#define TIL_POTION_2 0x3aa
/*Potion #3*/
#define TIL_POTION_3 0x3ab
/*Potion #4*/
#define TIL_POTION_4 0x3ac
/*Potion #5*/
#define TIL_POTION_5 0x3ad
/*Potion #6*/
#define TIL_POTION_6 0x3ae
/*Potion #7*/
#define TIL_POTION_7 0x3af

#define TIL_CRANK 0x3f1
#define TIL_LOG_SAW 0x3f2

#define TIL_CHAIN 0x3fc
#define TIL_LIGHTSOURCE 0x3fd
#define TIL_HEATSOURCE 0x3fe

/*Oaken Door south magic lock[left]*/
#define TIL_OAKEN_DOOR_MAGIC_S_L 0x40c
/*Oaken Door south magic lock[right]*/
#define TIL_OAKEN_DOOR_MAGIC_S_R 0x40d
/*Oaken Door east magic lock[up]*/
#define TIL_OAKEN_DOOR_MAGIC_E_U 0x40e
/*Oaken Door east magic lock[down]*/
#define TIL_OAKEN_DOOR_MAGIC_E_D 0x40f

/*Windowed Door south magic lock[left]*/
#define TIL_WINDOWED_DOOR_MAGIC_S_L 0x41c
/*Windowed Door south magic lock[right]*/
#define TIL_WINDOWED_DOOR_MAGIC_S_R 0x41d
/*Windowed Door east magic lock[up]*/
#define TIL_WINDOWED_DOOR_MAGIC_E_U 0x41e
/*Windowed Door east magic lock[down]*/
#define TIL_WINDOWED_DOOR_MAGIC_E_D 0x41f

/*Cedar Door south magic lock[left]*/
#define TIL_CEDAR_DOOR_MAGIC_S_L 0x42c
/*Cedar Door south magic lock[right]*/
#define TIL_CEDAR_DOOR_MAGIC_S_R 0x42d
/*Cedar Door east magic lock[up]*/
#define TIL_CEDAR_DOOR_MAGIC_E_U 0x42e
/*Cedar Door east magic lock[down]*/
#define TIL_CEDAR_DOOR_MAGIC_E_D 0x42f

/*Steel Door south magic lock[left]*/
#define TIL_STEEL_DOOR_MAGIC_S_L 0x43c
/*Steel Door south magic lock[right]*/
#define TIL_STEEL_DOOR_MAGIC_S_R 0x43d
/*Steel Door east magic lock[up]*/
#define TIL_STEEL_DOOR_MAGIC_E_U 0x43e
/*Steel Door east magic lock[down]*/
#define TIL_STEEL_DOOR_MAGIC_E_D 0x43f

#define TIL_COOK_FIRE_2	0x464
#define TIL_COOK_FIRE_3	0x465

#define TIL_COOK_FIRE_8	0x46a
#define TIL_COOK_FIRE_9	0x46b

#define TIL_SILVER_HORN	0x484
#define TIL_SILVER_HORN_1	0x485

/*Fire Field*/
#define TIL_FIRE_FIELD	0x48c
/*Poison Field*/
#define TIL_POISON_FIELD	0x48d
/*Protection Field*/
#define TIL_PROTECTION_FIELD	0x48e
/*Sleep Field*/
#define TIL_SLEEP_FIELD	0x48f

#define TIL_BOOKSTAND_1	0x4a9

#define TIL_SIGN_4	0x4dc
#define TIL_SIGN_5	0x4dd

/*Tangle Vine #0*/
#define TIL_TANGLE_VINE_0	0x5b2
/*Tangle Vine #1*/
#define TIL_TANGLE_VINE_1	0x5b3
/*Tangle Vine #2*/
#define TIL_TANGLE_VINE_2	0x5b4
/*Tangle Vine #3*/
#define TIL_TANGLE_VINE_3	0x5b5
/*Tangle Vine #4*/
#define TIL_TANGLE_VINE_4	0x5b6
/*Tangle Vine #5*/
#define TIL_TANGLE_VINE_5	0x5b7

/*broken lens #0*/
#define TIL_BROKEN_LENS_0	0x6d0
/*lens #0*/
#define TIL_LENS_0	0x6d1
/*broken lens #1*/
#define TIL_BROKEN_LENS_1	0x6d2
/*lens #1*/
#define TIL_LENS_1	0x6d3

#endif
