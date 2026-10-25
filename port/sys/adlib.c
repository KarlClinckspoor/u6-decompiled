/* The AdLib music driver U6ADLIB.DRV rewritten in C, playing through an emulated OPL2
 * (Nuked OPL3, port/third_party/nuked-opl3, in its OPL2-compatible mode).
 *
 * In DOS the driver is a small binary (LZW compressed in the game folder, 1662 bytes
 * unpacked) that U.EXE and GAME.EXE load and call through a far pointer whose offset
 * selects the entry (U's MUS(o), GAME's MUS_DRV(o)):
 *
 *   0  init(port)    the card's i/o port (388h): enables the waveforms (register 1 = 20h),
 *                    melodic mode (BDh = 0), CSM/NOTE-SEL off (8 = 0)
 *   3  silence()     every channel's frequency to 0, key off
 *   6  play(song)    starts a song (a .M file, LZW-loaded): sets the PIT to 60 Hz and
 *                    hooks int 8; the handler runs one step of the song each tick and
 *                    calls the BIOS's handler 18.2 times a second, as before
 *   9  stop()        unhooks int 8, the PIT back to 18.2 Hz, then silence()
 *
 * (GAME.EXE also calls entry 0Ch, which only the Roland driver has: a no-op here.) Each
 * entry pops its own arguments (pascal).
 *
 * The port runs the handler's step in the audio callback, 60.0011 times a second of
 * generated sound (1193182 / 4DAEh), so the music keeps its tempo exactly whatever the
 * host does; the game's thread only starts and stops songs (under the audio lock).
 *
 * A song is a stream of commands, one byte (high nibble: the command, low nibble: the
 * channel 0-8) and their operands:
 *   0c n      frequency of note n (bits 0-4 the note in the table, 5-7 the octave),
 *             key off                                               (sets the channel's freq)
 *   1c n      note on: the vibrato restarts, key off then key on at note n
 *   2c n      note n with key on, without a new attack (a slur)
 *   3c v      the carrier's level (OPL 40h+op: 0 loud .. 3Fh silent), stops a volume slide
 *   4c v      the modulator's level
 *   5c d      pitch bend: d (signed) added to the frequency every tick
 *   6c xy     vibrato: x its depth (ticks of a half period), y its amount
 *   7c i      instrument i (defined by 83h) on the channel
 *   81 n w    repeat n times from offset w (in the song); 0Fx returns   (16 levels)
 *   82 n      wait n ticks (0: 256): the end of this tick's commands
 *   83 i ...  instrument i: the 11 bytes that follow (OPL registers 20h, 40h, 60h, 80h, E0h
 *             of the modulator, then of the carrier, then C0h)
 *   85 cx     volume slide up: channel c, one step every x+1 ticks (to 3Fh: silent)
 *   86 cx     volume slide down (to 0)
 *   Ex        mark: where the song restarts when it ends
 *   Fx        end of a repeat (or, outside one, of the song: back to the mark)
 *   other commands (8x other than 1,2,3,5,6; 9x-Dx) do nothing.
 *
 * The driver keeps its variables in its own segment, after its jump table; this keeps
 * them at the same offsets in drv[] (initialised from the binary's data), so that the
 * odd cases behave as in DOS: a note above 23 reads its frequency from the command table
 * that follows the note table, more than 16 nested repeats ignore the 81h (its operands are
 * then read as commands), an instrument number above 31 lands on the driver's other
 * variables.
 */
#include <stdint.h>
#include <string.h>

#include "dosmem.h"
#include "../platform/plat.h"
#include "../third_party/nuked-opl3/opl3.h"

/* the driver's segment, offsets 0000-01C5 (the jump table, the variables, the tables) */
static uint8_t drv[0x400];
static const uint8_t drv_tables[] = {	/* 015Ah-01C5h of U6ADLIB.DRV */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x03,	/* 15A: int 8, busy, PIT sum, port */
	0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12,	/* 164: each channel's modulator */
	0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15,	/* 16D: and carrier */
	0x00, 0x00, 0x58, 0x01, 0x82, 0x01, 0xb0, 0x01, 0xcc, 0x01, 0x03, 0x02, 0x41, 0x02,	/* 176: notes */
	0x86, 0x02, 0x00, 0x00, 0x6a, 0x01, 0x96, 0x01, 0xc7, 0x01, 0xe4, 0x01, 0x1e, 0x02,
	0x5f, 0x02, 0xa8, 0x02, 0x00, 0x00, 0x47, 0x01, 0x6e, 0x01, 0x9a, 0x01, 0xb5, 0x01,
	0xe9, 0x01, 0x24, 0x02, 0x66, 0x02,
	0xe5, 0x03, 0xfe, 0x03, 0x2e, 0x04, 0x4b, 0x04, 0x63, 0x04, 0x78, 0x04, 0x88, 0x04,	/* 1A6: commands */
	0xa4, 0x04, 0xbd, 0x04, 0xd4, 0x03, 0xd4, 0x03, 0xd4, 0x03, 0xd4, 0x03, 0xd4, 0x03,
	0x6e, 0x05, 0x75, 0x05,
};

