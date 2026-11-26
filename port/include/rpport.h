/* rpport.h: the port's bindings for Exhume's runtime/replay/replay.c, which SRC/rpgame.h
   includes in place of its DOS ones when the compiler is not Turbo C (PLAN-exact-port.md,
   Phase 2). The same streams and the same dump as the replay DOS build, read from the port's
   own C: a session recorded in DOS replays here, call for call, and both write STATE.OUT in
   one format for tools/replay.py compare --cross.

   The names, the stop key, the masks and the order of the sections are rpgame.h's (its long
   comment says what each section holds); what differs is where the port keeps things:

   - the key read is port/sys/kbd.c's KBD_GetKey; the mouse poll is port/sys/mouse.c's
     u6_mouse_poll, which keeps x, y and the buttons in u6_mouse_regs as MOUSERP.ASM keeps
     them in its code segment; the key state and the joystick are bytes nothing changes;
   - the random state is port/sys/rand.c's two words (u6_rand_seed);
   - DS:0 and the vector table are stand-ins (port/sys/rpport.c): the port has neither, and
     tools/replay.py checks NULL in each build alone;
   - the files: REPLAY.IN is the recording given with --dos-replay, STATE.OUT and the others go
     to the port's home folder (rpport.c), where tools/replay.py's run_port looks;
   - the dump (task 1 of Phase 2): GSTA (gamestat.c's packed block, port/game/gamestat_port.c,
     DOS's offsets), OBJS and NPCS (BSS.ASM's arrays in the port's BSS block, port/sys/bss.c,
     and the far blocks in the DOS arena), SEGS (the arena's segments of the same blocks), the
     four far heap sections (the arena, laid out by the same LoadFile and __MemAlloc calls),
     RAND, NULL, CNTS, PAL (the emulated DAC, read through the same ports) and SCRN
     (port/gfx/mcga.c's video memory). DATA, BSS and STAK are not written yet: the port's
     DGROUP is not one block in DOS's layout, and writing it in that layout is task 2.

   RP_FAR_STORAGE as in DOS: replay.c's variables in one structure, its tags as RP_TAG
   numbers, so the code the DOS build runs is the code tested here. */
#ifndef RPPORT_H
#define RPPORT_H

#include "u6port.h"

#ifndef RP_FAR_STORAGE
#define RP_FAR_STORAGE
#endif

#define RP_MAGIC_TAG    RP_TAG('U', '6', 'G', 'R')

int KBD_GetKey(int keepLastOnly);
#define RP_KEY_READ(a)  KBD_GetKey(a)
#define RP_STOP_SCAN    0x43
#define RP_STOP_KEY(r)  ((r) == 0x143)

extern unsigned char u6_rp_keystate[1];
#define RP_KEYSTATE     u6_rp_keystate
#define RP_KEYSTATE_LEN 1

extern int16_t u6_mouse_regs[3];
void u6_mouse_poll(void);
#define RP_MOUSE_READ() u6_mouse_poll()
#define RP_MOUSE_DX     (u6_mouse_regs[0])
#define RP_MOUSE_DY     (u6_mouse_regs[1])
#define RP_BUTTONS_READ() (u6_mouse_regs[2])

extern int16_t u6_rp_joy[4];
#define RP_JOY_READ()   ((void)0)
#define RP_JOYB_READ()  ((void)0)
#define RP_JOY_X        (u6_rp_joy[0])
#define RP_JOY_Y        (u6_rp_joy[1])
#define RP_JOYB_0       (u6_rp_joy[2])
#define RP_JOYB_1       (u6_rp_joy[3])

uint32_t u6_rand_seed(void);
#define RP_RAND_SEED()  u6_rand_seed()

#define RP_NULL_SUM_FROM 0
#define RP_NULL_SUM     0x0DFA

/* main.c's rp_u6_shutdown, as the port has it: the temporary object blocks deleted (they are
   in SAVEGAME, which the runs compare); the music, int 1Ch and the video mode are the port's
   exit's (port_exit, which replay.c's exit is through compat.h) */
void u6_port_rp_shutdown(void);
#define RP_SHUTDOWN()   u6_port_rp_shutdown()

#define RP_TICK_US      54925UL
#define RP_PIT_DIVISOR  0xFFFFu
extern uint32_t u6_rp_clock;            /* read by no hook: Ultima VI calls no GAME_TIME */
#define RP_CLOCK        u6_rp_clock

int u6_rp_open_in(const char *name);
int u6_rp_open_out(const char *name, int flags);
#define RP_OPEN_IN(name)        u6_rp_open_in(name)
#define RP_OPEN_OUT(name, fl)   u6_rp_open_out(name, fl)

/* replay.c's port-only references (Exhume's runtime has them; this port does not yet):
   no enhancements, no cheats, no black box through replay.c */
extern int enhance_on;
char *enhance_names(int on, char *buf, int n);
int cheat_scripted(void);
int bc_open_host(const char *name, int flags, int mode);

/* ---- the dump ---- */

