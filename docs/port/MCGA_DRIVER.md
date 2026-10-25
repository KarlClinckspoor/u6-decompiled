# U6MCGA.DRV: the MCGA graphics driver

Ultima VI's VGA/MCGA (320x200x256, BIOS mode 13h) driver, reverse-engineered from
`U6MCGA.DRV` (GOG copy, 4961 bytes) and reimplemented in `port/gfx/mcga.c`. The
differential test is `port/tests/mcga_diff.py` (results at the end).

## The file and how the game loads it

* `U6MCGA.DRV` is LZW compressed in the game's usual format (4-byte unpacked size, then
  9 to 12-bit codes, 100h = reset, 101h = end; `port/tests/u6lzw.py`). Unpacked it is
  13796 bytes (35E4h) of real-mode code and data, position independent: everything is
  addressed `CS:xxxx`, so it runs at offset 0 of whatever segment it is loaded in.
* `InitGraphicsDriver` (`SRC/gfx.c`) loads it with `LoadFile` (`IsZFile = 1`), calls entry
  00h, then fills the tGr block's callbacks. `main.c` later sets `tGr.font` (U6.CH),
  `tGr.pWorkBuf` (`ScratchBuf`, 7900h bytes) and gives the driver all remaining memory
  with entry 2Ah (`InitGraphicsData`).
* The game is built with `tcc -mm -p`: medium model (far code, **near** data) and the
  **Pascal** calling convention. The `GR_*` macros set `GrDriver.iii.ofs` to the entry and
  far-call it: arguments are pushed left to right (the first macro argument is deepest on
  the stack, at the highest `[bp+n]`), a far pointer is pushed segment then offset, and
  the driver pops them itself with `RETF n`. All arguments are 16-bit words.
* Offset 0 holds a 27-entry jump table (`E9 xx xx`, 3 bytes an entry). 51h is a `NOP`.

## Layout of the driver segment

| offset | size | content |
|---|---|---|
| 0000 | 51h | jump table |
| 0052 | 34h | **struct tGr** (returned by entry 00h, see below) |
| 0086 | byte | BIOS video mode saved by entry 00h (3 in the file) |
| 0087 | byte | 90h, unused |
| 0088 | 800h words | segment of each tile's pixels (offset 0); 0 = object tile not in memory. Loaded from TILEINDX.VGA |
| 1088 | 800h bytes | tile format (MASKTYPE.VGA part 1) |
| 1888 | 600h bytes | object tile → block number (MASKTYPE.VGA part 2), read as `[1688h + tile]` for tiles 200h-7FFh |
| 1E88 | C0h words | block → position in OBJTILES.VGA, in paragraphs (MASKTYPE.VGA part 3) |
| 2008 | 200 words | `y * 320` |
| 2198 | | code of 00h, 03h, 18h |
| 2200 | word | segment after the last cache page (= animmask segment) |
| 2202 | word | segment of the first cache page |
| 2204 | word | the least recently used page (the next one to be loaded into) |
| 2208 | | `"maptiles.vga" "objtiles.vga" "tileindx.vga" "masktype.vga" "animmask.vga"` (2208, 2215, 2222, 222F, 223C) |
| 2249 | | code: tile loader, tile unpacker, 2Dh, 42h, 2Ah |
| 26C0 | 16 words | sprite slot segments |
| 26E0, 26E2 | words | pivot x * 8, pivot y * 8 (entry 33h) |
| 26E4 | 128 bytes | `round(64 * sin(2πi/128))`, signed |
| 2764 | | code: slot check, 33h, 36h, 39h |
| 28EC | | `RETF` (entry 3Ch) |
| 28F0 | 15 words | LFSR taps for 2..16 bits: 3, 6, C, 14, 30, 60, B8, 110, 240, 500, CA0, 1B00, 3500, 6000, B400 |
| 290E | | code: 45h, 48h, 1Eh |
| 2A70 | word | drawing color, in both bytes (entry 06h) |
| 2A72 | | code: 06h, 09h, 12h, 15h |
| 2C00, 2C40, 2C80 | 32 words each | entry 3Fh table: source tile * 2, destination tile * 2, mask offset |
| 2CC0 | word | segment of ANIMMASK.VGA's data |
| 2CC2 | | code: 4Bh, 3Fh, 24h, 27h |
| 2D70 | 8 words | entry 4Eh: start angles 0, 3D, 81, BF, 104, 142, 17E, 1C3 (of 200h) |
| 2D80 | | code: 4Eh |
| 2F20 | 2Ch | mouse code temporaries: copy of the surface (7 words), clipped rectangle |
| 2F64 | 140h | mouse code: one clipped RLE run unpacked |
| 30A4 | | code: 30h, 0Ch, 0Fh, 1Bh, 21h |

