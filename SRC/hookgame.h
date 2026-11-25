/* hookgame.h: the original tokens of Exhume's record and replay hooks for GAME.EXE
   (runtime/include/portable.h includes this file first, and the DOS build stages it
   beside the sources, so it keeps an 8.3 name).

   In the DOS build each hook is the original tokens, so GAME.EXE's bytes are the same
   (tools/gate.py). Compiled with -DREPLAY (the replay DOS build) and in the port, KEY(a)
   is rp_key(a) and RECORDED(id, x) is rp_value(id, x): the value the game read, recorded,
   or the recorded one, replayed.

   In use (PLAN-exact-port.md section 8, tasks 4 and 5): the key reads, every value
   GAME.EXE's C reads from outside the program, and what the assembly library reads: the
   mouse reset (OSILIB/MOUSERP.ASM, the replay build's MOUSE.ASM, passes AX and BX through
   rp_value; its polls are replay.c's MOUSE and BUTTONS streams), and the delay loop's
   calibration and the memory size that OSI_lowInit measures (RECORD_VAR in main.c).
   OSILIB/DELAYRP.ASM keeps OSI_delay's tick count out of DGROUP; it records nothing. */
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
#define RV_CPUSPEED     10  /* main, after OSI_lowInit: CpuSpeed, the delay loop calibration */
#define RV_MEMSIZE      11  /*            and D_2FC8, int 12h's memory size in KB (read nowhere) */
#define RV_MOUSE_RESET  12  /* MOUSE_INIT (MOUSERP.ASM): int 33h AX=0, AX (0: no mouse) */
#define RV_MOUSE_NBTN   13  /*            and BX (the number of buttons) */
/* MOUSERP.ASM repeats RV_MOUSE_RESET and RV_MOUSE_NBTN as EQUs (MASM reads no C header). */

/* A recorded value of int size: rp_value takes and gives a long, and Turbo C would push
   that long to a function with no prototype (OSI_randomize), so the hook casts it back.
   RECORDED alone is for a long (farcoreleft). */
#if defined(__TURBOC__) && !defined(REPLAY)
#define RECORDED16(id, x) (x)
#else
#define RECORDED16(id, x) ((int16)RECORDED(id, x))
#endif

/* RECORD_VAR(id, lv): a statement, `RECORD_VAR(RV_CPUSPEED, CpuSpeed);`, for a variable that
   the assembly library set from outside the program (OSI_lowInit's timing loop and int 12h):
   nothing in the DOS build (an empty statement, no code; the gate proves it), and in the
   replay build and the port the variable recorded, or set to its recorded value. */
#if defined(__TURBOC__) && !defined(REPLAY)
#define RECORD_VAR(id, lv)
#else
#define RECORD_VAR(id, lv) ((lv) = RECORDED(id, (lv)))
#endif