/* the variables (offsets in the driver's segment) */
#define RET_POS     0x00C	/* [16] words: a repeat's return position (from the song's start) */
#define REP_COUNT   0x04C	/* [16] words: its count */
#define REP_START   0x08C	/* [16] words: its start */
#define REP_SP      0x0CC	/* word: 2 * the repeats open */
#define SONG_OFS    0x0CE	/* dword: the song */
#define SONG_SEG    0x0D0
#define POS         0x0D2	/* word: where the next tick reads */
#define MARK        0x0D4	/* word: where the song restarts */
#define FREQ        0x0D6	/* [9] words: each channel's A0h/B0h value (2000h: key on) */
#define VIB_PHASE   0x0E8	/* [9] */
#define VOLUME      0x0F1	/* [9] the carrier's level */
#define SLIDE_RATE  0x0FA	/* [9] */
#define SLIDE_COUNT 0x103	/* [9] */
#define BEND        0x10C	/* [9] signed */
#define VIB_DEPTH   0x115	/* [9] */
#define VIB_AMOUNT  0x11E	/* [9] */
#define VIB_DOWN    0x127	/* [9] */
#define SLIDE_DIR   0x130	/* [9] +1, -1 (FFh) or 0 */
#define WAIT        0x139	/* byte: ticks to the next commands */
#define INSTR       0x13A	/* [] words: each instrument's 11 bytes (an offset in the song) */
#define MODULATOR   0x164	/* [9] */
#define CARRIER     0x16D	/* [9] */
#define NOTES       0x176	/* [24] words, then the command table */

#define W(o) ((uint16_t)(drv[(o) & 0x3ff] | drv[((o) + 1) & 0x3ff] << 8))
#define SETW(o, v) setw((o), (uint16_t)(v))

static void setw(unsigned o, uint16_t v)
{
	drv[o & 0x3ff] = (uint8_t)v;
	drv[(o + 1) & 0x3ff] = (uint8_t)(v >> 8);
}

static opl3_chip chip;
static int chip_rate;
static int playing;	/* int 8 is hooked */
static uint32_t tick_frac;	/* the 60 Hz ticks, in samples (fixed point) */
static int volume = 100;

/* what the chip's registers hold: written again when the chip is reset for another rate */
static uint8_t opl_regs[256], opl_set[256];

static void opl(uint8_t reg, uint8_t val)
{
	opl_regs[reg] = val;
	opl_set[reg] = 1;
	OPL3_WriteReg(&chip, reg, val);
}

static void chip_reset(int rate)
{
	int r;
	OPL3_Reset(&chip, (uint32_t)rate);
	chip_rate = rate;
	for (r = 0; r < 256; r++)
		if (opl_set[r])
			OPL3_WriteReg(&chip, (uint16_t)r, opl_regs[r]);
}

/* the song's bytes: es:si, 16-bit offsets in its segment */
static uint8_t song_byte(uint16_t si)
{
	return *dos_seg_ptr(W(SONG_SEG), si);
}

/* 02B1: channel bl's frequency registers from cx */
static void set_freq(int ch, uint16_t cx)
{
	opl((uint8_t)(0xA0 + ch), (uint8_t)cx);
	opl((uint8_t)(0xB0 + ch), (uint8_t)(cx >> 8));
}

/* 02A9: the carrier's level */
static void set_volume(int ch, uint8_t v)
{
	opl((uint8_t)(drv[CARRIER + ch] + 0x40), v);
}

/* 0348: note byte n to an A0h/B0h value (octave in bits 10-12) */
static uint16_t note_freq(uint8_t n)
{
	return (uint16_t)(W(NOTES + (n & 0x1f) * 2) + ((n & 0xe0) << 5));
}

/* 02C4: instrument at si into channel ch */
static void load_instrument(int ch, uint16_t si)
{
	static const uint8_t regs[5] = {0x20, 0x40, 0x60, 0x80, 0xE0};
	uint8_t mod = drv[MODULATOR + ch], car = drv[CARRIER + ch];
	int i;
	for (i = 0; i < 5; i++)
		opl((uint8_t)(mod + regs[i]), song_byte(si++));
	for (i = 0; i < 5; i++)
		opl((uint8_t)(car + regs[i]), song_byte(si++));
	opl((uint8_t)(0xC0 + ch), song_byte(si));
}

