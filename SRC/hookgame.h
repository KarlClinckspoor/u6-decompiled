/* hookgame.h: the original tokens of Exhume's record and replay hooks for GAME.EXE
   (runtime/include/portable.h includes this file first, and the DOS build stages it
   beside the sources, so it keeps an 8.3 name).

   In the DOS build each hook is the original tokens, so GAME.EXE's bytes are the same
   (tools/gate.py). Compiled with -DREPLAY (the replay DOS build) and in the port, KEY(a)
   is rp_key(a) and RECORDED(id, x) is rp_value(id, x): the value the game read, recorded,
   or the recorded one, replayed.

   In use so far (PLAN-exact-port.md section 8, task 4): the key reads, and every value
   GAME.EXE's C reads from outside the program. What the assembly library reads (the
   clock, the mouse, the delay loop's calibration) comes with task 5. */
#define HOOK_KEY_ARG                    /* the key read takes an argument: KEY(a) */
#define ORIG_KEY(a)     KBD_GetKey(a)
#define HOOK_RECORDED                   /* RECORDED(id, x), replay.c's rp_value */

/* The ids of the recorded values (a byte each in the recording, checked on replay). */
#define RV_SEED         1   /* main: OSI_timeSeed(), the random seed from the time of day */
#define RV_FARCORE      2   /* main: farcoreleft(), the far heap the game takes whole */
#define RV_DOSVER_MAJOR 3   /* DISK_init: int 21h AH=30h, AL */
#define RV_DOSVER_MINOR 4   /*            and AH */
#define RV_DRV_ERROR    5   /* DISK_init: IOCTL 4408h of a drive, the carry */
#define RV_DRV_FIXED    6   /*            and AX (0 removable, 1 fixed) */
#define RV_DRV_ALIAS    7   /* DISK_init: IOCTL 440Eh of a drive, AL (its logical drive map) */
#define RV_EQUIPMENT    8   /* DISK_init: int 11h, AX (below DOS 3.20) */
#define RV_CUR_DRIVE    9   /* DISK_init: OSI_getDrive(), the current drive's letter */

/* A recorded value of int size: rp_value takes and gives a long, and Turbo C would push
   that long to a function with no prototype (OSI_randomize), so the hook casts it back.
   RECORDED alone is for a long (farcoreleft). */
#if defined(__TURBOC__) && !defined(REPLAY)
#define RECORDED16(id, x) (x)
#else
#define RECORDED16(id, x) ((int16)RECORDED(id, x))
#endif
