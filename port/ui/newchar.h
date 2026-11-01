/* A character without U.EXE's screens (tests, --new-avatar): newchar.c. */
#ifndef PORT_NEWCHAR_H
#define PORT_NEWCHAR_H

#include <stdint.h>

/* 1 if savegame\objlist (through dosfs) has no avatar yet (AvatarPortrait 0) */
int newchar_needed(void);
/* no screen: "NAME[,M|F[,PORTRAIT 1-6[,VIRTUE 1-8]]]" (tests, --new-avatar) */
int newchar_quick(const char *spec);
/* what U.EXE writes for a choice of virtues: picks[v] = times virtue v was chosen */
void newchar_stats(const int picks[8], int *str, int *dex, int *intel, int *karma);
int newchar_write(const char *name, int female, int portrait, const int picks[8]);

#endif