/* 0383: entry 3 */
static void silence(void)
{
	int ch;
	for (ch = 8; ch >= 0; ch--) {
		SETW(FREQ + ch * 2, 0);
		set_freq(ch, 0);
	}
}

/* 03A0: one tick of the int 8 handler */
static void tick(void)
{
	uint16_t si = W(POS);
	int ch;
	long guard = 0;

	if (drv[WAIT] == 0 || --drv[WAIT] == 0) {
		/* (a song without a wait would hang DOS in its timer interrupt; the port gives up
		 * after 64K commands in one tick) */
		while (guard++ < 0x10000) {
			uint8_t c = song_byte(si++), op;
			ch = c & 0x0f;
			switch (c >> 4) {
			case 0x0:	/* frequency, key off */
				op = song_byte(si++);
				SETW(FREQ + (ch & 0xf) * 2, note_freq(op));
				set_freq(ch, note_freq(op));
				continue;
			case 0x1: {	/* note on */
				uint16_t f;
				drv[VIB_DOWN + ch] = 0;
				drv[VIB_PHASE + ch] = 0;
				op = song_byte(si++);
				f = note_freq(op);
				set_freq(ch, f);	/* key off first: a new attack */
				SETW(FREQ + ch * 2, f | 0x2000);
				set_freq(ch, (uint16_t)(f | 0x2000));
				continue;
			}
			case 0x2: {	/* note, no new attack */
				uint16_t f = (uint16_t)(note_freq(song_byte(si++)) | 0x2000);
				SETW(FREQ + ch * 2, f);
				set_freq(ch, f);
				continue;
			}
			case 0x3:	/* the carrier's level */
				drv[SLIDE_DIR + ch] = 0;
				drv[VOLUME + ch] = song_byte(si++);
				set_volume(ch, drv[VOLUME + ch]);
				continue;
			case 0x4:	/* the modulator's level */
				opl((uint8_t)(drv[MODULATOR + ch] + 0x40), song_byte(si++));
				continue;
			case 0x5:	/* pitch bend */
				drv[BEND + ch] = song_byte(si++);
				continue;
			case 0x6:	/* vibrato */
				op = song_byte(si++);
				drv[VIB_DEPTH + ch] = (uint8_t)(op >> 4);
				drv[VIB_AMOUNT + ch] = (uint8_t)(op & 0x0f);
				continue;
			case 0x7: {	/* instrument */
				uint8_t i = song_byte(si++);
				load_instrument(ch, W(INSTR + i * 2));
				continue;
			}
			case 0x8:
				switch (c & 0x0f) {
				case 1: {	/* repeat */
					uint16_t sp = W(REP_SP);
					if (sp >= 0x20)
						continue;
					SETW(REP_COUNT + sp, song_byte(si));
					si++;
					SETW(RET_POS + sp, (uint16_t)(si - W(SONG_OFS) + 2));
					si = (uint16_t)((song_byte(si) | song_byte((uint16_t)(si + 1)) << 8) + W(SONG_OFS));
					SETW(REP_START + sp, si);
					SETW(REP_SP, (uint16_t)(sp + 2));
					continue;
				}
				case 2:		/* wait */
					drv[WAIT] = song_byte(si++);
					SETW(POS, si);
					goto effects;
				case 3: {	/* an instrument */
					uint8_t i = song_byte(si++);
					SETW(INSTR + i * 2, si);
					si = (uint16_t)(si + 11);
					continue;
				}
				case 5:		/* volume slide up */
				case 6: {	/* and down */
					uint8_t b = song_byte(si++);
					int c2 = b >> 4;
					drv[SLIDE_DIR + c2] = (uint8_t)((c & 0x0f) == 5 ? 1 : 0xff);
					drv[SLIDE_COUNT + c2] = drv[SLIDE_RATE + c2] = (uint8_t)((b & 0x0f) + 1);
					continue;
				}
				default:
					continue;
				}
			case 0xE:	/* mark */
				SETW(MARK, si);
				continue;
			case 0xF: {	/* end of a repeat, or of the song */
				uint16_t sp = W(REP_SP);
				if (!sp) {
					si = W(MARK);
					continue;
				}
				SETW(REP_COUNT + sp - 2, (uint16_t)(W(REP_COUNT + sp - 2) - 1));
				if (W(REP_COUNT + sp - 2)) {
					si = W(REP_START + sp - 2);
				} else {
					si = (uint16_t)(W(RET_POS + sp - 2) + W(SONG_OFS));
					SETW(REP_SP, (uint16_t)(sp - 2));
				}
				continue;
			}
			default:	/* 9x-Dx */
				continue;
			}
		}
		SETW(POS, si);
	}
effects:
	/* 05A1: pitch bends, vibratos, volume slides, channel 8 down to 0 */
	for (ch = 8; ch >= 0; ch--) {
		uint8_t dir;
		if (drv[BEND + ch]) {
			uint16_t f = (uint16_t)(W(FREQ + ch * 2) + (int8_t)drv[BEND + ch]);
			SETW(FREQ + ch * 2, f);
			set_freq(ch, f);
		} else if (drv[VIB_AMOUNT + ch] && (W(FREQ + ch * 2) & 0x2000)) {
			int8_t al = (int8_t)drv[VIB_PHASE + ch];
			int16_t ax;
			if (al >= (int8_t)drv[VIB_DEPTH + ch])
				drv[VIB_DOWN + ch] = 1;
			else if (al <= 0)
				drv[VIB_DOWN + ch] = 0;
			if (drv[VIB_DOWN + ch])
				al = (int8_t)(al - 2);
			al = (int8_t)(al + 1);
			drv[VIB_PHASE + ch] = (uint8_t)al;
			ax = (int16_t)(al - (drv[VIB_DEPTH + ch] >> 1));
			ax = (int16_t)((int8_t)ax * (int8_t)drv[VIB_AMOUNT + ch]);
			set_freq(ch, (uint16_t)(W(FREQ + ch * 2) + ax));
		}
		dir = drv[SLIDE_DIR + ch];
		if (dir && --drv[SLIDE_COUNT + ch] == 0) {
			int8_t v;
			drv[SLIDE_COUNT + ch] = drv[SLIDE_RATE + ch];
			v = (int8_t)(dir + drv[VOLUME + ch]);
			if (v > 0x3f) {
				v = 0x3f;
				drv[SLIDE_DIR + ch] = 0;
			} else if (v < 0) {
				v = 0;
				drv[SLIDE_DIR + ch] = 0;
			}
			drv[VOLUME + ch] = (uint8_t)v;
			set_volume(ch, (uint8_t)v);
		}
	}
}

