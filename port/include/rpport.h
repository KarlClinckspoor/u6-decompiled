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
   - the files (rpport.c, chosen by port/sys/session.c): REPLAY.IN is the recording given with
     --replay or --dos-replay; RECORD.OUT and STATE.OUT go to --record's folder, else to the
     folder the game runs in (the home folder, where tools/replay.py's run_port looks); the
     black box's RECORD.OUT goes to its session folder, by replay.c's rp_blackbox_name;
   - the dump (task 1 of Phase 2): GSTA (gamestat.c's packed block, port/game/gamestat_port.c,
     DOS's offsets), OBJS and NPCS (BSS.ASM's arrays in the port's BSS block, port/sys/bss.c,
     and the far blocks in the DOS arena), SEGS (the arena's segments of the same blocks), the
     four far heap sections (the arena, laid out by the same LoadFile and __MemAlloc calls),
     RAND, NULL, CNTS, PAL (the emulated DAC, read through the same ports) and SCRN
     (port/gfx/mcga.c's video memory); DATA and BSS, DGROUP in DOS's layout, written from the
     port's globals by the table port/tools/gen_dgroup.py makes (port/game/dgroup_port.c,
     u6_rp_dgroup in rpport.c; task 2), every pointer in DOS's form; GDRV with the graphics
     driver's segment as DOS's driver keeps it (mcga.c's drv[], the tGr block in DOS's layout:
     u6_rp_gdrv); STAK, six zero bytes (the
     DOS build's stack measure, which the port has no counterpart for: masked in a cross
     comparison).

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

/* replay.c's port-only references: the session's fixes (port/sys/fixes.h: a recording made with
   any carries their names, format 5), no cheats (test set-ups go through the saved game instead) */
#include <stdint.h>
extern uint32_t enhance_on;
char *enhance_names(uint32_t mask, char *buf, int n);
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
#define RP_NSECTIONS(full, level, fb) (6 + ((level) ? 8 : 0) + ((full) ? 2 : 0))

/* rpport.c: the 30 words of SEGS in rpgame.h's order, and the far heap's bounds (linear
   addresses in the arena: FarHeap, the music driver, its end, Mem_BP) */
void u6_rp_segs(uint16_t w[30]);
void u6_rp_heap_bounds(uint32_t b[4]);
unsigned char *u6_rp_lin(uint32_t lin);
unsigned char *mcga_screen(void);
const unsigned char *u6_rp_dgroup(void);    /* DS:0..EDF9 in DOS's layout */
/* GDRV with the graphics driver's segment as DOS's driver keeps it (mcga.c's drv[], the tGr
   block in DOS's layout at CS:0052) */
const unsigned char *u6_rp_gdrv(uint32_t a, uint32_t b);

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
        section(RP_TAG('G', 'D', 'R', 'V'), rp_b_[1] > rp_b_[0] ? rp_b_[1] - rp_b_[0] : 0); \
        if (rp_b_[1] > rp_b_[0]) \
            dump_far((unsigned char *)u6_rp_gdrv(rp_b_[0], rp_b_[1]), rp_b_[1] - rp_b_[0]); \
        RP_U6_HEAP(RP_TAG('M', 'D', 'R', 'V'), rp_b_[1], rp_b_[2]); \
        RP_U6_HEAP(RP_TAG('F', 'A', 'R', 'B'), rp_b_[2], rp_b_[3]); \
        section(RP_TAG('G', 'R', 'M', 'M'), Mem_Remain); \
        dump_far(Mem_BP, Mem_Remain); \
    } while (0)

/* the DAC and the frame as the DOS build reads them (rpport.c's u6_rp_screen: in mode 13h the
   DAC through the emulated ports and A000:0000; before the game sets it, text mode 3's) */
const unsigned char *u6_rp_screen(unsigned char pal[768]);
#define RP_U6_SCREEN() do { \
        static unsigned char rp_pal_[768]; \
        const unsigned char *rp_scr_ = u6_rp_screen(rp_pal_); \
        section(RP_TAG('P', 'A', 'L', ' '), 768); \
        dump_bytes(rp_pal_, 768); \
        section(RP_TAG('S', 'C', 'R', 'N'), 64000U); \
        dump_far((unsigned char *)rp_scr_, 64000U); \
    } while (0)

#define RP_U6_DGROUP() do { \
        const unsigned char *rp_dg_ = u6_rp_dgroup(); \
        section(RP_TAG('D', 'A', 'T', 'A'), 0x322E); \
        dump_far((unsigned char *)rp_dg_, 0x322E); \
        section(RP_TAG('B', 'S', 'S', ' '), 0xBBCC); \
        dump_far((unsigned char *)rp_dg_ + 0x322E, 0xBBCC); \
    } while (0)

#define RP_U6_STAK() do { \
        section(RP_TAG('S', 'T', 'A', 'K'), 6); \
        dump_word(0); dump_word(0); dump_word(0); \
    } while (0)

#define RP_DUMP(full, level, fb) do { \
        if (level) RP_U6_GAME(); \
        RP_U6_DGROUP(); \
        if (level) { RP_U6_SEGS(); RP_U6_HEAPS(); } \
        RP_U6_STAK(); \
        dump_rand(); \
        dump_null(); \
        dump_counts(); \
        if (full) RP_U6_SCREEN(); \
    } while (0)

#endif
