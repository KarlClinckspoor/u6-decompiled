# U.EXE's OSI library in the port

`U.EXE` (main menu, introduction, character creation) links its own version of Origin's
OSI library, `SRC_U/OSILIB/` (assembly, and four C modules), different from GAME.EXE's
(`docs/port/OSILIB.md`): text windows with word-wrapped and justified printing, surfaces,
lines, the mouse pointer drawn through the graphics driver, palette fades, dissolves, LZW
and whole-file loading, a timer tick with a callback. The port has it in C in
`port/u/osilib/`.

| DOS source | port | contents |
|---|---|---|
| SURFACE.C, SURFSHR.C | `surface.c` | SURF_alloc, SURF_free, SURF_share |
| TXTPORT, TXTCLR, CLREOL, GOTOXY, TXTJUST, TXTCHAR, TXTSCRL.ASM, TXTPRINT.C | `text.c` | TXT_select, TXT_clear, TXT_clearEos, clreol, gotoxy, TXT_puts, TXT_putc, TXT_printf, TXT_sprintf, TXT_sappend, TXT_scrollUp/Down and the internal routines |
| DRAW.ASM | `draw.c` | DRW_line, DRW_lineFunc, DRW_getPixel (not called by U) |
| MOUSEINI, MOUSESRV, MOUSECUR, MOUSEHIT.ASM | `mouse.c` | MOUSE_INIT/NOINIT/POINTER/SERVE/RESET/SHOW/HIDE/DRAW/SAVE_BG/RESTORE_BG, MOUSE_IN_AREA, MOUSE_IN_RECT, SHAPE_IN_RECT, the driver table |
| VMODE, VSYNC, PALETTE.ASM | `video.c` | VID_getMode/setMode, VID_inRetrace/waitRetrace/mouseRetrace/retrace, PAL_set/get/load; presenting, the frame clock |
| FADE.C | `fade.c` | FADE_step, FADE_in, FADE_out |
| DISSOLVE.ASM | `dissolve.c` | DIS_dissolve (MCGA), DIS_dissolveCGA, DIS_dissolveTDY, DIS_dissolveEGA (stub), DIS_farcpy |
| LZW.ASM + LZ.INC, LOADNEAR, LOADFAR.ASM | `load.c` | LZW_load, LOAD_near, LOAD_far, LOAD_to |
| OSI_FILE.ASM | `osifile.c`, `osigame.c` | OSI_open/close/create/delete/read/write/rename/copy/fileSize/memory, on GAME's `port/sys/osi.c` |
| TIMER.C (+ LIBBSS.ASM) | `timer.c` | TIM_install/remove/setCallback/resetCallback/calibrate/delay, the tick poll |
| (Turbo C's conio) | `conio.c` | kbhit, getch |

`uosi.h` declares everything (and the port's helpers, `uosi_*`); `ufar.h` has the far
memory helpers. Test: `python3 port/tests/uosi_diff.py` (see [Tests](#tests)).

## Conventions

* **Names.** The files are compiled like U's own C (`-std=gnu89 -fsigned-char -fwrapv`,
  force-included `port/include/compat.h` then `port/u/uport.h`), so the original names
  written in them (`TXT_clear`, `MOUSE_SERVE`, `pText`, `loopsPerTick`...) become
  `U_TXT_clear`, `U_MOUSE_SERVE`... through `urename.h`: U's library never meets GAME's
  routines of the same name (`OSI_open`, `MOUSE_INIT`, ...). Not renamed: `kbhit` and
  `getch` (Turbo C's names; GAME's port has none), the port's `uosi_*`, and the port/sys
  functions the library calls (`decompress`, `osi_ticks_live`, `osi_poll_idle`,
  `dos_divide_error`, `dosfs_*`, `plat_*`, `vga_dac_*`).
* **Types** are U's (`SRC_U/u.h`: `struct tSurface`, `tText`, `tMouse` packed, far pointers
  as host pointers) with the explicit widths of `SRC_U/portable.h`; registers are `uint16`
  locals, compares signed where the assembly uses JL/JG and unsigned for JB/JA, offsets
  wrap at 64 KB. Routines U's C sees as returning `int` return `int`; where the original's
  AX is not meaningful (MOUSE_POINTER, MOUSE_NOINIT, SURF_alloc...) they return 0.
* **Memory.** Pixels are `segment:offset` in the DOS arena (`port/sys/dosmem.h`; the screen
  is A000:0000). Far pointers into the arena (scanline tables, shapes, the font) are taken
  back as their segment:offset so that offset arithmetic wraps in their segment as on the
  8086 (`ufar.h`); other pointers (U's globals, the host heap) are used as they are. Near
  memory (malloc, calloc) is the host's heap; far memory (farmalloc, farcalloc, farfree) the
  port's Turbo C far heap (compat.h; farcalloc, which compat.h lacks, is farmalloc + clear).
* **The driver.** In DOS the library calls the graphics driver's entries 3 (draw the
  pointer), 6 (save under it), 9 (restore), 12h (a character, register-called) and 0Ch
  (fill a rectangle) through the table `drvDrawPointer...` (D_45BC, 5 far pointers) that
  main.c fills in, and FADE_in calls 0Fh through `gfxDrv`. The port calls `UDRV_3`,
  `UDRV_6`, `UDRV_9`, `UDRV_0x12`, `UDRV_0xc`, `UDRV_0xf` of `port/u/ugfx.c` directly;
  `drvDrawPointer` is kept as the 10 words main.c writes.
* **Inputs are live.** U's keys, mouse and ticks are read from the platform directly, not
  through the recorder (`rp_value`): U runs before a session's recording starts. If U were
  ever to run during a recording, nothing of it would be recorded.

## Text windows

A `struct tText` (selected by TXT_select: `pText`, D_45B2) prints on a surface's window
with a font: word 0 its height, byte 4 + c the width of character c (the glyphs are the
driver's business: entry 12h). Character 13 is a carriage return (to the window's left),
10 a new line; a character that does not fit before the right of the window first goes to
a new line; a new line whose next line would start at or below the window's bottom
scrolls the window up by one text line instead (`TXT_newLine`).

**TXT_puts** prints a line at a time: the first word always goes on the line, then the
words while the cursor stays left of the window's right (`cx < winRight`, unsigned, the
character just added included); the end of the string or a 10 ends the line. The line's
width is summed from its first character to `lineEnd`, which for the last line is past the
0 or the 10, so their widths count. Then by `justify`:

| justify | |
|---|---|
| 0 | as it is |
| 1 | against the window's right when it fits, else from the window's left |
| 2 | centred in the room from the cursor: `curX += (room - width + 1) / 2` when it fits |
| 3 (and any other) | full: the leading spaces and the first word as they are, then the room left (window's right - cursor + 1 - the rest's width) shared between the spaces by an 8-bit DIV: AL pixels each, one more for the first AH; not for the last line of a paragraph (ending with 0 or 10) or a line that does not fit |

After a line that did not end with 0 or 10 comes a new line and the spaces are skipped.
U's console uses justification 3.

**TXT_printf / TXT_sprintf / TXT_sappend** (TXTPRINT.C): `%d %u %D %U %x %c %s` (the
numbers through Turbo C's itoa/ultoa/ltoa, hexadecimal upper case: the port has its own
copies of these four), `%X %Y` set the cursor, `%F %B` the colors, `%P` prints the
window's buffer with TXT_puts; another character after `%` is output as it is (a `%` at the
very end outputs the 0 and goes on reading past it, as in DOS). TXT_printf prints,
TXT_sprintf formats into the window's buffer from its start, TXT_sappend adds to it (each
character followed by a 0). On the host U's `int16` arguments arrive promoted to `int`,
its `int32` as `int32` (`int` on the port's hosts); a call whose arguments do not match its
format (which would shift the words in DOS) cannot be reproduced.

**Clearing**: `TXT_clearLine(x, y)` fills from x to the window's right, `font height + 1`
rows (bottom = y + height), in the background color (driver entry 0Ch on a copy of the
surface, D_45A4). TXT_clear puts the cursor at the window's top left and clears to the end
(`TXT_clrEos`: a text line at a time while y < bottom, the first from the cursor); clreol
clears the cursor's line. `gotoxy` moves the cursor (Origin's CM.LIB had Turbo C's conio
clreol/gotoxy replaced by these).

