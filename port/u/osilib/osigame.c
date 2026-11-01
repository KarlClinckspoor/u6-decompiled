/* The bridge from U's OSI file routines (osifile.c) to GAME.EXE's (port/sys/osi.c).
 *
 * This file is compiled with U's force-includes like the others, which rename OSI_open
 * and the rest to U_OSI_open...; here the names must be GAME's, so they are #undef'ed
 * before port/sys/osi.h is included, and the functions this file defines have port names
 * (uosi_game_*), which are not renamed. */
#undef OSI_ErrorHandler
#undef OSI_ErrorCode
#undef OSI_ErrorFunc
#undef OSI_ErrorFile
#undef OSI_callback
#undef OSI_open
#undef OSI_close
#undef OSI_create
#undef OSI_delete
#undef OSI_read
#undef OSI_write
#undef OSI_rename
#undef OSI_copy
#undef OSI_fileSize
#undef OSI_memory

/* osi.h's copies of GAME's struct tSurface and struct tMouse would clash with U's
 * (ugfx.h, force-included): this file does not need them */
#define __MOUSE_H__
struct tMouse;
#include "../../sys/osi.h"

#include "osigame.h"

typedef void *(*handler_fn)();
#define GAME_CALLBACK ((handler_fn)(void (*)(void))OSI_callback)

/* U's error state while its routine runs (osifile.c keeps U's copies) */
static union tHandler saved_handler;
static int16_t saved_code, saved_func;
static char *saved_file;

void uosi_game_enter(struct uosi_err *u)
{
	saved_handler = OSI_ErrorHandler;
	saved_code = OSI_ErrorCode;
	saved_func = OSI_ErrorFunc;
	saved_file = OSI_ErrorFile;
	/* U's default handler is its own OSI_callback, a bare RET as GAME's: GAME's code
	 * takes AX after it for the error code only when it recognises its own */
	OSI_ErrorHandler.f = u->handler && u->handler != u->callback ? (handler_fn)u->handler : GAME_CALLBACK;
	OSI_ErrorCode = u->code;
	OSI_ErrorFunc = u->func;
	OSI_ErrorFile = u->file;
}

void uosi_game_leave(struct uosi_err *u)
{
	u->code = OSI_ErrorCode;
	u->func = OSI_ErrorFunc;
	u->file = OSI_ErrorFile;
	OSI_ErrorHandler = saved_handler;
	OSI_ErrorCode = saved_code;
	OSI_ErrorFunc = saved_func;
	OSI_ErrorFile = saved_file;
}

int16_t uosi_game_open(char *name) { return OSI_open(name); }
int16_t uosi_game_close(int16_t fd) { return OSI_close(fd); }
int16_t uosi_game_create(char *name) { return OSI_create(name); }
int uosi_game_delete(char *name) { return OSI_delete(name); }
uint32_t uosi_game_read(int16_t fd, uint32_t offset, uint32_t length, void *buf)
{
	return OSI_read(fd, offset, length, buf);
}
uint32_t uosi_game_write(int16_t fd, uint32_t offset, uint32_t length, void *buf)
{
	return OSI_write(fd, offset, length, buf);
}
int uosi_game_rename(char *oldName, char *newName) { return OSI_rename(oldName, newName); }
int uosi_game_copy(char *src, char *dst) { return OSI_copy(src, dst); }
uint32_t uosi_game_fileSize(int16_t fd) { return OSI_fileSize(fd); }
uint32_t uosi_game_memory(void) { return OSI_memory(); }
