/* rpgame.h: GAME.EXE's bindings for Exhume's runtime/replay/replay.c (its first comment lists
   them), which includes this file. Only the replay DOS build reads it: replay.c compiled with
   GAME's switches and -DRP_FAR_STORAGE, and the sources that use the hooks with -DREPLAY
   (PLAN-exact-port.md, Phase 1). The gate never compiles it; the port does not link replay.c
   yet (port/sys/rphooks.c until Phase 2). It keeps an 8.3 name, since the DOS build stages it.

   RP_FAR_STORAGE: nothing here may put data in DGROUP (GAME.EXE's near data must not move):
   no string literals, no variables. What the bindings need to keep lives in code segments of
   the replay build's own assembly (OSILIB/MOUSERP.ASM) and is reached through far pointers
   made from public labels, which Turbo C takes as far functions (a fixup, no data).

   MINIMAL, task 5's: the dump (RP_LEVEL, RP_NSECTIONS, RP_DUMP), RP_CLOCK and RP_PIT_DIVISOR
   are placeholders that task 6 replaces (marked TASK 6 below). */
#ifndef RPGAME_H
#define RPGAME_H

#include "u6port.h"

#ifndef RP_FAR_STORAGE
#define RP_FAR_STORAGE                  /* replay.c keeps nothing in DGROUP */
#endif

/* RECORD.OUT's first four bytes, "U6GR" (RP_TAG is replay.c's) */
#define RP_MAGIC_TAG    RP_TAG('U', '6', 'G', 'R')

/* the key read: KBD_GetKey(keepLastOnly) (hookgame.h's HOOK_KEY_ARG and ORIG_KEY). It answers
   the ASCII code, or 100h + the scan code for a key with none. The stop key is F9, 143h: the
   BIOS's int 16h function 0, which KBD_GetKey uses, never gives F11 or F12, and the game uses
   F1 to F8 (a party member, console.c) and F10 but not F9. Neither recording nor replay lets
   the game see it. */
int KBD_GetKey(int keepLastOnly);
#define RP_KEY_READ(a)  ORIG_KEY(a)
#define RP_STOP_SCAN    0x43
#define RP_STOP_KEY(r)  ((r) == 0x143)

/* the key state the key read changes: Ultima VI keeps none, and replay.c needs at least one
   byte, so one that never changes, outside DGROUP (MOUSERP.ASM) */
extern void cdecl u6_rp_keystate(void);
#define RP_KEYSTATE     ((unsigned char far *)u6_rp_keystate)
#define RP_KEYSTATE_LEN 1

/* the mouse (MOUSERP.ASM): one int 33h AX=3 a poll, by u6_mouse_poll, which keeps x (halved, as
   the game uses it), y and the buttons in u6_mouse_regs, three words of MOUSERP's code segment;
   RP_BUTTONS_READ answers the buttons of the poll RP_MOUSE_READ just made (_MOUSE_SERVE calls
   rp_mouse, then rp_mbuttons) */
extern void cdecl u6_mouse_poll(void);
extern void cdecl u6_mouse_regs(void);
#define RP_MOUSE_REGS   ((int far *)u6_mouse_regs)
#define RP_MOUSE_READ() u6_mouse_poll()
#define RP_MOUSE_DX     (RP_MOUSE_REGS[0])
#define RP_MOUSE_DY     (RP_MOUSE_REGS[1])
#define RP_BUTTONS_READ() (RP_MOUSE_REGS[2])

/* the joystick: Ultima VI has none and never calls JOY_READ or JOY_BUTTONS; replay.c's hooks
   for them still compile, so four words outside DGROUP (MOUSERP.ASM) and reads that do nothing */
extern void cdecl u6_rp_joy(void);
#define RP_JOY_READ()   ((void)0)
#define RP_JOYB_READ()  ((void)0)
#define RP_JOY_X        (((int far *)u6_rp_joy)[0])
#define RP_JOY_Y        (((int far *)u6_rp_joy)[1])
#define RP_JOYB_0       (((int far *)u6_rp_joy)[2])
#define RP_JOYB_1       (((int far *)u6_rp_joy)[3])

/* the random state: RAND.ASM's two words, D_3088 (low) and D_308A (high), as one dword */
extern unsigned long D_3088;
#define RP_RAND_SEED()  (D_3088)

/* C0's null pointer check: Turbo C 2.0 sums DS:0..30h to 0DFAh (replay.c's first comment) */
#define RP_NULL_SUM_FROM 0
#define RP_NULL_SUM     0x0DFA

/* what main does at its end before exit(0): main.c's rp_u6_shutdown, compiled with -DREPLAY
   (the music stopped, the temporary object blocks deleted, int 1Ch and the video mode put back) */
int rp_u6_shutdown(void);
#define RP_SHUTDOWN()   rp_u6_shutdown()

/* the clock: the BIOS's 18.2 Hz tick, which OSI_delay's int 1Ch handler counts. Neither is read
   through a hook: Ultima VI never calls GAME_TIME and runs no timer callback through
   SLAVE_TIMER, so these only let replay.c compile. TASK 6: RP_CLOCK, and RP_PIT_DIVISOR if the
   music driver's sound reads (SND_READ) are recorded; 0FFFFh stands for the BIOS's 65536. */
#define RP_TICK_US      54925UL
#define RP_PIT_DIVISOR  0xFFFFu
#define RP_CLOCK        0UL

/* The dump. TASK 6 replaces all of this with the real sections (the game state, masks, the
   far blocks' segments). For now: one game section, DGRP, DGROUP from DS:0 to _BSSEND
   (EDFAh in GAME.MAP: _DATA and the BSS.ASM block), then the common ones. */
#define RP_DGROUP_END   0xEDFAu
#define RP_LEVEL        1
#define RP_NSECTIONS(full, level, fb) ((full) ? 7 : 4)
#define RP_DUMP(full, level, fb) do { \
        section(RP_TAG('D', 'G', 'R', 'P'), RP_DGROUP_END); \
        dump_far((unsigned char far *)MK_FP(_DS, 0), RP_DGROUP_END); \
        dump_rand(); \
        dump_null(); \
        dump_counts(); \
        if (full) dump_screen(); \
    } while (0)

#endif
