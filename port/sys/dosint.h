/* Stand-ins for the DOS services the game's C calls directly (Turbo C's dos.h and
 * process.h): int86/int86x/intdos/intdosx/segread, outportb/inportb, execl, exit.
 *
 * The emulated machine: MS-DOS 5.00 with one drive, C: (fixed), holding the game folder;
 * a VGA whose DAC is port/gfx/vgadac.h; 640 KB of conventional memory.
 * Calls the port does not know are logged once and fail (carry set, AX = 1).
 */
#ifndef DOSINT_H
#define DOSINT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* union REGS, struct SREGS and the prototypes of int86, int86x, intdos, segread,
 * outportb and inportb: the game's stand-in for Borland's dos.h. The byte view of REGS
 * aliases the word view as on the 8086 (al = low byte of ax): the host must be little
 * endian. */
#include "../include/dos.h"

int intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs);

#if defined(__GNUC__) || defined(__clang__)
#define DOS_NORETURN __attribute__((noreturn))
#else
#define DOS_NORETURN
#endif

/* execl (declared by port/include/process.h): the port cannot start END.EXE or
 * ULTIMA6.EXE. It logs the request, calls the exec handler if one is set (which may end
 * the program, or start another one), and then fails as DOS would for a missing program:
 * returns -1 (errno ENOENT). Both callers in main.c retry forever, so later calls pump the
 * host's events, sleep a little, and end the program once the user closes the window (or
 * a replay is over).
 * The game ends the arguments with a plain 0, an int: read back through "..." as a
 * pointer on a 64-bit host its upper half may be garbage, so an argument whose low 32
 * bits are 0 ends the list. dos_execl_list takes them as an array instead. */
int execl(const char *path, const char *arg0, ...);
int dos_execl_list(int n, const char *const *args);
typedef void (*dos_exec_handler)(const char *path, int argc, const char *const *argv);
void dos_set_exec_handler(dos_exec_handler handler);

/* Ends the process from port/sys (OSI_delay's and execl's endless loops): silences the
 * speaker, shuts the platform down, exit(code). The game's own exit is port_exit
 * (port/include/compat.h), defined by port/sys/portmain.c. */
void dos_exit(int code) DOS_NORETURN;
/* If set (portmain: port_exit), dos_exit calls it instead, so that a recording is
 * flushed; it must not return. */
extern void (*dos_exit_hook)(int code);

/* The CPU's divide error (a DIV whose quotient does not fit): Turbo C's startup code
 * prints "Divide error" and ends the program; so does the port (plat_fatal). */
void dos_divide_error(const char *where) DOS_NORETURN;

/* ---- the emulated DOS state, shared with OSI_selectDrive/OSI_getDrive ---- */
#define DOS_VERSION_MAJOR 5
#define DOS_VERSION_MINOR 0
#define DOS_GAME_DRIVE 2               /* C: */
int dos_current_drive(void);          /* 0 = A: */
int dos_drive_valid(int drive);
void dos_select_drive(int drive);     /* int 21h 0Eh: ignored for an invalid drive */
/* Wall clock, live (not recorded): DOS int 21h 2Ah/2Ch values. */
void dos_time_live(uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *hundredths);
void dos_date_live(uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *weekday);

#ifdef __cplusplus
}
#endif

#endif
