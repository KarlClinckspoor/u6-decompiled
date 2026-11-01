# OSILIB in the port

In DOS, Ultima VI's `GAME.EXE` links a small assembly support library, `SRC/OSILIB/*.ASM`
(files, keyboard, LZW unpacking, mouse, drives, timer, random numbers, PC speaker). The
port replaces it with C in `port/sys/`, together with the few DOS services the game's C
calls directly (`int86`, `intdos`, `outportb`, `farmalloc`, `execl`, `exit`).

| DOS source | port | contents |
|---|---|---|
| OSI_FILE.ASM | `osi.c` (on `dosfs.c`) | OSI_open/close/create/delete/read/write/rename/copy/fileSize/memory, error handler protocol |
| LOW.ASM | `osi.c` | OSI_selectDrive, OSI_getDrive, OSI_lowInit, CpuSpeed and the other globals |
| KBD.ASM | `kbd.c` | KBD_GetKey |
| INFLATE.ASM + LZ.INC | `lzw.c` | decompress |
| MOUSE.ASM | `mouse.c` | MOUSE_INIT/POINTER/SERVE/SHOW/HIDE/DRAW/SAVE_BG/IN_AREA |
| DELAY.ASM, DELAY2.ASM | `timer.c` | OSI_delay_init/clean, OSI_delay, OSI_sDelay, the tick counter |
| RAND.ASM | `rand.c` | OSI_rand, OSI_randomize, OSI_timeSeed |
| SOUND1-5.ASM, SOUND.INC | `sound.c` | OSI_sound, OSI_nosound, OSI_playNote, OSI_playNoise, OSI_playWavedNote |
| (C library, BIOS, DOS) | `dosint.c`, `dosmem.c`, `dosfs.c` | int86/int86x/intdos/intdosx/segread, outportb/inportb, execl, the DOS arena and far heap, the file system |

Prototypes: `port/sys/osi.h` (library), `dosint.h`, `dosfs.h`, `dosmem.h`. Tests:
`port/tests/run_osilib_tests.sh` (see [Tests](#tests)).

## Conventions

* **Names and types.** In DOS the routines were `pascal` (callee pops, names upper-cased at
  link time: `OSI_Read` and `OSI_Rand` in the game are the same routines as `OSI_read`,
  `OSI_rand`, so both spellings exist) or `cdecl`. Here they are plain C functions with the
  types the game's C sees: the explicit types of `SRC/osi_file.h` and `SRC/mouse.h`
  (`int16`, `uint32`), and `int` results / `int` parameters for the routines the game calls
  without a prototype or declares with an implicit `int`. Such `int` parameters are cut to
  16 bits inside (`(uint16_t)x`), as Turbo C's 16-bit `int` held them: the host's 32-bit
  arithmetic can pass e.g. `(int)65000` to `OSI_playWavedNote`, which must act as -536.
  Results the game sees as `int` hold the 16-bit result sign-extended.
* **Structures.** The game's C is compiled with `#pragma pack(1)` (Turbo C's byte
  alignment, `port/include/compat.h`), so `osi.h`'s copies of `struct tMouse` (43 bytes on
  a 64-bit host) and `struct tSurface` (18 bytes) are packed too.
* **Far pointers** are host pointers; those the game builds from segments point into the
  DOS arena (`dosmem.h`). Offset arithmetic inside a segment that the original did in
  16 bits (the LZW table) is done with `dos_seg_ptr(seg, (uint16_t)off)`.
* **Recorded inputs.** Every value read from the outside world goes through
  `rp_value(stream, live)` (`port/replay`): timer ticks (RP_TICKS), keys (RP_KEY), mouse
  x/y/buttons (RP_MOUSE_*), mouse presence (RP_MISC), the random seed (RP_SEED), DOS
  date/time (RP_DATE). `osi_replaying()` (a replay that has not run out) means: read no
  live input, never sleep, keep the speaker silent.
* **Divide errors.** Some routines divide in 16 bits; where the quotient overflows the
  8086 raised a divide error and Turbo C's startup code ended the program ("Divide
  error"). The port does the same (`dos_divide_error` → `plat_fatal`). It happens only on
  arguments the game never passes (a frequency below 19 Hz, `OSI_playNoise` with a maximum
  of 99, `CpuSpeed` >= 6144).

## Time

### The tick counter

DOS: channel 0 of the PIT interrupts 1193182/65536 = 18.2065 times a second; int 8 calls
int 1Ch. `OSI_delay_init` hooks int 1Ch with a handler that increments the word `D_305A`.

Port: tick *n* starts at n·65536/1193182 s after `plat_init` (from `plat_ms()`).
`osi_ticks()` reads it through `rp_value(RP_TICKS)`: one recorded value per read. When a
replay hands over to the live clock (the recording ran out, or `rp_take_over`), the count
goes on from the last replayed tick (an offset is kept) instead of jumping to the new
session's clock. Under replay each tick read is passed to `osi_replay_tick_hook`
(portmain sets `session_pace_tick`, which can pace a replay); without the hook a replay
runs as fast as the host can.

### OSI_delay(n)

DOS: spins until `D_305A >= n` (unsigned), then sets `D_305A = 0`. So it waits for *n*
ticks **counted from the end of the previous OSI_delay**, and returns at once when the game
was slower than that: a frame limiter. The counter is 16 bits (wraps after an hour).
`OSI_delay_clean` restores the old int 1Ch vector, which freezes the counter: an
OSI_delay after it hangs the DOS game (it is only called on the way out).

Port: OSI_delay remembers the tick at which it last returned; each pass of its loop reads
the counter once (`osi_ticks`) and compares `(uint16_t)(t - base) >= (uint16_t)n`. A pass
that must wait calls `osi_wait_tick(t)`: `plat_pump()`, then (live only) sleeps until 1 ms
before the next tick is due and busy-waits the last millisecond; if the tick arrived during
the pump it does not sleep. Under replay it only pumps. After `OSI_delay_clean` the
counter is frozen as in DOS; a wait that could never end pumps and sleeps until the user
quits (`dos_exit`). The game calls OSI_delay(1) once per animation frame
(`MUS_PlayAmbient`, the prompt loop), OSI_delay(30) after the requiem.

### Busy delay loops: OSI_sDelay and the sound routines

DOS: `DELAY_CXAX` (SOUND.INC) runs `dec word [lo] / jnz` AX times inside a CX loop (0
standing for 65536). It is calibrated by `CpuSpeed` (D_2FC6): `OSI_lowInit` counts the
iterations N of a loop of the same kind during one tick and sets `CpuSpeed = N*18/750`
(that MUL/DIV itself overflows — a divide error — on machines doing more than 2.7 million
iterations a tick, i.e. fast 486s and up). One iteration therefore lasts
1 tick · 18 / (750 · CpuSpeed), and the routines are written in those units:

| call | iterations | duration |
|---|---|---|
| `OSI_sDelay(i, n)` | n · (CpuSpeed >> shift[i]) | n · 1.318 ms >> shift |
| `OSI_playNoise` step s | s · (CpuSpeed >> 4) | s · 82 µs |
| `OSI_playWavedNote` step, tempo t | t · (CpuSpeed / 24) | t · 54.9 µs (1/1000 tick) |

`OSI_sDelay`'s shift table `D_3066` is a table of words indexed with a *byte* offset
(index 1 reads shift 0, index 8 reads shift 2: the game uses 1 and 8).

Port: `OSI_lowInit` sets `CpuSpeed = osi_cpu_speed` (default 1000, a 286/16 class
machine) instead of measuring it, so that it is the same on every run. The loops count
their iterations exactly as the original (`osi_cxax_loops`) and wait the corresponding real
time (`osi_spin_loops` → `osi_spin_ns`), so their durations do not depend on the chosen
CpuSpeed except for rounding (and the quirks: CpuSpeed < 100 gives waved notes no delay,
3072..6143 a negative `CBW` tempo, >= 6144 a divide error, all reproduced).
`osi_spin_ns` keeps a running target time: consecutive short delays (noise steps of
82 µs) accumulate and are waited for when the target is ahead of `plat_ms()`; after a pause
(more than 20 ms behind) it restarts from now. It sleeps while more than 3 ms remain,
busy-waits the rest, and calls `plat_pump()` at most once per host millisecond. These loops
read nothing the game decides on, so they are not recorded; under replay they return at
once.

### Busy polling

Some loops poll the keyboard without waiting (`while(!(key = KBD_GetKey(1)));` in
FileErrorHandler). DOS spun at full speed. The port counts polls that found nothing with no
wait in between; after 200 of them each further poll sleeps 1 ms (not under replay). This
only stretches time that the game spends spinning anyway.

## Keyboard: KBD_GetKey(keepLastOnly)

DOS: int 16h AH=1 tests the BIOS buffer, AH=0 takes a key (scan code in AH, ASCII in AL).
With `keepLastOnly` != 0 every key in the buffer is taken and the last one kept (so key
repeat does not pile up), otherwise at most one. Translation:

| BIOS word | result |
|---|---|
| 0000h (no key, or Ctrl-Break's 0000h) | 0 |
| AL = 0 (extended key) | 100h + scan code |
| scan 47h..51h (keypad) except 4Ah (-) and 4Eh (+) | 100h + scan code (keypad digits become directions) |
| scan 0Eh (backspace) | 10Eh |
| otherwise | AL |

Port: the BIOS words come from `plat_key_get()`. Each call is a poll point:
`plat_pump()`, then the word KBD_GetKey ends up with (0 when the buffer was empty) goes
through `rp_value(RP_KEY)` — one value per call — then the same translation. Under replay
the host's keyboard is not read.

## Mouse

DOS: the library never installs an int 33h event handler. The driver's state is read only
by `MOUSE_SERVE` (int 33h AX=3: x in 640-wide virtual pixels in mode 13h, halved; y; buttons
in BX), and MOUSE_SERVE is called only by the game's code (its input loops, the disk
prompt) and once by MOUSE_INIT. When x, y or a button changed since the last call
(`D_2FBA..D_2FC0`; MOUSE_INIT forces a change with x = 8001h), it hides the pointer, stores
the new state in the game's `struct tMouse` (`buttonL = BX & 1`, `buttonR = BX & 2`), calls
`Mouse.onMove` (CON_UpdateMousePointer, which chooses the pointer shape and may call
MOUSE_POINTER), shows the pointer and returns 1; otherwise it returns 0.

**Poll points.** So in DOS the mouse state and the callback already change only at points
fixed by the game's own code, and the port keeps exactly those points: there is no other.
Each MOUSE_SERVE of an installed mouse calls `plat_pump()`, reads x, y and buttons (game
pixels 0..319, 0..199, from `plat_mouse_state`) through `rp_value(RP_MOUSE_X/Y/BTN)` — three
values per call — and then does what the original does, calling `Mouse.onMove` at the same
place. MOUSE_INIT reads the presence of a mouse (int 33h AX=0) through `rp_value(RP_MISC)`;
with a mouse it returns 2 (buttons), sets the show counter to -1 (hidden) and serves once
(onMove is still the dummy then: main.c installs CON_UpdateMousePointer after MOUSE_INIT).
The DOS driver's reset centres its pointer; the port starts from the host pointer's place.

**Showing.** `_nMouseSem` nests: -1 hidden, 0 shown. MOUSE_SHOW increments it and, on
reaching 0, saves the background (driver entry 0Ch, `MOUSE_SAVE_BG`) and draws the pointer
(30h, `MOUSE_DRAW`). MOUSE_HIDE decrements it and, on reaching -1, waits for the vertical
retrace (port 3DAh bit 3) and restores the background (0Fh). MOUSE_POINTER(shapes, i) does
nothing without a mouse or when shape and table are unchanged, else hide, set, show. The
port calls the C driver's `GRDRV_0C`, `GRDRV_30`, `GRDRV_0F` (`port/gfx/mcga.h`) with the
tMouse fields MOUSE.ASM pushed (surface, saveBuf, posX, posY, shapes, shapeIndex), as
pointers. It does not wait for the retrace: the screen is presented by `plat_pump`. (The DOS
code also leaves `GrDriver`'s offset word set to the entry number; the game always sets it
again before calling the driver.)

**MOUSE_IN_AREA(area)**: 0 without a mouse or while hidden; else whether the pointer
shape's box overlaps the rectangle, with signed compares: the shape header is at
`shapes + dword[shapes + 4 + 4·index]`, its words are the extents right, left, up, down of
the hot spot, and the test is `x - left <= area.right && x + right >= area.left &&
y - up <= area.bottom && y + down >= area.top`. `MOUSE_0297` (hot spot inside the area)
and `MOUSE_02CF` (shape entirely inside) are not called by the game; they are ported too.

## LZW: decompress(fd, offset, dest, workSeg)

DOS (INFLATE.ASM, LZ.INC, close to Tom Pfau's LZW code): seek `fd` to `offset`, read the
4-byte unpacked size (used only for a block size that is never used), unpack the stream
behind it into `dest`: 9 to 12-bit codes, LSB first; 256 clears the table, 257 ends. The
**work segment** holds, at `workSeg:0`, the table (3 bytes a code: prefix word, suffix byte;
entry n at n·3, 16-bit offsets) and at `(workSeg+300h):0` the 1 KB input buffer, read 1024
bytes at a time; when the next code starts at byte 1021 or later the unread tail is moved
to the front and the rest refilled with a read of as many bytes as were consumed (a short
read near the end of the file leaves older bytes there). At the end code the table
(1800h words) is cleared; the input buffer keeps its last contents. Reads are plain int 21h
calls (no OSI error handler); the file is left after the last block read. AX returns the
offset of the normalised `dest`. A failed header read leaves through a near RET (a crash in
DOS; the port logs it and returns 0).

Port: the same algorithm, operating on the DOS arena at the same addresses, so the work
segment's contents, the file position, the output and the behaviour when output and work
area overlap are the original's. The decoder's character stack (the CPU stack in DOS) is an
array; only a corrupt stream can overflow it (logged, unpacking stops). The callers place
the work segment after the data (LoadFile, ShowMainScreen, the pointer shapes) or inside
ScratchBuf (portraits, conversations).

## Random numbers

DOS (RAND.ASM): a 32-bit state in two words, `D_308A:D_3088` (high:low), initially 0:1.
Each draw computes state · 65539 (RANDU) with add/rcl/add/adc/add. `OSI_rand(a, b)` draws,
then with min/max the smaller/larger of a and b (unsigned) returns
`min + high_word % (max - min + 1)`, or `min` when that range is the whole 64K.
`OSI_randomize(s)` sets the low word to `s | 1` (the high word stays). `OSI_timeSeed`
reads the DOS time (int 21h 2Ch) and returns
`((DH<<3 : DL) + (CH<<1 : CL<<2)) ^ 91EBh) & 0FFFh` with 8-bit shifts.

Port: the instructions one by one. The seed `OSI_timeSeed` returns goes through
`rp_value(RP_SEED)`. The state is visible to tests and digests via `osi_rand_state()`.

## Files

### The DOS file system (`dosfs.c`)

The game opens DOS names relative to the game folder (`savegame\objblkaa`, `u6pal`,
`MAPTILES.VGA`). dosfs gives it a DOS file system made of:

* the **data root**: the user's game folder, only ever read;
* the **overlay**: the port's directory laid over it (portmain gives
  `session_game_home()`; by default `plat_home_dir()`).

Names: separators `\` or `/`; an optional `C:` (the only drive) and leading `\` (the game
folder stands for the root of C:); `.` and `..`; each part upper-cased and cut to 8.3 as DOS
does, wildcards and invalid characters refused. Each part is matched without case against
the host directory entries (an exact-case match first).

Reads look in the overlay first, then in the data root. Creating, writing, deleting and
renaming happen in the overlay only:

* a file of the data root opened for writing is copied to the overlay on its **first
  write** (every handle open on it is moved to the copy);
* creating makes the file in the overlay, with its directories named as in the data root;
* deleting (or renaming away) a file that exists in the data root records it in the
  overlay's `.dosfs-deleted` list, which hides it from then on (also in later sessions)
  until a file of that name is created again;
* renaming a data root file copies it to the new name in the overlay.

The calls follow the DOS functions: DOS handles (lowest free of 20, 0..4 being the standard
devices: reads give nothing, writes are accepted), DOS error codes (2 file not found, 3 path
not found, 4 too many open files, 5 access denied — a directory, the target of a rename
exists, writing a read-only handle —, 6 invalid handle), seeking past the end allowed,
a write of 0 bytes sets the file's size to the position, a short write without error when
the disk is full. File I/O is standard C stdio; listing a directory, `mkdir` and setting a
size use POSIX or Win32 calls in one small section of dosfs.c.

What a session stages (recordings): the overlay's files **and** its `.dosfs-deleted`.

### OSI_FILE.ASM (`osi.c`)

| routine | DOS | error function |
|---|---|---|
| `OSI_open(name)` | 3D02h (read/write) | 1 |
| `OSI_close(h)` | 3Eh | 2 |
| `OSI_create(name)` | 3Ch, attributes 0 | 3 |
| `OSI_delete(name)` | 41h | 4 |
| `OSI_read(h, offset, length, buf)` | 4200h then 3Fh blocks | 5 |
| `OSI_write(h, offset, length, buf)` | 4200h then 40h blocks | 6 |
| `OSI_rename(old, new)` | 56h | 7 |
| `OSI_copy(src, dst)` | 48h, open, 3Ch, read/write loop | 8 |
| `OSI_fileSize(h)` | 4202h (leaves the position at the end), 0 on error | – |
| `OSI_memory()` | 48h with BX=FFFFh: largest free block in bytes | – |

**Error protocol.** On a DOS error the routine sets `OSI_ErrorCode` (the DOS code, or -1
for a short read/write), `OSI_ErrorFunc` (table above) and calls `OSI_ErrorHandler.f`
(initially `OSI_callback`, which does nothing; the game installs FileErrorHandler, and
OSI_callback while probing disks). If the handler cleared `OSI_ErrorCode` the DOS call is
made again; `OSI_ErrorFile` is set to the name before every attempt of open, create,
delete and rename (the old name for rename). Otherwise open/create return -1, the others
-1 or 0 on success.

**OSI_read/OSI_write.** If the high word of `offset` is negative (e.g. -1) there is no
seek: the current position is used. A failed seek calls the handler once and ends the
routine (returning 0 bytes for read). The transfer is made in blocks of FFF0h bytes (the
buffer advancing by FFFh paragraphs). A block that fails calls the handler and is retried
while the handler clears the code; when it gives up, the original carries on with AX — the
value the handler left in AX — as the block's byte count: the port uses the error code when
the handler is OSI_callback (a bare RET leaves it in AX) and 0 for a C handler (whose AX the
port cannot know; only on real I/O errors, which hosts practically never give). A block that
moved fewer bytes than asked (end of file, disk full) calls the handler once more with -1
and ends the routine. OSI_read returns the total read. OSI_write returns DX:AX as left by
the original (AX the count of the last block, DX its buffer offset); the game ignores it.

**OSI_copy** (not called by the game) allocates all free DOS memory as its buffer. Since the
game holds all memory, the original would loop forever on a 0-byte buffer; the port reports
error 8 (not enough memory) instead.

### Drives (LOW.ASM, `dosint.c`)

The port emulates MS-DOS 5.00 with one fixed drive, C:, holding the game folder:
`OSI_getDrive()` is 'C'; `OSI_selectDrive(letter)` returns 1 for C (any case) and for
anything else calls `DriveErrorCallback` and returns 0 (in DOS a missing floppy would also
go through the int 24h handler, which sets `D_2FCC`). `DISK_init` (disk.c) then sees: DOS
5.0; IOCTL 4408h says C: is fixed and every other drive invalid; IOCTL 440Eh says C: has a
single name. So every disk resolves to C: as on a hard disk install, provided the
identification files `U6.1`..`U6.8` are in the game folder (the GOG and floppy-installed
folders have them).

`OSI_lowInit` installed int 24h (critical error: set `D_2FCC`, fail the call) and int 23h
(Ctrl-Break: ignored) handlers, cleared an unused clipping table, read the memory size
(int 12h) and measured CpuSpeed; the port only sets CpuSpeed (above).

## DOS services called by the game's C (`dosint.c`, `dosmem.c`)

* `int86`, `int86x`, `intdos`, `intdosx`, `segread` with the stand-in REGS/SREGS of
  `port/include/dos.h`:
  * int 21h: 0Eh select disk, 19h current disk, 2Ah/2Ch date/time (through RP_DATE),
    30h version (5.00), 4408h/440Eh (drives, above), 4Ch exit;
  * int 10h: 1010h set one DAC register (palette cycling of colour FFh), 1012h set a block
    from ES:DX (SetVGAPalette), 1015h/1017h read back — on `port/gfx/vgadac.h`;
  * int 11h equipment (no diskette), int 12h memory (640 KB);
  * anything else is logged once and fails (carry set, AX=1).
* `outportb(3C8h/3C9h/3C7h)` → `vga_dac_write_index/data`, `read_index`;
  `inportb(3C9h)` reads the DAC, `inportb(3DAh)` toggles the retrace bits; other ports are
  logged once.
* `farmalloc`/`farcoreleft`/`farfree` (via compat.h) → `dos_farmalloc` & co: Turbo C 2.0's
  far heap in the DOS arena (10000h..A0000h): 8-byte block header, blocks paragraph aligned,
  so `farmalloc(farcoreleft())` — what main.c does — returns seg:0008 and takes everything.
  `MK_FP`/`FP_SEG`/`FP_OFF` → `dos_mk_fp` & co: arena pointers convert to normalised
  seg:off; pointers outside the arena get a "foreign" segment (F000h + n, a 64 KB window
  per base) so that `MK_FP(FP_SEG(p), FP_OFF(p))` gives `p` back.
* `execl` (END.EXE with the time spent in Britannia, ULTIMA6.EXE when there is no
  character): logged, then the exec handler (portmain: `on_exec`) is called; if it returns,
  execl fails as DOS does for a missing program (-1, errno ENOENT). Both callers then call
  execl again forever (`while(1) execl(...)` / `do ... while(1)`), so later calls pump,
  sleep 20 ms and end the program when the user closes the window (or a replay is over).
  The game ends the argument list with a plain int 0, which a 64-bit host's `...` may not
  read back as a null pointer: an argument whose low 32 bits are 0 ends the list.
* `exit` is `port_exit` (portmain.c). `dos_exit` is port/sys's own way out (the endless
  loops above); it calls `dos_exit_hook` first if set, so portmain can route it to
  `port_exit` and flush a recording.

## Tests

`port/tests/run_osilib_tests.sh` builds `libosi_test.so` and `test_osi_unit` from the port/sys
files with test stand-ins for the platform (`osi_stubs.c`: simulated clock, key queue,
mouse, speaker log, driver-entry log) and runs:

* **test_osi_unit** (C): dosfs (names, cases, 8.3, drive/root/`..`, every error code,
  handle numbering and the 20-handle limit, copy on write seen by other handles, create,
  sizes, zero-length writes, delete/rename with the deleted list, persistence across
  restarts, the data root never changing), the OSI error protocol (handler calls, retries,
  OSI_ErrorFile, block sizes, short reads, seek failures, the default handler's AX),
  OSI_delay on a simulated clock (frame limiting, already-due waits, the 16-bit wrap,
  clean), record then replay (same ticks and keys, no sleeping, live keys ignored, the
  hand-over to the live clock), dosmem (far heap, foreign pointers) and dosint (drives,
  DAC, execl).
* **osilib_diff.py** (Python, unicorn): the original routines from `GAME.EXE` of this
  source's DOS build, relocated and run with DS = DGROUP, against the C:
  RAND (720 000 draws over 12 seeds: result and state after every draw; OSI_timeSeed for
  20 256 clock values), LZW (all 521 streams of the game folder: 116 LZW files and every
  entry of PORTRAIT.A/B/Z and CONVERSE.A/B — output, work area, surrounding bytes, file
  position, AX), KBD (20 000 polls on random BIOS queues), SOUND (234 calls of every sound
  routine at several CpuSpeed values: port 42h divisors, port 61h gate per step, delay loop
  iteration counts, 8.7 million iterations), MOUSE (10 302 operations: return values,
  driver calls with their x/y/shape, onMove calls, and a sweep of MOUSE_IN_AREA's four
  boundaries). Deliberately broken copies of the C (an off-by-one in the LZW refill, the
  randomize `|1`, the backspace key, the noise seed, `>` for `>=` in the waved note, a
  swapped IN_AREA extent, a short table clear) are each caught.

All pass. Not covered by the differential tests: real-time behaviour (timing is checked on
the simulated clock only) and the rendering of sound by the host.