/* ---- the audio side: runs in the audio callback, under its lock ---- */

#define TICK_DEN 1193182u	/* ticks are 4DAEh PIT periods: rate * 19886 / 1193182 samples */

static void render(int16_t *out, int frames, int rate)
{
	int i;
	if (rate != chip_rate)
		chip_reset(rate);
	for (i = 0; i < frames; i++) {
		int16_t s[2];
		int32_t v;
		if (playing) {
			tick_frac += TICK_DEN;
			while (tick_frac >= (uint32_t)rate * 19886u) {
				tick_frac -= (uint32_t)rate * 19886u;
				tick();
			}
		}
		OPL3_GenerateResampled(&chip, s);
		v = out[i] + (int32_t)s[0] * volume / 100;
		out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
	}
}

static void attach(void)
{
	static int done;
	if (!done) {
		done = 1;
		memcpy(drv + 0x15A, drv_tables, sizeof drv_tables);
		chip_reset(44100);
		plat_set_music(render);
	}
}

/* ---- the entries ---- */

void MUSDRV_0(int port)
{
	attach();
	plat_audio_lock();
	SETW(0x162, (uint16_t)port);
	opl(1, 0x20);
	opl(0xBD, 0);
	opl(8, 0);
	plat_audio_unlock();
}

void MUSDRV_3(void)
{
	attach();
	plat_audio_lock();
	silence();
	plat_audio_unlock();
}

void MUSDRV_6(void *song)
{
	attach();
	plat_audio_lock();
	memset(drv + 0x0C, 0, 0x12E);
	SETW(SONG_OFS, dos_fp_off(song));
	SETW(SONG_SEG, dos_fp_seg(song));
	SETW(POS, dos_fp_off(song));
	SETW(MARK, dos_fp_off(song));
	tick_frac = 0;
	playing = 1;
	plat_audio_unlock();
}

void MUSDRV_9(void)
{
	attach();
	plat_audio_lock();
	playing = 0;
	silence();
	plat_audio_unlock();
}

void MUSDRV_0xc(int channel, int a, int b)
{
	(void)channel;
	(void)a;
	(void)b;
}

/* the music's volume, 0-100 (the settings) */
void adlib_set_volume(int v)
{
	volume = v < 0 ? 0 : v > 100 ? 100 : v;
}

/* the driver's variables (tests: port/tests/adlib_diff.py compares them with the original's) */
uint8_t *adlib_state(void)
{
	return drv;
}
