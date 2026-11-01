/*
 * Ultima VI - The False Prophet: END.EXE (the endgame: the Codex, the lenses, the kings)
 * shared declarations of the decompilation. The types (struct tSurface, tText, tMouse,
 * union tDrv/tMus, DRV(), MUS()) are U.EXE's: SRC_U/u.h.
 */
#ifndef END_H
#define END_H

#include "u.h"

/*the uninitialised data (_BSS) the modules share (DS offsets)*/
/*1816*/extern int16 videoMode;/*video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
/*1831*/extern int16 scene;/*the scene of the ending (0..15): which text is shown*/
/*1833*/extern uint16 starSeedX;/*random generator of the stars' x*/
/*1835*/extern uint16 starSeedY;/*random generator of the stars' y*/
/*1E4C*/extern char starColor[];/*[64] the stars' colours*/
/*1E8C*/extern struct tSurface screen;/*the screen*/
/*1EEC*/extern char starX[];/*[64] the stars' x (0..112)*/
/*1F2C*/extern char starY[];/*[64] the stars' y (0..93)*/
/*1F6C*/extern int16 scanlineOfs[];/*[200] the offset of each line on the screen*/

#endif
