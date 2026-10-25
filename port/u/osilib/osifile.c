/* OSI_FILE.ASM: the OSI file routines with their error handler protocol (pascal in DOS).
 *
 * U.EXE's OSI_FILE.ASM is GAME.EXE's, instruction for instruction (only the assembler's
 * spelling differs), so the port uses GAME's port/sys/osi.c (over dosfs.c; its protocol
 * is in docs/port/OSILIB.md) through osigame.c. U.EXE has its own copy of the error state
 * (OSI_ErrorHandler, OSI_ErrorCode, OSI_ErrorFunc, OSI_ErrorFile, at D_45D8..D_45E0): these
 * are its variables, swapped in for the duration of each call, so that U's errors never
 * reach the handler GAME's code installs (FileErrorHandler) and the other way round. U
 * never changes its handler: it stays OSI_callback, which does nothing. */
#include "uosi.h"
#include "osigame.h"

void (*OSI_ErrorHandler)(void) = (void (*)(void))OSI_callback;   /* D_45D8 */
int16 OSI_ErrorCode = 0;   /* D_45DC */
int16 OSI_ErrorFunc = 0;   /* D_45DE */
char *OSI_ErrorFile = 0;   /* D_45E0 */

/* the default handler: a bare RET */
int OSI_callback(void)
{
	return 0;
}

static struct uosi_err e;

static void enter(void)
{
	e.handler = OSI_ErrorHandler;
	e.callback = (void (*)(void))OSI_callback;
	e.code = OSI_ErrorCode;
	e.func = OSI_ErrorFunc;
	e.file = OSI_ErrorFile;
	uosi_game_enter(&e);
}

static void leave(void)
{
	uosi_game_leave(&e);
	OSI_ErrorCode = e.code;
	OSI_ErrorFunc = e.func;
	OSI_ErrorFile = e.file;
}

int16 OSI_open(char *name)
{
	int16 r;

	enter();
	r = uosi_game_open(name);
	leave();
	return r;
}

int16 OSI_close(int16 fd)
{
	int16 r;

	enter();
	r = uosi_game_close(fd);
	leave();
	return r;
}

int16 OSI_create(char *name)
{
	int16 r;

	enter();
	r = uosi_game_create(name);
	leave();
	return r;
}

int OSI_delete(char *name)
{
	int r;

	enter();
	r = uosi_game_delete(name);
	leave();
	return r;
}

int32 OSI_read(int16 fd, int32 offset, int32 length, void *buf)
{
	uint32_t r;

	enter();
	r = uosi_game_read(fd, (uint32_t)offset, (uint32_t)length, buf);
	leave();
	return (int32)r;
}

int32 OSI_write(int16 fd, int32 offset, int32 length, void *buf)
{
	uint32_t r;

	enter();
	r = uosi_game_write(fd, (uint32_t)offset, (uint32_t)length, buf);
	leave();
	return (int32)r;
}

int OSI_rename(char *oldName, char *newName)
{
	int r;

	enter();
	r = uosi_game_rename(oldName, newName);
	leave();
	return r;
}

int OSI_copy(char *src, char *dst)
{
	int r;

	enter();
	r = uosi_game_copy(src, dst);
	leave();
	return r;
}

uint32 OSI_fileSize(int16 fd)
{
	uint32_t r;

	enter();
	r = uosi_game_fileSize(fd);
	leave();
	return r;
}

uint32 OSI_memory(void)
{
	return uosi_game_memory();
}
