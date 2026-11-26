/* rpgame.h: GAME.EXE's bindings for Exhume's runtime/replay/replay.c (its first comment lists
   them), which includes this file. Only the replay DOS build reads it: replay.c compiled with
   GAME's switches and -DRP_FAR_STORAGE, and the sources that use the hooks with -DREPLAY
   (PLAN-exact-port.md, Phase 1). The gate never compiles it. The port links replay.c too, with
   its own bindings (port/include/rpport.h, below). It keeps an 8.3 name, since the DOS build stages it
   (with CRLF line ends: Turbo C 2.0 reads an LF-only header as one line).

   RP_FAR_STORAGE: nothing here may put data in DGROUP (GAME.EXE's near data must not move):
   no string literals, no variables. What the bindings need to keep lives in code segments of
   the replay build's own assembly (OSILIB/MOUSERP.ASM, OSILIB/STACKRP.ASM) and is reached
   through far pointers made from public labels, which Turbo C takes as far functions (a fixup,
   no data). The game's own variables are declared here by name (replay.c does not include
   u6.h); GAME is compiled with -p, so they link by their upper-case names. */
#ifndef RPGAME_H
#define RPGAME_H

/* the port (not Turbo C): the same streams and dump from the port's own C,
   port/include/rpport.h (PLAN-exact-port.md, Phase 2); the rest of this file is the
   replay DOS build's */
#ifndef __TURBOC__
#include "rpport.h"
#else

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

/* The stack check (PLAN-exact-port.md section 9), STACKRP.ASM: at the harness's start (the first
   hook call, main's first statement) u6_stack_fill fills the free stack with zeros (not a pattern:
   any other value changes what uninitialised locals read, and DOSBox-X's memory is zero there), from the near
   heap's break (C0's __brklvl: its copies of the arguments and the environment, EDFAh up to
   about EE9Ah, are left alone) up to 40 bytes below its caller's SP, and keeps where it started
   and that SP in u6_stack_info, two words of its code segment; u6_stack_low answers the lowest
   address written since (the first byte from the fill's start that is not 0, a floor), 0 before the
   fill. The STAK section holds the three. */
extern void cdecl u6_stack_fill(void);
extern unsigned cdecl u6_stack_low(void);
extern void cdecl u6_stack_info(void);
#define RP_START()      u6_stack_fill()

/* The clock. Ultima VI reads no clock through a hook: it never calls GAME_TIME (its only clock is
   the int 1Ch tick that OSI_delay's frame limiter counts, DELAYRP.ASM, and OSI_delay decides
   nothing), runs no timer callback through SLAVE_TIMER and has no SND_READ (the game never reads
   the music driver's state). So replay.c reads RP_CLOCK nowhere that matters (rp_time and
   pit_moment are never called), the dumps' clock field stays 0, and no periodic dump (kind 2,
   every ck_mask ticks of the replayed clock) is ever taken: the dumps are the input ones (each
   key read that gives a key, each change of the buttons; full), the end, and UWRPHOOK's (a full
   dump at each hook call in a range of call counts, to find where two runs part). For the day a
   hook does read it, these describe the clock there is: the BIOS tick count at 0040:006Ch,
   18.2065 Hz, 54925 us, PIT divisor 65536 (FFFFh, which a word holds; the AdLib driver runs the
   PIT at 4DAEh, 60 Hz, while a song plays, and calls the BIOS's handler 18.2 times a second). */
#define RP_TICK_US      54925UL
#define RP_PIT_DIVISOR  0xFFFFu
#define RP_CLOCK        (*(unsigned long far *)MK_FP(0x40, 0x6C))

