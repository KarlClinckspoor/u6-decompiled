/*
 * Ultima VI - The False Prophet: END.EXE (the endgame: the Codex, the lenses, the kings)
 * shared declarations of the decompilation. The types (struct tSurface, tText, tMouse,
 * union tDrv/tMus, DRV(), MUS()) are U.EXE's: SRC_U/u.h.
 */
#ifndef END_H
#define END_H

#include "u.h"

/*the uninitialised data (_BSS) the modules share (DS offsets)*/
extern int16 videoMode;/*1816 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0x0d EGA, 0x13 MCGA*/
extern int16 scene;/*1831 the scene of the ending (0..15): which text is shown*/
extern uint16 starSeedX, starSeedY;/*1833, 1835 random generators of the stars' x and y*/
extern char starColor[];/*1E4C [64] the stars' colours*/
extern struct tSurface screen;/*1E8C the screen*/
extern char starX[];/*1EEC [64] the stars' x (0..112)*/
extern char starY[];/*1F2C [64] the stars' y (0..93)*/
extern int16 scanlineOfs[];/*1F6C [200] the offset of each line on the screen*/

#endif
