/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __OBJ_H__
#define __OBJ_H__

/*(0x049|(1<<10)) 1097 - Moonstone (cres.wax.)*/
/*(0x051|(1<<10)) 1105 - Storm Ring*4*/
/*(0x113|(1<<10)) 1299 - Red Potion*/
/*(0x049|(2<<10)) 2121 - Moonstone (1rst 1/4)*/
/*(0x050|(2<<10)) 2128 - Fire Ring*4*/
/*(0x057|(2<<10)) 2135 - Nugget of the Moons*4*/
/*(0x113|(2<<10)) 2323 - Yellow Potion*/
/*(0x049|(3<<10)) 3145 - Moonstone (gibb.wax.)*/
/*(0x04f|(3<<10)) 3151 - Lightning Ring*4*/
/*(0x113|(3<<10)) 3347 - Green Potion*/
/*(0x049|(4<<10)) 4169 - Moonstone (full moon)*/
/*(0x113|(4<<10)) 4371 - Orange Potion*/
/*(0x049|(5<<10)) 5193 - Moonstone (gibb.wan.)*/
/*(0x113|(5<<10)) 5395 - Purple Potion*/
/*(0x049|(6<<10)) 6217 - Moonstone (last 1/4)*/
/*(0x113|(6<<10)) 6419 - Black Potion*/
/*(0x049|(7<<10)) 7241 - Moonstone (cres.wan.)*/
/*(0x113|(7<<10)) 7443 - White Potion*/