/* ---- The dump ------------------------------------------------------------------------------

   Each dump's sections, in order (tag, what, length in bytes; DS offsets from GAME.MAP, the same
   in the replay build, which rpbuild checks; "level" is RP_LEVEL, the graphics driver given its
   memory, InitGraphicsData, after which every far block exists; GAME_Load follows at once):

   level only, the game state as a reader of a saved game knows it:
     GSTA  the saved-state block, DS:2C4A..2CCB (gamestat.c: IsOnQuest .. GameStateEnd, which is
           not saved): time, date, karma, map position, moons, quest flags          82h = 130
     OBJS  the object arrays, all C00h objects (the first 100h are the NPCs): ObjStatus (C00h),
           ObjPos (2400h), ObjShapeType (1800h), Amount (1800h), Link (1800h)     7800h = 30720
     NPCS  the rest of a saved game's NPC tables, with the party, in OBJLIST's order (GAME_Save):
           NPCStatus, STREN, DEXTE, INTEL, ExpPoints (200h), HitPoints, Names (E0h), Party
           (10h), PartySize (1), *Level, *SchedIndex, *NPCMode, *NPCComMode, MAGIC, MovePts,
           *OrigShapeType (200h), TalkFlags, *Leader, *NPCFlag, *PathData (100h each unless
           said; * a far block). With OBJS's first 100h ObjStatus, 300h ObjPos, 200h ObjShapeType
           and 200h Amount, and GSTA, it is a saved game's OBJLIST byte for byte       13F1h = 5105
   always, the whole of DGROUP's data:
     DATA  DS:0000..322D, _DATA (the initialised data: C0's, the game's, the C library's) and
           _EMUSEG, _CRTSEG; section offset = DS offset                           322Eh = 12846
     BSS   DS:322E..EDF9, _BSS (BSS.ASM, the uninitialised data, zeroed by C0); section offset
           + 322Eh = DS offset                                                     BBCCh = 48076
   level only, the far heap (main takes it whole with farmalloc(farcoreleft()), and LoadFile
   and __MemAlloc hand it out in this order; offsets within a section follow from the files'
   sizes and so are the same in every session with the same game data and CONFIG.U6):
     SEGS  the segment of each far pointer the game keeps, as words (below), for tools/replay.py's
           [replay.segments]                                                       2 * 30 = 60
     GDRV  FarHeap up to the music driver: the mouse's save area (400h) and pointer shapes
           (800h), then, from a paragraph, the graphics driver U6xxx.DRV (its code and its
           variables, GrInfo's struct tGr among them)
     MDRV  the music driver U6xxx.DRV, up to the next block (with VGA, the palette; otherwise
           the font): its code and its variables, which its int 8 handler changes (masked,
           below); empty without a music card
     FARB  the rest of the game's blocks, up to Mem_BP: VGAPalette (300h, VGA only), the font
           U6.CH, TerrainType, TileFlag, BaseTile, TypeWeight, TileFlag2, ScratchBuf (7900h, the
           map view's back buffer and LZW work area), Schedule, NPCMode, NPCComMode, SchedIndex,
           Leader, NPCFlag, OrigShapeType, Level, PathData, ChunkIndexBuf (600h), TalkBuf
           (2800h, conversations and the song playing)
     GRMM  the graphics driver's memory, the rest of the far heap from Mem_BP (Mem_Remain bytes,
           GR_INIT_MEMORY): its tile cache
     (with VGA, mouse, AdLib and DOSBox-X's farcoreleft() of 55408h: GDRV 16888, MDRV 1662,
     FARB 60098, GRMM 270544 bytes)
   always:
     STAK  the stack check (STACKRP.ASM): the fill's start, its caller's SP (the fill ends
           40 bytes below), the lowest address written so far (each a word)        6
     RAND, NULL, CNTS   replay.c's (RAND: D_3088 and D_308A)
   in a full dump (in Ultima VI every dump is one: the input ones, the end, UWRPHOOK's):
     PAL   the DAC, 768 six-bit values, as replay.c's dump_screen reads it
     SCRN  the 320x200 frame, A000:0000, 64000 bytes, one byte a pixel, rows of 320 (mode 13h)

   The screen: mode 13h is chain-4, where a read of A000:x gives plane x & 3 at x whatever the
   read map says; so replay.c's dump_screen (the four planes through the read map, PAL, CRTC)
   gets the same 64 KB four times (seen in task 5's dumps), and tools/replay.py's scanout, which
   takes the planes as unchained (mode X, Underworld's), would make no picture of them. SCRN is
   the frame as the game draws it (Screen.segment A000h, rows of 140h), a quarter of the size,
   independent of how the VGA (DOSBox-X's, a real one, the port's) lays chain-4 out in its
   planes, and the same as a port's 320x200 frame buffer. The CRTC is not dumped: the game never
   changes it (no scrolling, no split). The first dumps come before the graphics driver sets
   mode 13h (MOUSE_INIT's first button read): their SCRN is what A000:0000 holds in text mode.

   SEGS, the words in order: DGROUP (_DS), FarHeap, Mem_BP, GrDriver, MUS_Driver, GrInfo,
   GrInfo->font, Mouse.shapes, Mouse.saveBuf, PointerShapes, VGAPalette, TerrainType, TileFlag,
   BaseTile, TypeWeight, TileFlag2, ScratchBuf, MapSurface.segment, Schedule, NPCMode,
   NPCComMode, SchedIndex, Leader, NPCFlag, OrigShapeType, Level, PathData, ChunkIndexBuf,
   TalkBuf, GrMemory. Everything after DGROUP is a far heap block (heap_block 1, FarHeap);
   DGROUP, and the code segments that the game keeps in far function pointers (Mouse.onMove,
   OSI_ErrorHandler, GrInfo's callbacks), move with the load segment (exe_range).

   Masks, for tools/replay.py's [[replay.mask]] (task 7 puts them in exhume.toml): replay.c's
   convention is to dump every byte as it is and let replay.py skip the ranges that are the
   program's machinery rather than game state; none of them is zeroed here. As TOML ranges
   (lo inclusive, hi exclusive):
     DATA lo 0x98  hi 0x9C  always   C0's __STARTTIME, the BIOS tick count at start-up (for
                              clock(), which GAME does not link, so nothing reads it); a
                              different value in every run. Recording it as a VALUE would
                              make it deterministic, at the price of a hook in main.c and a
                              stand-in in the port for a dead variable; masked instead
     MDRV lo 0x0C  hi 0x162 always   the AdLib driver's variables (port/sys/adlib.c names them:
                              the repeat stack, the song position, each channel's frequency,
                              vibrato and slides, the ticks to the next commands, the
                              instruments, its busy flag and PIT sum), which its int 8 handler
                              steps 60 times a second whenever it comes; seen to differ at
                              D2..DB, E8..EB, 127..12A, 139, 160..161. Not masked: its code,
                              its tables and the saved int 8 vector at 15Ah (written by play,
                              at the game's call). Another music card's driver has its own
                              layout: these offsets are U6ADLIB.DRV's
     STAK lo 4     hi 6     always   the lowest stack address written: interrupts (the timer,
                              the music driver's int 8, the keyboard) push onto the game's
                              stack wherever its SP is when they come, and recording and
                              replaying take different paths through the harness, so this can
                              differ between runs; a measurement, not state
   Not masked, and why they need not be: the IVT's int 8 and 1Ch vectors (set by the music
   driver's play and stop and by OSI_delay_init, at the game's own calls); OSI_delay's tick count
   (DELAYRP.ASM keeps it out of DGROUP, D_305A stays 0); the stack-junk byte (STACK_JUNK_SET);
   LOW's calibration counter D_2FCA (always ends at 1). */