### struct tGr (CS:0052)

| tGr offset | field | initial | read by |
|---|---|---|---|
| 00 | `header[6]` | words 0, 320, 200 | nobody |
| 06 | `font` far | 0 | 1Eh |
| 0A, 0B | `textFg`, `textBg` | 48h, 31h | 1Eh |
| 0C | `xorMode` | 0 | 09h, 12h |
| 0D | `unknown0d` | 1 | nobody |
| 0E | `animOn` | 1 | **nobody** (the game sets it during Time Stop; this driver ignores it) |
| 0F | `toBackBuffer` | 0 | 09h, 12h |
| 10 | `pfnDiskConfirm` | | tile loader (`DISK_confirm(5)`, 5 = DISK_MAP) |
| 14 | `reserved14[8]` | 0 | nobody |
| 1C | `pWorkBuf` far | | 09h, 12h, 33h..48h, 4Eh: the 176x176 back buffer |
| 20 | `pDriverMem` far | 0 | written by 2Ah (`seg:0000`) |
| 24 | `pfnLoadFile` | | 2Ah: `LoadFileGr(char far *name, void far *dest, int isZ)` |
| 28 | `pfnOpen` | | tile loader: `OSI_open(char *name)`, called with DS = CS (near pointer to the driver's string) |
| 2C | `pfnRead` | | tile loader: `OSI_read(handle, long offset, long length, void far *buf)` |
| 30 | `pfnClose` | | tile loader: `OSI_close(handle)` |

All callbacks are far Pascal functions (callee pops).

## Memory given by the game (entry 2Ah)

`GR_INIT_MEMORY(seg, size)` (size in paragraphs, everything the game has left) lays
out, from `seg:0`:

1. MAPTILES.VGA unpacked (tiles 0-1FFh), `tileindx[200h]` paragraphs (1CAAh).
2. N cache pages of 80h paragraphs (2 KB), N = `((seg + size - start) >> 7) - 3`
   (16-bit arithmetic). Each page: a header paragraph (`+0` previous page, `+2` next
   page, `+4` first tile, `+6` number of tiles), then up to 7F0h bytes of tiles. The pages
   form a circular doubly linked list; `CS:2204` is the least recently used.
3. 80h paragraphs: ANIMMASK.VGA unpacked (2048 bytes); segment at `CS:2200`/`CS:2CC0`.
4. 16 sprite slots of 10h paragraphs (256 bytes, 16x16).

Less than 3000h paragraphs: returns AX = 0 and leaves `pDriverMem` 0 (the game then
exits). With 3000h paragraphs N = 35; the original needs at least about 1CAAh + 4*80h.

## File formats

* **MAPTILES.VGA**: LZW; the 512 map tiles, each at `tileindx[t]` paragraphs.
* **TILEINDX.VGA**: raw, 800h words. Entries 0-1FFh: paragraph offset of each map tile
  in MAPTILES; entry 200h: MAPTILES' size in paragraphs (the start of the cache). Entries
  201h-7FFh are read but cleared (object tiles are found through MASKTYPE).
* **MASKTYPE.VGA**: LZW, 3968 bytes: 800h format bytes, 600h block numbers (tiles
  200h-7FFh), C0h words of block positions in OBJTILES.VGA (paragraphs).
  Format byte: bits 0-1 drawing type (0 opaque 256 bytes, 1 transparent 256 bytes with
  FFh = transparent, 2 and 3 packed); bit 3 = packed in OBJTILES (size given by its first
  byte) for the loader; bit 1 also selects unpacking in the sprite/outline code. The
  game data has 0 (opaque), 5 (transparent) and 0Ah (packed).
* **OBJTILES.VGA**: raw. Block b starts at paragraph `blockofs[b]`; the driver reads 7F0h
  bytes from there into a page (the read is short at the end of the file; the game's
  error handler ignores short reads). The tiles of a block are the consecutive tiles with
  that block number, packed one after the other: 100h bytes each, or for packed tiles
  `first byte` paragraphs.
* **Packed tile** (format 2/3): byte 0 = size in paragraphs (ignored when drawing), then
  runs `{int16 displacement, uint8 n, n pixels}` until n = 0. The displacement moves the
  destination from the end of the previous run (from the tile's top left for the first)
  in a 176-pixel-wide raster (the back buffer's). For the screen (320 wide) the driver
  converts: `while (disp >= 16) { dest += 144; disp -= 176; }` after adding disp; for
  the 16-wide unpack buffer the same with -160 and the destination masked to 0..FFh.
* **ANIMMASK.VGA**: LZW, 32 masks of 64 bytes for entry 3Fh: byte counts alternately
  copy, skip, copy, ..., starting with copy (may be 0), ending with a 0.
* **U6.CH** (the game loads it into `tGr.font`): 8 bytes a character, top row first,
  bit 7 = left pixel.
* **Pixmap** (entry 18h, `struct tPixmap`): word width, word height, pixels row by row
  (PAPER.BMP once unpacked, portraits).
* **Pointer shapes** (U6MCGA.PTR, given to entries 0Ch/0Fh/30h): dword size, then a
  dword offset (from the table's start) per shape; entry 30h refuses an index unless
  `index * 4 < (int16)` the low word of the first offset (so 11 indexes pass for 10
  shapes). Shape: words `right, left, up, down` (extent from the hot spot), then runs
  `{word n2, int16 dx, int16 dy, data}` until a word 0; `n2 = pixels * 2 + rle`. Literal
  data: the pixels. RLE data: bytes `b`, `b >> 1` pixels, followed by one color byte
  (b odd) or by the `b >> 1` pixels (b even). U6MCGA.PTR has no RLE runs.

## Entries

`[bp+6]` is the last argument pushed. "Screen" is A000:offset with offset `[2008h + 2y] + x`
in 16 bits. "Back buffer" is `pWorkBuf + y * 176 + x` with 16-bit offset arithmetic.

| entry | macro | arguments (push order) | RETF | effect |
|---|---|---|---|---|
| 00h | GR_INIT | mode (unused) | 2 | int 10h AH=0Fh saves the mode at CS:0086; sets mode 13h (clears video memory, BIOS default palette). Returns DX:AX = CS:0052 (tGr). |
| 03h | GR_SHUTDOWN | | 0 | int 10h AH=0, AL = saved mode |
| 06h | GR_SET_COLOR | c | 2 | `CS:2A70 = c & FFh` in both bytes |
| 09h | GR_PUT_PIXEL | x, y | 4 | one pixel of the color: screen, or the back buffer if `toBackBuffer`; XORed if `xorMode` |
| 0Ch | (mouse) | surface (near), saveBuf (far), posX, posY, shapes (far), index | 10h | save the pointer rectangle (below) |
| 0Fh | (mouse) | same | 10h | restore it |
| 12h | GR_FILL_RECT | x0, y0, x1, y1 | 8 | fill, corners swapped if needed (unsigned compares); screen or back buffer (176 wide), XOR mode as 09h |
| 15h | GR_SCROLL_RECT_Y | x0, y0, x1, y1, dy | 0Ah | screen only; dy < 0: rows y0+|dy|..y1 move to y0..; the last |dy| rows unchanged |
| 18h | GR_DRAW_PIXMAP | p (far), x, y | 8 | pixmap to the screen at x,y, no clipping |
| 1Bh | | a, b | 4 | nothing |
| 1Eh | GR_PUT_CHAR | ch, x, y | 6 | 8x8 character `font + ch*8` to the screen, textFg/textBg (never XOR, never back buffer) |
| 21h | | | 0 | nothing |
| 24h | GR_INIT_PALETTE | | 0 | nothing (the game calls it for non-VGA only) |
| 27h | GR_CYCLE_PALETTE | a | **0** | nothing, and does not pop its argument (non-VGA only, so never called with this driver) |
| 2Ah | GR_INIT_MEMORY | seg, size | 4 | see above; returns 0, or on success the AX of the last LoadFileGr call (the game ignores it) |
| 2Dh | GR_PUT_TILE_SCREEN | tile, x, y | 6 | tile to the screen at x,y |
| 30h | (mouse) | surface (near), posX, posY, shapes (far), index | 0Ch | draw the pointer |
| 33h | GR_DEFINE_SPRITE | slot, tile, rot, pivotX, pivotY | 0Ah | rotated copy of the tile into a sprite slot |
| 36h | GR_DRAW_SPRITE | slot, x, y | 6 | slot (FFh transparent) into the back buffer, centred at x,y |
| 39h | GR_SAVE_SPRITE_BG | slot, x, y | 6 | back buffer 16x16 centred at x,y into the slot |
| 3Ch | | | 0 | nothing |
| 3Fh | GR_ANIMATE_TILES | | 0 | shore animation |
| 42h | GR_PUT_TILE | tile, x, y | 6 | tile into the back buffer at x,y |
| 45h | GR_COPY_TO_SCREEN | x0, y0, x1, y1, dx, dy | 0Ch | back buffer rectangle to the screen at dx,dy |
| 48h | GR_FADE_TO_SCREEN | x0, y0, x1, y1 | 8 | same rectangle, same place, pixel by pixel in random order |
| 4Bh | GR_SET_ANIM_FRAME | tile, frame | 4 | the tile shows the frame's pixels |
| 4Eh | GR_CODEX_EFFECT | | 0 | swirl of the map window |

No entry returns anything else in AX/DX that the game uses.

### 2Dh / 42h: tiles

* The tile's pixels are made resident (tile loader below), then drawn 16x16: format 0
  copied, format 1 with FFh transparent, formats 2-3 as packed runs.
* **2Dh** looks the format up with the tile number **unmasked** (`[1088h + tile]`), then
  loads `tile & 7FFh`. Destination stride 320.
* **42h** masks the tile with 7FFh for both. Bits 11-12 of the tile select an outline
  mode: only the tile's pixels equal to 0 (black) are drawn, in color 0Bh (bits = 800h),
  0Ch (1000h) or 09h (1800h); all others are transparent. Bits 13-14 are ignored. The
  game's MagicalHalo uses 1800h (target, light blue) and 1000h (caster, light red). The
  outline is built in the unpack buffer (below).
* Destination: back buffer `pWorkBuf + y*176 + x`, stride 176. No clipping anywhere.

### The tile loader (2249h) and the unpack buffer (23C6h)

* Tile pointer 0 (object tile not loaded): evict the least recently used page (every tile
  of its first-tile/count list whose pointer is >= the cache start is set to 0; tiles
  aliased by 4Bh to map tiles are left alone), call `DISK_confirm(5)`,
  `OSI_open("objtiles.vga")`, `OSI_read(h, blockofs[block] * 16, 7F0h, page+1:0)`,
  `OSI_close(h)`. The block's tiles are the run of tiles around this one (from the first
  tile >= 200h going down, up to 7FFh going up) with the same block number; each one's
  pointer is set to its place in the page **only if it is 0** (an aliased tile keeps its
  alias, but its size still counts), and the page header gets the first tile and count.
* Then, for a tile in the cache (pointer >= cache start), its page (`((ptr - start) &
  FF80h) + start`) becomes the most recently used: if it is the LRU page the LRU pointer
  just moves to the next page; else unless it already is the MRU (LRU's previous) it is
  unlinked and inserted before the LRU page.
* The unpack buffer (used by 33h and the outline mode of 42h): the tile is made resident,
  then the **LRU page itself** is emptied (its tiles evicted, count 0, LRU pointer not
  moved) and its data area receives the tile as 16x16 bytes: copied if format bit 1 is
  clear, else filled with FFh and the packed runs unpacked (16 wide, offsets & FFh).
  Note: the format byte is looked up with the caller's tile number (unmasked in 33h).

### 33h, 36h, 39h: sprites

* **33h** stores `pivotX*8`, `pivotY*8` at CS:26E0/26E2 first. Slots >= 16 (signed)
  return. The slot check (2764h) always succeeds: it ends with `CLC` before its `JNZ`.
  A **negative slot is not rejected** and writes to the segment read at `26C0h + 2*slot`.
  For each destination pixel (row v, column u, both -15, -13, ..., 15 in half pixels):
  `sx = u*cos - v*sin + pivotX*8`, `sy = u*sin + v*cos + pivotY*8` (8-bit signed
  products, sin/cos = table[rot & 7Fh], table[(rot+20h) & 7Fh], 16-bit sums); source
  column `(sx << 1) >> 8` and row `(sy << 1) >> 8` (bytes); both < 16: the unpacked tile's
  pixel, else FFh. So rot is 1/128 turn and the pivot is in 1/16 pixel (7Fh ≈ centre).
* **36h** (slot unsigned < 16): transparent 16x16 at back buffer `y*176 + x - 588h`.
* **39h** (slot unsigned < 16): offset `pWorkBuf.off + y*176 + x - 588h` (16 bits); if
  negative or >= 6EA0h, nothing; else 16 rows of 16 bytes into the slot. The bound
  includes pWorkBuf's offset.
* Slots: the game uses 0-7 for saved backgrounds, 8-15 for missile images.

### 3Fh and 4Bh: animation

* **4Bh**(tile, frame): if `frame >= 200h` (unsigned) nothing. Else `tileptr[tile] =
  tileptr[frame]` and `format[tile] = (format[tile] & 0Ch) | (format[frame] & 3)`. The tile
  shares the frame's pixels (no copy). `tile` is not range checked.
* **3Fh**: for each of 32 pairs (source 0Bh,0Bh,0Dh,0Dh,0Fh,0Fh,09h,09h,... into
  destination 10h..2Fh), copy the source tile's pixels into the destination tile's
  through mask `k*40h` of ANIMMASK. It writes into the tiles' memory, so it follows
  4Bh aliases. Does not look at `animOn`.

### 45h, 48h: back buffer to screen

* **45h**: x0 rounded down to even, x1 up to odd, dx rounded down to even (word copies);
  rows y0..y1 (`y1 - y0 + 1` in 16 bits; 0 rows = 65536).
* **48h**: w = x1-x0+1, h = y1-y0+1; n = bits of `(w*h - 1) & FFFFh`; taps =
  `[28F0h + 2*(n-2)]`; s = 1; repeat: q = s / w, r = s % w (unsigned DIV); if
  `(int16)q < h` copy the pixel (x0+r, y0+q) from the back buffer to the same place on
  the screen; `s = s & 1 ? (s >> 1) ^ taps : s >> 1`; until s = 1; then pixel (x0,y0).
  Every pixel of the rectangle ends up copied (maximal-length taps). w*h <= 2 never
  returns to 1 (hang) and w = 0 divides by zero; the game only uses (8,8,167,167).

### 4Eh: the codex effect

For R = 58h, 54h, ..., 4 and rlim = min(R, 4Ch); 12 times (angle offset +8 each, from
0): 8 arms with angle `start[k] - offset`, incremented after each use; for r = 0 .. rlim-1:
`y = 58h + (tri(a - 80h) * r >> 6)`, `x = 58h + (tri(a) * r >> 6)`, with
`tri(a) = (a & 1FFh) < 100h ? 80h - (a & 1FFh) : (a & 1FFh) - 180h` (a triangle wave; 16-bit
signed product, arithmetic shift). If x, y are within `[58h - rlim, 58h + rlim)` the
word-aligned screen offset of (x,y) is remembered in slot `[r & 7][k]` (the 8x8 table is
reset to FFFFh for each of the 12 passes, but a slot is **not** cleared when the point
falls outside, so it keeps the position from 8 steps before), and the value is the
word at that aligned screen offset if x and y are both in 10h..9Fh, else the word in the
back buffer at row `y` if only y is outside 10h..9Fh, but at row `58h - rlim` if x is
outside (**bug**: CX still holds the bound). Outside the window the value is 0. The value
is then written (word) at the position remembered in slot `[(r+1) & 7][k]`, i.e. 7 radius
steps further in, if any. In DOS this is visible as an animation (no delays, CPU speed);
the C produces the same final screen.

### 12h, 15h details

* **12h** with a width of 1 in XOR mode: after XORing the byte, the word loop runs with
  CX = 0, i.e. 65536 times, XORing the whole 64 KB segment twice: no visible change.
* **15h** with dy > 0 does nothing (it adds `[2008h + dy]` to SI, not `[2008h + 2*dy]`, and
  returns). dy = 0 nothing. Height = |dy|: 65536 rows are copied (the segment wraps);
  height < |dy|: 65535 rows (and in practice a word access at offset FFFFh, see below).
  The game only scrolls by -8 its text windows (CON_ScrollUpCb), all 10 rows or more.

### The mouse entries (0Ch, 0Fh, 30h)

MOUSE.ASM pushes: `surface` (near pointer, `Mouse.surface`), `saveBuf` (far, 0Ch/0Fh only),
`posX`, `posY`, `shapes` (far), `shapeIndex`. The surface is `struct tSurface` (mouse.h):
`segment`, far `pScanlines` (offset of each line in the segment), `right, left, top,
bottom` (inclusive, compared signed). The game's surfaces: `Screen` (A000h, lines
y*320, 0..319 x 0..199) and `MapSurface` (ScratchBuf's segment, lines
`ScratchBuf.off + 588h + y*176`, 0..159 x 0..159).

* Shape = `shapes + dword[shapes + 4 + index*4]` (32-bit linear sum, normalised).
* **0Ch/0Fh**: rectangle x `posX-left .. posX+right`, y `posY-up .. posY+down`, clipped to
  the surface (returns if entirely outside); the save buffer holds the whole rectangle
  (stride = full width), only the visible part is copied (surface→buffer for 0Ch,
  buffer→surface for 0Fh). No shape index check.
* **30h**: `index*4 >= (int16)word[shapes+4]` → nothing. If the whole extent is inside
  the surface, runs are drawn unclipped (line `posY+dy`, column `posX+dx`). Otherwise each
  run is clipped: rows outside top..bottom skipped, columns clipped left/right; an RLE run
  that is cut is first unpacked into CS:2F64 (320 bytes before code; longer runs would
  overwrite code). In the clipped path a run word whose `n2 >> 1` is 0 also ends the shape.

## Things to know

* **Word access at offset FFFFh**: 18h, 2Dh (opaque), 42h (opaque), 45h, 15h, 12h and the
  mouse copy with `MOVSW`/`STOSW`. If a row crosses the end of the 64 KB segment at an odd
  offset, a 286 or later raises #GP (int 0Dh in real mode: DOS hangs), an 8086 wraps
  within the segment. The C wraps byte by byte (8086 behaviour). The game never does it
  (all its rectangles are on the screen / in the back buffer).
* The driver never clips; out-of-range coordinates write elsewhere in the segment, and
  out-of-range y index the `y*320` table past its end (into code). The C keeps the
  driver's segment at the DOS offsets so that this reads the same values when the
  unpacked image is attached (`mcga_attach_image`); without it, the code bytes read 0.
* `animOn` and `unknown0d` are not used by this driver (they may be by the EGA/CGA ones).
* The fade and codex effects run without any delay: their speed in DOS was the CPU's.
* `GR_CYCLE_PALETTE` (27h) would unbalance the stack (plain `RETF`); only called for
  non-VGA adapters.
* Entry 00h: the BIOS mode set also loads the default VGA palette; the game replaces it
  right away (`LoadPalette`, U6PAL via int 10h 1012h). The C sets the IBM/DOSBox default
  palette in `vga_palette` (not checked by the test: unicorn has no BIOS).

## The C (port/gfx/mcga.c)

* One function per entry, `GRDRV_xx`, arguments in push order (mcga.h has the macro
  mapping and the mouse prototypes). `GRDRV_00` returns a `struct McgaInfo *` laid out as
  the host compiles `struct tGr` (gr.h with `far` empty).
* The screen is a 64 KB buffer (`mcga_screen()`: A000:0000-FFFF, the first 64000 bytes
  are shown); `plat_video_source(screen, vga_palette)` at init, `plat_video_dirty()`
  after any entry that changed it.
* The driver's segment is a 64 KB array with the tables at their DOS offsets (built-in
  copies of the file's data; `mcga_attach_image()` to start from the real image).
* Far pointers from the game are converted back to segment:offset (`dos_linear`,
  normalised offset 0..15) and all offset arithmetic is 16-bit, so wrapping matches DOS
  as long as the DOS pointer was normalised too (the game's are: farmalloc/`__MemAlloc`).
  Segments (driver memory, pages, sprites, surface segment) address the DOS arena,
  A000h is redirected to the screen buffer. A pointer outside the arena is used directly.
* Callback types the C calls through tGr: `int16 DISK_confirm(int16)`,
  `int16 LoadFileGr(char *, void *, int16)`, `int16 OSI_open(char *)`,
  `uint32 OSI_read(int16, uint32, uint32, void *)`, `int16 OSI_close(int16)`.
  **LoadFileGr is given destinations inside the driver's own segment** (`drv + 88h`,
  `drv + 1088h`: host memory outside the arena): the port's LoadFileGr/`decompress`
  must accept a non-arena destination.
* The callbacks can re-enter the driver (`DISK_confirm` may print); the C reads the
  driver's variables after each callback at the same points as the original.

## Test (port/tests/mcga_diff.py)

The original image runs in unicorn (real mode, 1 MB) with stubs for the five callbacks
(serving the files from the game data) and int 10h; the C is built with
`port/tests/mcga_shim.c` (an arena and platform stubs) and called through ctypes. Both
get the same memory (font, PAPER.BMP, a portrait-sized pixmap, U6MCGA.PTR, synthetic
pointer shapes with RLE runs, random back buffer) and the same calls. After every call
the test compares the 64 KB screen, the arena from 30000h to A0000h (back buffer, driver
memory: map tiles, cache pages and their headers, animmask, sprites; the mouse save
buffer; pixmaps), low memory, the driver's segment (tables, tile pointers, LRU, sprite
segments, pivots, color, the mouse's RLE buffer; all of it when the image is attached),
`pDriverMem`, the exact sequence of callback calls and arguments, return values, the
stack balance of 27h, and that `plat_video_dirty()` was called when the screen changed.

Three configurations, 4000 random calls each plus game-like frames (map window with
object tiles and outlines, frame, animation, missiles, sky, console scroll, inventory
digits, storm cloak XOR noise, spell flash) and edge cases (every 7th tile on screen and
back buffer, pointer shapes at all clipping edges of both surfaces, sprite positions
around the bounds, rotations, odd-width pixmap, scroll/fill corner cases, the codex
effect, fades of several sizes, out-of-range slots/frames):

1. image attached, 304 KB of driver memory (91 cache pages), wild arguments;
2. image attached, 3000h paragraphs (35 pages: constant eviction), wild arguments;
3. built-in tables, in-range arguments.

Result: **all 55,532 calls match** (seed 6; other seeds too). Every byte of the original
code is executed except dead bytes (orphan `RETF`s at 21F9, 2455, 2A66, 2AD9, 2ADC and
padding).

Not tested: word accesses at offset FFFFh (unicorn neither wraps nor faults), 48h with
w*h <= 2 or w = 0 (DOS hangs / divides by zero; the C gives up after 65536 steps), the
BIOS default palette.
