/* The OSI support library (the assembly files of SRC/OSILIB in the DOS build), in C.
 *
 * The game's C calls these routines by the names below. In DOS they were pascal (names
 * uppercase at link level, callee pops) or cdecl far procedures; here they are ordinary C
 * functions. The types follow what the game's C sees (SRC/osi_file.h and mouse.h declare
 * some routines with int16/uint32 types, the others are called without a prototype, so
 * returning int and taking promoted int arguments): routines the game sees as returning
 * int return an int holding the 16-bit result sign-extended, and int parameters are cut
 * to 16 bits inside, as Turbo C's 16-bit int would have them (the host's int arithmetic
 * can pass values like (int)65000). Far pointers are host pointers (into the DOS arena,
 * dosmem.h, when the game built them from segments).
 *
 * docs/port/OSILIB.md describes every routine, its DOS behaviour and how the port does it.
 *
 * The types below are those of SRC/u6.h (union tHandler) and SRC/mouse.h (struct tSurface,
 * struct tMouse). They are only defined here when those headers have not been included,
 * so a game file may include this header after u6.h to get the prototypes.
 */
#ifndef PORT_OSI_H
#define PORT_OSI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game's C is compiled with every struct packed (port/include/compat.h: #pragma
 * pack(1), Turbo C's byte alignment): these copies must be laid out the same way. */
#pragma pack(push, 1)

#ifndef __U6_H__
/* SRC/u6.h: a far code/data pointer, seen as offset:segment words or as a pointer. */
union tHandler {
	struct {
		uint16_t ofs, seg;
	} iii;
	void *p;
	void *(*f)();
};
#endif

#ifndef __MOUSE_H__
/* SRC/mouse.h */
struct tSurface {
	uint16_t segment;
	uint16_t *pScanlines;
	uint16_t right, left, top, bottom;   /* compared as signed numbers by MOUSE.ASM */
};

struct tMouse {
	unsigned char buttonL, buttonR;      /* buttonL = buttons & 1, buttonR = buttons & 2 */
	int16_t posX, posY;
	uint16_t unused06;
	void *shapes;                        /* pointer shape table */
	uint16_t shapeIndex;
	struct tSurface *surface;            /* surface the pointer is drawn on */
	void *saveBuf;                       /* 0x400 bytes background save area */
	void (*onMove)();                    /* called after each position/button change */
	unsigned char installed;
};
#endif

#pragma pack(pop)

/* ==== OSI_FILE.ASM: files, with the error handler protocol ====
 * On an error the routine stores the DOS error code (-1: fewer bytes read/written than
 * asked) in OSI_ErrorCode and its number in OSI_ErrorFunc (1 open, 2 close, 3 create,
 * 4 delete, 5 read, 6 write, 7 rename, 8 copy), then calls OSI_ErrorHandler.f. If the
 * handler clears OSI_ErrorCode the DOS call is retried (open, close, create, delete,
 * rename, each read/write call), otherwise the routine fails. */
extern union tHandler OSI_ErrorHandler;  /* D_032E, initially OSI_callback */
extern int16_t OSI_ErrorCode;            /* D_0332 */
extern int16_t OSI_ErrorFunc;            /* D_0334 */
extern char *OSI_ErrorFile;              /* D_0336: last name given to open/create/delete/rename */

int OSI_callback(void);                  /* the default handler: does nothing */
int16_t OSI_open(char *path);            /* read/write; handle or -1 */
int16_t OSI_close(int16_t handle);       /* 0 or -1 */
int16_t OSI_create(char *path);          /* create/truncate; handle or -1 */
int OSI_delete(char *path);              /* 0 or -1 */
/* offset < 0 (high word negative): no seek, use the current position. Returns the number
 * of bytes read. */
uint32_t OSI_read(int16_t handle, uint32_t offset, uint32_t length, void *buf);
uint32_t OSI_write(int16_t handle, uint32_t offset, uint32_t length, void *buf);
int OSI_rename(char *oldName, char *newName);       /* 0 or -1 */
int OSI_copy(char *src, char *dst);      /* not called by the game */
uint32_t OSI_fileSize(int16_t handle);   /* leaves the file position at the end; 0 on error */
uint32_t OSI_memory(void);               /* largest free DOS block in bytes; not called */
/* The game calls OSI_read as OSI_Read once (console.c): the same routine in DOS. */
uint32_t OSI_Read(int handle, uint32_t offset, uint32_t length, void *buf);

/* ==== KBD.ASM ====
 * 0 when no key; ASCII; or 0x100 + scan code for keys without ASCII, backspace and the
 * numeric keypad (except - and +). keepLastOnly: empty the BIOS buffer, return the last key. */
int KBD_GetKey(int keepLastOnly);

/* ==== INFLATE.ASM + LZ.INC ====
 * Seek fd to offset, read the 4-byte unpacked size and the LZW stream after it and unpack
 * it to dest. workSeg is a DOS segment (dosmem.h) of 0x3400 bytes: hash table (0x3000)
 * then the 1 KB input buffer; the table is cleared at the end. Returns the offset word of
 * the normalised dest pointer (the original returned it in AX). */