**Scrolling** copies each scanline of the window (left to right, REP MOVSW + MOVSB) one
text line up or down and clears the line left free. **Quirk:** in DOS this never works.
The line copy (C_1317_00F7) is called near (`PUSH AX / CALL`) but ends with a RETF (it is
inside a FAR procedure), which takes the pushed line number for CS: the first line copied
returns into low memory and U.EXE crashes. So U's texts never reach the bottom of their
windows (no scroll happens); the port scrolls as intended.

A character wider than its window loops for ever in DOS (new line after new line); the
port draws it after the first new line that did not help (logged).

## Surfaces

**SURF_alloc**: `farcalloc(1, width * height)` (a long product of the window's int sizes)
for the pixels and `farmalloc(height * 2)` for the table; scanline i is at offset
`FP_OFF(pixels) + i * width` (16 bits) of `FP_SEG(pixels)`. The table is indexed from 0,
so the window's top must be 0 (U's surfaces are). On failure the original prints
" << No far memory! >> " (or "ytable") on the current text window, waits for a key and goes
on with segment 0; with no table it then writes it at 0000:0000 (the vectors): the port
skips that. **SURF_free** frees the table and `MK_FP(segment, table[0])`. **SURF_share(src,
s1, ..., 0)** gives each surface src's segment and table. U ends the list with a plain int
0, which a 64-bit host may read back as a pointer with garbage in its upper half: a pointer
whose low 32 bits are 0 ends the list (as dosint.c's execl does).

## Lines and pixels (DRAW.ASM)

DRW_line: a horizontal line (y1 == y2) is clipped and filled; any other is Bresenham's,
each point drawn when inside the window. The directions come from the borrow of x1 - x2,
an unsigned compare: a line between a negative and a positive coordinate goes the other
way with a huge count, and the count is tested with DEC/JS, so a count of 8000h or more
draws one point. DRW_lineFunc gives each point to a function (in DOS the line goes on from
the x and y left on the stack, so a plot function that changed its parameters would move
the line; a C function cannot). DRW_getPixel: -1 outside the window. **Quirk:** it reads
the surface's segment through DS after loading DS with the scanline table's segment, which
is right only when the table is in DGROUP: it is for every surface U passes (the screen and
its copies use `scanlineOfs`); the port reads the surface's segment.

## The mouse

As in GAME.EXE, no event handler: the mouse driver (int 33h) is read by MOUSE_SERVE only
(and MOUSE_INIT). MOUSE_SERVE: when a button bit or the position changed since the last
change (`lastX/Y/L/R`, D_45B4; MOUSE_RESET sets lastX to 8001h), the pointer is hidden if
it was shown, the state stored (`buttonL = BX & 1`, `buttonR = BX & 2`), `onMove` called,
the pointer shown again if it was; it returns 1, else 0. MOUSE_INIT stores the structure's
address (`pMouse`), clears it (onMove = MOUSE_NOP), asks the driver (AX=0), and with a mouse
sets `installed`, hides the pointer (`nMouseSem` -1), resets and serves once; it returns the
number of buttons (BX), 0 without a mouse. MOUSE_NOINIT does the same without the driver.

**Showing does not nest** (unlike GAME.EXE's MOUSE.ASM): MOUSE_SHOW acts only when
`nMouseSem` is -1 (saves what is under the pointer, entry 6, then draws it, entry 3);
MOUSE_HIDE only when it is 0: in BIOS mode 13h (the byte at 0040:0049) it waits for the
vertical retrace (VID_inRetrace), then restores (entry 9). MOUSE_POINTER(shapes, i): nothing
without a mouse or when table and index are the same, else hide, change, show.

**Hit tests** (signed compares): MOUSE_IN_RECT: the hot spot is in the window (and a mouse
installed). MOUSE_IN_AREA: the pointer is shown and its shape's box overlaps the window;
the box is the shape header at `shapes + dword[shapes + 4 + 4 * index]` (the sum
normalised), whose words are the extents right, left, up, down from the hot spot.
SHAPE_IN_RECT(s, shapes, i, x, y): that box at (x, y) is entirely inside the window.

Port: MOUSE_SERVE of an installed mouse is a poll point (the screen presented, the
platform pumped, the timer polled), then reads `plat_mouse_state` (game pixels, clamped
to 0..319, 0..199); `plat_mouse_present` stands for int 33h AX=0 and the driver has 2
buttons. The busy-poll throttle of port/sys/timer.c (`osi_poll_idle`) counts the polls that
saw no change. Before MOUSE_INIT (`pMouse` 0) the original would read a structure at DS:0
(C0M's data); the port's routines return 0.

## The screen, the retrace, the palette

The screen is A000:0000 in the arena, shown with the DAC of `port/gfx/vgadac.h`
(`port/u/urun.c` makes them the platform's video source when U sets mode 13h through
DRV(0)). The driver does not tell the platform when it draws, so every poll point and wait
of the library calls `plat_video_dirty()` before pumping; the platform presents at most
once per host frame.

**The retrace.** VGA mode 13h refreshes at 25.175 MHz / (800 · 449) = 70.086 Hz; bit 3 of
port 3DAh (the vertical retrace) is on during the 2-line sync pulse, 64 µs per frame. The
port counts frames on `plat_ms()` (frame f starts at f / 70.086 s):

* `VID_waitRetrace` (wait for the end of the retrace, then the start of the next) waits
  until the next frame starts;
* `VID_inRetrace` (wait until in the retrace) also waits until the next frame starts,
  unless the display is still in the retrace a VID_waitRetrace just ended in: in DOS code
  that does little between the two (GetKey: VID_waitRetrace, then MOUSE_SERVE, whose
  MOUSE_HIDE calls VID_inRetrace) does not lose a frame. The port gives that once, when the
  call comes in the same millisecond of the same frame (its clock has 1 ms steps);
* `VID_retrace` returns 8 under the same condition, else 0; `VID_mouseRetrace` is
  VID_inRetrace when the pointer is shown.

The waits are on the frame grid, not "14.3 ms from now", so a loop paced by them (the
fades, the dissolves, GetKey's blinking cursor, the moongate) runs at 70.086 Hz as long as
a pass takes less than a frame. A wait presents the screen, pumps and polls the timer,
sleeps until a millisecond before the frame and polls through the last one. (If the
platform's present blocks on a host vsync of 60 Hz, a loop that presents every frame runs
at the host's rate.)

`VID_setMode` records the BIOS mode (the library's notion starts at 13h: the port's U
always runs in it) and for 13h clears A000:0000-FFFF; mode 3, at U's exit, shows nothing.

**PAL_set(i, rgb) / PAL_get(i, rgb)**: one DAC register from / into three words (their
low bytes; PAL_get writes high bytes 0), ports 3C8h/3C7h/3C9h → `vga_dac_*`. U also passes
6-byte char arrays. **PAL_load(name)** (not called by U): the 768 bytes at 30h of a palette
file, shifted from 8 to 6 bits, into the DAC 16 colors per vertical retrace; it returns the
high byte of the file's word at 20h, -1 when the file cannot be opened. DOS reads into a
DOS memory block without checking the count; the port's buffer starts cleared.

## Fades (FADE.C)

**FADE_step(cur, target, n)** moves each of n words one step towards its target so that all
arrive in the same number of steps (the largest distance), as the points of Bresenham's
lines: error terms start at steps / 2; a value moves when its error passes the step count.
It returns 0 (and frees its arrays) once arrived. U's moongate uses it to move shapes too.

**FADE_in(src, dst)**: the colors used in src's window (every pixel read), saved; set to
the color of dst's top left pixel (DRW_getPixel), after a vertical retrace; src copied to
dst (entry 0Fh); then FADE_step from that color back to the saved ones, the DAC set after
each vertical retrace. **FADE_out(s, color)**: the colors of s's window fade to the DAC's
color `color`, a step per retrace. A window width or height of 0 reads 65536 (LOOP,
DEC/JNZ). **Quirk:** FADE_out reads s's scanline table as a near pointer (DS:offset), right
for every surface U passes (screen, textArea: `scanlineOfs`); the port reads the table
where it is. 63 steps at most: about 0.9 s.

## Dissolves (DISSOLVE.ASM)

The pixels are copied in the order of a maximal-length linear feedback shift register
(Galois form, shifting right, feedback from the table at D_4966 + 4·n for n bits) with as
many bits as x and y need (2 + floor(log2 width) + floor(log2 height)): its low bits are x,
the bits above it (16 bits) y; values outside the window are skipped, (0, 0), which the
register never gives, is copied last. Both windows have the source's size; the target's
only gives its top left corner. The routine copies the first 200 entries of both surfaces'
scanline tables into its data (D_4626, D_47B6); a window reaching past row 199 reads the
variables after the tables, which the port reproduces by keeping the routine's data as a
byte block at its DOS offsets (reads outside D_4612..D_49E9 give 0).

* `DIS_dissolve` (MCGA): a vertical retrace every count / (7 · speed + 1) pixels (count =
  width · height - 1, the low word; 7 · speed + 1 = 0 is a divide error);
* `DIS_dissolveCGA` (4 pixels a byte) and `DIS_dissolveTDY` (Tandy, 2 a byte): the next
  BIOS tick (int 1Ah) every `speed` pixels: `uosi_wait_tick`;
* `DIS_dissolveEGA` goes through the EGA's planes and latches (ports 3C4h, 3CEh): not
  ported, it does nothing (logged once);
* on a 1x1 window no value is ever inside: DOS loops for ever; the port stops after a whole
  period of the register (logged).

`DIS_farcpy(from, to, n)`: REP MOVSW n / 2, then the odd byte (chargen.c copies the name).

## Loading files

The files are DOS files of the port's file system (`port/sys/dosfs.c`), through plain DOS
calls (no OSI error handler), as in DOS.

* **LZW_load(name)**: open (read/write), the 4-byte header (unpacked size), farmalloc
  `(size >> 4) + 1` paragraphs, unpack. The unpacking is GAME's `decompress()`
  (`port/sys/lzw.c`): LZ.INC is the same in both programs. U's LZW_load lends the decoder
  two buffers of its near heap (the 3000h-byte table, the 1 KB input buffer), freed
  afterwards; decompress() takes one work segment (table at 0, input at +300h), so the port
  lends it 0600:0000 (linear 6000h-93FFh, low memory nothing else uses: DOS itself in a PC).
  0 when the file cannot be opened or its header read. In DOS a failed farmalloc is not
  noticed (the data goes to 0000:0000; the port returns 0, logged), and after a failed open
  handle 0 may be closed (flags left by DOS; the port does not).
* **LOAD_near(name)**: a file smaller than 64 KB into memory it mallocs; 0 for a missing
  file, 64 KB or more, a failed or short read, or an empty file (Turbo C's malloc(0) is
  NULL).
* **LOAD_far(name)**: into memory it farmallocs (the whole size), but one DOS read of the
  size's low 16 bits: a file of 64 KB or more is loaded short (one of exactly 64 KB reads
  nothing and is still a success). **LOAD_to(name, buf)**: the same into buf, returns the
  size's low word (as an int).

## Files (OSI_FILE.ASM)

U.EXE's OSI_FILE.ASM is GAME.EXE's instruction for instruction (only the assembler's
spelling differs), so the port uses `port/sys/osi.c` (its error protocol is described in
`docs/port/OSILIB.md`) through `osigame.c`, the one file that sees GAME's names (it
`#undef`s the renames). U has its own error state, as its own copy of the module had:
`OSI_ErrorHandler`, `OSI_ErrorCode`, `OSI_ErrorFunc`, `OSI_ErrorFile` (U_*) are swapped in
for the duration of each call (`uosi_game_enter/leave`), so that U's errors never reach the
handler GAME's code installs (FileErrorHandler) and GAME's state is left as it was. U never
changes its handler: it stays its OSI_callback, a bare RET, which GAME's code is given as
its own OSI_callback (so that the "AX after the handler" rule is the same).

## Time (TIMER.C)

**The tick.** DOS: `TIM_install` hooks int 1Ch (once: a second call only resets
`timerTicks`); its handler calls `timerHook` (TIM_setCallback; TIM_resetCallback and
TIM_install set an empty one), counts `timerTicks` and chains to the old handler, 18.2065
times a second (1193182 Hz / 65536). U's intro runs the moongate's light (FlashMoongate)
and the stars' twinkling (Twinkle) from it. Port: no interrupts. The ticks are port/sys's
(`osi_ticks_live()`: tick n starts at n · 65536 / 1193182 s on `plat_ms()`), and
`uosi_poll()` runs the handler once for each tick that has come since the last poll. It is
called by every poll point and wait of the library: MOUSE_SERVE, kbhit, getch, the retrace
waits, TIM_delay, the dissolves' tick waits, PAL_load. U's loops reach one of these at
least once a frame (their 70 Hz waits, or their input polls), so a callback runs within a
frame of its tick. After a stall of more than a second (the window dragged, a debugger),
only the last 18 ticks are run. Polls are not re-entered (a callback that polled).

**loopsPerTick.** `TIM_calibrate` installs the timer, waits for tick 2, counts the
iterations of a `while(timerTicks != t) loopsPerTick++` loop during tick 3, and removes the
timer (also when it was installed before, as in DOS). U scales its animations with the
count: `ScaleSpeed(a, b)` (intro.c) goes from b at 1500 to a at 20000 and beyond, the
bedroom's first frames last `loopsPerTick / 400` frames, `Delay(n)` busy-loops
`n · loopsPerTick / 400` times. That loop is 8 instructions (add/adc to a long in memory,
two loads, two compares, two jumps), about 47 cycles on a 286 or 386, about 175 on an 8088:
1500 is an IBM PC (4.77 MHz), 20000 a 286 at 20 MHz or a 386SX/16-386/20 — the fast end of
what Ultima VI was made for in 1990, and where ScaleSpeed stops. The port sets
**loopsPerTick = 20000** (`uosi_loops_per_tick`, `UOSI_LOOPS_PER_TICK` in uosi.h), after
the same wait for the third tick as the original (110 to 165 ms): U then runs everywhere as
on a fast machine of its time, its animations at the speeds its authors chose for one, and
the same on every run. (`Delay()` itself is U's C, a busy loop: on the host it takes no
time; it is the program flow's to give it a duration.)

**TIM_delay(ms)**: `n = (ms / 55) · loopsPerTick` (16-bit division, 32-bit product) iterations
of the same loop as the calibration's, so they last `n / loopsPerTick` ticks: the port
waits `(ms / 55) · 54.925 ms` of real time (TIM_delay(2000): 36 ticks, 1.977 s), polling
the timer at each tick so that the callbacks run during the wait as they did, and leaves
`delayCount = n`. A negative ms counts up to 2^32 + n (hours in DOS, as long in the port).

**Waits** (`uosi_wait_frame`, `uosi_wait_tick`, `uosi_wait_ns`) present, pump and poll
while they sleep; under test `uosi_wait_hook` replaces them.

## Keyboard: kbhit, getch

Turbo C's: `kbhit` is DOS function 0Bh (AL FFh or 0, then CBW: -1 when a character is
there), `getch` DOS function 07h (no echo, no Ctrl-C check; AH = 0). DOS gives the ASCII
byte of the BIOS word; for a key without one (ASCII 0, or E0h, the enhanced keyboard's grey
keys, which int 16h AH=0 turns into 0) it gives 0 and keeps the scan code: the next getch
returns it at once and kbhit says a character is there. Port: the BIOS words of
`plat_key_get`. Both are poll points; a kbhit that finds nothing goes through the busy-poll
throttle (after 200 empty polls in a row each sleeps 1 ms); getch waits for a key, polling
every millisecond. Ctrl-C (2E03h) is the character 3.

## Tests

`python3 port/tests/uosi_diff.py [--quick] [--seed N] [test...]` builds the library (with
U's force-includes), `port/u/ugfx.c`, the port/sys files it uses, `port/tests/osi_stubs.c`
(platform stand-ins: a simulated clock, key queue, mouse) and `port/tests/uosi_shim.c`
into a shared library, and runs the original routines of `U.EXE` (the game folder's,
loaded with its relocations, DS = SS = DGROUP) in unicorn with MCGADRV.BIN (unpacked from
the game folder) at its own segment for the driver entries. Turbo C's heap functions,
getch and kbhit are patched to stand-ins that hand the original the addresses the C got,
so both sides work on the same memory; int 21h (files of the game folder), int 33h, int 1Ah
and the DAC/retrace ports are emulated. The scroll's RETF (above) is turned into a RET in
the emulated image so that scrolling can be compared.

* **TEXT** TXT_puts in every justification, TXT_putc, TXT_printf/sprintf/sappend with every
  format (and `%` followed by another character), TXT_clear, TXT_clearEos, clreol, gotoxy,
  TXT_scrollUp/Down, with the real font U6.SET, on the screen and on a far surface, random
  windows and colors (including the font's own): the pixels, the cursor, colors, the buffer
  and its pointer after each operation;
* **SURF** SURF_alloc (the farcalloc/farmalloc sizes asked, segment, table), SURF_free (what
  is freed), SURF_share; **DRAW** DRW_line with clipping, negative and huge coordinates,
  DRW_getPixel;
* **MOUSE** random sequences of INIT/NOINIT/SERVE/SHOW/HIDE/POINTER/DRAW/SAVE_BG/IN_AREA/
  IN_RECT/SHAPE_IN_RECT on INTRO.PTR and a sweep of every boundary of the hit tests: results,
  screen, save buffer, the structure, nMouseSem, onMove calls;
* **FADE** FADE_step on random vectors; FADE_in/FADE_out: the DAC at every vertical retrace,
  the screen; **DISS** DIS_dissolve, DIS_dissolveCGA, DIS_dissolveTDY on random windows
  (also past row 199) and speeds: the target segment at every wait and at the end;
  DIS_farcpy;
* **LZW** LZW_load of every LZW file of the game folder (data, the size asked, the pointer);
  **LOAD** LOAD_near, LOAD_far; **PAL** PAL_set/PAL_get;

and unit tests of what is bound to the platform, on the simulated clock: **TIMER**
(TIM_calibrate's value and duration, callbacks at poll points, the catch-up limit,
TIM_delay's duration), **VSYNC** (140 retraces in 2 s on the 14/15 ms grid, VID_inRetrace
after VID_waitRetrace), **KEYS** (kbhit/getch with extended and grey keys), **MOUSEP**
(MOUSE_INIT/SERVE on the platform's mouse, clamping, no mouse), **FILES** (U's OSI file
routines, U's error state set, GAME's untouched and its handler not called).

Deliberately broken copies of the C (an off-by-one in centring, `>` for `>=` at the window's
bottom, the hexadecimal upper case, the cleared line's height, the spaces' extra pixel,
Bresenham's error compare, the unsigned direction, the dissolve's count and masks, the
fades' start error and target color, the hit test's extent, the pointer shown again after a
serve, the scanline step, the LZW and LOAD_far allocations, a DAC byte) are each caught.
Not covered by the differential test: real-time behaviour (unit tests on the simulated
clock only), the EGA dissolve.
