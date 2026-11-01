/* OSI_FILE.ASM (files with the error handler protocol) and LOW.ASM (drives, startup).
 *
 * The DOS calls are dosfs.c's. The error protocol is the original's, macro by macro:
 *
 * OSIERR_1 (open, close, create, delete, rename): on a DOS error, OSI_ErrorCode = the DOS
 *   error, OSI_ErrorFunc = the function number, call OSI_ErrorHandler; if the handler
 *   cleared OSI_ErrorCode the DOS call is made again (open, create, delete and rename set
 *   OSI_ErrorFile again first), else the routine returns -1.
 * OSIERR_2 (each read/write call of OSI_read/OSI_write): the same, but when the handler
 *   gives up the routine carries on with AX = whatever the handler left in AX as the byte
 *   count (see handler_ax), which then normally differs from the count asked for and
 *   leads to the short count report below.
 * OSI_read/OSI_write work in blocks of 0FFF0h bytes (advancing the buffer by 0FFFh
 *   paragraphs); a block that moved fewer bytes than asked calls the handler once more
 *   with OSI_ErrorCode = -1 and ends the routine (no retry). A failed seek calls the
 *   handler and ends the routine (no retry either).
 */
#include "osi.h"
#include "dosfs.h"
#include "dosint.h"
#include "dosmem.h"
#include "../platform/plat.h"

#include <stdlib.h>

/* ==== globals of OSI_FILE.ASM and LOW.ASM ==== */

typedef void *(*handler_fn)();

/* the generic function pointer type in between keeps compilers quiet about the cast */
#define CALLBACK_FN ((handler_fn)(void (*)(void))OSI_callback)

union tHandler OSI_ErrorHandler = {.f = CALLBACK_FN};   /* D_032E */
int16_t OSI_ErrorCode = 0;     /* D_0332 */
int16_t OSI_ErrorFunc = 0;     /* D_0334 */
char *OSI_ErrorFile = 0;       /* D_0336 */

uint16_t CpuSpeed = 0;         /* D_2FC6 */
int16_t OSI_Started = 0;       /* D_2FFC */
int16_t TextIdle = 1;          /* D_2FFE */

/* OSI_nullCallback (C_3368_00DC) */
static void osi_null_callback(void)
{
}

void (*DriveErrorCallback)() = osi_null_callback;   /* D_3004 */

int OSI_callback(void)
{
	return 0;
}

/* AX after the handler returned. OSI_callback is a bare RET, so AX still holds the error
 * code it was called with; a handler written in C (FileErrorHandler) leaves whatever its
 * code last put in AX, which the port cannot know: 0. */
static uint16_t handler_ax;

/* OSIERR_CB */
static void call_handler(int16_t func, int16_t code)
{
	handler_fn f;

	OSI_ErrorCode = code;
	OSI_ErrorFunc = func;
	f = OSI_ErrorHandler.f;
	if (f == 0) {
		plat_log("OSI: error %d in function %d with no error handler (DOS would crash)", code, func);
		handler_ax = (uint16_t)code;
		return;
	}
	if (f == CALLBACK_FN) {
		handler_ax = (uint16_t)code;
		return;
	}
	handler_ax = 0;
	f();
}

/* ==== OSI_FILE.ASM ==== */