extern unsigned char IsOnQuest, GameStateEnd;
extern unsigned char ObjStatus[], ObjPos[], ObjShapeType[], Amount[], Link[];
extern unsigned char NPCStatus[], STREN[], DEXTE[], INTEL[], ExpPoints[], HitPoints[];
extern unsigned char Names[], Party[], PartySize, MAGIC[], MovePts[], TalkFlags[];
extern unsigned char *Level, *SchedIndex, *NPCMode, *NPCComMode;
extern unsigned char *OrigShapeType, *Leader, *NPCFlag, *PathData;
extern unsigned char *GrMemory, *Mem_BP;
extern uint32_t Mem_Remain;

#define RP_LEVEL        (GrMemory != 0)
#define RP_NSECTIONS(full, level, fb) (3 + ((level) ? 8 : 0) + ((full) ? 2 : 0))

/* rpport.c: the 30 words of SEGS in rpgame.h's order, and the far heap's bounds (linear
   addresses in the arena: FarHeap, the music driver, its end, Mem_BP) */
void u6_rp_segs(uint16_t w[30]);
void u6_rp_heap_bounds(uint32_t b[4]);
unsigned char *u6_rp_lin(uint32_t lin);
unsigned char *mcga_screen(void);

#define RP_U6_GAME() do { \
        section(RP_TAG('G', 'S', 'T', 'A'), (unsigned)(&GameStateEnd - &IsOnQuest)); \
        dump_far(&IsOnQuest, (unsigned)(&GameStateEnd - &IsOnQuest)); \
        section(RP_TAG('O', 'B', 'J', 'S'), 0x7800); \
        dump_far(ObjStatus, 0xC00); dump_far(ObjPos, 0x2400); \
        dump_far(ObjShapeType, 0x1800); dump_far(Amount, 0x1800); dump_far(Link, 0x1800); \
        section(RP_TAG('N', 'P', 'C', 'S'), 0x13F1); \
        dump_far(NPCStatus, 0x100); dump_far(STREN, 0x100); dump_far(DEXTE, 0x100); \
        dump_far(INTEL, 0x100); dump_far(ExpPoints, 0x200); dump_far(HitPoints, 0x100); \
        dump_far(Names, 0xE0); dump_far(Party, 0x10); dump_far(&PartySize, 1); \
        dump_far(Level, 0x100); dump_far(SchedIndex, 0x100); dump_far(NPCMode, 0x100); \
        dump_far(NPCComMode, 0x100); dump_far(MAGIC, 0x100); dump_far(MovePts, 0x100); \
        dump_far(OrigShapeType, 0x200); dump_far(TalkFlags, 0x100); dump_far(Leader, 0x100); \
        dump_far(NPCFlag, 0x100); dump_far(PathData, 0x100); \
    } while (0)

#define RP_U6_SEGS() do { \
        uint16_t rp_w_[30]; int rp_i_; \
        u6_rp_segs(rp_w_); \
        section(RP_TAG('S', 'E', 'G', 'S'), 60); \
        for (rp_i_ = 0; rp_i_ < 30; rp_i_++) dump_word(rp_w_[rp_i_]); \
    } while (0)

#define RP_U6_HEAP(tag, a, b) do { \
        section(tag, (b) > (a) ? (b) - (a) : 0); \
        if ((b) > (a)) dump_far(u6_rp_lin(a), (b) - (a)); \
    } while (0)

#define RP_U6_HEAPS() do { \
        uint32_t rp_b_[4]; \
        u6_rp_heap_bounds(rp_b_); \
        RP_U6_HEAP(RP_TAG('G', 'D', 'R', 'V'), rp_b_[0], rp_b_[1]); \
        RP_U6_HEAP(RP_TAG('M', 'D', 'R', 'V'), rp_b_[1], rp_b_[2]); \
        RP_U6_HEAP(RP_TAG('F', 'A', 'R', 'B'), rp_b_[2], rp_b_[3]); \
        section(RP_TAG('G', 'R', 'M', 'M'), Mem_Remain); \
        dump_far(Mem_BP, Mem_Remain); \
    } while (0)

/* the DAC through the emulated ports, as the DOS build reads it, then the frame */
#define RP_U6_SCREEN() do { \
        int rp_i_; \
        section(RP_TAG('P', 'A', 'L', ' '), 768); \
        outportb(0x3C7, 0); \
        for (rp_i_ = 0; rp_i_ < 768; rp_i_++) { \
            bounce[rp_i_ & 0xFF] = inportb(0x3C9); \
            if ((rp_i_ & 0xFF) == 0xFF) dump_bytes(bounce, 256); \
        } \
        section(RP_TAG('S', 'C', 'R', 'N'), 64000U); \
        dump_far(mcga_screen(), 64000U); \
    } while (0)

#define RP_DUMP(full, level, fb) do { \
        if (level) { RP_U6_GAME(); RP_U6_SEGS(); RP_U6_HEAPS(); } \
        dump_rand(); \
        dump_null(); \
        dump_counts(); \
        if (full) RP_U6_SCREEN(); \
    } while (0)

#endif
