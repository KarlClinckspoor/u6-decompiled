/* stand-in for Turbo C's conio.h, the parts U.EXE's C uses (port/u/osilib: the keyboard
 * over the platform layer; gotoxy and clreol are the OSI library's, renamed U_*) */
#ifndef U_CONIO_H
#define U_CONIO_H
int kbhit(void);
int getch(void);
int gotoxy(int16 x, int16 y);
int clreol(void);
#endif
