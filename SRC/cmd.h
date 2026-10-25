/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
#ifndef __CMD_H__
#define __CMD_H__

/*CON_getch result: a direction was chosen (AdvanceDir)*/
#define CMD_DIRECTION 0x80
/*attack*/
#define CMD_ATTACK 0x81
/*cast*/
#define CMD_CAST 0x82
/*talk*/
#define CMD_TALK 0x83
/*look*/
#define CMD_LOOK 0x84
/*get*/
#define CMD_GET 0x85
/*drop*/
#define CMD_DROP 0x86
/*move*/
#define CMD_MOVE 0x87
/*use*/
#define CMD_USE 0x88
/*Rest command*/
#define CMD_REST 0x89
/*begin/break combat*/
#define CMD_COMBAT 0x8A
/*a target was selected: Selection.x/y/obj filled (map, inventory, party member, spell line)*/
#define CMD_TARGET 0x8E
/*pass*/
#define CMD_PASS 0x8F
/*Status:Portrait*/
#define CMD_STAT_PORTRAIT 0x90
/*Status:Party*/
#define CMD_STAT_PARTY 0x91
/*Status:Equipment*/
#define CMD_STAT_EQUIP 0x92
/*party view: scroll list up*/
#define CMD_PARTY_SCROLL_UP 0x93
/*party view: scroll list down*/
#define CMD_PARTY_SCROLL_DOWN 0x94
/*equipment view: scroll inventory up one row*/
#define CMD_INVEN_SCROLL_UP 0x95
/*equipment view: scroll inventory down one row*/
#define CMD_INVEN_SCROLL_DOWN 0x96

/*portrait/equipment view: next party member*/
#define CMD_NEXT_MEMBER 0x99
/*portrait/equipment view: previous party member*/
#define CMD_PREV_MEMBER 0x9A
/*mkMouseSelection result: pointer on an inventory (or container) slot*/
#define CMD_HIT_INVEN_SLOT 0x9B
/*mkMouseSelection result: pointer on an equipment slot*/
#define CMD_HIT_EQUIP_SLOT 0x9C
/*mkMouseSelection result: object on the pointed map tile*/
#define CMD_HIT_MAP_OBJ 0x9D
/*StatusDisplay showing a conversation partner's portrait (ShowPortraitAndInventory)*/
#define CMD_STAT_TALK 0x9E
/*equipment view: leave the shown container (to parent/owner, or party view)*/
#define CMD_CONTAINER_UP 0x9F
/*equipment view: show the content of the clicked container*/
#define CMD_CONTAINER_OPEN 0xA0
/*Status:Spellbook*/
#define CMD_STAT_SPELLBOOK 0xA1
/*0xA2 = spellbook button 0: jumps to the first page of the previous circle (SpellPageNav[0])*/
#define CMD_SPELLBOOK_PREV_CIRCLE 0xA2
/*0xA3 = button 1 / LEFT key: previous page; 'RIGHT' is misleading (next page is 0xA4)*/
#define CMD_SPELLBOOK_PREV_PAGE 0xA3
/*spellbook button 2 / right key*/
#define CMD_SPELLBOOK_NEXT_PAGE 0xA4
/*spellbook button 3: first page of the next circle*/
#define CMD_SPELLBOOK_NEXT_CIRCLE 0xA5
/*never produced; CON_getch only refreshes the status panel (low confidence)*/
#define CMD_REFRESH_STATUS 0xA6
/*equipment view: ready the clicked inventory object*/
#define CMD_READY 0xA7
/*equipment view: unready the clicked equipped object*/
#define CMD_UNREADY 0xA8
/*right click / free-aim ENTER: run DefaultCommand on Selection*/
#define CMD_DEFAULT_ACTION 0xA9
/*Save game command (<CTRL+S>)*/
#define CMD_SAVE 0xAA


#endif