/* the game's variables the dump reads (u6.h, gamestat.h, BSS.ASM; their DS offsets above) */
extern unsigned char IsOnQuest, GameStateEnd;
extern unsigned char ObjStatus[], ObjPos[], ObjShapeType[], Amount[], Link[];
extern unsigned char NPCStatus[], STREN[], DEXTE[], INTEL[], ExpPoints[], HitPoints[];
extern unsigned char Names[], Party[], PartySize, MAGIC[], MovePts[], TalkFlags[];
extern unsigned char far *Level, far *SchedIndex, far *NPCMode, far *NPCComMode;
extern unsigned char far *OrigShapeType, far *Leader, far *NPCFlag, far *PathData;
extern unsigned char far *FarHeap, far *Mem_BP, far *GrInfo, far *PointerShapes, far *VGAPalette;
extern unsigned char far *TerrainType, far *TileFlag, far *BaseTile, far *TypeWeight;
extern unsigned char far *TileFlag2, far *ScratchBuf, far *Schedule, far *ChunkIndexBuf;
extern unsigned char far *TalkBuf, far *GrMemory;
extern unsigned long Mem_Remain;
extern unsigned GrDriver[2], MUS_Driver[2];     /* union tHandler: the entry's offset, the segment */
extern unsigned char Mouse[];                   /* struct tMouse: shapes at 08h, saveBuf at 11h */
extern unsigned MapSurface[];                   /* struct tSurface: its segment first */

