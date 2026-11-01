/* U.EXE (menu, introduction, character creation) in the port: port/u/urun.c */
#ifndef U_URUN_H
#define U_URUN_H

#include <stdint.h>

/* runs U.EXE's main (the files and the DOS arena must be set up); it returns when U
 * quits, or does not return (U's execl of GAME.EXE goes through dosint's exec handler) */
int u_run(int argc, char **argv);

/* MCGADRV.BIN's mode set (entry 0): the screen is A000:0000 of the arena (END.EXE uses it
 * too) */
void u_mode_set(void);

#endif