/*1  - Leather Helm*/
#define OBJ_LEATHER_HELM 0x001
/*2  - Chain Coif*/
#define OBJ_CHAIN_COIF 0x002
/*3  - Iron Helm*/
#define OBJ_IRON_HELM 0x003
/*4  - Spiked Helm*/
#define OBJ_SPIKED_HELM 0x004
/*5  - Winged Helm*/
#define OBJ_WINGED_HELM 0x005
/*6  - Brass Helm*/
#define OBJ_BRASS_HELM 0x006
/*7  - Gargoyle Helm*/
#define OBJ_GARGOYLE_HELM 0x007
/*8  - Magic Helm*/
#define OBJ_MAGIC_HELM 0x008
/*9  - Wooden Shield*/
#define OBJ_WOODEN_SHIELD 0x009
/*10 - Curved Heater*/
#define OBJ_CURVED_HEATER 0x00A
/*11 - Winged Helm*/
#define OBJ_WINGED_SHIELD 0x00B
/*12 - Kite Shield*/
#define OBJ_KITE_SHIELD 0x00C
/*13 - Spiked Shield*/
#define OBJ_SPIKED_SHIELD 0x00D
/*14 - Black Shield*/
#define OBJ_BLACK_SHIELD 0x00E
/*15 - Door Shield*/
#define OBJ_DOOR_SHIELD 0x00F
/*16 - Magic Shield*/
#define OBJ_MAGIC_SHIELD 0x010
/*17 - Cloth Armor*/
#define OBJ_CLOTH_ARMOR 0x011
/*18 - Leather Armor*/
#define OBJ_LEATHER_ARMOR 0x012
/*19 - Ring Mail*/
#define OBJ_RING_MAIL 0x013
/*20 - Scale Mail*/
#define OBJ_SCALE_MAIL 0x014
/*21 - Chain Mail*/
#define OBJ_CHAIN_MAIL 0x015
/*22 - Plate Mail*/
#define OBJ_PLATE_MAIL 0x016
/*23 - Magic Armor*/
#define OBJ_MAGIC_ARMOR 0x017
/*24 - Spiked Collar*/
#define OBJ_SPIKED_COLLAR 0x018
/*25 - Guild Belt*/
#define OBJ_GUILD_BELT 0x019
/*26 - Gargoyle Belt*/
#define OBJ_GARGOYLE_BELT 0x01A
/*27 - Leather Boots*/
#define OBJ_LEATHER_BOOTS 0x01B
/*28 - Swamp Boots*/
#define OBJ_SWAMP_BOOTS 0x01C
/*29 - Earth (Dirt)*/
#define OBJ_EARTH_DIRT 0x01D
/*30 - Floor (Wood)*/
#define OBJ_FLOOR_WOOD 0x01E
/*31 - Floor (Brick)*/
#define OBJ_FLOOR_BRICK 0x01F
/*32 - Floor (Tile)*/
#define OBJ_FLOOR_TILE 0x020
/*33 - Sling*/
#define OBJ_SLING 0x021
/*34 - Club*/
#define OBJ_CLUB 0x022
/*35 - Main Gauche*/
#define OBJ_MAIN_GAUCHE 0x023
/*36 - Spear*/
#define OBJ_SPEAR 0x024
/*37 - Throwing Axe*/
#define OBJ_THROWING_AXE 0x025
/*38 - Dagger*/
#define OBJ_DAGGER 0x026
/*39 - Mace*/
#define OBJ_MACE 0x027
/*40 - Morning Star*/
#define OBJ_MORNING_STAR 0x028
/*41 - Bow*/
#define OBJ_BOW 0x029
/*42 - Crossbow*/
#define OBJ_CROSSBOW 0x02A
/*43 - Sword*/
#define OBJ_SWORD 0x02B
/*44 - 2-handed Hammer*/
#define OBJ_TWO_HANDED_HAMMER 0x02C
/*45 - 2-handed Axe*/
#define OBJ_TWO_HANDED_AXE 0x02D
/*46 - 2-handed Sword*/
#define OBJ_TWO_HANDED_SWORD 0x02E
/*47 - Halberd*/
#define OBJ_HALBERD 0x02F
/*48 - Glass Sword*/
#define OBJ_GLASS_SWORD 0x030
/*49 - Boomerang*/
#define OBJ_BOOMERANG 0x031
/*50 - Triple Crossbow*/
#define OBJ_TRIPLE_CROSSBOW 0x032
/*51 - Force Field*/
#define OBJ_FORCE_FIELD 0x033
/*52 - Wizard's Eye*/
#define OBJ_WIZARDS_EYE 0x034
/*53 - Web*/
#define OBJ_WEB 0x035
/*54 - Magic Bow*/
#define OBJ_MAGIC_BOW 0x036
/*55 - Arrow*/
#define OBJ_ARROW 0x037
/*56 - Bolt*/
#define OBJ_BOLT 0x038
/*57 - Spellbook*/
#define OBJ_SPELLBOOK 0x039
/*58 - Spell*1*/
#define OBJ_SPELL 0x03A
/*59 - Codex*/
#define OBJ_CODEX 0x03B
/*60 - Book of Prophecies*/
#define OBJ_BOOK_OF_PROPHECIES 0x03C
/*61 - Book of Circles*/
#define OBJ_BOOK_OF_CIRCLES 0x03D
/*62  - Vortex Cube*/
#define OBJ_VORTEX_CUBE 0x03E
/*63  - Lockpick*/
#define OBJ_LOCKPICK 0x03F
/*64  - Key*2*/
#define OBJ_KEY 0x040
/*65  - Black Pearl*3*/
#define OBJ_BLACK_PEARL 0x041
/*66  - Blood Moss*3*/
#define OBJ_BLOOD_MOSS 0x042
/*67  - Garlic*3*/
#define OBJ_GARLIC 0x043
/*68  - Ginseng*3*/
#define OBJ_GINSENG 0x044
/*69  - Mandrake*3*/
#define OBJ_MANDRAKE 0x045
/*70  - Nightshade*3*/
#define OBJ_NIGHTSHADE 0x046
/*71  - Spidersilk*3*/
#define OBJ_SPIDERSILK 0x047
/*72  - Ash*3*/
#define OBJ_ASH 0x048
/*73  - Moonstone (new moon)*/
#define OBJ_MOONSTONE 0x049
/*74  - Ankh Amulet*/
#define OBJ_ANKH_AMULET 0x04A
/*75  - Snake Amulet*/
#define OBJ_SNAKE_AMULET 0x04B
/*76  - Amulet of Submission*/
#define OBJ_AMULET_OF_SUBMISSION 0x04C
/*77  - Gem*/
#define OBJ_GEM 0x04D
/*78  - Staff*/
#define OBJ_STAFF 0x04E
/*79  - Lightning Wand*/
#define OBJ_LIGHTNING_WAND 0x04F
/*80  - Fire Wand*/
#define OBJ_FIRE_WAND 0x050
/*81  - Storm Cloak*/
#define OBJ_STORM_CLOAK 0x051
/*82  - Ring*/
#define OBJ_RING 0x052
/*83  - Flask of Oil*/
#define OBJ_FLASK_OF_OIL 0x053
/*84  - Red Gate*/
#define OBJ_RED_GATE 0x054
/*85  - Moongate*/
#define OBJ_MOONGATE 0x055
/*86  - Gavel*/
#define OBJ_GAVEL 0x056
/*87  - Orb of the Moons*/
#define OBJ_ORB_OF_THE_MOONS 0x057
/*88  - Gold*/
#define OBJ_GOLD 0x058
/*89  - Gold Nugget*/
#define OBJ_GOLD_NUGGET 0x059
/*90  - Torch*/
#define OBJ_TORCH 0x05A
/*91  - Zu Ylem*/
#define OBJ_ZU_YLEM 0x05B
/*92  - Silver Snake Venom*/
#define OBJ_SILVER_SNAKE_VENOM 0x05C
/*93  - Sextant*/
#define OBJ_SEXTANT 0x05D
/*94  - Spinning Wheel*/
#define OBJ_SPINNING_WHEEL 0x05E
/*95  - Bunch of Grapes*/
#define OBJ_BUNCH_OF_GRAPES 0x05F
/*96  - Butter*/
#define OBJ_BUTTER 0x060
/*97  - Gargish Vocabulary*/
#define OBJ_GARGISH_VOCABULARY 0x061
/*98  - Open Chest*/
#define OBJ_OPEN_CHEST 0x062
/*99  - Backpack*/
#define OBJ_BACKPACK 0x063
/*100 - Scythe*/
#define OBJ_SCYTHE 0x064
/*101 - Pitchfork*/
#define OBJ_PITCHFORK 0x065
/*102 - Rake*/
#define OBJ_RAKE 0x066
/*103 - Pick*/
#define OBJ_PICK 0x067
/*104 - Shovel*/
#define OBJ_SHOVEL 0x068
/*105 - Hoe*/
#define OBJ_HOE 0x069
/*106 - Wooden Ladder*/
#define OBJ_WOODEN_LADDER 0x06A
/*107 - Yoke*/
#define OBJ_YOKE 0x06B
/*108 - Oven Spatula*/
#define OBJ_OVEN_SPATULA 0x06C
/*109 - Rolling Pin*/
#define OBJ_ROLLING_PIN 0x06D
/*110 - Spatula*/
#define OBJ_SPATULA 0x06E
/*111 - Ladle*/
#define OBJ_LADLE 0x06F
/*112 - Cooking Sheet*/
#define OBJ_COOKING_SHEET 0x070
/*113 - Cleaver*/
#define OBJ_CLEAVER 0x071
/*114 - Knife*/
#define OBJ_KNIFE 0x072
/*115 - Wine*/
#define OBJ_WINE 0x073
/*116 - Mead*/
#define OBJ_MEAD 0x074
/*117 - Ale*/
#define OBJ_ALE 0x075
/*118 - Wineglass*/
#define OBJ_WINEGLASS 0x076
/*119 - Plate*/
#define OBJ_PLATE 0x077
/*120 - Mug*/
#define OBJ_MUG 0x078
/*121 - Silverware*/
#define OBJ_SILVERWARE 0x079
/*122 - Candle*/
#define OBJ_CANDLE 0x07A
/*123 - Mirror*/
#define OBJ_MIRROR 0x07B
/*124 - Tunic*/
#define OBJ_TUNIC 0x07C
/*125 - Hanger*/
#define OBJ_HANGER 0x07D
/*126 - Dress*/
#define OBJ_DRESS 0x07E
/*127 - Skillet*/
#define OBJ_SKILLET 0x07F
/*128 - Loaf of Bread*/
#define OBJ_LOAF_OF_BREAD 0x080
/*129 - Portion of Meat*/
#define OBJ_PORTION_OF_MEAT 0x081
/*130 - Rolls*/
#define OBJ_ROLLS 0x082
/*131 - Cake*/
#define OBJ_CAKE 0x083
/*132 - Cheese*/
#define OBJ_CHEESE 0x084
/*133 - Ham*/
#define OBJ_HAM 0x085
/*134 - Horse's Carcass*/
#define OBJ_HORSES_CARCASS 0x086
/*135 - Horse Chops (yum!<G>)*/
#define OBJ_HORSE_CHOPS 0x087
/*136 - Skewer*/
#define OBJ_SKEWER 0x088
/*137 - Pants*/
#define OBJ_PANTS 0x089
/*138 - Plant*/
#define OBJ_PLANT 0x08A
/*139 - Flowers*/
#define OBJ_FLOWERS 0x08B
/*140 - Wall Mount*/
#define OBJ_WALL_MOUNT 0x08C
/*141 - Decorative Sword*/
#define OBJ_DECORATIVE_SWORD 0x08D
/*142 - Decorative Shield*/
#define OBJ_DECORATIVE_SHIELD 0x08E
/*143 - Picture*/
#define OBJ_PICTURE 0x08F
/*144 - Tapestry*/
#define OBJ_TAPESTRY 0x090
/*145 - Candelabra*/
#define OBJ_CANDELABRA 0x091
/*146 - Person Sleeping*/
#define OBJ_PERSON_SLEEPING 0x092
/*147 - Cauldron*/
#define OBJ_CAULDRON 0x093
/*148 - Cauldron (filled)*/
#define OBJ_CAULDRON_FILLED 0x094
/*149 - Ship Deep*/
#define OBJ_SHIP_DEED 0x095
/*150 - Inkwell*/
#define OBJ_INKWELL 0x096
/*151 - Book*/
#define OBJ_BOOK 0x097
/*152 - Note*/
#define OBJ_NOTE 0x098
/*153 - Panpipes*/
#define OBJ_PANPIPES 0x099
/*154 - Telescope*/
#define OBJ_TELESCOPE 0x09A
/*155 - Crystal Ball*/
#define OBJ_CRYSTAL_BALL 0x09B
/*156 - Harpsichord*/
#define OBJ_HARPSICHORD 0x09C
/*157 - Harp*/
#define OBJ_HARP 0x09D
/*158 - Lute*/
#define OBJ_LUTE 0x09E
/*159 - Clock*/
#define OBJ_CLOCK 0x09F
/*160 - Endtable*/
#define OBJ_ENDTABLE 0x0A0
/*161 - Water Vase*/
#define OBJ_WATER_VASE 0x0A1
/*162 - Stove*/
#define OBJ_STOVE 0x0A2
/*163 - Bed*/
#define OBJ_BED 0x0A3
/*164 - Fireplace*/
#define OBJ_FIREPLACE 0x0A4
/*165 - Stalagmite*/
#define OBJ_STALAGMITE 0x0A5
/*166 - Sack of Grain*/
#define OBJ_SACK_OF_GRAIN 0x0A6
/*167 - Sack of Flour*/
#define OBJ_SACK_OF_FLOUR 0x0A7
/*168 - Remains*/
#define OBJ_REMAINS 0x0A8
/*169 - Rubber Ducky<G>*/
#define OBJ_RUBBER_DUCKY 0x0A9
/*170 - Urn of Ashes*/
#define OBJ_URN_OF_ASHES 0x0AA
/*171 - Fumarole*/
#define OBJ_FUMAROLE 0x0AB
/*172 - Spikes*/
#define OBJ_SPIKES 0x0AC
/*173 - Trap*/
#define OBJ_TRAP 0x0AD
/*174 - Switch*/
#define OBJ_SWITCH 0x0AE
/*175 - Electric Field*/
#define OBJ_ELECTRIC_FIELD 0x0AF
/*176 - Chest of Drawers*/
#define OBJ_CHEST_OF_DRAWERS 0x0B0
/*177 - Desk*/
#define OBJ_DESK 0x0B1
/*178 - Bucket*/
#define OBJ_BUCKET 0x0B2
/*179 - Bucket of Water*/
#define OBJ_BUCKET_OF_WATER 0x0B3
/*180 - Bucket of Milk*/
#define OBJ_BUCKET_OF_MILK 0x0B4
/*181 - Churn*/
#define OBJ_CHURN 0x0B5
/*182 - Beehive*/
#define OBJ_BEEHIVE 0x0B6
/*183 - Honey Jar*/
#define OBJ_HONEY_JAR 0x0B7
/*184 - Jar of Honey*/
#define OBJ_JAR_OF_HONEY 0x0B8
/*185 - Cloth*/
#define OBJ_CLOTH 0x0B9
/*186 - Open Barrel*/
#define OBJ_OPEN_BARREL 0x0BA
/*187 - Jug*/
#define OBJ_JUG 0x0BB
/*188 - Bag*/
#define OBJ_BAG 0x0BC
/*189 - Cask*/
#define OBJ_CASK 0x0BD
/*190 - Bale of Wool*/
#define OBJ_BALE_OF_WOOL 0x0BE
/*191 - Basket*/
#define OBJ_BASKET 0x0BF
/*192 - Open Crate*/
#define OBJ_OPEN_CRATE 0x0C0
/*193 - Small Jug*/
#define OBJ_SMALL_JUG 0x0C1
/*194 - Milk Bottle*/
#define OBJ_MILK_BOTTLE 0x0C2
/*195 - Wheat*/
#define OBJ_WHEAT 0x0C3
/*196 - Vat*/
#define OBJ_VAT 0x0C4
/*197 - Wine Cask*/
#define OBJ_WINE_CASK 0x0C5
/*198 - Cutting Table*/
#define OBJ_CUTTING_TABLE 0x0C6
/*199 - Loom*/
#define OBJ_LOOM 0x0C7
/*200 - Hood (for forge)*/
#define OBJ_HOOD_FOR_FORGE 0x0C8
/*201 - Fire (from forge)*/
#define OBJ_FORGE_FIRE 0x0C9
/*202 - Horseshoes*/
#define OBJ_HORSESHOES 0x0CA
/*203 - Pliers*/
#define OBJ_PLIERS 0x0CB
/*204 - Hammer*/
#define OBJ_HAMMER 0x0CC
/*205 - Water Trough*/
#define OBJ_WATER_TROUGH 0x0CD
/*206 - Brazier*/
#define OBJ_BRAZIER 0x0CE
/*207 - Rod*/
#define OBJ_ROD 0x0CF
/*208 - Hook*/
#define OBJ_HOOK 0x0D0
/*209 - Meat (Rib type)*/
#define OBJ_MEAT_RIB 0x0D1
/*210 - Ribs*/
#define OBJ_RIBS 0x0D2
/*211 - Dead Animal*/
#define OBJ_DEAD_ANIMAL 0x0D3
/*212 - Fan*/
#define OBJ_FAN 0x0D4
/*213 - Mouse Hole*/
#define OBJ_MOUSE_HOLE 0x0D5
/*214 - Wine Press*/
#define OBJ_WINE_PRESS 0x0D6
/*215 - Stable*/
#define OBJ_STABLE 0x0D7
/*216 - Bookshelf*/
#define OBJ_BOOKSHELF 0x0D8
/*217 - Anvil*/
#define OBJ_ANVIL 0x0D9
/*218 - Bellows*/
#define OBJ_BELLOWS 0x0DA
/*219 - Oven*/
#define OBJ_OVEN 0x0DB
/*220 - Flag*/
#define OBJ_FLAG 0x0DC
/*221 - Cannon*/
#define OBJ_CANNON 0x0DD
/*222 - Cannon Balls*/
#define OBJ_CANNON_BALLS 0x0DE
/*223 - Powder Keg*/
#define OBJ_POWDER_KEG 0x0DF
/*224 - Foot Rail*/
#define OBJ_FOOT_RAIL 0x0E0
/*225 - Spool of Thread*/
#define OBJ_SPOOL_OF_THREAD 0x0E1
/*226 - Spool of Silk*/
#define OBJ_SPOOL_OF_SILK 0x0E2
/*227 - Pennant*/
#define OBJ_PENNANT 0x0E3
/*228 - Table (square corner)*/
#define OBJ_TABLE_SQUARE_CORNER_0E4 0x0E4
/*229 - Shadow*/
#define OBJ_SHADOW_0E5 0x0E5
/*230 - Table (round corner)*/
#define OBJ_TABLE_ROUND_CORNER_0E6 0x0E6
/*231 - Shadow*/
#define OBJ_SHADOW_0E7 0x0E7
/*232 - Spittoon*/
#define OBJ_SPITTOON 0x0E8
/*233 - Well*/
#define OBJ_WELL 0x0E9
/*234 - Fountain*/
#define OBJ_FOUNTAIN 0x0EA
/*235 - Sundial*/
#define OBJ_SUNDIAL 0x0EB
/*236 - Bell*/
#define OBJ_BELL 0x0EC
/*237 - Table (middle)*/
#define OBJ_TABLE_MIDDLE 0x0ED
/*238 - Shadow*/
#define OBJ_SHADOW_0EE 0x0EE
/*239 - Table (round corner)*/
#define OBJ_TABLE_ROUND_CORNER_0EF 0x0EF
/*240 - Shadow*/
#define OBJ_SHADOW_0F0 0x0F0
/*241 - Silk Cloth*/
#define OBJ_SILK_CLOTH 0x0F1
/*242 - Rune of Honesty*/
#define OBJ_RUNE_OF_HONESTY 0x0F2
/*243 - Rune of Compassion*/
#define OBJ_RUNE_OF_COMPASSION 0x0F3
/*244 - Rune of Valor*/
#define OBJ_RUNE_OF_VALOR 0x0F4
/*245 - Rune of Justice*/
#define OBJ_RUNE_OF_JUSTICE 0x0F5
/*246 - Rune of Sacrifice*/
#define OBJ_RUNE_OF_SACRIFICE 0x0F6
/*247 - Rune of Honor*/
#define OBJ_RUNE_OF_HONOR 0x0F7
/*248 - Rune of Spirituality*/
#define OBJ_RUNE_OF_SPIRITUALITY 0x0F8
/*249 - Rune of Humility*/
#define OBJ_RUNE_OF_HUMILITY 0x0F9
/*250 - Table (square corner)*/
#define OBJ_TABLE_SQUARE_CORNER_0FA 0x0FA
/*251 - Shadow*/
#define OBJ_SHADOW_0FB 0x0FB
/*252 - Chair*/
#define OBJ_CHAIR 0x0FC
/*253 - Campfire*/
#define OBJ_CAMPFIRE 0x0FD
/*254 - Cross*/
#define OBJ_CROSS 0x0FE
/*255 - Tombstone*/
#define OBJ_TOMBSTONE 0x0FF
/*256 - Protection Ring*/
#define OBJ_PROTECTION_RING 0x100
/*257 - Regeneration Ring*/
#define OBJ_REGENERATION_RING 0x101
/*258 - Invisibility Ring*/
#define OBJ_INVISIBILITY_RING 0x102
/*259 - Table Leg*/
#define OBJ_TABLE_LEG_103 0x103
/*260 - Shadow*/
#define OBJ_SHADOW_104 0x104
/*261 - Table Leg*/
#define OBJ_TABLE_LEG_105 0x105
/*262 - Shadow*/
#define OBJ_SHADOW_106 0x106
/*263 - Stocks*/
#define OBJ_STOCKS 0x107
/*264 - Fishing Pole*/
#define OBJ_FISHING_POLE 0x108
/*265 - Fish*/
#define OBJ_FISH 0x109
/*266 - Grave*/
#define OBJ_GRAVE 0x10A
/*267 - Guillotine*/
#define OBJ_GUILLOTINE 0x10B
/*268 - Lever*/
#define OBJ_LEVER 0x10C
/*269 - Drawbridge*/
#define OBJ_DRAWBRIDGE 0x10D
/*270 - Balloon Plans*/
#define OBJ_BALLOON_PLANS 0x10E
/*271 - Doorsill*/
#define OBJ_DOORSILL 0x10F
/*272 - Steps*/
#define OBJ_STEPS_110 0x110
/*273 - Tile*/
#define OBJ_TILE 0x111
/*274 - Yew Log*/
#define OBJ_YEW_LOG 0x112
/*275 - [Blue] Potion*/
#define OBJ_POTION 0x113
/*276 - Steps*/
#define OBJ_STEPS_114 0x114
/*277 - Yew Board*/
#define OBJ_YEW_BOARD 0x115
/*278 - Passthrough*/
#define OBJ_PASSTHROUGH_116 0x116
/*279 - Table*/
#define OBJ_TABLE 0x117
/*280 - Passthrough*/
#define OBJ_PASSTHROUGH_118 0x118
/*281 - Fence*/
#define OBJ_FENCE 0x119
/*282 - Bars*/
#define OBJ_BARS 0x11A
/*283 - Anchor*/
#define OBJ_ANCHOR 0x11B
/*284 - Rope*/
#define OBJ_ROPE 0x11C
/*285 - Pole*/
#define OBJ_POLE 0x11D
/*286 - Walkway*/
#define OBJ_WALKWAY 0x11E
/*287 - Water Wheel*/
#define OBJ_WATER_WHEEL 0x11F
/*288 - Crank*/
#define OBJ_CRANK 0x120
/*289 - Log Saw*/
#define OBJ_LOG_SAW 0x121
/*290 - Millstone*/
#define OBJ_MILLSTONE 0x122
/*291 - Shaft*/
#define OBJ_SHAFT 0x123
/*292 - Gearwork*/
#define OBJ_GEARWORK 0x124
/*293 - Chain*/
#define OBJ_CHAIN 0x125
/*294 - Lightsource*/
#define OBJ_LIGHTSOURCE 0x126
/*295 - Heatsource*/
#define OBJ_HEATSOURCE 0x127
/*296 - Xylophone*/
#define OBJ_XYLOPHONE 0x128
/*297 - Oaken Door*/
#define OBJ_OAKEN_DOOR 0x129
/*298 - Windowed Door*/
#define OBJ_WINDOWED_DOOR 0x12A
/*299 - Cedar Door*/
#define OBJ_CEDAR_DOOR 0x12B
/*300 - Steel Door*/
#define OBJ_STEEL_DOOR 0x12C
/*301 - Doorway*/
#define OBJ_DOORWAY 0x12D
/*302 - Archway*/
#define OBJ_ARCHWAY 0x12E
/*303 - Carpet*/
#define OBJ_CARPET 0x12F
/*304 - Cook Fire*/
#define OBJ_COOK_FIRE 0x130
/*305 - Ladder*/
#define OBJ_LADDER 0x131
/*306 - Trellis*/
#define OBJ_TRELLIS 0x132
/*307 - Volcano*/
#define OBJ_VOLCANO 0x133
/*308 - Hole*/
#define OBJ_HOLE 0x134
/*309 - Bones (archway)*/
#define OBJ_BONES_ARCHWAY 0x135
/*310 - Portcullis*/
#define OBJ_PORTCULLIS 0x136
/*311 - Stone Table*/
#define OBJ_STONE_TABLE 0x137
/*312 - Stone Lion*/
#define OBJ_STONE_LION 0x138
/*313 - Silver Horn*/
#define OBJ_SILVER_HORN 0x139
/*314 - Floor (stone)*/
#define OBJ_FLOOR_STONE 0x13A
/*315 - Stone*/
#define OBJ_STONE 0x13B
/*316 - Lamppost*/
#define OBJ_LAMPPOST 0x13C
/*317 - Fire Field*/
#define OBJ_FIRE_FIELD 0x13D
/*318 - Poison Field*/
#define OBJ_POISON_FIELD 0x13E
/*319 - Protection Field*/
#define OBJ_PROTECTION_FIELD 0x13F
/*320 - Sleep Field*/
#define OBJ_SLEEP_FIELD 0x140
/*321 - Statue*/
#define OBJ_STATUE 0x141
/*322 - Pool*/
#define OBJ_POOL 0x142
/*323 - Monolith*/
#define OBJ_MONOLITH 0x143
/*324 - Pillar*/
#define OBJ_PILLAR 0x144
/*325 - Bookstand*/
#define OBJ_BOOKSTAND 0x145
/*326 - Mine Shaft*/
#define OBJ_MINE_SHAFT 0x146
/*327 - Throne*/
#define OBJ_THRONE 0x147
/*328 - Altar*/
#define OBJ_ALTAR 0x148
/*329 - Altar of Spirituality*/
#define OBJ_ALTAR_OF_SPIRITUALITY 0x149
/*330 - Mat*/
#define OBJ_MAT 0x14A
/*331 - Government Sign*/
#define OBJ_GOVERNMENT_SIGN 0x14B
/*332 - Sign*/
#define OBJ_SIGN 0x14C
/*333 - Gargoyle Sign*/
#define OBJ_GARGOYLE_SIGN 0x14D
/*334 - Secret Door*/
#define OBJ_SECRET_DOOR 0x14E
/*335 - Egg*/
#define OBJ_EGG 0x14F
/*336 - Charge*/
#define OBJ_CHARGE 0x150
/*337 - Effect*/
#define OBJ_EFFECT 0x151
/*338 - Blood*/
#define OBJ_BLOOD 0x152
/*339 - Dead Body*/
#define OBJ_DEAD_BODY 0x153
/*340 - Dead Cyclops*/
#define OBJ_DEAD_CYCLOPS 0x154
/*341 - Dead Gargoyle*/
#define OBJ_DEAD_GARGOYLE 0x155
/*342 - Giant Rat*/
#define OBJ_GIANT_RAT 0x156
/*343 - Insects*/
#define OBJ_INSECTS 0x157
/*344 - Giant Bat*/
#define OBJ_GIANT_BAT 0x158
/*345 - Giant Squid*/
#define OBJ_GIANT_SQUID 0x159
/*346 - Sea Serpent*/
#define OBJ_SEA_SERPENT 0x15A
/*347 - Reaper*/
#define OBJ_REAPER 0x15B
/*348 - Sheep*/
#define OBJ_SHEEP 0x15C
/*349 - Dog*/
#define OBJ_DOG 0x15D
/*350 - Deer*/
#define OBJ_DEER 0x15E
/*351 - Wolf*/
#define OBJ_WOLF 0x15F
/*352 - Ghost*/
#define OBJ_GHOST 0x160
/*353 - Gremlin*/
#define OBJ_GREMLIN 0x161
/*354 - Mouse*/
#define OBJ_MOUSE 0x162
/*355 - Gazer*/
#define OBJ_GAZER 0x163
/*356 - Bird*/
#define OBJ_BIRD 0x164
/*357 - Corpser*/
#define OBJ_CORPSER 0x165
/*358 - Snake*/
#define OBJ_SNAKE 0x166
/*359 - Rabbit*/
#define OBJ_RABBIT 0x167
/*360 - Rot Worms*/
#define OBJ_ROT_WORMS 0x168
/*361 - Giant Spider*/
#define OBJ_GIANT_SPIDER 0x169
/*362 - Winged Gargoyle*/
#define OBJ_WINGED_GARGOYLE 0x16A
/*363 - Gargoyle*/
#define OBJ_GARGOYLE 0x16B
/*364 - Acid Slug*/
#define OBJ_ACID_SLUG 0x16C
/*365 - Tangle Vine (pod)*/
#define OBJ_TANGLE_VINE_POD 0x16D
/*366 - Tangle Vine (vine)*/
#define OBJ_TANGLE_VINE_VINE 0x16E
/*367 - Daemon*/
#define OBJ_DAEMON 0x16F
/*368 - Skeleton*/
#define OBJ_SKELETON 0x170
/*369 - Drake*/
#define OBJ_DRAKE 0x171
/*370 - Headless*/
#define OBJ_HEADLESS 0x172
/*371 - Troll*/
#define OBJ_TROLL 0x173
/*372 - Mongbat*/
#define OBJ_MONGBAT 0x174
/*373 - Wisp*/
#define OBJ_WISP 0x175
/*374 - Hydra*/
#define OBJ_HYDRA 0x176
/*375 - Slime*/
#define OBJ_SLIME 0x177
/*376 - Fighter*/
#define OBJ_FIGHTER 0x178
/*377 - Swashbuckler*/
#define OBJ_SWASHBUCKLER 0x179
/*378 - Mage*/
#define OBJ_MAGE 0x17A
/*379 - Villager*/
#define OBJ_VILLAGER 0x17B
/*380 - Merchant*/
#define OBJ_MERCHANT 0x17C
/*381 - Child*/
#define OBJ_CHILD 0x17D
/*382 - Guard*/
#define OBJ_GUARD 0x17E
/*383 - Jester*/
#define OBJ_JESTER 0x17F
/*384 - Peasant*/
#define OBJ_PEASANT 0x180
/*385 - Farmer*/
#define OBJ_FARMER 0x181
/*386 - Musician (long cape)*/
#define OBJ_MUSICIAN_LONG_CAPE 0x182
/*387 - Woman*/
#define OBJ_WOMAN 0x183
/*388 - Cat*/
#define OBJ_CAT 0x184
/*389 - Silver Tablet*/
#define OBJ_SILVER_TABLET 0x185
/*390 - Silver Fragment*/
#define OBJ_SILVER_FRAGMENT 0x186
/*391 - Farmer*/
#define OBJ_FARMER_2 0x187
/*392 - Musician (short cape)*/
#define OBJ_MUSICIAN_SHORT_CAPE 0x188
/*393 - Shrine*/
#define OBJ_SHRINE 0x189
/*394 - Britannia Lens (blue)*/
#define OBJ_BRITANNIA_LENS_BLUE 0x18A
/*395 - Broken Lens (violet)*/
#define OBJ_BROKEN_LENS_VIOLET 0x18B
/*396 - Gargoyle Lens (violet)*/
#define OBJ_GARGOYLE_LENS_VIOLET 0x18C
/*397 - Statue of Mondain*/
#define OBJ_STATUE_OF_MONDAIN 0x18D
/*398 - Statue of Minax*/
#define OBJ_STATUE_OF_MINAX 0x18E
/*399 - Statue of Exodus*/
#define OBJ_STATUE_OF_EXODUS 0x18F
/*400 - Map Part one*/
#define OBJ_MAP_PART_ONE 0x190
/*401 - Map Part two*/
#define OBJ_MAP_PART_TWO 0x191
/*402 - Map Part three*/
#define OBJ_MAP_PART_THREE 0x192
/*403 - Map Part four*/
#define OBJ_MAP_PART_FOUR 0x193
/*404 - Map Part five*/
#define OBJ_MAP_PART_FIVE 0x194
/*405 - Map Part six*/
#define OBJ_MAP_PART_SIX 0x195
/*406 - Map Part seven*/
#define OBJ_MAP_PART_SEVEN 0x196
/*407 - Map Part eight*/
#define OBJ_MAP_PART_EIGHT 0x197
/*408  - Map Part nine*/
#define OBJ_MAP_PART_NINE 0x198
/*409  - Lord British*/
#define OBJ_LORD_BRITISH 0x199
/*410  - Avatar*/
#define OBJ_AVATAR 0x19A
/*411  - Dragon*/
#define OBJ_DRAGON 0x19B
/*412  - Ship*/
#define OBJ_SHIP 0x19C
/*413  - Silver Serpent*/
#define OBJ_SILVER_SERPENT 0x19D
/*414  - Skiff*/
#define OBJ_SKIFF 0x19E
/*415  - Raft*/
#define OBJ_RAFT 0x19F
/*416  - Nothing (REALLY!) - guardians of the codex?*/
#define OBJ_NOTHING 0x1A0
/*417  - Dragon Egg*/
#define OBJ_DRAGON_EGG 0x1A1
/*418  - Hatched Dragon Egg*/
#define OBJ_HATCHED_DRAGON_EGG 0x1A2
/*419  - Pull Chain*/
#define OBJ_PULL_CHAIN 0x1A3
/*420  - Balloon (deflated)*/
#define OBJ_BALLOON_DEFLATED 0x1A4
/*421  - Mammoth Silk Bag*/
#define OBJ_MAMMOTH_SILK_BAG 0x1A5
/*422  - Balloon Basket*/
#define OBJ_BALLOON_BASKET 0x1A6
/*423  - Balloon (inflated)*/
#define OBJ_BALLOON_INFLATED 0x1A7
/*424  - Cyclops*/
#define OBJ_CYCLOPS 0x1A8
/*425  - Hydra*/
#define OBJ_HYDRA_BODY 0x1A9
/*426  - Giant Scorpion*/
#define OBJ_GIANT_SCORPION 0x1AA
/*427  - Giant Ant*/
#define OBJ_GIANT_ANT 0x1AB
/*428  - Cow*/
#define OBJ_COW 0x1AC
/*429  - Alligator*/
#define OBJ_ALLIGATOR 0x1AD
/*430  - Horse*/
#define OBJ_HORSE 0x1AE
/*431 - (mounted horse)?*/
#define OBJ_MOUNTED_HORSE 0x1AF