#define RP_LEVEL        (GrMemory != 0)
#define RP_NSECTIONS(full, level, fb) (6 + ((level) ? 8 : 0) + ((full) ? 2 : 0))

/* a far pointer's 20-bit address, and back (main.c's PTR2NORM and NORM2PTR) */
#define RP_U6_LIN(p)    (((uint32)FP_SEG(p) << 4) + FP_OFF(p))
#define RP_U6_PTR(l)    ((unsigned char far *)MK_FP((unsigned)((l) >> 4), (unsigned)(l) & 15))
#define RP_U6_SEG(p)    FP_SEG(p)
#define RP_U6_WSEG(base, off) (*(unsigned far *)((unsigned char far *)(base) + (off) + 2))

/* near bytes, or a far block, to the dump */
#define RP_U6_NEAR(v, n) dump_far((unsigned char far *)(v), n)
#define RP_U6_FAR(p, n)  dump_far((unsigned char far *)(p), n)

/* a section of the far heap from linear address a to b (empty if b <= a) */
#define RP_U6_HEAP(tag, a, b) do { \
        section(tag, (b) > (a) ? (b) - (a) : 0); \
        if ((b) > (a)) dump_far(RP_U6_PTR(a), (b) - (a)); \
    } while (0)

/* the words of SEGS, through replay.c's bounce buffer (free while a dump is written) */
#define RP_U6_NSEGS     30
#define RP_U6_W(i, w)   (((unsigned RP_FAR *)bounce)[i] = (w))

#define RP_U6_GAME() do { \
        section(RP_TAG('G', 'S', 'T', 'A'), (unsigned)&GameStateEnd - (unsigned)&IsOnQuest); \
        RP_U6_NEAR(&IsOnQuest, (unsigned)&GameStateEnd - (unsigned)&IsOnQuest); \
        section(RP_TAG('O', 'B', 'J', 'S'), 0x7800); \
        RP_U6_NEAR(ObjStatus, 0xC00); RP_U6_NEAR(ObjPos, 0x2400); \
        RP_U6_NEAR(ObjShapeType, 0x1800); RP_U6_NEAR(Amount, 0x1800); RP_U6_NEAR(Link, 0x1800); \
        section(RP_TAG('N', 'P', 'C', 'S'), 0x13F1); \
        RP_U6_NEAR(NPCStatus, 0x100); RP_U6_NEAR(STREN, 0x100); RP_U6_NEAR(DEXTE, 0x100); \
        RP_U6_NEAR(INTEL, 0x100); RP_U6_NEAR(ExpPoints, 0x200); RP_U6_NEAR(HitPoints, 0x100); \
        RP_U6_NEAR(Names, 0xE0); RP_U6_NEAR(Party, 0x10); RP_U6_NEAR(&PartySize, 1); \
        RP_U6_FAR(Level, 0x100); RP_U6_FAR(SchedIndex, 0x100); RP_U6_FAR(NPCMode, 0x100); \
        RP_U6_FAR(NPCComMode, 0x100); RP_U6_NEAR(MAGIC, 0x100); RP_U6_NEAR(MovePts, 0x100); \
        RP_U6_FAR(OrigShapeType, 0x200); RP_U6_NEAR(TalkFlags, 0x100); RP_U6_FAR(Leader, 0x100); \
        RP_U6_FAR(NPCFlag, 0x100); RP_U6_FAR(PathData, 0x100); \
    } while (0)

#define RP_U6_DGROUP() do { \
        section(RP_TAG('D', 'A', 'T', 'A'), 0x322E); \
        dump_far((unsigned char far *)MK_FP(_DS, 0), 0x322E); \
        section(RP_TAG('B', 'S', 'S', ' '), 0xBBCC); \
        dump_far((unsigned char far *)MK_FP(_DS, 0x322E), 0xBBCC); \
    } while (0)

