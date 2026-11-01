/* END.EXE (the endgame) in the port: port/end/erun.c */
#ifndef E_ERUN_H
#define E_ERUN_H

/* runs END.EXE's main with GAME.EXE's arguments (config, years, months, days): the files
 * and the DOS arena must be set up. It returns when END's main returns. */
int e_run(int argc, char **argv);

#endif