/*
#define OBJ_1B0 0x1B0
#define OBJ_1B1 0x1B1
#define OBJ_1B2 0x1B2
#define OBJ_1B3 0x1B3
#define OBJ_1B4 0x1B4
#define OBJ_1B5 0x1B5
#define OBJ_1B6 0x1B6
#define OBJ_1B7 0x1B7
#define OBJ_1B8 0x1B8
#define OBJ_1B9 0x1B9
#define OBJ_1BA 0x1BA
#define OBJ_1BB 0x1BB
#define OBJ_1BC 0x1BC
#define OBJ_1BD 0x1BD
#define OBJ_1BE 0x1BE
#define OBJ_1BF 0x1BF
#define OBJ_1C0 0x1C0
#define OBJ_1C1 0x1C1
#define OBJ_1C2 0x1C2
#define OBJ_1C3 0x1C3
#define OBJ_1C4 0x1C4
#define OBJ_1C5 0x1C5
#define OBJ_1C6 0x1C6
#define OBJ_1C7 0x1C7
#define OBJ_1C8 0x1C8
#define OBJ_1C9 0x1C9
#define OBJ_1CA 0x1CA
#define OBJ_1CB 0x1CB
#define OBJ_1CC 0x1CC
#define OBJ_1CD 0x1CD
#define OBJ_1CE 0x1CE
#define OBJ_1CF 0x1CF
#define OBJ_1D0 0x1D0
#define OBJ_1D1 0x1D1
#define OBJ_1D2 0x1D2
#define OBJ_1D3 0x1D3
#define OBJ_1D4 0x1D4
#define OBJ_1D5 0x1D5
#define OBJ_1D6 0x1D6
#define OBJ_1D7 0x1D7
#define OBJ_1D8 0x1D8
#define OBJ_1D9 0x1D9
#define OBJ_1DA 0x1DA
#define OBJ_1DB 0x1DB
#define OBJ_1DC 0x1DC
#define OBJ_1DD 0x1DD
#define OBJ_1DE 0x1DE
#define OBJ_1DF 0x1DF
#define OBJ_1E0 0x1E0
#define OBJ_1E1 0x1E1
#define OBJ_1E2 0x1E2
#define OBJ_1E3 0x1E3
#define OBJ_1E4 0x1E4
#define OBJ_1E5 0x1E5
#define OBJ_1E6 0x1E6
#define OBJ_1E7 0x1E7
#define OBJ_1E8 0x1E8
#define OBJ_1E9 0x1E9
#define OBJ_1EA 0x1EA
#define OBJ_1EB 0x1EB
#define OBJ_1EC 0x1EC
#define OBJ_1ED 0x1ED
#define OBJ_1EE 0x1EE
#define OBJ_1EF 0x1EF
#define OBJ_1F0 0x1F0
#define OBJ_1F1 0x1F1
#define OBJ_1F2 0x1F2
#define OBJ_1F3 0x1F3
#define OBJ_1F4 0x1F4
#define OBJ_1F5 0x1F5
#define OBJ_1F6 0x1F6
#define OBJ_1F7 0x1F7
#define OBJ_1F8 0x1F8
#define OBJ_1F9 0x1F9
#define OBJ_1FA 0x1FA
#define OBJ_1FB 0x1FB
#define OBJ_1FC 0x1FC
#define OBJ_1FD 0x1FD
#define OBJ_1FE 0x1FE
#define OBJ_1FF 0x1FF
#define OBJ_201 0x201
#define OBJ_202 0x202
#define OBJ_203 0x203
#define OBJ_204 0x204
#define OBJ_205 0x205
#define OBJ_206 0x206
#define OBJ_207 0x207
#define OBJ_208 0x208
#define OBJ_209 0x209
#define OBJ_20A 0x20A
#define OBJ_20B 0x20B
#define OBJ_20C 0x20C
#define OBJ_20D 0x20D
#define OBJ_20E 0x20E
#define OBJ_20F 0x20F
#define OBJ_210 0x210
#define OBJ_211 0x211
#define OBJ_212 0x212
#define OBJ_213 0x213
#define OBJ_214 0x214
#define OBJ_215 0x215
#define OBJ_216 0x216
#define OBJ_217 0x217
#define OBJ_218 0x218
#define OBJ_219 0x219
#define OBJ_21A 0x21A
#define OBJ_21B 0x21B
#define OBJ_21C 0x21C
#define OBJ_21D 0x21D
#define OBJ_21E 0x21E
#define OBJ_21F 0x21F
#define OBJ_220 0x220
#define OBJ_221 0x221
#define OBJ_222 0x222
#define OBJ_223 0x223
#define OBJ_224 0x224
#define OBJ_225 0x225
#define OBJ_226 0x226
#define OBJ_227 0x227
#define OBJ_228 0x228
#define OBJ_229 0x229
#define OBJ_22A 0x22A
#define OBJ_22B 0x22B
#define OBJ_22C 0x22C
#define OBJ_22D 0x22D
#define OBJ_22E 0x22E
#define OBJ_22F 0x22F
#define OBJ_230 0x230
#define OBJ_231 0x231
#define OBJ_232 0x232
#define OBJ_233 0x233
#define OBJ_234 0x234
#define OBJ_235 0x235
#define OBJ_236 0x236
#define OBJ_237 0x237
#define OBJ_238 0x238
#define OBJ_239 0x239
#define OBJ_23A 0x23A
#define OBJ_23B 0x23B
#define OBJ_23C 0x23C
#define OBJ_23D 0x23D
#define OBJ_23E 0x23E
#define OBJ_23F 0x23F
#define OBJ_240 0x240
#define OBJ_241 0x241
#define OBJ_242 0x242
#define OBJ_243 0x243
#define OBJ_244 0x244
#define OBJ_245 0x245
#define OBJ_246 0x246
#define OBJ_247 0x247
#define OBJ_248 0x248
#define OBJ_249 0x249
#define OBJ_24A 0x24A
#define OBJ_24B 0x24B
#define OBJ_24C 0x24C
#define OBJ_24D 0x24D
#define OBJ_24E 0x24E
#define OBJ_24F 0x24F
#define OBJ_250 0x250
#define OBJ_251 0x251
#define OBJ_252 0x252
#define OBJ_253 0x253
#define OBJ_254 0x254
#define OBJ_255 0x255
#define OBJ_256 0x256
#define OBJ_257 0x257
#define OBJ_258 0x258
#define OBJ_259 0x259
#define OBJ_25A 0x25A
#define OBJ_25B 0x25B
#define OBJ_25C 0x25C
#define OBJ_25D 0x25D
#define OBJ_25E 0x25E
#define OBJ_25F 0x25F
#define OBJ_260 0x260
#define OBJ_261 0x261
#define OBJ_262 0x262
#define OBJ_263 0x263
#define OBJ_264 0x264
#define OBJ_265 0x265
#define OBJ_266 0x266
#define OBJ_267 0x267
#define OBJ_268 0x268
#define OBJ_269 0x269
#define OBJ_26A 0x26A
#define OBJ_26B 0x26B
#define OBJ_26C 0x26C
#define OBJ_26D 0x26D
#define OBJ_26E 0x26E
#define OBJ_26F 0x26F
#define OBJ_270 0x270
#define OBJ_271 0x271
#define OBJ_272 0x272
#define OBJ_273 0x273
#define OBJ_274 0x274
#define OBJ_275 0x275
#define OBJ_276 0x276
#define OBJ_277 0x277
#define OBJ_278 0x278
#define OBJ_279 0x279
#define OBJ_27A 0x27A
#define OBJ_27B 0x27B
#define OBJ_27C 0x27C
#define OBJ_27D 0x27D
#define OBJ_27E 0x27E
#define OBJ_27F 0x27F
#define OBJ_280 0x280
#define OBJ_281 0x281
#define OBJ_282 0x282
#define OBJ_283 0x283
#define OBJ_284 0x284
#define OBJ_285 0x285
#define OBJ_286 0x286
#define OBJ_287 0x287
#define OBJ_288 0x288
#define OBJ_289 0x289
#define OBJ_28A 0x28A
#define OBJ_28B 0x28B
#define OBJ_28C 0x28C
#define OBJ_28D 0x28D
#define OBJ_28E 0x28E
#define OBJ_28F 0x28F
#define OBJ_290 0x290
#define OBJ_291 0x291
#define OBJ_292 0x292
#define OBJ_293 0x293
#define OBJ_294 0x294
#define OBJ_295 0x295
#define OBJ_296 0x296
#define OBJ_297 0x297
#define OBJ_298 0x298
#define OBJ_299 0x299
#define OBJ_29A 0x29A
#define OBJ_29B 0x29B
#define OBJ_29C 0x29C
#define OBJ_29D 0x29D
#define OBJ_29E 0x29E
#define OBJ_29F 0x29F
#define OBJ_2A0 0x2A0
#define OBJ_2A1 0x2A1
#define OBJ_2A2 0x2A2
#define OBJ_2A3 0x2A3
#define OBJ_2A4 0x2A4
#define OBJ_2A5 0x2A5
#define OBJ_2A6 0x2A6
#define OBJ_2A7 0x2A7
#define OBJ_2A8 0x2A8
#define OBJ_2A9 0x2A9
#define OBJ_2AA 0x2AA
#define OBJ_2AB 0x2AB
#define OBJ_2AC 0x2AC
#define OBJ_2AD 0x2AD
#define OBJ_2AE 0x2AE
#define OBJ_2AF 0x2AF
#define OBJ_2B0 0x2B0
#define OBJ_2B1 0x2B1
#define OBJ_2B2 0x2B2
#define OBJ_2B3 0x2B3
#define OBJ_2B4 0x2B4
#define OBJ_2B5 0x2B5
#define OBJ_2B6 0x2B6
#define OBJ_2B7 0x2B7
#define OBJ_2B8 0x2B8
#define OBJ_2B9 0x2B9
#define OBJ_2BA 0x2BA
#define OBJ_2BB 0x2BB
#define OBJ_2BC 0x2BC
#define OBJ_2BD 0x2BD
#define OBJ_2BE 0x2BE
#define OBJ_2BF 0x2BF
#define OBJ_2C0 0x2C0
#define OBJ_2C1 0x2C1
#define OBJ_2C2 0x2C2
#define OBJ_2C3 0x2C3
#define OBJ_2C4 0x2C4
#define OBJ_2C5 0x2C5
#define OBJ_2C6 0x2C6
#define OBJ_2C7 0x2C7
#define OBJ_2C8 0x2C8
#define OBJ_2C9 0x2C9
#define OBJ_2CA 0x2CA
#define OBJ_2CB 0x2CB
#define OBJ_2CC 0x2CC
#define OBJ_2CD 0x2CD
#define OBJ_2CE 0x2CE
#define OBJ_2CF 0x2CF
#define OBJ_2D0 0x2D0
#define OBJ_2D1 0x2D1
#define OBJ_2D2 0x2D2
#define OBJ_2D3 0x2D3
#define OBJ_2D4 0x2D4
#define OBJ_2D5 0x2D5
#define OBJ_2D6 0x2D6
#define OBJ_2D7 0x2D7
#define OBJ_2D8 0x2D8
#define OBJ_2D9 0x2D9
#define OBJ_2DA 0x2DA
#define OBJ_2DB 0x2DB
#define OBJ_2DC 0x2DC
#define OBJ_2DD 0x2DD
#define OBJ_2DE 0x2DE
#define OBJ_2DF 0x2DF
#define OBJ_2E0 0x2E0
#define OBJ_2E1 0x2E1
#define OBJ_2E2 0x2E2
#define OBJ_2E3 0x2E3
#define OBJ_2E4 0x2E4
#define OBJ_2E5 0x2E5
#define OBJ_2E6 0x2E6
#define OBJ_2E7 0x2E7
#define OBJ_2E8 0x2E8
#define OBJ_2E9 0x2E9
#define OBJ_2EA 0x2EA
#define OBJ_2EB 0x2EB
#define OBJ_2EC 0x2EC
#define OBJ_2ED 0x2ED
#define OBJ_2EE 0x2EE
#define OBJ_2EF 0x2EF
#define OBJ_2F0 0x2F0
#define OBJ_2F1 0x2F1
#define OBJ_2F2 0x2F2
#define OBJ_2F3 0x2F3
#define OBJ_2F4 0x2F4
#define OBJ_2F5 0x2F5
#define OBJ_2F6 0x2F6
#define OBJ_2F7 0x2F7
#define OBJ_2F8 0x2F8
#define OBJ_2F9 0x2F9
#define OBJ_2FA 0x2FA
#define OBJ_2FB 0x2FB
#define OBJ_2FC 0x2FC
#define OBJ_2FD 0x2FD
#define OBJ_2FE 0x2FE
#define OBJ_2FF 0x2FF
#define OBJ_301 0x301
#define OBJ_302 0x302
#define OBJ_303 0x303
#define OBJ_304 0x304
#define OBJ_305 0x305
#define OBJ_306 0x306
#define OBJ_307 0x307
#define OBJ_308 0x308
#define OBJ_309 0x309
#define OBJ_30A 0x30A
#define OBJ_30B 0x30B
#define OBJ_30C 0x30C
#define OBJ_30D 0x30D
#define OBJ_30E 0x30E
#define OBJ_30F 0x30F
#define OBJ_310 0x310
#define OBJ_311 0x311
#define OBJ_312 0x312
#define OBJ_313 0x313
#define OBJ_314 0x314
#define OBJ_315 0x315
#define OBJ_316 0x316
#define OBJ_317 0x317
#define OBJ_318 0x318
#define OBJ_319 0x319
#define OBJ_31A 0x31A
#define OBJ_31B 0x31B
#define OBJ_31C 0x31C
#define OBJ_31D 0x31D
#define OBJ_31E 0x31E
#define OBJ_31F 0x31F
#define OBJ_320 0x320
#define OBJ_321 0x321
#define OBJ_322 0x322
#define OBJ_323 0x323
#define OBJ_324 0x324
#define OBJ_325 0x325
#define OBJ_326 0x326
#define OBJ_327 0x327
#define OBJ_328 0x328
#define OBJ_329 0x329
#define OBJ_32A 0x32A
#define OBJ_32B 0x32B
#define OBJ_32C 0x32C
#define OBJ_32D 0x32D
#define OBJ_32E 0x32E
#define OBJ_32F 0x32F
#define OBJ_330 0x330
#define OBJ_331 0x331
#define OBJ_332 0x332
#define OBJ_333 0x333
#define OBJ_334 0x334
#define OBJ_335 0x335
#define OBJ_336 0x336
#define OBJ_337 0x337
#define OBJ_338 0x338
#define OBJ_339 0x339
#define OBJ_33A 0x33A
#define OBJ_33B 0x33B
#define OBJ_33C 0x33C
#define OBJ_33D 0x33D
#define OBJ_33E 0x33E
#define OBJ_33F 0x33F
#define OBJ_340 0x340
#define OBJ_341 0x341
#define OBJ_342 0x342
#define OBJ_343 0x343
#define OBJ_344 0x344
#define OBJ_345 0x345
#define OBJ_346 0x346
#define OBJ_347 0x347
#define OBJ_348 0x348
#define OBJ_349 0x349
#define OBJ_34A 0x34A
#define OBJ_34B 0x34B
#define OBJ_34C 0x34C
#define OBJ_34D 0x34D
#define OBJ_34E 0x34E
#define OBJ_34F 0x34F
#define OBJ_350 0x350
#define OBJ_351 0x351
#define OBJ_352 0x352
#define OBJ_353 0x353
#define OBJ_354 0x354
#define OBJ_355 0x355
#define OBJ_356 0x356
#define OBJ_357 0x357
#define OBJ_358 0x358
#define OBJ_359 0x359
#define OBJ_35A 0x35A
#define OBJ_35B 0x35B
#define OBJ_35C 0x35C
#define OBJ_35D 0x35D
#define OBJ_35E 0x35E
#define OBJ_35F 0x35F
#define OBJ_360 0x360
#define OBJ_361 0x361
#define OBJ_362 0x362
#define OBJ_363 0x363
#define OBJ_364 0x364
#define OBJ_365 0x365
#define OBJ_366 0x366
#define OBJ_367 0x367
#define OBJ_368 0x368
#define OBJ_369 0x369
#define OBJ_36A 0x36A
#define OBJ_36B 0x36B
#define OBJ_36C 0x36C
#define OBJ_36D 0x36D
#define OBJ_36E 0x36E
#define OBJ_36F 0x36F
#define OBJ_370 0x370
#define OBJ_371 0x371
#define OBJ_372 0x372
#define OBJ_373 0x373
#define OBJ_374 0x374
#define OBJ_375 0x375
#define OBJ_376 0x376
#define OBJ_377 0x377
#define OBJ_378 0x378
#define OBJ_379 0x379
#define OBJ_37A 0x37A
#define OBJ_37B 0x37B
#define OBJ_37C 0x37C
#define OBJ_37D 0x37D
#define OBJ_37E 0x37E
#define OBJ_37F 0x37F
#define OBJ_380 0x380
#define OBJ_381 0x381
#define OBJ_382 0x382
#define OBJ_383 0x383
#define OBJ_384 0x384
#define OBJ_385 0x385
#define OBJ_386 0x386
#define OBJ_387 0x387
#define OBJ_388 0x388
#define OBJ_389 0x389
#define OBJ_38A 0x38A
#define OBJ_38B 0x38B
#define OBJ_38C 0x38C
#define OBJ_38D 0x38D
#define OBJ_38E 0x38E
#define OBJ_38F 0x38F
#define OBJ_390 0x390
#define OBJ_391 0x391
#define OBJ_392 0x392
#define OBJ_393 0x393
#define OBJ_394 0x394
#define OBJ_395 0x395
#define OBJ_396 0x396
#define OBJ_397 0x397
#define OBJ_398 0x398
#define OBJ_399 0x399
#define OBJ_39A 0x39A
#define OBJ_39B 0x39B
#define OBJ_39C 0x39C
#define OBJ_39D 0x39D
#define OBJ_39E 0x39E
#define OBJ_39F 0x39F
#define OBJ_3A0 0x3A0
#define OBJ_3A1 0x3A1
#define OBJ_3A2 0x3A2
#define OBJ_3A3 0x3A3
#define OBJ_3A4 0x3A4
#define OBJ_3A5 0x3A5
#define OBJ_3A6 0x3A6
#define OBJ_3A7 0x3A7
#define OBJ_3A8 0x3A8
#define OBJ_3A9 0x3A9
#define OBJ_3AA 0x3AA
#define OBJ_3AB 0x3AB
#define OBJ_3AC 0x3AC
#define OBJ_3AD 0x3AD
#define OBJ_3AE 0x3AE
#define OBJ_3AF 0x3AF
#define OBJ_3B0 0x3B0
#define OBJ_3B1 0x3B1
#define OBJ_3B2 0x3B2
#define OBJ_3B3 0x3B3
#define OBJ_3B4 0x3B4
#define OBJ_3B5 0x3B5
#define OBJ_3B6 0x3B6
#define OBJ_3B7 0x3B7
#define OBJ_3B8 0x3B8
#define OBJ_3B9 0x3B9
#define OBJ_3BA 0x3BA
#define OBJ_3BB 0x3BB
#define OBJ_3BC 0x3BC
#define OBJ_3BD 0x3BD
#define OBJ_3BE 0x3BE
#define OBJ_3BF 0x3BF
#define OBJ_3C0 0x3C0
#define OBJ_3C1 0x3C1
#define OBJ_3C2 0x3C2
#define OBJ_3C3 0x3C3
#define OBJ_3C4 0x3C4
#define OBJ_3C5 0x3C5
#define OBJ_3C6 0x3C6
#define OBJ_3C7 0x3C7
#define OBJ_3C8 0x3C8
#define OBJ_3C9 0x3C9
#define OBJ_3CA 0x3CA
#define OBJ_3CB 0x3CB
#define OBJ_3CC 0x3CC
#define OBJ_3CD 0x3CD
#define OBJ_3CE 0x3CE
#define OBJ_3CF 0x3CF
#define OBJ_3D0 0x3D0
#define OBJ_3D1 0x3D1
#define OBJ_3D2 0x3D2
#define OBJ_3D3 0x3D3
#define OBJ_3D4 0x3D4
#define OBJ_3D5 0x3D5
#define OBJ_3D6 0x3D6
#define OBJ_3D7 0x3D7
#define OBJ_3D8 0x3D8
#define OBJ_3D9 0x3D9
#define OBJ_3DA 0x3DA
#define OBJ_3DB 0x3DB
#define OBJ_3DC 0x3DC
#define OBJ_3DD 0x3DD
#define OBJ_3DE 0x3DE
#define OBJ_3DF 0x3DF
#define OBJ_3E0 0x3E0
#define OBJ_3E1 0x3E1
#define OBJ_3E2 0x3E2
#define OBJ_3E3 0x3E3
#define OBJ_3E4 0x3E4
#define OBJ_3E5 0x3E5
#define OBJ_3E6 0x3E6
#define OBJ_3E7 0x3E7
#define OBJ_3E8 0x3E8
#define OBJ_3E9 0x3E9
#define OBJ_3EA 0x3EA
#define OBJ_3EB 0x3EB
#define OBJ_3EC 0x3EC
#define OBJ_3ED 0x3ED
#define OBJ_3EE 0x3EE
#define OBJ_3EF 0x3EF
#define OBJ_3F0 0x3F0
#define OBJ_3F1 0x3F1
#define OBJ_3F2 0x3F2
#define OBJ_3F3 0x3F3
#define OBJ_3F4 0x3F4
#define OBJ_3F5 0x3F5
#define OBJ_3F6 0x3F6
#define OBJ_3F7 0x3F7
#define OBJ_3F8 0x3F8
#define OBJ_3F9 0x3F9
#define OBJ_3FA 0x3FA
#define OBJ_3FB 0x3FB
#define OBJ_3FC 0x3FC
#define OBJ_3FD 0x3FD
#define OBJ_3FE 0x3FE
#define OBJ_3FF 0x3FF*/

#endif