#define RP_U6_SEGS() do { \
        RP_U6_W(0, _DS); RP_U6_W(1, RP_U6_SEG(FarHeap)); RP_U6_W(2, RP_U6_SEG(Mem_BP)); \
        RP_U6_W(3, GrDriver[1]); RP_U6_W(4, MUS_Driver[1]); RP_U6_W(5, RP_U6_SEG(GrInfo)); \
        RP_U6_W(6, RP_U6_WSEG(GrInfo, 6)); RP_U6_W(7, RP_U6_WSEG(Mouse, 8)); \
        RP_U6_W(8, RP_U6_WSEG(Mouse, 0x11)); RP_U6_W(9, RP_U6_SEG(PointerShapes)); \
        RP_U6_W(10, RP_U6_SEG(VGAPalette)); RP_U6_W(11, RP_U6_SEG(TerrainType)); \
        RP_U6_W(12, RP_U6_SEG(TileFlag)); RP_U6_W(13, RP_U6_SEG(BaseTile)); \
        RP_U6_W(14, RP_U6_SEG(TypeWeight)); RP_U6_W(15, RP_U6_SEG(TileFlag2)); \
        RP_U6_W(16, RP_U6_SEG(ScratchBuf)); RP_U6_W(17, MapSurface[0]); \
        RP_U6_W(18, RP_U6_SEG(Schedule)); RP_U6_W(19, RP_U6_SEG(NPCMode)); \
        RP_U6_W(20, RP_U6_SEG(NPCComMode)); RP_U6_W(21, RP_U6_SEG(SchedIndex)); \
        RP_U6_W(22, RP_U6_SEG(Leader)); RP_U6_W(23, RP_U6_SEG(NPCFlag)); \
        RP_U6_W(24, RP_U6_SEG(OrigShapeType)); RP_U6_W(25, RP_U6_SEG(Level)); \
        RP_U6_W(26, RP_U6_SEG(PathData)); RP_U6_W(27, RP_U6_SEG(ChunkIndexBuf)); \
        RP_U6_W(28, RP_U6_SEG(TalkBuf)); RP_U6_W(29, RP_U6_SEG(GrMemory)); \
        section(RP_TAG('S', 'E', 'G', 'S'), 2 * RP_U6_NSEGS); \
        dump_bytes(bounce, 2 * RP_U6_NSEGS); \
    } while (0)

/* the far heap's sections: the music driver's block starts at its segment (LoadFile aligns it to
   a paragraph) and ends at the next block, VGAPalette with VGA, else the font (GrInfo->font) */
#define RP_U6_MDRV_END  (VGAPalette ? RP_U6_LIN(VGAPalette) : \
                         RP_U6_LIN(*(unsigned char far * far *)(GrInfo + 6)))
#define RP_U6_MDRV      (MUS_Driver[1] ? (uint32)MUS_Driver[1] << 4 : RP_U6_MDRV_END)
#define RP_U6_HEAPS() do { \
        RP_U6_HEAP(RP_TAG('G', 'D', 'R', 'V'), RP_U6_LIN(FarHeap), RP_U6_MDRV); \
        RP_U6_HEAP(RP_TAG('M', 'D', 'R', 'V'), RP_U6_MDRV, RP_U6_MDRV_END); \
        RP_U6_HEAP(RP_TAG('F', 'A', 'R', 'B'), RP_U6_MDRV_END, RP_U6_LIN(Mem_BP)); \
        section(RP_TAG('G', 'R', 'M', 'M'), Mem_Remain); \
        dump_far(Mem_BP, Mem_Remain); \
    } while (0)

#define RP_U6_STAK() do { \
        RP_U6_W(0, ((unsigned far *)u6_stack_info)[0]); \
        RP_U6_W(1, ((unsigned far *)u6_stack_info)[1]); \
        RP_U6_W(2, u6_stack_low()); \
        section(RP_TAG('S', 'T', 'A', 'K'), 6); \
        dump_bytes(bounce, 6); \
    } while (0)

/* the DAC as replay.c's dump_screen reads it (the game never reads it, so its read index is
   ours), then the frame */
#define RP_U6_SCREEN() do { \
        int rp_i_; \
        section(RP_TAG('P', 'A', 'L', ' '), 768); \
        outportb(0x3C7, 0); \
        for (rp_i_ = 0; rp_i_ < 768; rp_i_++) { \
            bounce[rp_i_ & 0xFF] = inportb(0x3C9); \
            if ((rp_i_ & 0xFF) == 0xFF) dump_bytes(bounce, 256); \
        } \
        section(RP_TAG('S', 'C', 'R', 'N'), 64000U); \
        dump_far((unsigned char far *)MK_FP(0xA000, 0), 64000U); \
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

#endif /* __TURBOC__ */

#endif
