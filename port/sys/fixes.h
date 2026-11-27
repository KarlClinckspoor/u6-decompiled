/* The fixes: corrections of the original game's faults (docs/CODE_REVIEW.md's critical
 * findings), each off unless the player turns it on (--fixes, or fixes= in u6port.cfg). Off,
 * the port plays exactly as DOS does: the game's code tests them only in port-only lines of
 * SRC (SRC/u6port.h's U6_FIX, under !__TURBOC__), so the DOS builds never see them. The set is
 * chosen before the game starts and never changes while it runs; a session played with fixes
 * is recorded with them (replay.c's format 5) and replays with them. docs/port/FIXES.md. */
#ifndef PORT_FIXES_H
#define PORT_FIXES_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* bit i of enhance_on: the fix in entry i of the table (fixes.c) */
enum {
	FIX_VORTEX_CUBE,        /* CR-001 */
	FIX_INVENTORY_SCROLL,   /* CR-002 */
	FIX_CAST_TARGET,        /* CR-003 */
	FIX_KEG_FUSE,           /* CR-004 */
	FIX_HEALER_BODY,        /* CR-005 */
	FIX_LENS_BEAM,          /* CR-006 */
	FIX_FULL_OBJECT_TABLE,  /* CR-007 */
	FIX_FOUNTAIN_WISH,      /* CR-008 */
	FIX_MAP_SEAM,           /* CR-010 */
	FIX_COUNT
};

extern uint32_t enhance_on;     /* replay.c's name: the session's flags */

/* LIST ("all", "none", or names comma-separated, a "-" before a name turning it off) into
 * *mask; -1 (and a message) for an unknown name, *mask unchanged */
int fixes_parse(const char *list, uint32_t *mask);
/* MASK's names comma-separated ("" for none); replay.c writes them into a recording */
char *enhance_names(uint32_t mask, char *buf, int n);
/* --fixes list */
void fixes_list(FILE *f);
/* the fixes a recording was made with (format 5's chunk of stream 9) into *mask: 0, or -1
 * for a recording that names a fix this build lacks */
int fixes_from_recording(const char *path, uint32_t *mask);
/* one line on stderr when any is on */
void fixes_log(void);

#endif