int decompress(int16_t fd, uint32_t offset, void *dest, uint16_t workSeg);

/* ==== MOUSE.ASM ==== */
int MOUSE_INIT(struct tMouse *mouse);       /* number of buttons, 0 if no mouse */
int MOUSE_POINTER(void *shapes, uint16_t shapeIndex);
int MOUSE_SERVE(void);                      /* 1 if the state changed (onMove was called) */
int MOUSE_SHOW(void);
int MOUSE_HIDE(void);
int MOUSE_DRAW(void);
int MOUSE_SAVE_BG(void);
int16_t MOUSE_IN_AREA(struct tSurface *area);
/* Uncalled helpers of MOUSE.ASM, kept for completeness. */
int16_t MOUSE_0297(struct tSurface *area);
int16_t MOUSE_02CF(struct tSurface *area, void *shapes, uint16_t shapeIndex, int16_t x, int16_t y);

/* ==== LOW.ASM ==== */
extern uint16_t CpuSpeed;                /* D_2FC6: delay loop iterations per 1/41.67 tick */
extern int16_t OSI_Started;              /* D_2FFC */
extern int16_t TextIdle;                 /* D_2FFE, initially 1 */
extern void (*DriveErrorCallback)();     /* D_3004, initially OSI_nullCallback */
int OSI_selectDrive(int letter);          /* 1 if the drive is now current, else 0 (callback) */
int OSI_getDrive(void);                  /* current drive letter 'A'.. */
void OSI_lowInit(void);

/* ==== DELAY.ASM, DELAY2.ASM ==== */
void OSI_delay_init(void);
void OSI_delay_clean(void);
/* Wait until ticks timer ticks have passed since the last OSI_delay returned. */
void OSI_delay(int ticks);
/* Busy delay of count * (CpuSpeed >> table[speedIndex]) loop iterations. */
void OSI_sDelay(int speedIndex, int count);

/* ==== RAND.ASM ==== */
int OSI_rand(int a, int b);               /* uniform in [min(a,b), max(a,b)] (unsigned) */
int OSI_Rand(int a, int b);               /* spell.c's spelling of OSI_rand */
void OSI_randomize(int seed);
int OSI_timeSeed(void);

/* ==== SOUND1-5.ASM: the PC speaker ==== */
void OSI_nosound(void);
void OSI_sound(int frequency);
void OSI_playNote(int frequency, int duration);
void OSI_playNoise(int step, int length, int maxFrequency);
void OSI_playWavedNote(int increment, int tempo, int count, int threshold, int sweep);

/* ==== port side (not in the DOS library) ==== */

/* SoundFlag is the game's (gamestat.c). */
extern int16_t SoundFlag;

/* The CpuSpeed OSI_lowInit sets (default 1000, a 286/16 class machine). The delays are
 * timed in real time from it, so it only changes their rounding; see OSILIB.md. */
extern uint16_t osi_cpu_speed;

/* The 18.2065 Hz timer tick counter (ticks since plat_init), read through
 * rp_value(RP_TICKS). Every read is a recorded value. */
uint32_t osi_ticks(void);
/* 1 while a replay gives the inputs (rp_mode RP_REPLAY and not run out): no waiting,
 * no live input read, the speaker silent. */
int osi_replaying(void);
/* The same, live, not recorded: for code that only paces itself (no game decision). */
uint32_t osi_ticks_live(void);
/* Called under replay with each tick value the game reads (0: none): lets the session
 * pace a replay (session_pace_tick); without it a replay runs as fast as it can. */
extern void (*osi_replay_tick_hook)(uint32_t tick);

/* Internal to port/sys. */
void osi_wait_tick(uint32_t seen);  /* one pass of a tick wait (seen: the tick read): pump, sleep */
void osi_spin_ns(uint64_t ns);      /* wait ns of real time (busy loops of the original) */
uint64_t osi_loop_ns(uint64_t iterations);  /* duration of delay loop iterations */
uint64_t osi_cxax_loops(uint16_t cx, uint16_t ax);  /* DELAY_CXAX's count (0 = 65536) */
void osi_spin_loops(uint64_t iterations);   /* wait that many delay loop iterations */
extern uint64_t osi_loop_count;     /* delay loop iterations so far (tests) */
void osi_poll_idle(int gotSomething);       /* a key/mouse poll: throttles busy polling */
uint16_t osi_seed_from_time(uint8_t hour, uint8_t minute, uint8_t second, uint8_t hundredths);

/* Test hooks. */
void osi_rand_state(uint16_t *lo, uint16_t *hi);
void osi_rand_set_state(uint16_t lo, uint16_t hi);
int osi_kbd_translate(uint16_t biosKey);
/* called with (0, divisor) for each port 42h write, (1, on) for each gate change and
 * (2, rising edges) for each ~4 ms window of a waved note, (3, on) for each of its steps */
extern void (*osi_sound_trace)(int what, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif
