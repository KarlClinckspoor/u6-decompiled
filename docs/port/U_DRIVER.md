# MCGADRV.BIN: U.EXE's MCGA graphics driver

`U.EXE` (main menu, introduction, character creation) has its own graphics drivers,
smaller than `GAME.EXE`'s `U6MCGA.DRV` (`MCGA_DRIVER.md`) and with a different interface:
`MCGADRV.BIN` for VGA/MCGA (320x200x256, BIOS mode 13h), `EGADRV.BIN`, `TGADRV.BIN`,
`CGADRV.BIN`. This is the MCGA one, reverse-engineered from the GOG copy and reimplemented
in `port/u/ugfx.c` (`port/u/ugfx.h`). The differential test is `port/tests/ugfx_diff.py`
(results at the end).

## The file and how U.EXE loads it

* `MCGADRV.BIN` is LZW compressed in the usual format (`port/tests/u6lzw.py`); unpacked it
  is 2632 bytes (A48h) of position-independent real-mode code and data: the driver's own
  variables are addressed `CS:xxxx`, so it runs at offset 0 of whatever segment it lands in.
* `main.c` loads it with `LoadZFile("mcgadrv.bin")` (`lzw.asm`) into `gfxDrv`, a far
  pointer whose offset is set to the entry before each call: `DRV(o)(args)` in `u.h`
  expands to `gfxDrv.p.ofs = (o); (*gfxDrv.f)(args)`. It also stores entries 3, 6, 9, 12h
  and 0Ch in the OSI library's table `drvDrawPointer` (`OSILIB/MOUSECUR.ASM`): the mouse
  pointer, text characters (`TXTCHAR.ASM`) and line clearing (`TXTSCRL.ASM`) call them
  directly.
* U.EXE is `tcc -mm`: medium model (far code, **near** data) and **cdecl**. The caller
  pushes the arguments right to left and removes them; the first argument is at `[BP+6]`
  in the driver, every entry ends with a plain `RETF`. Struct pointers (`struct tSurface *`,
  `struct tText *`) are near (DGROUP, read through DS); shape files and buffers are far.
  Entry 12h is the exception: it is called with its arguments in registers.
* Offset 0 holds a 9-entry jump table (`E9 xx xx`), 1Bh a `NOP`.

## Layout of the driver segment

| offset | size | content |
|---|---|---|
| 0000 | 1Bh | jump table: 0 → 08CE, 3 → 02A0, 6 → 0510, 9 → 0673, 0C → 07D6, 0F → 081E, 12 → 08F0, 15 → 08CD, 18 → 0A0F |
| 001C | 14 bytes | 03h: copy of the `struct tSurface` (segment, pScanlines far, right, left, top, bottom) |
| 002A..0040 | words | 06h/09h: clipped x0 (2A), pixel segment (2E), scanline segment (30), box height (34), rows left (36), box x0 (38), clipped width (3A), box y0 (3C), clipped y0 (3E), box width (40). 0Fh: source left (2A), destination left (2C), source segment (2E), the scanline segments (30, 32) |
| 0042, 0044 | words | 03h: x, y |
| 0054..005E | words | 12h: surface segment (54), BX = the tText (56), rows left (58), width (5A), 2*y (5C), DS (5E); 30 = scanline segment |
| 0060 | 140h | 03h: one RLE run unpacked when it is clipped at an end |
| 01A0 | 100h | `0, 1, 2, ..., FFh`: entry 12h's color table |
| 02A0 | | code: 03h, 06h, 09h, 0Ch, 0Fh, `RETF` (15h), 00h, 12h, 18h |

All initial data is zero except the color table; the variables are always written before
they are read within a call, so only the run buffer and the color table carry anything
from one call to the next (see the run buffer overflow below).

## Data formats

### struct tSurface (`u.h`, `OSI.INC`)

| offset | field | |
|---|---|---|
| 00 | `segment` | segment of the pixels: A000h for the screen, or `FP_SEG` of a `farcalloc` buffer |
| 02 | `pScanlines` far | one word a line: the offset of line y in `segment`, indexed by the absolute y |
| 06, 08, 0A, 0C | `right`, `left`, `top`, `bottom` | the clipping window, inclusive, compared **signed** |

The screen: A000h, `scanlineOfs` (filled by entry 0: `y * 320`), window 0..319 x 0..199.
`SURF_alloc` (`OSILIB/SURFACE.C`) gives a surface a buffer of the window's size and a table
`FP_OFF(buffer) + i * width` for i = 0..height-1 (so it assumes top = 0). Several entries use
a surface only as a rectangle (0Ch fills "the surface"; `TXT_clearLine` builds one).

### struct tText (`u.h`, `OSI.INC`)

`surface` (near), `curX`, `curY`, `font` (far), `fgColor`, `bgColor` (bytes), `buffer`,
`bufPtr`, `justify`. Entry 12h reads surface, curX, font, the colors, and writes curX.

### Shape files (.SHP, INTRO.PTR)

LZW files, loaded whole by `LZW_load`. Once unpacked:

| offset | |
|---|---|
| 0 | dword: the file's size |
| 4 | dword per shape: its offset from the start of the file |
| ... | the shapes |

A shape:

| | |
|---|---|
| 4 words | `right, left, up, down`: the extent of its box from the hot spot (the box is x-left..x+right, y-up..y+down) |
| runs | `{word n2, int16 dx, int16 dy, data}`, until a word 0. `n2 = pixels * 2 + rle`; the run starts at (x+dx, y+dy) and goes right |
| literal data | the `n2 >> 1` pixels |
| RLE data | chunks of a byte `b` and: one color byte repeated `b >> 1` times (b odd), or `b >> 1` pixels (b even), until the run's pixels are done |

There is no transparent color: what is not covered by a run is not drawn (pixel 0 is
drawn like any other). The format is the one of `U6MCGA.PTR` for GAME.EXE's entry 30h,
which is the same code (`MCGA_DRIVER.md`). The game's files (all parse exactly: every RLE
run's chunks add up, every run is inside its box):

| file | shapes | literal runs | RLE runs | longest run | largest box |
|---|---|---|---|---|---|
| MAINMENU.SHP | 1 | 13 | 101 | 223 | 223x114 |
| TITLES.SHP | 2 | 628 | 395 | 59 | 283x52 |
| INTRO.SHP | 13 | 767 | 822 | 320 | 320x200 |
| INTRO_1.SHP | 71 | 2356 | 1859 | 320 | 320x200 |
| INTRO_2.SHP | 29 | 5971 | 56 | 320 | 320x200 |
| INTRO_3.SHP | 27 | 907 | 4822 | 320 | 320x200 |
| GYPSY.SHP | 200 | 3283 | 2302 | 320 | 320x200 |
| WOODS.SHP | 2 | 400 | 0 | 320 | 320x200 |
| VELLUM1.SHP | 19 | 192 | 309 | 283 | 284x118 |
| MONTAGE.SHP | 15 | 674 | 229 | 56 | 56x64 |
| INTRO.PTR | 9 | 112 | 0 | 13 | 13x14 |

(END.SHP and BLOCKS.SHP, which U.EXE does not use, have the same format.)

### Fonts (U6.SET, `tText.font`)

LZW (`LZW_load("u6.set")`), 7004 bytes unpacked:

| offset | |
|---|---|
| 0 | word: height (8) |
| 2, 3 | the two pixel values the glyphs are drawn with: "foreground" (0Fh) and "background" (0) |
| 4 | 256 bytes: the width of each character (0..9) |
| 104h, 204h | 256 bytes each: low and high byte of each glyph's offset from the start of the font |
| 304h | the glyphs: `height` rows of `width` pixel bytes |

## Entries

| entry | C | U.EXE's calls |
|---|---|---|
| 00h | `UDRV_0(uint16_t *scanlines)` | `DRV(0)(scanlineOfs)` once (main.c) |
| 03h | `UDRV_3(struct tSurface *s, int16_t x, int16_t y, void *shapes, int16_t index)` | 149 `DRV(3)`; the mouse pointer |
| 06h | `UDRV_6(struct tSurface *s, void *saveBuf, int16_t x, int16_t y, void *shapes, int16_t index)` | `drvSavePointer` (MOUSECUR.ASM) |
| 09h | `UDRV_9(...)`, the same arguments | `drvRestPointer` |
| 0Ch | `UDRV_0xc(struct tSurface *s, int16_t color)` | 168 `DRV(0xc)`; `drvFillRect` (TXTSCRL.ASM) |
| 0Fh | `UDRV_0xf(struct tSurface *src, struct tSurface *dst)` | 70 `DRV(0xf)` |
| 12h | `UDRV_0x12(struct tText *t, int16_t ch, int16_t height, int16_t width, int16_t y2)` | `drvPutChar` (TXTCHAR.ASM), registers |
| 15h | `UDRV_0x15(struct tSurface *s)` | 53 `DRV(0x15)(&screen)` |
| 18h | `UDRV_0x18(struct tSurface *s, int16_t x, int16_t y, int16_t color)` | 12 `DRV(0x18)` (moongate.c: the stars) |

### 00h: scanline offsets, mode 13h

Writes `y * 320` for y = 0..199 into the table (a near pointer), then int 10h AX=0013h: mode
13h, which clears the video memory and loads the BIOS default palette. The C clears
A000:0000-FFFF in the arena and calls the hook set with `ugfx_set_mode_hook` (for the
port to reset the DAC; U.EXE sets its own palette afterwards anyway).

### 03h: draw a shape

1. The surface is copied into CS:001C.
2. `index * 4` (16-bit) is compared **signed** with the low word of the first offset of the
   table (`[shapes + 4]` = 4 + 4 * the number of shapes): nothing is drawn when it is
   greater or equal. So the index equal to the number of shapes passes (its "offset" is the
   first 4 bytes of shape 0, its `right` and `left`), and so do negative indexes (index -1
   reads the file's size, others read before the table); U.EXE never does either.
3. The shape is `shapes + dword[shapes + 4 + index * 4]`, computed as a 32-bit linear
   address and normalised (segment = linear >> 4, offset = linear & 15): shape files may be
   larger than 64 KB, each shape is read within 64 KB of its normalised start.
4. If the box is inside the window (`x + right <= wright`, `x - left >= wleft`,
   `y - up >= wtop`, `y + down <= wbottom`, signed), every run is drawn **without clipping**
   (line `y + dy` of the scanline table, column `x + dx`). Otherwise (02A2..050D) each run is
   clipped: a run whose row is outside top..bottom or whose columns are all outside
   left..right is skipped (the RLE data walked over), a literal run is cut at both ends,
   an RLE run that is cut is first unpacked into CS:0060 and its visible part copied.

Quirks (all reproduced by the C):

* **The two paths end differently.** The clipped path stops at a run word whose
  `n2 >> 1` is 0, i.e. 0 or 1; the unclipped path only at 0: a word 1 there is an RLE run of
  0 pixels which still reads one chunk byte (and if that chunk's count is not 0, the
  remaining count goes below zero and the run goes on until it reaches 0 modulo 65536).
* **The box decides, not the runs.** A shape whose runs go outside its box is drawn outside
  the window when the box is inside.
* **The run buffer.** CS:0060 has 320 bytes before the color table at CS:01A0, and the
  longest RLE run of the game's files is exactly 320. A longer RLE run clipped at an end
  overwrites the color table, and characters drawn afterwards (entry 12h) come out in the
  colors of those pixels (12h only puts back the two entries it changes). Runs longer than
  576 pixels would overwrite the driver's code: not reproduced (the C has no code there).
* `MOVSW`/`STOSW` with a `MOVSB`/`STOSB` for an odd count: see "Word access" below.

### 06h, 09h: the box of a shape, from and to a buffer

The shape is found as in 03h but its index is **not** checked. Box: width
`right + left + 1`, height `up + down + 1`, at `(x - left, y - up)`. Nothing is done when the
box is entirely outside the window; otherwise its part inside the window is copied from the
surface into `saveBuf` (06h) or back (09h). `saveBuf` holds the whole box, row after row
(stride = the box's width), the visible part at its place in it. MOUSECUR.ASM calls 06h,
03h, then 09h with the same position and shape to show and hide the pointer.

### 0Ch: fill a rectangle

Fills `left..right` x `top..bottom` of the surface with the color's low byte, through the
scanline table. No clipping: the window **is** the rectangle (a rectangle outside the
screen writes elsewhere in the segment, rows past the table read whatever follows it).

### 0Fh: copy a rectangle

Copies the source's window to the destination's window, top left corner on top left
corner: the smaller of the two widths and of the two heights (compared **unsigned**), row
by row through each surface's scanline table. Source and destination may be the same
pixels: the copy goes forward a word at a time (`REP MOVSW`).

### 12h: a character of text (register call)

`TXT_putcAL` (TXTCHAR.ASM) calls it with AX = the character, BX = the `struct tText *`, CX =
the font's height, DX = the character's width (`font[4 + ch]`), SI = 2 * curY (the cursor's
row as an offset in the scanline table), DS = DGROUP. It has checked that the character
fits (else it went to a new line first). The C takes them as
`UDRV_0x12(t, ch, height, width, y2)`.

* The glyph: `font + (font[104h + ch] | font[204h + ch] << 8)` (16-bit offset arithmetic).
* If `fgColor == font[2]` and `bgColor == font[3]`, the glyph's rows are copied as they are;
  otherwise through the color table with entry `font[2]` set to fgColor and then entry
  `font[3]` to bgColor (so bgColor wins when the font has font[2] == font[3]); the two
  entries are put back afterwards. Other pixel values are drawn as they are.
* `height` rows of `width` bytes at `scanline[curY] + curX` (the row from SI, +2 a row), no
  clipping. Height 0 draws 65536 rows.
* Then `curX += width`, and curX is set to the window's right when it is past it.

### 15h: nothing

A lone `RETF` (in the EGA driver, probably the page flip). U.EXE calls it with `&screen`
in 53 places, after drawing.

### 18h: a pixel

Puts the color's low byte at (x, y) when `top <= y <= bottom` and `left <= x <= right`.

## Things to know

* **Counts of 0.** The row loops are `DEC`/`JNZ`: a height of 0 (a window with
  bottom = top - 1, 12h with height 0) means 65536 rows; a width of 0 copies nothing. Widths computed from bad windows (right < left - 1)
  are huge. U.EXE's own windows are all valid.
* **Word access at offset FFFFh.** The copies and fills use `REP MOVSW`/`STOSW`: a row
  that crosses the end of its 64 KB segment at an odd offset would make a 286 or later
  fault (#GP; DOS hangs), an 8086 wrap within the segment. The C wraps byte by byte (8086
  behaviour); U.EXE never draws there.
* **No clipping** in 0Ch, 0Fh, 12h; out-of-range coordinates write elsewhere in the
  segment, rows outside the table read whatever words follow or precede it.
* All coordinate compares are signed (`JL`/`JG`), with the 16-bit wrap of their operands
  (an `x + right` past 32767 is negative).

## The C (port/u/ugfx.c)

* One function per entry, `UDRV_<the literal of the call site>` (`UDRV_0xf` for
  `DRV(0xf)`), arguments in U.EXE's C order; ugfx.h documents each with its mapping.
  `struct tSurface` and `struct tText` are declared as the host compiles U's (packed,
  `far` empty) under the guard `U_STRUCTS_DEFINED`, shared with `SRC_U/u.h`.
* A close transcription: the driver's variables are kept in a 64 KB array at their DOS
  offsets (`ugfx_segment()`; all but 56h and 5Eh, which hold DGROUP offsets and DS),
  registers are `uint16_t`, compares are signed or unsigned as the original's jumps,
  `REP MOVSW` is copied a word at a time (both bytes read, then both written).
* Memory: pixels are `surface->segment:offset` in the DOS arena (`dos_mk_fp`, A000h is the
  screen); far pointers (`pScanlines`, shapes, `saveBuf`, the font) are turned back into
  segment:offset with `dos_fp_seg`/`dos_fp_off` (normalised), so that offsets wrap at 64 KB
  as in DOS (for pointers into the arena; U's are farmalloc'd). Struct pointers are read
  directly. The C does not depend on SDL nor on the platform layer.

## Test (port/tests/ugfx_diff.py)

The original image runs in unicorn (real mode, 1 MB; int 10h AX=0013h clears A000) and the
C is built with `port/tests/ugfx_shim.c` (a stand-alone arena) and called through ctypes.
Both get the same memory: random screen, off-screen buffer and save buffer, scanline
tables (the screen's, SURF_alloc-style ones with widths 320, 176 and 57, random words
around them), U6.SET and a variant with font[2] == font[3] at an odd offset, and the shape
file of the moment (at various offsets 0..15 of a paragraph). After every call it compares the
whole arena except DGROUP and the driver's segment (which hold the emulator's structs and
stack), the driver's data (CS:001C-029F but 56h and 5Eh), the tText, entry 0's table and
mode hook.

The calls (seed 6):

1. Entry 0.
2. The mouse pointer: each INTRO.PTR shape saved (06h), drawn (03h) and restored (09h) at
   17 x 17 positions across every clipping edge of the screen.
3. Every character of U6.SET with the font's colors, U.EXE's text colors and swapped ones;
   600 text calls with random windows, colors equal to one or both of font[2]/font[3],
   heights and widths other than the font's (and height 0), the cursor near the right.
4. **Every shape of every shape file** (all 12 .SHP files and both .PTR files, 415
   shapes): drawn with its box at the corner of the back buffer (unclipped), at a random
   place inside the screen, at three random places partly outside it, on a random window;
   its box saved, the screen scribbled over (0Ch), the box restored. Then the indexes the
   bound check lets through (the number of shapes, negative ones) and stops.
5. 4000 random calls of every entry: windows partly or wholly outside their surface, empty
   ones, negative, huge and wrapping coordinates, overlapping copies on the same pixels,
   shapes pointers not normalised.
6. Synthetic shapes: RLE runs of 330 to 576 pixels clipped (the color table overwritten,
   then text through it), a run word of 1, a box that lies, mixed chunks, negative extents;
   and one RLE and one literal run of each length 1..24 at every clipping offset -2..+2
   around both ends of three windows.

Result: **all 21,036 calls match** (and with seeds 1, 2, 3, with gcc and clang). Every byte
of the driver's code (02A0-0A47) is executed. Calls that are not valid tests are skipped
and the original's memory rolled back (the C is not called): 52 that make the original
touch a word at offset FFFFh of a segment (unicorn neither wraps nor faults there), 19 that
run more than 5 million instructions (garbage shapes reached with an index the bound check
lets through), 2 that unpack a run over the driver's own code.

    python3 port/tests/ugfx_diff.py              # about 3 minutes
    python3 port/tests/ugfx_diff.py --quick      # a sample of the shapes and characters

Not tested: the BIOS palette of the mode set (the hook), self-modification by runs longer
than 576 pixels, word accesses at offset FFFFh.
