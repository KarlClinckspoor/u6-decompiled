/* osigame.c: GAME.EXE's OSI file routines (port/sys/osi.c) called for U (osifile.c). */
#ifndef UOSI_OSIGAME_H
#define UOSI_OSIGAME_H

#include <stdint.h>

/* U's copy of OSI_FILE.ASM's error state (D_45D8..D_45E0) */
struct uosi_err {
	void (*handler)(void);    /* OSI_ErrorHandler */
	void (*callback)(void);   /* U's OSI_callback (stands for GAME's while it is the handler) */
	int16_t code, func;       /* OSI_ErrorCode, OSI_ErrorFunc */
	char *file;               /* OSI_ErrorFile */
};

/* GAME's error state saved and U's put in its place, and back */
void uosi_game_enter(struct uosi_err *u);
void uosi_game_leave(struct uosi_err *u);

int16_t uosi_game_open(char *name);
int16_t uosi_game_close(int16_t fd);
int16_t uosi_game_create(char *name);
int uosi_game_delete(char *name);
uint32_t uosi_game_read(int16_t fd, uint32_t offset, uint32_t length, void *buf);
uint32_t uosi_game_write(int16_t fd, uint32_t offset, uint32_t length, void *buf);
int uosi_game_rename(char *oldName, char *newName);
int uosi_game_copy(char *src, char *dst);
uint32_t uosi_game_fileSize(int16_t fd);
uint32_t uosi_game_memory(void);

#endif