int16_t OSI_open(char *path)
{
	int16_t h;
	int err;

	for (;;) {
		OSI_ErrorFile = path;
		err = dosfs_open(path, 2, &h);
		if (!err)
			return h;
		call_handler(1, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return -1;
	}
}

int16_t OSI_close(int16_t handle)
{
	int err;

	for (;;) {
		err = dosfs_close(handle);
		if (!err)
			return 0;
		call_handler(2, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return -1;
	}
}

int16_t OSI_create(char *path)
{
	int16_t h;
	int err;

	for (;;) {
		OSI_ErrorFile = path;
		err = dosfs_create(path, &h);
		if (!err)
			return h;
		call_handler(3, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return -1;
	}
}

int OSI_delete(char *path)
{
	int err;

	for (;;) {
		OSI_ErrorFile = path;
		err = dosfs_delete(path);
		if (!err)
			return 0;
		call_handler(4, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return -1;
	}
}

/* One DOS read or write with OSIERR_2: the count moved, or the handler's AX. */
static uint16_t rw_block(int write, int16_t handle, uint8_t *buf, uint16_t count)
{
	uint16_t done;
	int err;

	for (;;) {
		if (write)
			err = dosfs_write(handle, buf, count, &done);
		else
			err = dosfs_read(handle, buf, count, &done);
		if (!err)
			return done;
		call_handler(write ? 6 : 5, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return handler_ax;
	}
}

uint32_t OSI_read(int16_t handle, uint32_t offset, uint32_t length, void *buf)
{
	uint8_t *p = (uint8_t *)buf;
	uint32_t total = 0;
	uint16_t ax;
	int err;

	if ((int16_t)(offset >> 16) >= 0) {
		err = dosfs_seek(handle, 0, (int32_t)offset, 0);
		if (err) {
			call_handler(5, (int16_t)err);
			return total;
		}
	}
	while (length > 0xFFF0u) {
		ax = rw_block(0, handle, p, 0xFFF0u);
		total += ax;
		if (ax != 0xFFF0u) {
			call_handler(5, -1);
			return total;
		}
		length -= 0xFFF0u;
		p += 0xFFF0u;
	}
	ax = rw_block(0, handle, p, (uint16_t)length);
	total += ax;
	if (ax != (uint16_t)length)
		call_handler(5, -1);
	return total;
}

uint32_t OSI_Read(int handle, uint32_t offset, uint32_t length, void *buf)
{
	return OSI_read((int16_t)handle, offset, length, buf);
}

/* Returns DX:AX as the original leaves them: AX the count of the last DOS write (or the
 * handler's AX), DX the offset word of its buffer. The game does not use it. */
uint32_t OSI_write(int16_t handle, uint32_t offset, uint32_t length, void *buf)
{
	uint8_t *p = (uint8_t *)buf;
	uint16_t ax;
	int err;

	if ((int16_t)(offset >> 16) >= 0) {
		err = dosfs_seek(handle, 0, (int32_t)offset, 0);
		if (err) {
			call_handler(6, (int16_t)err);
			return handler_ax;
		}
	}
	while (length > 0xFFF0u) {
		ax = rw_block(1, handle, p, 0xFFF0u);
		if (ax != 0xFFF0u) {
			call_handler(6, -1);
			return handler_ax;
		}
		length -= 0xFFF0u;
		p += 0xFFF0u;
	}
	ax = rw_block(1, handle, p, (uint16_t)length);
	if (ax != (uint16_t)length) {
		call_handler(6, -1);
		return handler_ax;
	}
	return ((uint32_t)dos_fp_off(p) << 16) | ax;
}

int OSI_rename(char *oldName, char *newName)
{
	int err;

	for (;;) {
		OSI_ErrorFile = oldName;
		err = dosfs_rename(oldName, newName);
		if (!err)
			return 0;
		call_handler(7, (int16_t)err);
		if (OSI_ErrorCode != 0)
			return -1;
	}
}

/* OSI_copy: a buffer of all free DOS memory, OSI_open the source (its own error
 * protocol), create the destination with a bare DOS call, then OSI_read/OSI_write at the
 * current positions until a read comes back short (each short read, the last one at
 * least, reports -1 to the handler as an OSI_read error). Failures before the copy report
 * error 8 with AX (-1 after a failed OSI_open). The game never calls it.
 * Deviation: when no DOS memory is free (the game holds it all) the original would copy
 * with a 0-byte buffer forever; the port reports error 8 (not enough memory). */
int OSI_copy(char *src, char *dst)
{
	uint16_t paras = dos_free_paragraphs();
	uint32_t size = (uint32_t)paras << 4, n;
	uint8_t *buf;
	int16_t in, out;
	int err;

	if (paras == 0) {
		call_handler(8, 8);
		return -1;
	}
	buf = (uint8_t *)malloc(size);
	if (!buf) {
		call_handler(8, 8);
		return -1;
	}
	in = OSI_open(src);
	if (in < 0) {
		call_handler(8, -1);
		free(buf);
		return -1;
	}
	err = dosfs_create(dst, &out);
	if (err) {
		call_handler(8, (int16_t)err);
		free(buf);
		return -1;
	}
	do {
		n = OSI_read(in, 0xFFFFFFFFu, size, buf);
		OSI_write(out, 0xFFFFFFFFu, n, buf);
	} while (n >= size);
	OSI_close(in);
	OSI_close(out);
	free(buf);
	return 0;
}

uint32_t OSI_fileSize(int16_t handle)
{
	uint32_t pos;

	if (dosfs_seek(handle, 2, 0, &pos) != 0)
		return 0;
	return pos;
}

uint32_t OSI_memory(void)
{
	return (uint32_t)dos_free_paragraphs() << 4;
}

/* ==== LOW.ASM ==== */

static uint8_t selected_drive;   /* D_2FC2 */
static uint16_t crit_error;      /* D_2FCC: set by the int 24h handler */

int OSI_selectDrive(int letter)
{
	int8_t al = (int8_t)(uint8_t)letter;   /* C_0008 compares AL as signed */

	crit_error = 0;
	if (al >= 'a' && al <= 'z')
		al = (int8_t)(al - ('a' - 'A'));
	if (al >= 'A' && al <= 'Z') {
		selected_drive = (uint8_t)(al - 'A');
		dos_select_drive(selected_drive);           /* int 21h 0Eh */
		if (crit_error == 0 && dos_current_drive() == selected_drive) {
			crit_error = 0;
			return 1;
		}
	}
	crit_error = 1;
	if (DriveErrorCallback)
		DriveErrorCallback();
	return 0;
}

int OSI_getDrive(void)
{
	return 'A' + dos_current_drive();
}

/* Installs the critical error (24h) and Ctrl-Break (23h) handlers, which the port does
 * not need, clears some unused state and measures CpuSpeed: the port sets
 * osi_cpu_speed instead (a measure would differ from run to run). */
void OSI_lowInit(void)
{
	dosmem_init();
	CpuSpeed = osi_cpu_speed;
}
